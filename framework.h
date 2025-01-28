#pragma once
#include "callspoof.h"

#define WSINTERNAL_MOD_NAME "T7InternalWS.dll"
#define WSTORE_MOD_NAME "WSBlackOps3.exe"
#define WSTORE_UPDATER_WSID 3413662211
#define WSTORE_UPDATER_FILENAME "wsupdate.ff"
#define WSTORE_UPDATER_DEST_FILENAME "wsupdate.next"
#define VERSION_STRING "BO3Enhanced v1.03"

// If true, is development environment (printing, console, etc)
#define IS_DEVELOPMENT 0

// If true, enables the steamapi
#define ENABLE_STEAMAPI 1

// If false, disables all GDK components
#define USES_GDK 1

#if IS_DEVELOPMENT
#define DEV_SKIP_UPDATES 1
#define DEV_TEST_UPDATES 0
#define LOG_DEMONWARE 0
#define BADWORD_BYPASS 1
#define DWINVENTORY_UNLOCK_ALL 1
#define SCREENSHOT_OVERRIDE 1
#define ALLOC_CONSOLE 1
#define REMAP_EXPERIMENT 0
#define ALOG(...) nlog(__VA_ARGS__)
#else
#define ALOG(...)
#endif

// number of ticks to wait before checking steamauth (in ms)
#define STEAMAUTH_TICK_DELAY 15000
// number of ticks to wait before retrying steamauth for a client (in ms)
#define STEAMAUTH_CLIENT_RETRY_DELAY 30000
// number of ticks to wait before asking for a new auth ticket (minimum 1 minute for steam reasons) -- MUST BE A MULTIPLE OF STEAMAUTH_CLIENT_RETRY_DELAY
#define STEAMAUTH_DELAY_NEW_TIME (STEAMAUTH_CLIENT_RETRY_DELAY * 2)
// number of ticks to wait before disconnecting a client for failed steamauth (in ms)
#define STEAMAUTH_FAIL_TIMEOUT 180000
// time in ms to wait between checking to see if the active user is in a steam lobby
#define STEAMLOBBY_CHECK_DELAY 5000

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <istream>
#include <iostream>
#include <strsafe.h>

#define SystemHandleInformation 16
#include <Windows.h>
#include <iostream>
#include <Winternl.h>
#include <winnt.h>

#include <windows.h>
#include <stdio.h>

#define NT_SUCCESS(x) ((x) >= 0)
#define STATUS_INFO_LENGTH_MISMATCH 0xc0000004

#define SystemHandleInformation 16
#define ObjectBasicInformation 0
#define ObjectNameInformation 1
#define ObjectTypeInformation 2

typedef NTSTATUS(NTAPI* _NtQuerySystemInformation)(
	ULONG SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength
	);
typedef NTSTATUS(NTAPI* _NtDuplicateObject)(
	HANDLE SourceProcessHandle,
	HANDLE SourceHandle,
	HANDLE TargetProcessHandle,
	PHANDLE TargetHandle,
	ACCESS_MASK DesiredAccess,
	ULONG Attributes,
	ULONG Options
	);
typedef NTSTATUS(NTAPI* _NtQueryObject)(
	HANDLE ObjectHandle,
	ULONG ObjectInformationClass,
	PVOID ObjectInformation,
	ULONG ObjectInformationLength,
	PULONG ReturnLength
	);


typedef struct _SYSTEM_HANDLE
{
	ULONG ProcessId;
	BYTE ObjectTypeNumber;
	BYTE Flags;
	USHORT Handle;
	PVOID Object;
	ACCESS_MASK GrantedAccess;
} SYSTEM_HANDLE, * PSYSTEM_HANDLE;

typedef struct _SYSTEM_HANDLE_INFORMATION
{
	ULONG HandleCount;
	SYSTEM_HANDLE Handles[1];
} SYSTEM_HANDLE_INFORMATION, * PSYSTEM_HANDLE_INFORMATION;

typedef enum _POOL_TYPE
{
	NonPagedPool,
	PagedPool,
	NonPagedPoolMustSucceed,
	DontUseThisType,
	NonPagedPoolCacheAligned,
	PagedPoolCacheAligned,
	NonPagedPoolCacheAlignedMustS
} POOL_TYPE, * PPOOL_TYPE;

typedef struct _OBJECT_TYPE_INFORMATION
{
	ULONG TotalNumberOfObjects;
	ULONG TotalNumberOfHandles;
	ULONG TotalPagedPoolUsage;
	ULONG TotalNonPagedPoolUsage;
	ULONG TotalNamePoolUsage;
	ULONG TotalHandleTableUsage;
	ULONG HighWaterNumberOfObjects;
	ULONG HighWaterNumberOfHandles;
	ULONG HighWaterPagedPoolUsage;
	ULONG HighWaterNonPagedPoolUsage;
	ULONG HighWaterNamePoolUsage;
	ULONG HighWaterHandleTableUsage;
	ULONG InvalidAttributes;
	GENERIC_MAPPING GenericMapping;
	ULONG ValidAccess;
	BOOLEAN SecurityRequired;
	BOOLEAN MaintainHandleCount;
	USHORT MaintainTypeList;
	POOL_TYPE PoolType;
	ULONG PagedPoolUsage;
	ULONG NonPagedPoolUsage;
} OBJECT_TYPE_INFORMATION, * POBJECT_TYPE_INFORMATION;

namespace nt
{

    typedef struct _CLIENT_ID
    {
        PVOID UniqueProcess;
        PVOID UniqueThread;
    } CLIENT_ID, * PCLIENT_ID;


    typedef struct _PEB_LDR_DATA
    {
        ULONG Length;
        UCHAR Initialized;
        PVOID SsHandle;
        LIST_ENTRY InLoadOrderModuleList;
        LIST_ENTRY InMemoryOrderModuleList;
        LIST_ENTRY InInitializationOrderModuleList;
        PVOID EntryInProgress;
    } PEB_LDR_DATA, * PPEB_LDR_DATA;

    typedef struct _PROCESS_BASIC_INFORMATION
    {
        NTSTATUS ExitStatus;
        PPEB PebBaseAddress;
        KAFFINITY AffinityMask;
        KPRIORITY BasePriority;
        HANDLE UniqueProcessId;
        HANDLE InheritedFromUniqueProcessId;
    } PROCESS_BASIC_INFORMATION, * PPROCESS_BASIC_INFORMATION;

    typedef struct _UNICODE_STRING
    {
        WORD Length;
        WORD MaximumLength;
        WORD* Buffer;
    } UNICODE_STRING, * PUNICODE_STRING;

    typedef struct _CURDIR
    {
        UNICODE_STRING DosPath;
        PVOID Handle;
    } CURDIR, * PCURDIR;


    typedef struct _STRING
    {
        WORD Length;
        WORD MaximumLength;
        CHAR* Buffer;
    } STRING, * PSTRING;


    typedef struct _RTL_DRIVE_LETTER_CURDIR
    {
        WORD Flags;
        WORD Length;
        ULONG TimeStamp;
        STRING DosPath;
    } RTL_DRIVE_LETTER_CURDIR, * PRTL_DRIVE_LETTER_CURDIR;

    typedef struct _RTL_USER_PROCESS_PARAMETERS
    {
        ULONG MaximumLength;
        ULONG Length;
        ULONG Flags;
        ULONG DebugFlags;
        PVOID ConsoleHandle;
        ULONG ConsoleFlags;
        PVOID StandardInput;
        PVOID StandardOutput;
        PVOID StandardError;
        CURDIR CurrentDirectory;
        UNICODE_STRING DllPath;
        UNICODE_STRING ImagePathName;
        UNICODE_STRING CommandLine;
        PVOID Environment;
        ULONG StartingX;
        ULONG StartingY;
        ULONG CountX;
        ULONG CountY;
        ULONG CountCharsX;
        ULONG CountCharsY;
        ULONG FillAttribute;
        ULONG WindowFlags;
        ULONG ShowWindowFlags;
        UNICODE_STRING WindowTitle;
        UNICODE_STRING DesktopInfo;
        UNICODE_STRING ShellInfo;
        UNICODE_STRING RuntimeData;
        RTL_DRIVE_LETTER_CURDIR CurrentDirectores[32];
        ULONG EnvironmentSize;
    } RTL_USER_PROCESS_PARAMETERS, * PRTL_USER_PROCESS_PARAMETERS;


    struct _PEB_FREE_BLOCK;
    typedef struct _PEB_FREE_BLOCK* PPEB_FREE_BLOCK;

    typedef struct _PEB_FREE_BLOCK
    {
        PPEB_FREE_BLOCK Next;
        ULONG Size;
    } PEB_FREE_BLOCK, * PPEB_FREE_BLOCK;


    typedef struct _PEB
    {
        UCHAR InheritedAddressSpace;
        UCHAR ReadImageFileExecOptions;
        UCHAR BeingDebugged;
        UCHAR BitField;
        ULONG ImageUsesLargePages : 1;
        ULONG IsProtectedProcess : 1;
        ULONG IsLegacyProcess : 1;
        ULONG IsImageDynamicallyRelocated : 1;
        ULONG SpareBits : 4;
        PVOID Mutant;
        PVOID ImageBaseAddress;
        PPEB_LDR_DATA Ldr;
        PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
        PVOID SubSystemData;
        PVOID ProcessHeap;
        PRTL_CRITICAL_SECTION FastPebLock;
        PVOID AtlThunkSListPtr;
        PVOID IFEOKey;
        ULONG CrossProcessFlags;
        ULONG ProcessInJob : 1;
        ULONG ProcessInitializing : 1;
        ULONG ReservedBits0 : 30;
        union
        {
            PVOID KernelCallbackTable;
            PVOID UserSharedInfoPtr;
        };
        ULONG SystemReserved[1];
        ULONG SpareUlong;
        PPEB_FREE_BLOCK FreeList;
        ULONG TlsExpansionCounter;
        PVOID TlsBitmap;
        ULONG TlsBitmapBits[2];
        PVOID ReadOnlySharedMemoryBase;
        PVOID HotpatchInformation;
        VOID** ReadOnlyStaticServerData;
        PVOID AnsiCodePageData;
        PVOID OemCodePageData;
        PVOID UnicodeCaseTableData;
        ULONG NumberOfProcessors;
        ULONG NtGlobalFlag;
        LARGE_INTEGER CriticalSectionTimeout;
        ULONG HeapSegmentReserve;
        ULONG HeapSegmentCommit;
        ULONG HeapDeCommitTotalFreeThreshold;
        ULONG HeapDeCommitFreeBlockThreshold;
        ULONG NumberOfHeaps;
        ULONG MaximumNumberOfHeaps;
        VOID** ProcessHeaps;
        PVOID GdiSharedHandleTable;
        PVOID ProcessStarterHelper;
        ULONG GdiDCAttributeList;
        PRTL_CRITICAL_SECTION LoaderLock;
        ULONG OSMajorVersion;
        ULONG OSMinorVersion;
        WORD OSBuildNumber;
        WORD OSCSDVersion;
        ULONG OSPlatformId;
        ULONG ImageSubsystem;
        ULONG ImageSubsystemMajorVersion;
        ULONG ImageSubsystemMinorVersion;
        ULONG ImageProcessAffinityMask;
        ULONG GdiHandleBuffer[34];
        PVOID PostProcessInitRoutine;
        PVOID TlsExpansionBitmap;
        ULONG TlsExpansionBitmapBits[32];
        ULONG SessionId;
        ULARGE_INTEGER AppCompatFlags;
        ULARGE_INTEGER AppCompatFlagsUser;
        PVOID pShimData;
        PVOID AppCompatInfo;
        UNICODE_STRING CSDVersion;
        PVOID* ActivationContextData;
        PVOID* ProcessAssemblyStorageMap;
        PVOID* SystemDefaultActivationContextData;
        PVOID* SystemAssemblyStorageMap;
        ULONG MinimumStackCommit;
        PVOID* FlsCallback;
        LIST_ENTRY FlsListHead;
        PVOID FlsBitmap;
        ULONG FlsBitmapBits[4];
        ULONG FlsHighIndex;
        PVOID WerRegistrationData;
        PVOID WerShipAssertPtr;
    } PEB, * PPEB;


    struct _RTL_ACTIVATION_CONTEXT_STACK_FRAME;
    typedef struct _RTL_ACTIVATION_CONTEXT_STACK_FRAME* PRTL_ACTIVATION_CONTEXT_STACK_FRAME;

    typedef struct _RTL_ACTIVATION_CONTEXT_STACK_FRAME
    {
        PRTL_ACTIVATION_CONTEXT_STACK_FRAME Previous;
        _ACTIVATION_CONTEXT* ActivationContext;
        ULONG Flags;
    } RTL_ACTIVATION_CONTEXT_STACK_FRAME, * PRTL_ACTIVATION_CONTEXT_STACK_FRAME;

    typedef struct _ACTIVATION_CONTEXT_STACK
    {
        PRTL_ACTIVATION_CONTEXT_STACK_FRAME ActiveFrame;
        LIST_ENTRY FrameListCache;
        ULONG Flags;
        ULONG NextCookieSequenceNumber;
        ULONG StackId;
    } ACTIVATION_CONTEXT_STACK, * PACTIVATION_CONTEXT_STACK;

    typedef struct _GDI_TEB_BATCH
    {
        ULONG Offset;
        ULONG HDC;
        ULONG Buffer[310];
    } GDI_TEB_BATCH, * PGDI_TEB_BATCH;

    typedef struct _TEB_ACTIVE_FRAME_CONTEXT
    {
        ULONG Flags;
        CHAR* FrameName;
    } TEB_ACTIVE_FRAME_CONTEXT, * PTEB_ACTIVE_FRAME_CONTEXT;

    struct _PTEB_ACTIVE_FRAME;
    typedef struct _TEB_ACTIVE_FRAME* PTEB_ACTIVE_FRAME;
    typedef struct _TEB_ACTIVE_FRAME
    {
        ULONG Flags;
        PTEB_ACTIVE_FRAME Previous;
        PTEB_ACTIVE_FRAME_CONTEXT Context;
    } TEB_ACTIVE_FRAME, * PTEB_ACTIVE_FRAME;

    typedef struct _LDR_DATA_TABLE_ENTRY
    {
        LIST_ENTRY InLoadOrderLinks;
        LIST_ENTRY InMemoryOrderLinks;
        LIST_ENTRY InInitializationOrderLinks;
        PVOID DllBase;
        PVOID EntryPoint;
        ULONG SizeOfImage;
        UNICODE_STRING FullDllName;
        UNICODE_STRING BaseDllName;
        ULONG Flags;
        WORD LoadCount;
        WORD TlsIndex;
        union
        {
            LIST_ENTRY HashLinks;
            struct
            {
                PVOID SectionPointer;
                ULONG CheckSum;
            };
        };
        union
        {
            ULONG TimeDateStamp;
            PVOID LoadedImports;
        };
        _ACTIVATION_CONTEXT* EntryPointActivationContext;
        PVOID PatchInformation;
        LIST_ENTRY ForwarderLinks;
        LIST_ENTRY ServiceTagLinks;
        LIST_ENTRY StaticLinks;
    } LDR_DATA_TABLE_ENTRY, * PLDR_DATA_TABLE_ENTRY;

    typedef struct _TEB
    {
        NT_TIB NtTib;
        PVOID EnvironmentPointer;
        CLIENT_ID ClientId;
        PVOID ActiveRpcHandle;
        PVOID ThreadLocalStoragePointer;
        PPEB ProcessEnvironmentBlock;
        ULONG LastErrorValue;
        ULONG CountOfOwnedCriticalSections;
        PVOID CsrClientThread;
        PVOID Win32ThreadInfo;
        ULONG User32Reserved[26];
        ULONG UserReserved[5];
        PVOID WOW32Reserved;
        ULONG CurrentLocale;
        ULONG FpSoftwareStatusRegister;
        VOID* SystemReserved1[54];
        LONG ExceptionCode;
        PACTIVATION_CONTEXT_STACK ActivationContextStackPointer;
        UCHAR SpareBytes1[36];
        ULONG TxFsContext;
        GDI_TEB_BATCH GdiTebBatch;
        CLIENT_ID RealClientId;
        PVOID GdiCachedProcessHandle;
        ULONG GdiClientPID;
        ULONG GdiClientTID;
        PVOID GdiThreadLocalInfo;
        ULONG Win32ClientInfo[62];
        VOID* glDispatchTable[233];
        ULONG glReserved1[29];
        PVOID glReserved2;
        PVOID glSectionInfo;
        PVOID glSection;
        PVOID glTable;
        PVOID glCurrentRC;
        PVOID glContext;
        ULONG LastStatusValue;
        UNICODE_STRING StaticUnicodeString;
        WCHAR StaticUnicodeBuffer[261];
        PVOID DeallocationStack;
        VOID* TlsSlots[64];
        LIST_ENTRY TlsLinks;
        PVOID Vdm;
        PVOID ReservedForNtRpc;
        VOID* DbgSsReserved[2];
        ULONG HardErrorMode;
        VOID* Instrumentation[9];
        GUID ActivityId;
        PVOID SubProcessTag;
        PVOID EtwLocalData;
        PVOID EtwTraceData;
        PVOID WinSockData;
        ULONG GdiBatchCount;
        UCHAR SpareBool0;
        UCHAR SpareBool1;
        UCHAR SpareBool2;
        UCHAR IdealProcessor;
        ULONG GuaranteedStackBytes;
        PVOID ReservedForPerf;
        PVOID ReservedForOle;
        ULONG WaitingOnLoaderLock;
        PVOID SavedPriorityState;
        ULONG SoftPatchPtr1;
        PVOID ThreadPoolData;
        VOID** TlsExpansionSlots;
        ULONG ImpersonationLocale;
        ULONG IsImpersonating;
        PVOID NlsCache;
        PVOID pShimData;
        ULONG HeapVirtualAffinity;
        PVOID CurrentTransactionHandle;
        PTEB_ACTIVE_FRAME ActiveFrame;
        PVOID FlsData;
        PVOID PreferredLanguages;
        PVOID UserPrefLanguages;
        PVOID MergedPrefLanguages;
        ULONG MuiImpersonation;
        WORD CrossTebFlags;
        ULONG SpareCrossTebBits : 16;
        WORD SameTebFlags;
        ULONG DbgSafeThunkCall : 1;
        ULONG DbgInDebugPrint : 1;
        ULONG DbgHasFiberData : 1;
        ULONG DbgSkipThreadAttach : 1;
        ULONG DbgWerInShipAssertCode : 1;
        ULONG DbgRanProcessInit : 1;
        ULONG DbgClonedThread : 1;
        ULONG DbgSuppressDebugMsg : 1;
        ULONG SpareSameTebBits : 8;
        PVOID TxnScopeEnterCallback;
        PVOID TxnScopeExitCallback;
        PVOID TxnScopeContext;
        ULONG LockCount;
        ULONG ProcessRundown;
        UINT64 LastSwitchTime;
        UINT64 TotalSwitchOutTime;
        LARGE_INTEGER WaitReasonBitMap;
    } TEB, * PTEB;

    typedef struct _THREAD_BASIC_INFORMATION {




        NTSTATUS                ExitStatus;
        PTEB                   TebBaseAddress;
        CLIENT_ID               ClientId;
        KAFFINITY               AffinityMask;
        KPRIORITY               Priority;
        KPRIORITY               BasePriority;



    } THREAD_BASIC_INFORMATION, * PTHREAD_BASIC_INFORMATION;

}

__forceinline
struct nt::_PEB*
    NtCurrentPeb(
        VOID
    )

{
    return ((struct nt::_TEB*)__readgsqword(FIELD_OFFSET(NT_TIB, Self)))->ProcessEnvironmentBlock;
}


#define EXPORT extern "C" __declspec(dllexport)
#define REBASE(x) (*(unsigned __int64*)((unsigned __int64)(NtCurrentTeb()->ProcessEnvironmentBlock) + 0x10) + (unsigned __int64)(x))


constexpr uint32_t fnv_base_32 = 0x4B9ACE2F;

inline uint32_t fnv1a(const char* key) {

	const char* data = key;
	uint32_t hash = 0x4B9ACE2F;
	while (*data)
	{
		hash ^= *data;
		hash *= 0x1000193;
		data++;
	}
	hash *= 0x1000193; // bo3 wtf lol
	return hash;

}

template <unsigned __int32 NUM>
struct canon_const_builtins
{
	static const unsigned __int32 value = NUM;
};

constexpr unsigned __int32 fnv1a_const(const char* input)
{
	const char* data = input;
	uint32_t hash = 0x4B9ACE2F;
	while (*data)
	{
		hash ^= *data;
		hash *= 0x01000193;
		data++;
	}
	hash *= 0x01000193; // bo3 wtf lol
	return hash;
}

#define FNV32(x) canon_const_builtins<fnv1a_const(x)>::value

struct scr_vec3_t
{
	float x;
	float y;
	float z;

	scr_vec3_t operator+(scr_vec3_t const& obj)
	{
		return { x + obj.x, y + obj.y, z + obj.z };
	}

	scr_vec3_t operator-(scr_vec3_t const& obj)
	{
		return { x - obj.x, y - obj.y, z - obj.z };
	}

	scr_vec3_t operator*(float mp)
	{
		return { x + mp, y + mp, z + mp };
	}

	scr_vec3_t(float _x, float _y, float _z) : x(_x), y(_y), z(_z)
	{

	}

	scr_vec3_t() : x(0), y(0), z(0)
	{

	}
};

enum errorCode : int32_t
{
    ERROR_NONE = 0x0,
    ERROR_FATAL = 0x1,
    ERROR_DROP = 0x2,
    ERROR_FROM_STARTUP = 0x4,
    ERROR_SERVERDISCONNECT = 0x8,
    ERROR_DISCONNECT = 0x10,
    ERROR_SCRIPT = 0x20,
    ERROR_SCRIPT_DROP = 0x40,
    ERROR_LOCALIZATION = 0x80,
    ERROR_UI = 0x100,
    ERROR_LUA = 0x200,
    ERROR_SOFTRESTART = 0x400,
    ERROR_SOFTRESTART_KEEPDW = 0x800
};

enum MsgType
{
    MESSAGE_TYPE_NONE = -1,
    MESSAGE_TYPE_INFO_REQUEST = 0,
    MESSAGE_TYPE_INFO_RESPONSE = 1,
    MESSAGE_TYPE_LOBBY_STATE_PRIVATE = 2,
    MESSAGE_TYPE_LOBBY_STATE_GAME = 3,
    MESSAGE_TYPE_LOBBY_STATE_GAMEPUBLIC = 4,
    MESSAGE_TYPE_LOBBY_STATE_GAMECUSTOM = 5,
    MESSAGE_TYPE_LOBBY_STATE_GAMETHEATER = 6,
    MESSAGE_TYPE_LOBBY_HOST_HEARTBEAT = 7,
    MESSAGE_TYPE_LOBBY_HOST_DISCONNECT = 8,
    MESSAGE_TYPE_LOBBY_HOST_DISCONNECT_CLIENT = 9,
    MESSAGE_TYPE_LOBBY_HOST_LEAVE_WITH_PARTY = 0xA,
    MESSAGE_TYPE_LOBBY_CLIENT_HEARTBEAT = 0xB,
    MESSAGE_TYPE_LOBBY_CLIENT_DISCONNECT = 0xC,
    MESSAGE_TYPE_LOBBY_CLIENT_RELIABLE_DATA = 0xD,
    MESSAGE_TYPE_LOBBY_CLIENT_CONTENT = 0xE,
    MESSAGE_TYPE_LOBBY_MODIFIED_STATS = 0xF,
    MESSAGE_TYPE_JOIN_LOBBY = 0x10,
    MESSAGE_TYPE_JOIN_RESPONSE = 0x11,
    MESSAGE_TYPE_JOIN_AGREEMENT_REQUEST = 0x12,
    MESSAGE_TYPE_JOIN_AGREEMENT_RESPONSE = 0x13,
    MESSAGE_TYPE_JOIN_COMPLETE = 0x14,
    MESSAGE_TYPE_JOIN_MEMBER_INFO = 0x15,
    MESSAGE_TYPE_SERVERLIST_INFO = 0x16,
    MESSAGE_TYPE_PEER_TO_PEER_CONNECTIVITY_TEST = 0x17,
    MESSAGE_TYPE_PEER_TO_PEER_INFO = 0x18,
    MESSAGE_TYPE_LOBBY_MIGRATE_TEST = 0x19,
    MESSAGE_TYPE_MIGRATE_ANNOUNCE_HOST = 0x1A,
    MESSAGE_TYPE_MIGRATE_START = 0x1B,
    MESSAGE_TYPE_INGAME_MIGRATE_TO = 0x1C,
    MESSAGE_TYPE_MIGRATE_NEW_HOST = 0x1D,
    MESSAGE_TYPE_VOICE_PACKET = 0x1E,
    MESSAGE_TYPE_VOICE_RELAY_PACKET = 0x1F,
    MESSAGE_TYPE_DEMO_STATE = 0x20,
    MESSAGE_TYPE_COUNT = MESSAGE_TYPE_DEMO_STATE + 1
};

enum AssetPool
{
	xasset_physpreset,
	xasset_physconstraints,
	xasset_destructibledef,
	xasset_xanim,
	xasset_xmodel,
	xasset_xmodelmesh,
	xasset_material,
	xasset_computeshaderset,
	xasset_techset,
	xasset_image,
	xasset_sound,
	xasset_sound_patch,
	xasset_col_map,
	xasset_com_map,
	xasset_game_map,
	xasset_map_ents,
	xasset_gfx_map,
	xasset_lightdef,
	xasset_lensflaredef,
	xasset_ui_map,
	xasset_font,
	xasset_fonticon,
	xasset_localize,
	xasset_weapon,
	xasset_weapondef,
	xasset_weaponvariant,
	xasset_weaponfull,
	xasset_cgmediatable,
	xasset_playersoundstable,
	xasset_playerfxtable,
	xasset_sharedweaponsounds,
	xasset_attachment,
	xasset_attachmentunique,
	xasset_weaponcamo,
	xasset_customizationtable,
	xasset_customizationtable_feimages,
	xasset_customizationtablecolor,
	xasset_snddriverglobals,
	xasset_fx,
	xasset_tagfx,
	xasset_klf,
	xasset_impactsfxtable,
	xasset_impactsoundstable,
	xasset_player_character,
	xasset_aitype,
	xasset_character,
	xasset_xmodelalias,
	xasset_rawfile,
	xasset_stringtable,
	xasset_structuredtable,
	xasset_leaderboarddef,
	xasset_ddl,
	xasset_glasses,
	xasset_texturelist,
	xasset_scriptparsetree,
	xasset_keyvaluepairs,
	xasset_vehicle,
	xasset_addon_map_ents,
	xasset_tracer,
	xasset_slug,
	xasset_surfacefxtable,
	xasset_surfacesounddef,
	xasset_footsteptable,
	xasset_entityfximpacts,
	xasset_entitysoundimpacts,
	xasset_zbarrier,
	xasset_vehiclefxdef,
	xasset_vehiclesounddef,
	xasset_typeinfo,
	xasset_scriptbundle,
	xasset_scriptbundlelist,
	xasset_rumble,
	xasset_bulletpenetration,
	xasset_locdmgtable,
	xasset_aimtable,
	xasset_animselectortable,
	xasset_animmappingtable,
	xasset_animstatemachine,
	xasset_behaviortree,
	xasset_behaviorstatemachine,
	xasset_ttf,
	xasset_sanim,
	xasset_lightdescription,
	xasset_shellshock,
	xasset_xcam,
	xasset_bgcache,
	xasset_texturecombo,
	xasset_flametable,
	xasset_bitfield,
	xasset_attachmentcosmeticvariant,
	xasset_maptable,
	xasset_maptableloadingimages,
	xasset_medal,
	xasset_medaltable,
	xasset_objective,
	xasset_objectivelist,
	xasset_umbra_tome,
	xasset_navmesh,
	xasset_navvolume,
	xasset_binaryhtml,
	xasset_laser,
	xasset_beam,
	xasset_streamerhint,
	xasset__string,
	xasset_assetlist,
	xasset_report,
	xasset_depend
};

enum netsrc_t
{
    NS_NULL = -1,
    NS_CLIENT1 = 0,
    NS_CLIENT2 = 1,
    NS_CLIENT3 = 2,
    NS_CLIENT4 = 3,
    NS_SERVER = 4,
    NS_MAXCLIENTS = 4,
    NS_PACKET = 5
};

enum netadrtype_t
{
    NA_BOT = 0x0,
    NA_BAD = 0x1,
    NA_LOOPBACK = 0x2,
    NA_RAWIP = 0x3,
    NA_IP = 0x4,
};

enum LobbyType
{
    LOBBY_TYPE_INVALID = 0xFFFFFFFF,
    LOBBY_TYPE_PRIVATE = 0x0,
    LOBBY_TYPE_GAME = 0x1,
    LOBBY_TYPE_TRANSITION = 0x2,
    LOBBY_TYPE_COUNT = 0x3,
    LOBBY_TYPE_FIRST = 0x0,
    LOBBY_TYPE_LAST = 0x2,
    LOBBY_TYPE_AUTO = 0x3,
};

enum LobbyModule
{
    LOBBY_MODULE_INVALID = 0xFFFFFFFF,
    LOBBY_MODULE_HOST = 0x0,
    LOBBY_MODULE_CLIENT = 0x1,
    LOBBY_MODULE_PEER_TO_PEER = 0x3,
    LOBBY_MODULE_COUNT = 0x4,
};

enum LobbyMode
{
    LOBBY_MODE_INVALID = 0xFFFFFFFF,
    LOBBY_MODE_PUBLIC = 0x0,
    LOBBY_MODE_CUSTOM = 0x1,
    LOBBY_MODE_THEATER = 0x2,
    LOBBY_MODE_ARENA = 0x3,
    LOBBY_MODE_FREERUN = 0x4,
    LOBBY_MODE_COUNT = 0x5,
};

enum SessionActive
{
    SESSION_INACTIVE = 0x0,
    SESSION_KEEP_ALIVE = 0x1,
    SESSION_ACTIVE = 0x2,
};

typedef uint64_t XUID;

// notes:
// ugcinfo struct on steam is 0x4C8
//                on wstor is 0x4BC (-0xC)

struct ugcinfo_entry_steam // 0x4C8
{
	char pad[0x4C8];
	// name: 0x0 (0x64)
	// internalName: 0x64 (
	// description: 0xA4
	// ugcName: 0x84
	// ugcVersion: ??
};

#define WORKSHOP_MAX_ENTRIES 128
struct ugcinfo_steam
{
	uint32_t num_entries;
	uint32_t unk4;
	ugcinfo_entry_steam entries[WORKSHOP_MAX_ENTRIES];
};

struct ugcinfo_entry_wstor // 0x4BC
{
	char name[0x64];
	char internalName[0x20]; // 64
	char ugcName[0x20]; // 84
	char description[0x100]; // A4
	char ugcPathBasic[0x104]; // 1A4
	char ugcRoot[0x104]; // 2A8
	char ugcPath[0x104]; // 3AC
	uint32_t unk_4B0;
	uint32_t hash_id; // 4B4
	uint32_t unk_4B8;
};

struct ugcinfo_wstor
{
	uint32_t num_entries;
	ugcinfo_entry_wstor entries[WORKSHOP_MAX_ENTRIES];
};

struct netadr_t
{
    uint8_t ipv4[4];
    uint16_t port;
    uint16_t pad;
    netadrtype_t type;
    netsrc_t localNetID;
};

struct msg_t
{
    uint8_t overflowed; // 0x0
    uint8_t readOnly; // 0x1
    uint16_t pad1; // 0x2
    uint32_t pad2; // 0x4
    char* data; // 0x8
    char* splitData; // 0x10
    uint32_t maxsize; // 0x18
    uint32_t cursize; // 0x1C
    uint32_t splitSize; // 0x20
    uint32_t readcount; // 0x24
    uint32_t bit; // 0x28
    uint32_t lastEntityRef; // 0x2C
    uint32_t flush; // 0x30
    netsrc_t targetLocalNetID; // 0x34
};

struct LobbyMsg
{
    msg_t msg;
    MsgType msgType; // 0x38
    char encodeFlags;
    uint32_t packageType;
};

struct bdSecurityID
{
    uint64_t id;
};

struct bdSecurityKey
{
    uint8_t ab[16];
};

struct SerializedAdr
{
    uint8_t valid;
    uint8_t addrBuf[37];
};

struct LobbyParams
{
    uint32_t networkMode;
    uint32_t mainMode;
};

struct InfoResponseLobby
{
    uint8_t isValid; // 0x0
    uint8_t pad[3];
    uint32_t pad2;
    XUID hostXuid; // 0x8
    char hostName[0x20]; // 0x10
    bdSecurityID secId; // 0x30
    bdSecurityKey secKey; // 0x38
    SerializedAdr serializedAdr; // 0x48
    uint8_t pad3[2]; // 0x6E
    LobbyParams lobbyParams; // 0x70
    char ugcName[0x20]; // 0x78
    uint32_t ugcVersion; // 0x98
    uint8_t pad4[0x4]; // 0x9C
};

struct Msg_InfoResponse
{
    uint32_t nonce;
    uint32_t uiScreen;
    uint8_t natType;
    uint8_t pad[3];
    uint32_t pad2;
    InfoResponseLobby lobby[2];
};

struct LobbySession
{
    LobbyModule module;
    LobbyType type;
    LobbyMode mode;
    char pad[0x34];
    SessionActive active;
    char pad2[0x121D4];
};

struct FixedClientInfo
{
    XUID xuid;
    char gamertag[32];
};

struct SessionInfo
{
    char inSession;
    char pad[3];
    netadr_t netAdr;
    time_t lastMessageSentToPeer;
};

struct ActiveClient
{
    char pad[0x410];

    // 410
    FixedClientInfo fixedClientInfo;

    char pad2[0xA8];
    // 4E0
    SessionInfo sessionInfo[2];
};

struct SessionClient
{
    char pad[0x8];
    ActiveClient* activeClient;
};

#include "msdetours.h"
#define MDT_Activate(fn_name) DetourAttach(&(PVOID&)__ ## fn_name, fn_name)
#define MDT_Deactivate(fn_name) DetourDetach(&(PVOID&)__ ## fn_name, fn_name)
#define MDT_ORIGINAL(fn_name, fn_params) __ ## fn_name fn_params
#define MDT_ORIGINAL_PTR(fn_name) __ ## fn_name
#define MDT_Define_FASTCALL(off, fn_name, fn_return, fn_params) typedef fn_return (__fastcall* __ ## fn_name ## _t)fn_params; \
__ ## fn_name ## _t __ ## fn_name = (__ ## fn_name ## _t) off;\
fn_return __fastcall fn_name ## fn_params

#define MDT_Define_FASTCALL_STATIC(off, fn_name, fn_return, fn_params) typedef fn_return (__fastcall* __ ## fn_name ## _t)fn_params; \
static __ ## fn_name ## _t __ ## fn_name = (__ ## fn_name ## _t) off; fn_return __fastcall fn_name ## fn_params

#define MDT_Implement_STATIC(ns, fn_name, fn_return, fn_params) fn_return __fastcall ns::fn_name ## fn_params

template <typename T> void chgmem(__int64 addy, T copy)
{
	DWORD oldprotect;
	VirtualProtect((void*)addy, sizeof(T), PAGE_EXECUTE_READWRITE, &oldprotect);
	*(T*)addy = copy;
	VirtualProtect((void*)addy, sizeof(T), oldprotect, &oldprotect);
}

void chgmem(__int64 addy, __int32 size, void* copy);
void noprange(__int64 addy, __int32 size);
void nlog(const char* str, ...);
int istrcmp_(const char* s1, const char* s2);
void MSG_WriteString(msg_t* sb, const char* s);
char I_CleanChar(char character);
void MSG_WriteData(msg_t* buf, const void* data, int length);
void MSG_WriteInt64(msg_t* buf, uint64_t data);
void MSG_WriteUInt16(msg_t* buf, uint16_t data);
void MSG_Init(msg_t* buf, char* data, uint32_t length);
void MSG_InitReadOnly(msg_t* buf, char* data, uint32_t length);
void MSG_BeginReading(msg_t* msg);
uint64_t MSG_ReadInt64(msg_t* msg);
void MSG_GetBytes(msg_t* msg, int where, uint8_t* dest, int len);
uint8_t MSG_GetByte(msg_t* msg, int _where);
int MSG_ReadBit(msg_t* msg);
uint8_t MSG_ReadByte(msg_t* msg);
void MSG_ReadData(msg_t* msg, void* data, int len);
uint8_t sanityCheckAndGetMsgType(msg_t* msg);
bool dwInstantHandleLobbyMessage(XUID senderID, const uint32_t controllerIndex, msg_t* msg);
bool LobbyMsgRW_PrepReadMsg(LobbyMsg* lobbyMsg, char* data, int length);
void MSG_WriteByte(msg_t* msg, uint8_t c);
char* MSG_ReadStringLine(msg_t* msg, char* string, uint32_t maxChars);
uint32_t MSG_ReadInt32(msg_t* msg);
uint16_t MSG_ReadInt16(msg_t* msg);
bool HasHost();
XUID GetXUID(uint32_t controller);
LobbyType LobbySession_GetControllingLobbySession(LobbyModule lobbyModule);
LobbySession* LobbySession_GetSession(LobbyType t);
void MSG_WriteBit1(msg_t* msg);
void MSG_WriteBit0(msg_t* msg);
SessionClient* LobbySession_GetClientByClientNum(LobbySession* session, uint32_t index);
netadr_t LobbySession_GetClientNetAdrByIndex(LobbyType type, uint32_t clientnum);

extern bool g_needsUpdatePrompt;

namespace Iat_hook_
{
	void** find(const char* function, HMODULE module);

	uintptr_t detour_iat_ptr(const char* function, void* newfunction, HMODULE module = 0);
};

#define IHOOK_HEADER(fn_name, fn_return, fn_args) using fn_name ## _fn = fn_return ## (__stdcall*)fn_args; \
fn_name ## _fn o ## fn_name; \
fn_return WINAPI __ ## fn_name fn_args
#define IHOOK_INSTALL(fn_name, module__) o ## fn_name = (fn_name ## _fn)Iat_hook_::detour_iat_ptr(#fn_name, (void*)__ ## fn_name, module__);
#define IHOOK_ORIGINAL(fn_name, fn_args) o ## fn_name fn_args

#define SPOOF_GADGET 0x1027
#define SPOOF_TRAMP REBASE(SPOOF_GADGET)
#define SPOOFED_CALL(a, ...) spoof_call((void*)SPOOF_TRAMP, a, __VA_ARGS__)

#define EXPORT extern "C" __declspec(dllexport)

#define s_contentPackMetaData REBASE(0x310C6B8)
#define PTR_Content_GetAvailableContentPacks REBASE(0x221A140)
#define ugc_modslist ((ugcinfo_wstor*)REBASE(0x18A58F60))
#define ugc_mapslist ((ugcinfo_wstor*)REBASE(0x18A7ED68))
#define currently_loaded_mod ((ugcinfo_entry_wstor*)REBASE(0x18A585E0))
#define OsPathRoot ((char*)REBASE(0x199B6F90))
#define PTR_ui_error_callstack_ship REBASE(0x8DEE3A8)
#define s_playerData_ptr REBASE(0x33614D0)

#define Cbuf_AddText(text) ((void(__fastcall*)(uint64_t, const char*, uint64_t))REBASE(0x2204960))(0, text, 0)
#define Dvar_SetFromStringByName(dvar, val) ((void(__fastcall*)(const char*, const char*, uint64_t, uint64_t, uint64_t))REBASE(0x23ABA90))(dvar, val, 0, 0, 1)
#define Mods_UpdateUsermapsList() ((void(__fastcall*)())REBASE(0x21EF6D0))()
#define Mods_UpdateModsList() ((void(__fastcall*)())REBASE(0x21EF780))()
#define Mods_LoadModStart(a1, entry, a3) ((void(__fastcall*)(uint32_t, ugcinfo_entry_wstor*, uint8_t))REBASE(0x21F03D0))(a1, entry, a3)
#define NET_OutOfBandData(sock, adr, data, len) ((bool(__fastcall*)(netsrc_t, netadr_t, char*, uint32_t))REBASE(0x2246080))(sock, adr, data, len)
#define s_runningUILevel (*(char*)REBASE(0x148FD0EF))
#define LobbyMsg_HandleIM(targetController, senderXUID, buff, len) ((bool(__fastcall*)(uint32_t, XUID, char*, int))REBASE(0x1FF8870))(targetController, senderXUID, buff, len)
#define LobbyMsg_PrepReadMsg(lobbyMsg) ((bool(__fastcall*)(LobbyMsg*))REBASE(0x1FF8FA0))(lobbyMsg)
#define Com_Error(code, fmt, ...) ((void(__fastcall*)(const char*, uint32_t, uint32_t, const char*, ...))REBASE(0x2210B90))("", 0, code, fmt, __VA_ARGS__)

#define CRASH_LOG_NAME "crashes.log"