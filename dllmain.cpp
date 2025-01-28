// dllmain.cpp : Defines the entry point for the DLL application.
#include "framework.h"
#include <intrin.h>
#include <unordered_map>
#include <fstream>
#include "hks.h"
#include "pe.h"
#include "MemoryModule.h"

#if ENABLE_STEAMAPI
#include "steam.h"
#endif
#include "security.h"

// TODO(anthony): credits ssno (TAC research)
// TODO(anthony): repository rebase, cleanup, and readme for v1.0
// -- mention that friends list and game chat do not work
// TODO(anthony): record tutorial video for setting it up from scratch, including how to get the windows store exe files needed

// FUTURE MAYBE: steamchat (requires steamlobbies)
// FUTURE MAYBE: friends list
// FUTURE MAYBE(anthony): fix mic being set to 0%


HMODULE __thismodule;
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved);
bool g_needsUpdatePrompt = false;

#if REMAP_EXPERIMENT
thread_local char tls_data[0x2000];
#pragma optimize("", off)
void NTAPI tls_callback(PVOID DllHandle, DWORD dwReason, PVOID)
{

}
#pragma optimize("", on)

#pragma comment (linker, "/INCLUDE:_tls_used")
#pragma comment (linker, "/INCLUDE:tls_callback_func")
#pragma const_seg(".CRT$XLF")
EXTERN_C const PIMAGE_TLS_CALLBACK tls_callback_func = tls_callback;
#pragma const_seg()
#endif

bool is_remapped_exe = false;
bool dbg_is_console_available = false;
bool dbg_is_attached = false;
HANDLE dbg_hWriteConsole = NULL, dbg_hReadConsole = NULL;

typedef unsigned long long(__fastcall* ZwContinue_t)(PCONTEXT ThreadContext, BOOLEAN RaiseAlert);
ZwContinue_t ZwContinue = reinterpret_cast<ZwContinue_t>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "ZwContinue"));

typedef LONG(NTAPI* NtSuspendProcess)(IN HANDLE ProcessHandle);
void SuspendProcess()
{
    HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());

    NtSuspendProcess pfnNtSuspendProcess = (NtSuspendProcess)GetProcAddress(
        GetModuleHandle("ntdll"), "NtSuspendProcess");

    pfnNtSuspendProcess(processHandle);
    CloseHandle(processHandle);
}

void noprange(__int64 addy, __int32 size)
{
    unsigned char* data = new unsigned char[size];
    memset(data, 0x90, size);

    chgmem(addy, size, data);
    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)addy, size);

    delete[] data;
    data = 0;
}

int istrcmp_(const char* s1, const char* s2)
{
    if (!s1 || !s2)
    {
        return 0;
    }

    int c = 0;
    while (s1[c] && s2[c] && (tolower(s1[c]) == tolower(s2[c]))) c++;

    if (s1[c] || s2[c])
    {
        return 1;
    }

    return 0;
}

void MSG_Init(msg_t* buf, char* data, uint32_t length)
{
    if (!*(uint32_t*)REBASE(0x190B0990)) // msg_init
    {
        ((void(__fastcall*)())REBASE(0x2225880))(); // MSG_InitHuffman
    }
    memset(buf, 0LL, sizeof(msg_t));
    buf->data = data;
    buf->maxsize = length;
    buf->readOnly = 0;
    buf->splitData = 0LL;
    buf->splitSize = 0;
}

void MSG_InitReadOnly(msg_t* buf, char* data, uint32_t length)
{
    MSG_Init(buf, data, length);
    buf->readOnly = 1;
    buf->maxsize = length;
    buf->cursize = length;
}

void MSG_BeginReading(msg_t* msg)
{
    msg->overflowed = 0;
    msg->readcount = 0;
    msg->bit = 0;
}

uint8_t sanityCheckAndGetMsgType(msg_t* msg)
{
    uint8_t msgType = 0;
    if (MSG_ReadByte(msg) == '1')
    {
        msgType = MSG_ReadByte(msg);
    }
    return msgType;
}

char I_CleanChar(char character)
{
    if ((uint8_t)character == (uint8_t)0x92)
        return '\'';
    if (character == '%')
        return '.';
    return character;
}

void MSG_WriteData(msg_t* buf, const void* data, int length)
{
    auto newsize = length + buf->cursize;
    if (newsize > buf->maxsize)
    {
        buf->overflowed = 1;
    }
    else
    {
        memcpy(&buf->data[buf->cursize], data, length);
        buf->cursize = newsize;
    }
}

void MSG_WriteInt64(msg_t* buf, uint64_t data)
{
    auto newsize = sizeof(data) + buf->cursize;
    if (newsize > buf->maxsize)
    {
        buf->overflowed = 1;
    }
    else
    {
        *(uint64_t*)&buf->data[buf->cursize] = data;
        buf->cursize = newsize;
    }
}

void MSG_WriteUInt16(msg_t* buf, uint16_t data)
{
    auto newsize = sizeof(data) + buf->cursize;
    if (newsize > buf->maxsize)
    {
        buf->overflowed = 1;
    }
    else
    {
        *(uint16_t*)&buf->data[buf->cursize] = data;
        buf->cursize = newsize;
    }
}

void MSG_WriteString(msg_t* sb, const char* s)
{
    char string[1024];

    auto l = strlen(s);

    if (l > 1023)
    {
        MSG_WriteData(sb, "", 1);
        return;
    }
    
    for (int i = 0; i < l; i++)
    {
        string[i] = I_CleanChar(s[i]);
    }

    string[l] = 0;
    MSG_WriteData(sb, string, l + 1);
}

uint8_t MSG_GetByte(msg_t* msg, int _where)
{
    if (_where < msg->cursize)
    {
        return msg->data[_where];
    }
    return msg->splitData[_where - msg->cursize];
}

void MSG_GetBytes(msg_t* msg, int _where, uint8_t* dest, int len)
{
    for (int i = 0; i < len; ++i)
    {
        dest[i] = MSG_GetByte(msg, _where++);
    }
}

int MSG_ReadBit(msg_t* msg)
{
    int bit = msg->bit & 7;
    
    if (bit)
    {
        goto LABEL_5;
    }

    if (msg->readcount < msg->splitSize + msg->cursize)
    {
        msg->bit = 8 * msg->readcount++;
    LABEL_5:
        auto b = MSG_GetByte(msg, msg->bit >> 3);
        ++msg->bit;
        return (b >> bit) & 1;
    }

    msg->overflowed = 1;
    return -1;
}

uint8_t MSG_ReadByte(msg_t* msg)
{
    if (msg->readcount >= msg->splitSize + msg->cursize)
    {
        msg->overflowed = 1;
        return 0;
    }

    return MSG_GetByte(msg, msg->readcount++);
}

void MSG_ReadData(msg_t* msg, void* data, int len)
{
    auto newcount = len + msg->readcount;
    if (newcount > msg->cursize)
    {
        if (newcount > msg->splitSize + msg->cursize)
        {
            msg->overflowed = 1;
            // memset(data, 0xFF, len); they do this in the pdb but this causes certain crashes so... my impl will not be doing this!
        }
        else
        {
            auto cursize = msg->cursize - msg->readcount;
            if (cursize > 0)
            {
                memcpy(data, &msg->data[msg->readcount], cursize);
                len -= cursize;
                data = (char*)data + cursize;
            }
            if (len > 0)
            {
                memcpy(data, &msg->splitData[msg->readcount - msg->cursize], len);
            }
            msg->readcount = newcount;
        }
    }
    else
    {
        memcpy(data, &msg->data[msg->readcount], len);
        msg->readcount = newcount;
    }
}

uint64_t MSG_ReadInt64(msg_t* msg)
{
    uint64_t c = 0;
    auto newcount = msg->readcount + 8;
    if (newcount > msg->splitSize + msg->cursize)
    {
        msg->overflowed = 1;
        return c;
    }
    MSG_GetBytes(msg, msg->readcount, (uint8_t*)&c, 8);
    msg->readcount = newcount;
    return c;
}

uint32_t MSG_ReadInt32(msg_t* msg)
{
    uint32_t c = -1;
    auto newcount = msg->readcount + 4;
    if (newcount > msg->splitSize + msg->cursize)
    {
        msg->overflowed = 1;
        return c;
    }
    MSG_GetBytes(msg, msg->readcount, (uint8_t*)&c, 4);
    msg->readcount = newcount;
    return c;
}

uint16_t MSG_ReadInt16(msg_t* msg)
{
    uint16_t c = -1;
    auto newcount = msg->readcount + 2;
    if (newcount > msg->splitSize + msg->cursize)
    {
        msg->overflowed = 1;
        return c;
    }
    MSG_GetBytes(msg, msg->readcount, (uint8_t*)&c, 2);
    msg->readcount = newcount;
    return c;
}

void MSG_WriteByte(msg_t* msg, uint8_t c)
{
    if (msg->cursize >= msg->maxsize)
    {
        msg->overflowed = 1;
    }
    else
    {
        msg->data[msg->cursize++] = c;
    }
}

char* MSG_ReadStringLine(msg_t* msg, char* string, uint32_t maxChars)
{
    for (auto l = 0; ; ++l)
    {
        auto c = MSG_ReadByte(msg);
        if (c == '\n' || c == -1)
        {
            c = 0;
        }

        if (l < maxChars)
        {
            string[l] = I_CleanChar(c);
        }

        if (!c)
        {
            break;
        }
    }

    string[maxChars - 1] = 0;
    return string;
}

void MSG_WriteBit0(msg_t* msg)
{
    if (msg->cursize < msg->maxsize)
    {
        auto msgBit = msg->bit;
        if (!(msgBit & 7))
        {
            msgBit = 8 * msg->cursize;
            msg->data[msg->cursize++] = 0;
        }
        msg->bit = msgBit + 1;
    }
    else
    {
        msg->overflowed = 1;
    }
}

void MSG_WriteBit1(msg_t* msg)
{
    if (msg->cursize < msg->maxsize)
    {
        auto msgBit = msg->bit;
        auto bit = msg->bit & 7;
        if (!(msgBit & 7))
        {
            msgBit = 8 * msg->cursize;
            msg->data[msg->cursize++] = 0;
        }
        msg->data[msgBit >> 3] |= 1 << bit;
        msg->bit = msgBit + 1;
    }
    else
    {
        msg->overflowed = 1;
    }
}

SessionClient* LobbySession_GetClientByClientNum(LobbySession* session, uint32_t index)
{
    if (index == 0xFFFFFFFF)
    {
        return NULL;
    }
    return (SessionClient*)(48llu * index + (uint64_t)session + 0xF8);
}

netadr_t LobbySession_GetClientNetAdrByIndex(LobbyType type, uint32_t clientnum)
{
    auto session = LobbySession_GetSession(type);
    auto client = LobbySession_GetClientByClientNum(session, clientnum);

    if (!client->activeClient)
    {
        return netadr_t();
    }

    return client->activeClient->sessionInfo[session->type].netAdr;
}

LobbyType LobbySession_GetControllingLobbySession(LobbyModule lobbyModule)
{
    return (LobbyType)(*(uint32_t*)(REBASE(0xBEFF3C0) + 0x2A58llu * lobbyModule) > 0);
}

LobbySession* LobbySession_GetSession(LobbyType t)
{
    return (LobbySession*)(REBASE(0xBEFF380) + 0x2A58llu * t);
}

XUID GetXUID(uint32_t controllerIndex)
{
    if (controllerIndex)
    {
        return **(XUID**)REBASE(0x33614D8);
    }
    return **(XUID**)REBASE(0x33614D0);
}

bool HasHost()
{
    LobbyType sessionType = LOBBY_TYPE_PRIVATE;
    if (LobbySession_GetControllingLobbySession(LOBBY_MODULE_CLIENT))
    {
        sessionType = LOBBY_TYPE_GAME;
    }

    const auto session = LobbySession_GetSession(sessionType);
    if (!session)
    {
        return false;
    }

    uint64_t sessPtr = (uint64_t)session;
    return *(XUID*)(sessPtr + 0x60) == GetXUID(0);
}

bool dwInstantHandleLobbyMessage(XUID senderID, const uint32_t controllerIndex, msg_t* msg)
{
    char msgBuf[2048]{ 0 };

    auto size = msg->cursize - msg->readcount;
    if (size >= 2048)
    {
        return 1;
    }

    MSG_ReadData(msg, msgBuf, size);

    if (msg->overflowed)
    {
        return 1;
    }

    return LobbyMsg_HandleIM(controllerIndex, senderID, msgBuf, size);
}

bool LobbyMsgRW_PrepReadMsg(LobbyMsg* lobbyMsg, char* data, int length)
{
    MSG_Init(&lobbyMsg->msg, data, length);
    lobbyMsg->msg.cursize = length;
    return LobbyMsg_PrepReadMsg(lobbyMsg);
}

IHOOK_HEADER(IsProcessorFeaturePresent, BOOL, (DWORD processorFeature))
{
    if (processorFeature == 0x17)
    {
        // force a crash
        *(__int64*)0x17 = processorFeature;
        *(__int64*)processorFeature = 0x17;
    }
    return IHOOK_ORIGINAL(IsProcessorFeaturePresent, (processorFeature));
}

__int64 __fn_ptr_hook = 0;
void InstallHook(void* func)
{
    auto kiuserexceptiondispatcher = (__int64)GetProcAddress(GetModuleHandleA("ntdll.dll"), "KiUserExceptionDispatcher");

    if (kiuserexceptiondispatcher)
    {
        if (*(__int32*)kiuserexceptiondispatcher == 0x58B48FC)
        {
            auto distance = *(DWORD*)(kiuserexceptiondispatcher + 4);
            auto ptr = (kiuserexceptiondispatcher + 8) + distance;
            __fn_ptr_hook = ptr;

            auto OldProtection = 0ul;
            VirtualProtect(reinterpret_cast<void*>(ptr), 8, PAGE_EXECUTE_READWRITE, &OldProtection);
            *reinterpret_cast<void**>(ptr) = func;
            VirtualProtect(reinterpret_cast<void*>(ptr), 8, OldProtection, &OldProtection);
        }
    }
}

std::unordered_map<INT64, CONTEXT> SavedExceptions;
DWORD lck_dumping = NULL;
uint64_t old_address_step = 0;
void ExceptHook(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord)
{
    bool b_fatal = false;

    if (security::handle_exception(ExceptionRecord, ContextRecord))
    {
        ZwContinue(ContextRecord, false);
    }
    
    if ((ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) || (ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE) || (ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION))
    {
        if (IsDebuggerPresent())
        {
            return;
        }

        INT64 faultingModule = 0;
        char module_name[MAX_PATH];

        if (lck_dumping)
        {
            INT64 addy = (INT64)ExceptionRecord->ExceptionAddress;
            SavedExceptions[addy] = *ContextRecord;
        }
        else
        {
            lck_dumping = GetCurrentThreadId();

            FILE* f;
            f = fopen(CRASH_LOG_NAME, "a+"); // a+ (create + append)
            if (!f)
            {
                ExitProcess(-444);
            }

            ALOG("Crash log %p %u\n\n", time(NULL), GetCurrentThreadId());
            fprintf(f, "Crash log %p\n\n", time(NULL));
            INT64 addy = (INT64)ExceptionRecord->ExceptionAddress;
            SavedExceptions[addy] = *ContextRecord;
            while (SavedExceptions.size())
            {
                auto kvp = *SavedExceptions.begin();
                RtlPcToFileHeader((PVOID)kvp.first, (PVOID*)&faultingModule);
                if (faultingModule)
                {
                    GetModuleFileNameA((HMODULE)faultingModule, module_name, MAX_PATH);
                }
                else
                {
                    strcpy_s(module_name, "<Unknown Module>");
                }

                ALOG("Game: %p\n", (void*)REBASE(0x0));
                fprintf(f, "Game: %p\n", (void*)REBASE(0x0));

                ALOG("Module: %s\n", module_name);
                fprintf(f, "Module: %s\n", module_name);

                ALOG("[%p]Exception at (%p) (RIP:%p) (Rsp:%p) (RBP: %p)\n", faultingModule, kvp.first - faultingModule, kvp.second.Rip, kvp.second.Rsp, kvp.second.Rbp);
                fprintf(f, "[%p]Exception at (%p) (RIP:%p) (Rsp: %p)\n", faultingModule, kvp.first - faultingModule, kvp.second.Rip, kvp.second.Rsp, kvp.second.Rbp);

                ALOG("[%p]Rcx: (%p) Rdx: (%p) R8: (%p) R9: (%p)\n", kvp.first, kvp.second.Rcx, kvp.second.Rdx, kvp.second.R8, kvp.second.R9);
                fprintf(f, "[%p]Rcx: (%p) Rdx: (%p) R8: (%p) R9: (%p)\n", kvp.first, kvp.second.Rcx, kvp.second.Rdx, kvp.second.R8, kvp.second.R9);

                ALOG("[%p]Rax: (%p) Rbx: (%p) Rsi: (%p) Rdi: (%p)\n", kvp.first, kvp.second.Rax, kvp.second.Rbx, kvp.second.Rsi, kvp.second.Rdi);
                fprintf(f, "[%p]Rax: (%p) Rbx: (%p) Rsi: (%p) Rdi: (%p)\n", kvp.first, kvp.second.Rax, kvp.second.Rbx, kvp.second.Rsi, kvp.second.Rdi);

                ALOG("[%p]R10: (%p) R11: (%p) R12: (%p) R13: (%p)\n", kvp.first, kvp.second.R10, kvp.second.R11, kvp.second.R12, kvp.second.R13);
                fprintf(f, "[%p]R10: (%p) R11: (%p) R12: (%p) R13: (%p)\n", kvp.first, kvp.second.R10, kvp.second.R11, kvp.second.R12, kvp.second.R13);

                ALOG("[%p]Rbp: (%p) R14: (%p) R15: (%p) Dr7: (%p)\n", kvp.first, kvp.second.Rbp, kvp.second.R14, kvp.second.R15, kvp.second.Dr7);
                fprintf(f, "[%p]Rbp: (%p) R14: (%p) R15: (%p) Dr7: (%p)\n", kvp.first, kvp.second.Rbp, kvp.second.R14, kvp.second.R15, kvp.second.Dr7);

                ALOG("[%p]Dr0: (%p) Dr1: (%p) Dr2: (%p) Dr3: (%p)\n", kvp.first, kvp.second.Dr0, kvp.second.Dr1, kvp.second.Dr2, kvp.second.Dr3);
                fprintf(f, "[%p]Dr0: (%p) Dr1: (%p) Dr2: (%p) Dr3: (%p)\n", kvp.first, kvp.second.Dr0, kvp.second.Dr1, kvp.second.Dr2, kvp.second.Dr3);

                for (int i = 0; i < ((STATUS_ILLEGAL_INSTRUCTION == ExceptionRecord->ExceptionCode) ? 0x1200 : 0x400); i += 0x10)
                {
                    ALOG("[%p] %p %p\n", kvp.second.Rsp + i, *(int64_t*)(kvp.second.Rsp + i), *(int64_t*)(kvp.second.Rsp + i + 8));
                    fprintf(f, "[%p] %p %p\n", kvp.second.Rsp + i, *(int64_t*)(kvp.second.Rsp + i), *(int64_t*)(kvp.second.Rsp + i + 8));
                }
                SavedExceptions.erase(kvp.first);
            }

            // dump script context

            std::fflush(f);
            std::fclose(f);
            lck_dumping = NULL;
        }

        if (STATUS_ILLEGAL_INSTRUCTION != ExceptionRecord->ExceptionCode)
        {
            b_fatal = true;
        }
    }

    if (b_fatal)
    {
        if (lck_dumping && lck_dumping != GetCurrentThreadId())
        {
            HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, GetCurrentThreadId());
            ContextRecord->Rip = (INT64)SuspendThread;
            ContextRecord->Rcx = (INT64)hThread;
            ZwContinue(ContextRecord, false);
        }
        else
        {
            SuspendProcess();
        }
    }
}

void chgmem(__int64 addy, __int32 size, void* copy)
{
    DWORD oldprotect;
    VirtualProtect((void*)addy, size, PAGE_EXECUTE_READWRITE, &oldprotect);
    memcpy((void*)addy, copy, size);
    VirtualProtect((void*)addy, size, oldprotect, &oldprotect);
}

FILE* LOGFILE = NULL;
HWND notepad = NULL;
HWND edit = NULL;

void vnlog(const char* str, va_list ap)
{
    char buf[4096];

    vsprintf(buf, str, ap);
    strcat(buf, "^7\r\n");

    if (IsDebuggerPresent())
    {
        OutputDebugStringA(buf);
        return;
    }

    if (dbg_is_console_available && dbg_is_attached)
    {
        DWORD l;
        if (WriteFile(dbg_hWriteConsole, buf, strlen(buf) + 1, &l, 0))
        {
            return;
        }
    }

    if (GetConsoleWindow())
    {
        printf("%s", buf);
        return;
    }    

    if (!notepad)
    {
        notepad = FindWindow(NULL, "dconsole.txt - Notepad");
    }
    if (!notepad)
    {
        notepad = FindWindow(NULL, "*dconsole.txt - Notepad");
    }
    if (!edit)
    {
        edit = FindWindowEx(notepad, NULL, "EDIT", NULL);
    }
    SendMessage(edit, EM_REPLACESEL, TRUE, (LPARAM)buf);
}

void nlog(const char* str, ...)
{
    va_list ap;
    va_start(ap, str);
    vnlog(str, ap);
    va_end(ap);
}

void spoof_ownscontent()
{
    for (int i = 0; i < 5; i++)
    {
        *(uint8_t*)REBASE(0x148A852A + i) = 1;
    }
}

MDT_Define_FASTCALL(PTR_Content_GetAvailableContentPacks, Content_GetAvailableContentPacks_hook, uint64_t, (char a1))
{
    auto mask = (int*)s_contentPackMetaData;
    auto v7 = (uint8_t*)REBASE(0x18FAB490);
    for (int i = 0; i < 11; i++)
    {
        *v7 = 1;

        v7 += 130;
        mask += 20;
    }

    spoof_ownscontent();

    return 0xFFFFFFFFFFFFFFFF;
}

MDT_Define_FASTCALL(REBASE(0x21F0960), Com_InitMods_hook, void, ())
{
    MDT_ORIGINAL(Com_InitMods_hook, ());
    Dvar_SetFromStringByName("mods_enabled", "1");
}

MDT_Define_FASTCALL(REBASE(0x221A3A0), Content_DoWeHaveContentPack_hook, bool, ())
{
    spoof_ownscontent();
    return true;
}

MDT_Define_FASTCALL(REBASE(0x1EED6C0), MSStore_OwnsContent_hook, bool, ())
{
    spoof_ownscontent();
    return true;
}

MDT_Define_FASTCALL(REBASE(0x220F4F0), Com_MessagePrintHook, void, (int channel, int consoleType, const char* message, int other))
{
    ALOG("%s", message);
    MDT_ORIGINAL(Com_MessagePrintHook, (channel, consoleType, message, other));
}

MDT_Define_FASTCALL(REBASE(0x29C3A20), bdLogMessage_hook, void, (uint8_t type, const char* bchan, const char* chan, const char* file, const char* func, uint32_t line, const char* fmt, ...))
{
    va_list ap;
    char buf[4096];

    va_start(ap, fmt);

    char fmtbuf[512]{ 0 };
    const char* typeStr = type ? ((type == 2) ? "err-" : "warn") : "info";
    sprintf_s(fmtbuf, "[DW][%s][%s][@%s]%s", typeStr, chan, func, fmt);

    vnlog(fmtbuf, ap);

    va_end(ap);
}

MDT_Define_FASTCALL(REBASE(0x21F00B0), set_mod_super_hook, void, (int a1, char* fs_game, uint8_t a3))
{
    Mods_UpdateUsermapsList();
    Mods_UpdateModsList();

    ugcinfo_entry_wstor localinfo{ 0 };
    ugcinfo_entry_wstor* target = NULL;

    auto ugcmods = ugc_modslist;
    
    do
    {
        if (!fs_game) break;
        if (!*fs_game) break;

        for (uint32_t i = 0; i < ugcmods->num_entries; i++)
        {
            if (!strcmp(ugcmods->entries[i].name, fs_game))
            {
                target = &ugcmods->entries[i];
                break;
            }
        }
    } 
    while (false);
    
    if (!target)
    {
        target = &localinfo;

        sprintf_s(localinfo.name, "%s", fs_game);
        sprintf_s(localinfo.internalName, "%s", fs_game);
        sprintf_s(localinfo.ugcName, "%s", fs_game);
        sprintf_s(localinfo.ugcPathBasic, "mods/%s", fs_game);
        sprintf_s(localinfo.ugcRoot, "%s", OsPathRoot);
        sprintf_s(localinfo.ugcPath, "%s/mods/%s", OsPathRoot, fs_game);
        localinfo.unk_4B0 = 1;

        if (fs_game)
        {
            auto v39 = *fs_game;
            for (localinfo.hash_id = 5381; *fs_game; v39 = *fs_game)
            {
                ++fs_game;
                localinfo.hash_id = tolower(v39) + 33 * localinfo.hash_id;
            }
        }
        localinfo.unk_4B8 = 1;
    }

    // mods_reinit() (inlined)
    memset((void*)REBASE(0xF2DCC60), 0, 0x4100u);
    *(uint32_t*)REBASE(0xF2E0D88) = 0;

    if (*target->ugcName)
    {
        *currently_loaded_mod = *target;
    }
    else
    {
        memset(currently_loaded_mod, 0, sizeof(ugcinfo_entry_wstor));
    }

    // mods_resetunk(a1)
    ((void(__fastcall*)(uint32_t))REBASE(0x13E4FE0))(a1);

    Mods_LoadModStart(a1, target, a3);
}

MDT_Define_FASTCALL(REBASE(0x21F0000), Dvar_Callback_ModChanged_hook, void, (void* dvar_ptr))
{
    // do nothing. its normally used to validate that fs_game was set correctly but we dont care because if someone does something stupid its their fault
}

MDT_Define_FASTCALL(REBASE(0x237EDE0), UI_ValidateText_hook, bool, ())
{
    return true;
}

MDT_Define_FASTCALL(REBASE(0x1492A50), UI_IsBadWord_hook, int, ())
{
    return 0;
}

char* custom_jpg_buf = NULL;
uint32_t custom_jpg_buf_size = 0;

void load_custom_jpg()
{
    if (custom_jpg_buf)
    {
        delete[] custom_jpg_buf;
        custom_jpg_buf = NULL;
        custom_jpg_buf_size = 0;
    }

    std::ifstream ifs;
    ifs.open("screenshot.jpg", std::ifstream::in | std::ifstream::binary);

    if (!ifs.is_open())
    {
        return;
    }

    ifs.seekg(0, std::ios::end);
    size_t fileSize = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    custom_jpg_buf_size = (uint32_t)fileSize;
    custom_jpg_buf = new char[(uint32_t)fileSize];
    ifs.read(custom_jpg_buf, (uint32_t)fileSize);

    ifs.close();
}

MDT_Define_FASTCALL(REBASE(0x26D73E0), Demo_SaveScreenshotToContentServer_hook, void, (uint32_t localClientNum, int fileSlot))
{
    load_custom_jpg();

    if (custom_jpg_buf)
    {
        ALOG("Uploading custom screenshot...");
        *(uint64_t*)(*(uint64_t*)REBASE(0x9ABF730) + 0x5A8490) = (uint64_t)custom_jpg_buf;
        *(uint32_t*)(*(uint64_t*)REBASE(0x9ABF730) + 0x5A8498) = custom_jpg_buf_size;
    }
    
    MDT_ORIGINAL(Demo_SaveScreenshotToContentServer_hook, (localClientNum, fileSlot));
}

MDT_Define_FASTCALL(REBASE(0x1EFF100), Loot_GetInventoryItem_hook, uint64_t, (uint64_t a1, uint64_t a2))
{
    int v2; // er9
    __int64 v3; // rcx
    char* v4; // r10
    __int64 v5; // r8
    int v6; // edx
    __int64 v7; // rcx
    int32_t* i; // rax

    v2 = a2;
    v3 = 81064ULL * a1;
    v4 = (char*)REBASE(0xB60A130) + v3 + 856;
    if (*(int32_t*)((char*)REBASE(0xB60A130) + v3 + 80864) != 4)
        return 0;
    v5 = *(int*)((char*)REBASE(0xB60A130) + v3 + 80856);
    v6 = 0;
    if (v5 <= 0)
        return 0;
    v7 = 0;
    for (i = (int32_t*)v4; v2 != *i; i += 5)
    {
        ++v6;
        if (++v7 >= v5)
        {
            *(int*)((char*)REBASE(0xB60A130) + v3 + 80856) += 1;
            i += 5;
            *i = v2;
            i[1] = 0x7FFFFFFF;
            i[2] = 0xFFFFFFFF;
            i[3] = 0xFFFFFFFF;
            i[4] = 1;
            return (__int64)&v4[20 * v6];
        }
    }
    return (__int64)&v4[20 * v6];
}

void enable_fileshare()
{
    Dvar_SetFromStringByName("fileshare_enabled", "1");
    Dvar_SetFromStringByName("fileshareAllowDownload", "1");
    Dvar_SetFromStringByName("fileshareAllowPaintjobDownload", "1");
    Dvar_SetFromStringByName("fileshareAllowVariantDownload", "1");
    Dvar_SetFromStringByName("fileshareAllowEmblemDownload", "1");
    Dvar_SetFromStringByName("fileshareAllowDownloadingOthersFiles", "1");
}

MDT_Define_FASTCALL(REBASE(0x23C0110), anticheat_sus_peepo_hook, void, ())
{
    SPOOFED_CALL(((void(__fastcall*)())MDT_ORIGINAL_PTR(anticheat_sus_peepo_hook)));
}

MDT_Define_FASTCALL(REBASE(0x22649C0), anticheat_sus_peepo2_hook, void, ()) // probably anticheat pump
{
    SPOOFED_CALL(((void(__fastcall*)())MDT_ORIGINAL_PTR(anticheat_sus_peepo2_hook)));
}

MDT_Define_FASTCALL(REBASE(0x22C0140), anticheat_unk3_hook, uint64_t, (uint64_t a1, uint64_t a2))
{
    auto old = *(uint64_t*)REBASE(0x310C688);
    auto res = SPOOFED_CALL(((uint64_t(__fastcall*)(uint64_t, uint64_t))MDT_ORIGINAL_PTR(anticheat_unk3_hook)), a1, a2);
    *(uint64_t*)REBASE(0x310C688) = old;
    return res;
}

MDT_Define_FASTCALL(REBASE(0x22BE810), anticheat_unk4_hook, uint64_t, (uint64_t a1))
{
    auto old = *(uint64_t*)REBASE(0x310C688);
    auto res = SPOOFED_CALL(((uint64_t(__fastcall*)(uint64_t))MDT_ORIGINAL_PTR(anticheat_unk4_hook)), a1);
    *(uint64_t*)REBASE(0x310C688) = old;
    return res;
}

MDT_Define_FASTCALL(REBASE(0x22BD4C0), anticheat_unk5_hook, uint64_t, (uint64_t a1))
{
    auto old = *(uint64_t*)REBASE(0x310C688);
    auto res = SPOOFED_CALL(((uint64_t(__fastcall*)(uint64_t))MDT_ORIGINAL_PTR(anticheat_unk5_hook)), a1);
    *(uint64_t*)REBASE(0x310C688) = old;
    return res;
}

MDT_Define_FASTCALL(REBASE(0x22BC1E0), anticheat_unk6_hook, uint64_t, (uint64_t a1))
{
    auto old = *(uint64_t*)REBASE(0x310C688);
    auto res = SPOOFED_CALL(((uint64_t(__fastcall*)(uint64_t))MDT_ORIGINAL_PTR(anticheat_unk6_hook)), a1);
    *(uint64_t*)REBASE(0x310C688) = old;
    return res;
}

MDT_Define_FASTCALL((int64_t)GetProcAddress(GetModuleHandle("ntdll.dll"), "NtQuerySystemInformation"), NtQuerySystemInformation_hook, NTSTATUS, (const SYSTEM_INFORMATION_CLASS system_information_class,
    const PVOID system_information,
    const ULONG system_information_length,
    const PULONG return_length))
{
    uint64_t faultingModule;
    RtlPcToFileHeader((PVOID)_ReturnAddress(), (PVOID*)&faultingModule);

    if (faultingModule && faultingModule == REBASE(0))
    {
        ALOG("SLEEPY ANTICHEAT ACTIVATED");
        while (1)
        {
            Sleep(100000);
        }
    }

    auto result = MDT_ORIGINAL(NtQuerySystemInformation_hook, (system_information_class, system_information, system_information_length, return_length));
    return result;
}

MDT_Define_FASTCALL((int64_t)GetProcAddress(GetModuleHandle("ntdll.dll"), "NtSetInformationThread"), NtSetInformationThread_hook, NTSTATUS, (HANDLE ThreadHandle, THREAD_INFORMATION_CLASS ThreadInformationClass, PVOID ThreadInformation, ULONG ThreadInformationLength))
{
    if (ThreadInformationClass == 0x11) // ThreadHideFromDebugger
    {
        if (ThreadInformationLength)
        {
            return -1;
        }

        if ((int64_t)ThreadHandle != -2)
        {
            return -1;
        }
        
        // ALOG("CAUGHT ThreadHideFromDebugger @ %p thread %u", ((uint64_t)_ReturnAddress()) - REBASE(0), GetCurrentThreadId());
        return 0;
    }

    return MDT_ORIGINAL(NtSetInformationThread_hook, (ThreadHandle, ThreadInformationClass, ThreadInformation, ThreadInformationLength));
}

bool NtQueryInformationThread_hook_disable = false;
MDT_Define_FASTCALL((int64_t)GetProcAddress(GetModuleHandle("ntdll.dll"), "NtQueryInformationThread"), NtQueryInformationThread_hook, NTSTATUS, (HANDLE ThreadHandle, uint32_t ThreadInformationClass, PVOID ThreadInformation, ULONG ThreadInformationLength, PULONG ReturnLength))
{
    if (!NtQueryInformationThread_hook_disable && (ThreadInformationClass == 0x11)) // ThreadHideFromDebugger
    {
        if (ThreadInformationLength != 1)
        {
            return -1;
        }

        *(uint8_t*)ThreadInformation = 1;

        return 0;
    }

    return MDT_ORIGINAL(NtQueryInformationThread_hook, (ThreadHandle, ThreadInformationClass, ThreadInformation, ThreadInformationLength, ReturnLength));
}

MDT_Define_FASTCALL((int64_t)GetProcAddress(GetModuleHandle("kernelbase.dll"), "VirtualAlloc"), VirtualPretend, uint64_t, (LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect))
{
    if ((int64_t)_ReturnAddress() == REBASE(0x225920D) // in TLS callback
        || (int64_t)_ReturnAddress() == REBASE(0x23C583B) // in memory initializer
        || (int64_t)_ReturnAddress() == REBASE(0x2269D9D) // anticheat2.1-
        || (int64_t)_ReturnAddress() == REBASE(0x22A05D2) // anticheat2.2
        ) 
    {
        ALOG("VirtualPretend succeeded!");
        return (uint64_t)GetModuleHandleA("ntdll.dll") + 0x1000;
    }

    return MDT_ORIGINAL(VirtualPretend, (lpAddress, dwSize, flAllocationType, flProtect));
}

MDT_Define_FASTCALL(REBASE(0x736E0), anticheat_unk7_hook, void, ())
{
    // ALOG("anticheat unk7 invoked!!"); // apparently this guy isnt important LMAO
}

MDT_Define_FASTCALL(REBASE(0x77B60), anticheat_unk10_hook, void, ())
{
    // ALOG("anticheat unk10 invoked!!"); // apparently this guy isnt important LMAO
}

MDT_Define_FASTCALL(REBASE(0x7BBF0), anticheat_unk11_hook, void, ())
{
    // ALOG("anticheat unk11 invoked!!"); // apparently this guy isnt important LMAO
}

MDT_Define_FASTCALL(REBASE(0x1E54EF0), lua_pcall_hook, uint32_t, (lua_State* s, uint32_t nargs, uint32_t nresults, uint32_t errfunc))
{
    auto res = MDT_ORIGINAL(lua_pcall_hook, (s, nargs, nresults, errfunc));

    if ((uint64_t)_ReturnAddress() == REBASE(0x202C560))
    {
        const char* sourceData = 
"EnableGlobals()\n\
function IsServerBrowserEnabled() return false end";
        *(char*)(*(uint64_t*)((uint64_t)s + 16) + 472) = HKS_BYTECODE_SHARING_ON;
        HksCompilerSettings hks_compiler_settings;
        int result = hksi_hksL_loadbuffer(s, &hks_compiler_settings, sourceData, strlen(sourceData), sourceData);
        if (!result)
        {
            result = hks::vm_call_internal(s, 0, 0, 0);
        }
        *(char*)(*(uint64_t*)((uint64_t)s + 16) + 472) = HKS_BYTECODE_SHARING_SECURE;
    }

    return res;
}

void patch_lua()
{
    // disable server browser
    MDT_Activate(lua_pcall_hook);
}

void kill_gamechat()
{
    //noprange(REBASE(0x1EE81E1), 0x1EE81FB - 0x1EE81E1); // skip init
    //noprange(REBASE(0x1EE829B), 0x1EE82B5 - 0x1EE829B); // skip user creation
    //noprange(REBASE(0x1EE82BD), 0x1EE82C4 - 0x1EE82BD); // skip user creation 2

    noprange(REBASE(0x1EE864A), 0x1EE8664 - 0x1EE864A); // kill uninit
}

uint64_t fakeMemoryAddress = 0;
void ac_bypass_allocconsole()
{
    MDT_Activate(anticheat_sus_peepo_hook);
    MDT_Activate(anticheat_sus_peepo2_hook);
    MDT_Activate(anticheat_unk3_hook);
    MDT_Activate(anticheat_unk4_hook);
    MDT_Activate(anticheat_unk5_hook);
    MDT_Activate(anticheat_unk6_hook);
    MDT_Activate(NtQuerySystemInformation_hook);
    MDT_Activate(NtSetInformationThread_hook);
    MDT_Activate(NtQueryInformationThread_hook);
    MDT_Activate(VirtualPretend);
    MDT_Activate(anticheat_unk7_hook);
    MDT_Activate(anticheat_unk10_hook);
    MDT_Activate(anticheat_unk11_hook);

    // anticheat intitializer constants
    *(uint64_t*)REBASE(0x8E57400) = 0x7331D36860FF300D;
    *(uint64_t*)REBASE(0x8E57408) = 0x0D092BEA64EC3D509;
    *(uint64_t*)REBASE(0x8E573A8) = 0x0FD802801C8DEB7BD;

    chgmem<uint8_t>(REBASE(0x22C7FE0), 0xC3); // anticheat_unk13_THREADED can just be nuked lol
    chgmem<uint8_t>(REBASE(0x22F04C0), 0xC3); // anticheat_8 which is threaded
    chgmem<uint8_t>(REBASE(0x22628C0), 0xC3); // anticheat_2_thread
    chgmem<uint8_t>(REBASE(0x2263AC0), 0xC3); // AnticheatOnDLLLoad (called with LdrRegisterDllNotification in anticheat init)

    // skip where they copy the data into the ntdll binary in...
    chgmem<uint8_t>(REBASE(0x22595C4), 0xEB); // TLS callback
    chgmem<uint8_t>(REBASE(0x23C5C01), 0xEB); // memory init
    chgmem<uint8_t>(REBASE(0x226A15C), 0xEB); // anticheat2.1
    chgmem<uint8_t>(REBASE(0x22A09E9), 0xEB); // anticheat2.2

    // patch where they usually set threadinfo (anti-debug)
    unsigned char xorRaxRax[] = { 0x48, 0x31, 0xC0 };
    unsigned char xorEaxEax[] = { 0x31, 0xC0 };
    chgmem(REBASE(0x225B381), sizeof(xorRaxRax), xorRaxRax); 
    chgmem(REBASE(0x226B921), sizeof(xorEaxEax), xorEaxEax); // only 2 bytes here but they test eax :)
    chgmem(REBASE(0x23D3326), sizeof(xorRaxRax), xorRaxRax);
    
    // CreateRemoteThread check (225A173)
    noprange(REBASE(0x22598FA), 0x22598FC - 0x22598FA);
    noprange(REBASE(0x2259904), 0x2259906 - 0x2259904);
    chgmem<uint16_t>(REBASE(0x225990F), 0xE990);

    // you WILL keep running game. i DEMAND IT
    noprange(REBASE(0x23F120E), 0x23F1210 - 0x23F120E);
    noprange(REBASE(0x23F1217), 0x23F1219 - 0x23F1217);
    noprange(REBASE(0x23F1232), 0x23F1234 - 0x23F1232);
    noprange(REBASE(0x23F0FC0), 0x23F0FC6 - 0x23F0FC0);
    noprange(REBASE(0x23F0D6A), 0x23F0D70 - 0x23F0D6A);

    chgmem<uint16_t>(REBASE(0x23F123F), 0xE990);
    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)REBASE(0x23F123F), 2);

    // patch CheckRemoteDebuggerPresent
    noprange(REBASE(0x2282470), 0x2282476 - 0x2282470);
    chgmem<uint32_t>(REBASE(0x2282470), 0x90c03148); // xor rax, rax; nop
    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)REBASE(0x2282470), 1);

    // remove mutex checks (allows more than 1 of the game to run)
    noprange(REBASE(0x226D85A), 0x226D860 - 0x226D85A);
    noprange(REBASE(0x226CF83), 0x226CF85 - 0x226CF83);
    noprange(REBASE(0x226E40F), 0x226E415 - 0x226E40F);
    noprange(REBASE(0x226DE83), 0x226DE85 - 0x226DE83);

#if ALLOC_CONSOLE
    if (!IsDebuggerPresent())
    {
        AllocConsole();
        FILE* fDummy;
        freopen_s(&fDummy, "CONIN$", "r", stdin);
        freopen_s(&fDummy, "CONOUT$", "w", stderr);
        freopen_s(&fDummy, "CONOUT$", "w", stdout);
    }
#endif
}

//uint64_t gfxVertexDeclLck = 0;
//bool gfxVertexDeclPatchLck = false;
//MDT_Define_FASTCALL(REBASE(0x1DE9B10), R_UpdateVertexDecl_hook, void, (uint64_t gfxstate))
//{
//    MDT_ORIGINAL(R_UpdateVertexDecl_hook, (gfxstate));
//    auto v1 = *(uint64_t*)(gfxstate + 7544);
//    auto v3 = *(uint64_t*)(v1 + 8);
//    auto mat = **(uint64_t**)(gfxstate + 0x1D60);
//
//    if (strcmp((char*)mat, "white_replace"))
//    {
//        return;
//    }
//
//    if (v3)
//    {
//        auto vert = *(int*)(gfxstate + 0x40C8);
//        uint64_t res = *(uint64_t*)(v3 + 8llu * vert + 24llu);
//        uint64_t shader;
//        if (*(uint32_t*)v1 == 2)
//            shader = *(uint64_t*)(v1 + 48);
//        else
//            shader = *(uint64_t*)(v1 + 24);
//
//        if (!res)
//        {
//            ALOG("white_replace FAILURE: res %p vert %u mat %p v3 %p shader %p", (uint32_t)res, vert, mat, v3, shader);
//            SuspendProcess();
//        }
//        else if (!(gfxVertexDeclLck % 250))
//        {
//            if (!gfxVertexDeclPatchLck)
//            {
//                gfxVertexDeclPatchLck = true;
//                DWORD old;
//                VirtualProtect((LPVOID)v3, 255, PAGE_READONLY, &old);
//            }
//        }
//    }
//
//    ++gfxVertexDeclLck;
//}

/* LOL!!!

mov    DWORD PTR [rdi],eax
movabs rax,0x11223344556677
mov    rdx,QWORD PTR [rbx+0x8]
mov    ecx,DWORD PTR [rbx]
mov    r8,rbx
call   rax
jmp    rax

*/
char zone_patch_asm[] = { 0x89, 0x07, 0x48, 0xB8, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, 0x48, 0x8B, 0x53, 0x08, 0x8B, 0x0B, 0x49, 0x89, 0xD8, 0xFF, 0xD0, 0xFF, 0xE0 };
bool no_print_debug = false;
uint64_t DB_FreeXAssetHeader_hook(uint8_t assetType, uint64_t header, uint64_t entry)
{
    if (assetType == xasset_techset)
    {
        auto name = *(char**)header;
        if (name && !strcmp(name, "wc/lit_detail_advanced#882572b8"))
        {
            return REBASE(0x14E0DA3);
        }
    }

    // call original DB_FreeXAssetHeader
    ((void(__fastcall*)(uint32_t, uint64_t))REBASE(0x14DA3A0))(assetType, header);
    return REBASE(0x14E0D8C); // original return address
}

//MDT_Define_FASTCALL(REBASE(0x14DA3A0), DB_FreeXAssetHeader_hook_2, void, (uint8_t assetType, uint64_t header))
//{
//    // ALOG("UNLOAD INFO %u", (uint32_t)assetType);
//    if (xasset_techset == assetType || xasset_material == assetType || xasset_computeshaderset == assetType)
//    {
//        ALOG("UNLOAD INFO %u name: %s", (uint32_t)assetType, *(char**)header);
//    }
//
//    MDT_ORIGINAL(DB_FreeXAssetHeader_hook_2, (assetType, header));
//}

MDT_Define_FASTCALL(REBASE(0x1F009E0), Live_SystemInfo_Hook, bool, (int controllerIndex, int infoType, char* outputString, const int outputLen))
{
    if (infoType)
    {
        return MDT_ORIGINAL(Live_SystemInfo_Hook, (controllerIndex, infoType, outputString, outputLen));
    }

    strcpy_s(outputString, outputLen, VERSION_STRING);
    return true;
}

MDT_Define_FASTCALL(REBASE(0x291F620), Content_HasEntitlementOwnershipByRef_hook, uint8_t, ())
{
    return 1;
}

//defined in steam.cpp
extern void init_steamapi();

__int64 old_IsProcessorFeaturePresent = 0;
void add_prehooks()
{
    ALOG("installing prehooks...");

    // setup a callspoof trampoline
    chgmem<uint32_t>(SPOOF_TRAMP - 2, 0x27FFFFFF);

    InstallHook(ExceptHook);

    old_IsProcessorFeaturePresent = (__int64)&IsProcessorFeaturePresent;
    IHOOK_INSTALL(IsProcessorFeaturePresent, GetModuleHandle(0));


    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    // actual client stuff
    MDT_Activate(Com_InitMods_hook);
    MDT_Activate(Content_DoWeHaveContentPack_hook);
    MDT_Activate(MSStore_OwnsContent_hook);
    MDT_Activate(Com_MessagePrintHook);
    MDT_Activate(set_mod_super_hook);
    MDT_Activate(Dvar_Callback_ModChanged_hook);
    MDT_Activate(Live_SystemInfo_Hook);
    MDT_Activate(Content_HasEntitlementOwnershipByRef_hook);

    // testing stuff
#if BADWORD_BYPASS
    MDT_Activate(UI_ValidateText_hook);
    MDT_Activate(UI_IsBadWord_hook);
#endif
#if DWINVENTORY_UNLOCK_ALL
    MDT_Activate(Loot_GetInventoryItem_hook);
#endif
#if LOG_DEMONWARE
    MDT_Activate(bdLogMessage_hook);
#endif
#if SCREENSHOT_OVERRIDE
    MDT_Activate(Demo_SaveScreenshotToContentServer_hook);
#endif

    // MUST BE INITIALIZED BEFORE STEAM!!!!!!!
    security::init();

#if ENABLE_STEAMAPI
    // initialise the steamapi
    init_steamapi();
#endif

    // chgmem<uint8_t>(REBASE(0x20d7d3c), 0x01); ban me!!

    ac_bypass_allocconsole();
    kill_gamechat();
    patch_lua();

    // fix the weird zone unloading issue
    {
        // fixup a jump to our asm
        // note we only have 0xD byte space
        char patch[] = { 0x48, 0xB9, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, 0xFF, 0xE1 };
        *(uint64_t*)(patch + 2) = (uint64_t)zone_patch_asm;

        // install the patch
        chgmem(REBASE(0x14E0D7F), sizeof(patch), patch);

        // protect asm
        DWORD ____old;
        VirtualProtect((LPVOID)zone_patch_asm, sizeof(zone_patch_asm), PAGE_EXECUTE_READWRITE, &____old); // note: this *technically* causes a vulnerability... vprotect works in pages so this will RWX the entire page including nearby stuff!

        // fixup free entry hook
        *(uint64_t*)(zone_patch_asm + 4) = (uint64_t)&DB_FreeXAssetHeader_hook;
    }

    DetourTransactionCommit();


    chgmem<uint32_t>(REBASE(0x21F09C0 + 2), 0); // wipe flags of mods_enabled

    // nop out all the calls in the start of Mods_LoadModStart
    char fiveNops[5] = { 0x90, 0x90, 0x90, 0x90, 0x90 };

    chgmem(REBASE(0x21F0402), 5, fiveNops);
    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)REBASE(0x21F0402), 5);

    chgmem(REBASE(0x21F0434), 5, fiveNops);
    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)REBASE(0x21F0434), 5);

    chgmem(REBASE(0x21F0448), 5, fiveNops);
    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)REBASE(0x21F0448), 5);

    chgmem(REBASE(0x21F0450), 5, fiveNops);
    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)REBASE(0x21F0450), 5);

#if BADWORD_BYPASS
    // more offensiveness filter stuff
    chgmem<uint16_t>(REBASE(0x2F69AA8), 0xFF69);
#endif

    // work around mutex crash in xbox curl code by making it a recursive lock
    char mutexPatch[] = { 0xBA, 0x02, 0x01, 0x00, 0x00, 0xE8, 0x8A, 0x2F, 0xF8, 0xFF };
    chgmem(REBASE(0x2D7AAB2), sizeof(mutexPatch), mutexPatch);
}

void add_hooks()
{
    ALOG("installing hooks...");
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    MDT_Activate(Content_GetAvailableContentPacks_hook);
    security::init_delayed();

    DetourTransactionCommit();




    spoof_ownscontent();

    chgmem<uint8_t>(REBASE(0x14A2F4C), 0xEB); // Fastfile archive checksum (0x%08X 0x%08

    enable_fileshare();

    // show the callstack instead of just ui errors
    INT64 ptrDvar = *(INT64*)(PTR_ui_error_callstack_ship);
    *(DWORD*)(ptrDvar + 0x18) = 0; // clear flags

    Dvar_SetFromStringByName("ui_error_callstack_ship", "1", true);
    Dvar_SetFromStringByName("loot_enabled", "1", true);

    // anticheat related import caches
    /*dumpa_da_anticheats(REBASE(0x1A122AD8), (0x1A122C88 - 0x1A122AD8) / 8);
    dumpa_da_anticheats(REBASE(0x1A122D20), (0x1A122D50 - 0x1A122D20) / 8);
    dumpa_da_anticheats(REBASE(0x1A1118A8), (0x1A1118D0 - 0x1A1118A8) / 8);
    dumpa_da_anticheats(REBASE(0x8E573B8), (0x8E57400 - 0x8E573B8) / 8);
    dumpa_da_anticheats(REBASE(0x8E57410), (0x8E574A0 - 0x8E57410) / 8);*/
    // dumpa_da_anticheats(REBASE(0x8E573B8), (0x8E57498 - 0x8E573B8) / 8);
}

DWORD WINAPI watch_ready_inject(_In_ LPVOID lpParameter)
{
    for (;;)
    {
        Sleep(10);
        if (*(__int64*)REBASE(0x8DEE3D8))
        {
            break;
        }
    }

    add_hooks();
    return 0;
}

#if REMAP_EXPERIMENT
__declspec(dllexport) void* get_tls_data() // without this the compiler wont emit the tls data
{
    return &tls_data[0];
}

void replace_steam_exe()
{
    ALOG("replace_steam_exe");

    ALOG("Caching TLS info from steam bo3...");
    // cache the original tls info from steam bo3
    auto bo3 = Pe::PeNative::fromModule((const void*)REBASE(0));
    auto original_tls = *bo3.tls().descriptor().ptr;

    ALOG("original start: %p, Base address of memory: %p, addressofcallbacks %p", original_tls.StartAddressOfRawData, REBASE(0), original_tls.AddressOfCallBacks);

    typedef uint32_t NtUnmapViewOfSection_t(HANDLE processHandle, PVOID baseAddress);
    auto NtUnmapViewOfSection = (NtUnmapViewOfSection_t*)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtUnmapViewOfSection");

    // unmap the original bo3.exe from memory
    ALOG("Unmapping section...");
    NtUnmapViewOfSection(GetCurrentProcess(), (PVOID)REBASE(0));

    // map new one in
    std::ifstream ifs;
    ifs.open(WSTORE_MOD_NAME, std::ifstream::in | std::ifstream::binary);

    if (!ifs.is_open())
    {
        ALOG("ERROR: COULD NOT FIND OR OPEN %s", WSTORE_MOD_NAME);
        SuspendProcess();
        return;
    }

    ifs.seekg(0, std::ios::end);
    size_t fileSize = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    auto execBuf = new char[(uint32_t)fileSize];
    ifs.read(execBuf, (uint32_t)fileSize);
    ifs.close();

    ALOG("Mapping new module...");
    auto res = MemoryLoadLibrary(execBuf, fileSize, (void*)REBASE(0), true);

    delete[] execBuf;
    execBuf = NULL;

    if (!res)
    {
        ALOG("ERROR: FAILED TO MAP MAIN MODULE (0x%X)", GetLastError());
        SuspendProcess();
        return;
    }

    auto wbo3 = Pe::PeNative::fromModule((const void*)REBASE(0));

    // initialize TLS data (thanks momo5502 for example for this in boiii)
    auto our_mod = Pe::PeNative::fromModule((const void*)__thismodule); // (IMAGE_TLS_PTR)
    {
        auto target = (IMAGE_TLS_DIRECTORY64*)our_mod.tls().descriptor().ptr;
        auto source = (IMAGE_TLS_DIRECTORY64*)wbo3.tls().descriptor().ptr;

        auto target_start = target->StartAddressOfRawData;
        auto source_start = source->StartAddressOfRawData;
        auto source_size = source->EndAddressOfRawData - source->StartAddressOfRawData;
        auto tls_index = *(uint32_t*)target->AddressOfIndex;
        
        // copy existing tls index over to theirs
        chgmem<uint32_t>(source->AddressOfIndex, tls_index);

        DWORD old;
        VirtualProtect((LPVOID)target_start, source_size, PAGE_READWRITE, &old);

        // copy the tls data from windows store binary into our current thread tls memory region
        auto tls_base = *(LPVOID*)(__readgsqword(0x58) + 8ull * tls_index);
        memcpy(tls_base, (void*)source_start, source_size);

        // copy the data from windows store into our dll's tls region
        memcpy((void*)target_start, (void*)source_start, source_size);

        // find the bo3 LDR data entry and assign it the correct TLS index
        {
            auto peb = NtCurrentPeb();
            auto head = &peb->Ldr->InMemoryOrderModuleList;

            int mc = 0;
            auto entry = head->Flink;
            while (entry != head)
            {
                auto table_entry = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
                auto n = static_cast<int>(offsetof(LDR_DATA_TABLE_ENTRY, DllBase));

                if ((uint64_t)table_entry->DllBase == REBASE(0))
                {
                    *(uint16_t*)((uint64_t)table_entry + 0x6E) = (uint16_t)tls_index;

                    // also while we are here lets fix up other image attributes
                    *(uint64_t*)((uint64_t)table_entry + 0x40) = (uint64_t)wbo3.imageSize();
                    *(uint64_t*)((uint64_t)table_entry + 0x38) = (uint64_t)wbo3.entryPoint();

                    break;
                }

                entry = entry->Flink;
            }
        }

        // restore the original steam bo3's TLS buffer so that LdrpTlsList doesnt have bad data
        auto restored_tls_adr = VirtualAlloc((void*)original_tls.StartAddressOfRawData, source_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        memcpy(restored_tls_adr, (void*)source->StartAddressOfRawData, source_size);
    }

    // invoke the original entrypoint of this dll (to apply hooks, etc)
    ALOG("Calling our dll entrypoint...");
    DllMain(__thismodule, DLL_PROCESS_ATTACH, NULL);

    // invoke TLS of windows store bo3
    ALOG("Calling WSBO3's TLS...");
    for (const auto& cb : wbo3.tls())
    {
        cb.callback()((void*)REBASE(0), DLL_PROCESS_ATTACH, NULL);
    }

    for (const auto& cb : wbo3.tls())
    {
        // change our tls callback to be theirs
        chgmem<uint64_t>(our_mod.tls().descriptor().ptr->AddressOfCallBacks, (uint64_t)cb.callback());
        break;
    }

    // invoke the main module entrypoint
    ALOG("Calling WSBO3's entrypoint...");
    ((void(__fastcall*)(PVOID, uint32_t, const char*, uint32_t))wbo3.entryPoint())((void*)REBASE(0), DLL_PROCESS_ATTACH, "", SW_SHOW);

    ALOG("Exiting process...");
}

bool start_winstore_exe()
{
    if (*(uint32_t*)(REBASE(0) + 0x3C) != 0x160)
    {
        return false; // this is not the steam executable we expect
    }

    ALOG("Steam BO3 detected... patching module!");

    // 1. Remove the original TLS callbacks in bo3
    auto bo3 = Pe::PeNative::fromModule((const void*)REBASE(0));
    for (const auto& cb : bo3.tls())
    {
        chgmem<uint8_t>((uint64_t)cb.callback(), 0xC3);
    }

    // 2. Hook the entrypoint of the original bo3 to jump into our code
    char code[] = { 0x48, 0xB8, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, 0xFF, 0xE0 };
    *(uint64_t*)(code + 2) = (uint64_t)replace_steam_exe;
    chgmem(bo3.entryPoint(), sizeof(code), code);
    
    return true;
}
#endif

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:

    {
        __thismodule = hModule;

#if REMAP_EXPERIMENT
        if (start_winstore_exe())
        {
            is_remapped_exe = true;
            return TRUE;
        }
#endif
        if (*(uint32_t*)(REBASE(0) + 0x3C) != 0x1A0)
        {
            return TRUE; // this is not the windows store executable we expect
        }

        {
            add_prehooks();

            SECURITY_ATTRIBUTES sa;
            sa.nLength = sizeof(SECURITY_ATTRIBUTES);
            sa.bInheritHandle = 1;
            sa.lpSecurityDescriptor = 0;
            dbg_is_console_available = CreatePipe(&dbg_hReadConsole, &dbg_hWriteConsole, &sa, 0xF4240);
            ALOG("Console available...");

            CreateThread(NULL, NULL, watch_ready_inject, NULL, NULL, NULL);
        }
    }
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

EXPORT void dummy_proc() {}

EXPORT uint64_t GetDevPipe()
{
    return (uint64_t)dbg_hReadConsole;
}
EXPORT void NotifyDebuggerAttached(const char* password)
{
    dbg_is_attached = true;
}

namespace Iat_hook_
{
    void** find(const char* function, HMODULE module)
    {
        if (!module)
            module = GetModuleHandle(0);

        PIMAGE_DOS_HEADER img_dos_headers = (PIMAGE_DOS_HEADER)module;
        PIMAGE_NT_HEADERS img_nt_headers = (PIMAGE_NT_HEADERS)((BYTE*)img_dos_headers + img_dos_headers->e_lfanew);
        PIMAGE_IMPORT_DESCRIPTOR img_import_desc = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)img_dos_headers + img_nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        if (img_dos_headers->e_magic != IMAGE_DOS_SIGNATURE)
            printf("\n");

        for (IMAGE_IMPORT_DESCRIPTOR* iid = img_import_desc; iid->Name != 0; iid++) {
            for (int func_idx = 0; *(func_idx + (void**)(iid->FirstThunk + (size_t)module)) != nullptr; func_idx++) {
                char* mod_func_name = (char*)(*(func_idx + (size_t*)(iid->OriginalFirstThunk + (size_t)module)) + (size_t)module + 2);
                const intptr_t nmod_func_name = (intptr_t)mod_func_name;
                if (nmod_func_name >= 0) {
                    if (!::strcmp(function, mod_func_name))
                        return func_idx + (void**)(iid->FirstThunk + (size_t)module);
                }
            }
        }

        return 0;

    }

    uintptr_t detour_iat_ptr(const char* function, void* newfunction, HMODULE module)
    {
        auto&& func_ptr = find(function, module);
        if (*func_ptr == newfunction || *func_ptr == nullptr)
            return 0;

        DWORD old_rights, new_rights = PAGE_READWRITE;
        VirtualProtect(func_ptr, sizeof(uintptr_t), new_rights, &old_rights);
        uintptr_t ret = (uintptr_t)*func_ptr;
        *func_ptr = newfunction;
        VirtualProtect(func_ptr, sizeof(uintptr_t), old_rights, &new_rights);
        return ret;
    }
};