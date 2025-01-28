#include "security.h"
#include <unordered_set>
#if ENABLE_STEAMAPI
#include "steam.h"
#include "steam/steam_api.h"
#endif

sec_config security::conf{};

#if ENABLE_STEAMAPI
uint64_t next_update_friendslist_time = 0;
std::unordered_set<XUID> friends_set;
bool IsFriendByXUIDUncached(XUID xuid) // ok I say its "uncached" but thats because I don't want this running a billion times per second and I think it might hitch with huge friends lists.
{
    if (GetTickCount64() < next_update_friendslist_time || !s_runningUILevel) // we will just not update the friends list in game because I really think this will hitch. STEAMAPI SUCKS
    {
        return friends_set.find(xuid) != friends_set.end();
    }

    int num_friends = SteamFriends()->GetFriendCount(k_EFriendFlagImmediate);
    friends_set.clear();

    for (int i = 0; i < num_friends; i++)
    {
        CSteamID out_friend = SteamFriends()->GetFriendByIndex(i, k_EFriendFlagImmediate);
        friends_set.insert((XUID)out_friend.ConvertToUint64());
    }

    next_update_friendslist_time = GetTickCount64() + 30 * 1000; // once every 30 seconds
    return friends_set.find(xuid) != friends_set.end();
}
#endif

MDT_Define_FASTCALL(REBASE(0x14F6710), dwInstantDispatchMessage_Internal_hook, bool, (XUID senderID, const uint32_t controllerIndex, void* message, uint32_t messageSize))
{
    char msgBuf[2048]{ 0 };
    LobbyMsg lm{};
    msg_t msg;
    uint32_t joinMsgType;
    MSG_InitReadOnly(&msg, (char*)message, messageSize);
    MSG_BeginReading(&msg);

    switch (sanityCheckAndGetMsgType(&msg))
    {
        // unsupported message types
        case 0x65: // remote command
        case 0x6D: // mobile message
            return 1;

        case 0x66: // friend IM
        {
            if ((msg.cursize - msg.readcount) != 0x64)
            {
                return 1; // invalid message size (causes error popup which we dont want)
            }
            
            MSG_ReadData(&msg, &joinMsgType, sizeof(uint32_t));
            if (!joinMsgType)
            {
                return 1; // invalid msg type (type 0 causes a forced crash)
            }
        }
        break;

        case 0x68: // lobby message
        {
            msg_t copy = msg;

            auto size = copy.cursize - copy.readcount;
            if (size >= 2048)
            {
                return 1;
            }

            MSG_ReadData(&copy, msgBuf, size);
            if (copy.overflowed)
            {
                return 1;
            }

            if (!LobbyMsgRW_PrepReadMsg(&lm, msgBuf, size))
            {
                return 1; // bad msg
            }

            if (lm.msgType == MESSAGE_TYPE_INFO_RESPONSE)
            {
                // due to security updates we luckily dont have to reimpl all this
                break;
            }
// friends only is only supported for the steam version of the game at this time
#if ENABLE_STEAMAPI
            else
            {
                if (security::conf.is_friends_only)
                {
                    if (!senderID || ((GetXUID(0) == senderID) || (GetXUID(1) == senderID)))
                    {
                        break; // local
                    }
  
                    if (!gSteamInit || IsFriendByXUIDUncached(senderID))
                    {
                        break; // friend :)
                    }

                    // not a friend, dont respond
                    return 1;
                }
            }
#endif
        }
        break;
    }

    return MDT_ORIGINAL(dwInstantDispatchMessage_Internal_hook, (senderID, controllerIndex, message, messageSize));
}

MDT_Define_FASTCALL(REBASE(0x2246240), Sys_Checksum_hook, uint16_t, (const unsigned char* msg, uint32_t size))
{
    auto res = MDT_ORIGINAL(Sys_Checksum_hook, (msg, size));
    res ^= security::conf.password_history[1];

    auto orig = *(uint16_t*)(msg + size);
    if (orig == res)
    {
        return res;
    }

    // grace period of old packets using old password (usually sent from localhost)
    if (GetTickCount64() <= (security::conf.password_changed_time + 1500))
    {
        res = res ^ (uint16_t)security::conf.password_history[1] ^ (uint16_t)security::conf.password_history[0];
    }

    return res;
}

bool security::handle_exception(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord)
{
    if ((uint64_t)ExceptionRecord->ExceptionAddress == REBASE(0x23F26C1)) // Sys_ChecksumCopy; mov [rbx+rdi], ax
    {
        ContextRecord->Rip += 4;
        *(uint16_t*)(ContextRecord->Rbx + ContextRecord->Rdi) = (uint16_t)(ContextRecord->Rax ^ conf.password_history[1]);
        return true;
    }

    return false;
}

DWORD WINAPI watch_settings_updates(_In_ LPVOID lpParameter)
{
    for (;;)
    {
        if (security::conf.update_watcher_time(PATCH_CONFIG_LOCATION))
        {
            security::conf.loadfrom(PATCH_CONFIG_LOCATION);
        }
        Sleep(1000);
    }
    return 0;
}

void load_settings_initial()
{
    if (!security::conf.fs_exists(PATCH_CONFIG_LOCATION))
    {
        security::conf.saveto(PATCH_CONFIG_LOCATION);
    }
    else
    {
        security::conf.loadfrom(PATCH_CONFIG_LOCATION);
    }
}

MDT_Define_FASTCALL(REBASE(0x1FF8E70), LobbyMsgRW_PrepWriteMsg_Hook, bool, (msg_t* lobbyMsg, __int64 data, int length, int msgType))
{
    if (MDT_ORIGINAL(LobbyMsgRW_PrepWriteMsg_Hook, (lobbyMsg, data, length, msgType)))
    {
        if (ZBR_PREFIX_BYTE)
        {
            MSG_WriteByte(lobbyMsg, ZBR_PREFIX_BYTE);
            MSG_WriteByte(lobbyMsg, ZBR_PREFIX_BYTE2);
        }
        return true;
    }

    return false;
}

MDT_Define_FASTCALL(REBASE(0x1FF8FA0), LobbyMsgRW_PrepReadMsg_Hook, bool, (msg_t* lm))
{
    if (MDT_ORIGINAL(LobbyMsgRW_PrepReadMsg_Hook, (lm)) && (!ZBR_PREFIX_BYTE || ((MSG_ReadByte(lm) == ZBR_PREFIX_BYTE) && (MSG_ReadByte(lm) == ZBR_PREFIX_BYTE2))))
    {
        SALOG("ALLOWING LM DUE TO CORRECT PREFIX");
        return true;
    }
    SALOG("DROPPING LM DUE TO INVALID PREFIX");
    return false;
}

void security::init()
{
    load_settings_initial();

    MDT_Activate(dwInstantDispatchMessage_Internal_hook);
    MDT_Activate(Sys_Checksum_hook);
    MDT_Activate(LobbyMsgRW_PrepReadMsg_Hook);
    MDT_Activate(LobbyMsgRW_PrepWriteMsg_Hook);

    chgmem<uint32_t>(REBASE(0x23F26C1), 0x90900B0F); // Sys_ChecksumCopy; mov [rbx+rdi], ax -> ud2, nop, nop
}

void security::init_delayed()
{
    // pick a random nonce start index for game joins through demonware
    std::srand(time(NULL));
    *(uint32_t*)REBASE(0x14912B38) = rand();

    CreateThread(NULL, NULL, watch_settings_updates, NULL, NULL, NULL);
}

uint64_t security::canon_hash64(const char* input)
{
    static const uint64_t offset = 14695981039346656037ULL;
    static const uint64_t prime = 1099511628211ULL;

    uint64_t hash = offset;
    const char* _input = input;

    while (*_input)
    {
        hash = (hash ^ tolower(*_input)) * prime;
        _input++;
    }

    return 0x7FFFFFFFFFFFFFFF & hash;
}

bool sec_config::fs_exists(const char* filename)
{
    if (!std::filesystem::exists(filename))
    {
        return false;
    }
    if (INVALID_FILE_ATTRIBUTES == GetFileAttributes(filename) && GetLastError() == ERROR_FILE_NOT_FOUND)
    {
        return false;
    }
    return true;
}

bool sec_config::update_watcher_time(const char* path)
{
    bool did_exist_before = exists;
    if (!fs_exists(path))
    {
        exists = false;
        return did_exist_before != exists;
    }

    exists = true;
    auto time = std::filesystem::last_write_time(path);

    bool was_same_time = modified == time;
    modified = time;

    return (did_exist_before != exists) || !was_same_time;
}

void sec_config::loadfrom(const char* path)
{
    std::ifstream infile;
    infile.open(path, std::ifstream::in | std::ifstream::binary);

    if (!infile.is_open())
    {
        update_watcher_time(path);
        return;
    }

    std::string line;
    while (!std::getline(infile, line).eof())
    {
        auto sep = line.find("=");
        if (sep == std::string::npos || sep >= (line.length() - 1)) // must have a value
        {
            continue;
        }

        // is this config resilliant to whitespace issues? nope!
        auto token = line.substr(0, sep);
        auto val = line.substr(sep + 1);
        switch (fnv1a(token.data()))
        {
        case FNV32("playername"):
        {
            if (val.length() > 15)
            {
                val = val.substr(0, 15);
            }
            std::strcpy(playername, val.data());
            SALOG("Read playername from config: %s", playername);
        }
        break;
        case FNV32("isfriendsonly"):
        {
            std::istringstream ivalread(val);
            ivalread >> is_friends_only;
            if (ivalread.fail())
            {
                is_friends_only = 0; // its better to have it fail then to have people who cant disable this setting because of whatever reason
            }
            SALOG("Read isfriendsonly from config: %u", is_friends_only);
        }
        break;
        case FNV32("networkpassword"):
        {
            if (val.length() > 1023)
            {
                val = val.substr(0, 1023); // seriously?!
            }

            auto hash = security::canon_hash64(val.data());
            password_changed_time = GetTickCount64();
            password_history[0] = password_history[1];
            password_history[1] = hash;

            SALOG("Read network password from config!");
        }
        break;
        }
    }

    infile.close();
    update_watcher_time(path);
}

void sec_config::saveto(const char* path)
{
    std::ofstream outfile;
    outfile.open(path, std::ofstream::out | std::ofstream::binary);

    if (!outfile.is_open())
    {
        update_watcher_time(path);
        return;
    }

    outfile << "playername=" << playername << std::endl;
    outfile << "isfriendsonly=" << is_friends_only << std::endl;
    outfile.close();
    update_watcher_time(path);
}