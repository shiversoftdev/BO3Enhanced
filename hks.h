#pragma once
#include <stdint.h>

#define PLATFORM_MAC 0
#define PLATFORM_WIN64_STEAM 1
#define PLATFORM_WIN64_MSTOR 2

#define HKS_CURRENT_PLATFORM PLATFORM_WIN64_MSTOR

struct _jmp_buf;
struct hksInstruction;
struct lua_Debug;
struct lua_State;
struct LUIPoolUserData;
struct OpenedMenuData;
struct HksObject;
struct luaL_Reg;
struct HksCompilerSettings;

union HksValue;

namespace hks
{
	struct RuntimeProfileData;
	struct MemoryManager;
	struct DebugHook;
	struct DebugInstance;
	struct HksGlobal;
	struct ApiStack;
	struct CallStack;
	struct CallSite;
	struct cclosure;
	struct HksClosure;
	struct HashTable;
	struct UserData;
	struct StructInst;
	struct InternString;
	struct StringPinner;
	struct StringTable;
	struct Metatable;
	struct StaticStringCache;
	struct UpValue;
	struct Method;
	struct HksGcWeights;
	struct ResumeData_Header;
	struct ResumeData_State;
	struct ResumeData_Table;
	struct ResumeData_Closure;
	struct ResumeData_CClosure;
	struct ResumeData_Userdata;
	union ResumeData_Entry;
	struct GarbageCollector;
}

typedef int32_t hksBool;
typedef char hksChar;
typedef uint8_t hksByte;
typedef int16_t hksShort16;
typedef uint16_t hksUshort16;
typedef float HksNumber;
typedef int32_t hksInt32;
typedef uint32_t hksUint32;
typedef int64_t hksInt64;
typedef uint64_t hksUint64;
typedef size_t hksSize;
typedef int32_t ErrorCode;

enum HksError : hksInt32
{
	HKS_NO_ERROR = 0x0,
	LUA_ERRSYNTAX = -1,
	LUA_ERRFILE = -2,
	LUA_ERRRUN = -3,
	LUA_ERRMEM = -4,
	LUA_ERRERR = -5,
	HKS_THROWING_ERROR = -6,
	HKS_GC_YIELD = 0x1,
};

enum HksBytecodeEndianness : hksInt32
{
	HKS_BYTECODE_DEFAULT_ENDIAN = 0x0,
	HKS_BYTECODE_BIG_ENDIAN = 0x1,
	HKS_BYTECODE_LITTLE_ENDIAN = 0x2,
};

enum HksBytecodeSharingMode : hksInt64
{
	HKS_BYTECODE_SHARING_OFF = 0,
	HKS_BYTECODE_SHARING_ON = 1,
	HKS_BYTECODE_SHARING_SECURE = 2
};

enum HksObjectType : hksInt32
{
	TANY = -2,
	TNONE = -1,
	TNIL = 0x0,
	TBOOLEAN = 0x1,
	TLIGHTUSERDATA = 0x2,
	TNUMBER = 0x3,
	TSTRING = 0x4,
	TTABLE = 0x5,
	TFUNCTION = 0x6,
	TUSERDATA = 0x7,
	TTHREAD = 0x8,
	TIFUNCTION = 0x9,
	TCFUNCTION = 0xA,
	TUI64 = 0xB,
	TSTRUCT = 0xC,
	NUM_TYPE_OBJECTS = 0xE,
};

typedef hksUint32 hksBytecodeInstruction;
typedef hksUint32 HksNativeValueAsInt;
typedef HksObject HksRegister;
typedef void* (*lua_Alloc)(void*, void*, hksSize, hksSize);
typedef hksInt32(*hks_debug_map)(const char*, hksInt32);
typedef hksInt32(*lua_CFunction)(void*);
typedef void (*lua_Hook)(lua_State*, lua_Debug*);

namespace hks
{
	typedef void (*HksLogFunc)(lua_State*, const char*, ...);
	typedef void (*HksEmergencyGCFailFunc)(lua_State*, size_t);
	typedef CallSite* ErrorHandler;
	typedef int HksGcCost;
}

namespace hks
{
	struct GenericChunkHeader
	{
		hksSize m_flags;
	};

	struct ChunkHeader : GenericChunkHeader
	{
		ChunkHeader* m_next;
	};

	struct ChunkList
	{
		ChunkHeader m_head;
	};
}

struct _jmp_buf
{
	void* _jb[12];
};

struct hksInstruction
{
	hksBytecodeInstruction code;
};

struct lua_Debug
{
	hksInt32 event;
	const char* name;
	const char* namewhat;
	const char* what;
	const char* source;
	hksInt32 currentline;
	hksInt32 nups;
	hksInt32 nparams;
	hksBool ishksfunc;
	hksInt32 linedefined;
	hksInt32 lastlinedefined;
	char short_src[512];
	hksInt32 callstack_level;
	hksBool is_tail_call;
};

struct LUIPoolUserData
{
	hksUshort16 poolIndex;
	hksUshort16 iteration;
};

struct OpenedMenuData
{
	hksInt32 nameHash;
	LUIPoolUserData menuElementPoolData;
	char name[32];
};

union HksValue
{
	hks::cclosure* cClosure;
	hks::HksClosure* closure;
	hks::UserData* userData;
	hks::HashTable* table;
	hks::StructInst* tstruct;
	hks::InternString* str;
	lua_State* thread;
	void* ptr;
	HksNumber number;
	HksNativeValueAsInt native;
	hksInt32 boolean;
};

struct HksObject
{
	hksUint32 t;
	HksValue v;
};

struct luaL_Reg
{
	const char* name;
	lua_CFunction func;
};

int hks_identity_map(const char* filename, int lua_line);

struct HksCompilerSettings
{
	enum BytecodeSharingFormat : hksInt32
	{
		BYTECODE_DEFAULT = 0x0,
		BYTECODE_INPLACE = 0x1,
		BYTECODE_REFERENCED = 0x2,
	};


	enum IntLiteralOptions : hksInt32
	{
		INT_LITERALS_NONE = 0x0,
		INT_LITERALS_LUD = 0x1,
		INT_LITERALS_32BIT = 0x1,
		INT_LITERALS_UI64 = 0x2,
		INT_LITERALS_64BIT = 0x2,
		INT_LITERALS_ALL = 0x3,
	};

	hksBool m_emitStructCode = 0;
	hksInt32 padding;
	const hksChar** m_stripNames = 0;
	BytecodeSharingFormat m_bytecodeSharingFormat = BYTECODE_INPLACE;
	IntLiteralOptions m_enableIntLiterals = INT_LITERALS_NONE;

	hks_debug_map m_debugMap = hks_identity_map;
};

namespace hks
{
	enum GCResumePhase : hksInt32
	{
		GC_STATE_MARKING_UPVALUES = 0x0,
		GC_STATE_MARKING_GLOBAL_TABLE = 0x1,
		GC_STATE_MARKING_REGISTRY = 0x2,
		GC_STATE_MARKING_PROTOTYPES = 0x3,
		GC_STATE_MARKING_SCRIPT_PROFILER = 0x4,
		GC_STATE_MARKING_FINALIZER_STATE = 0x5,
		GC_TABLE_MARKING_ARRAY = 0x6,
		GC_TABLE_MARKING_HASH = 0x7,
	};

	struct ResumeData_Header
	{
		HksObjectType m_type;
	};

	struct ResumeData_State
	{
		ResumeData_Header h;
		int padding;
		lua_State* m_state;
		GCResumePhase m_phase;
		int padding2;
		UpValue* m_pending;
	};

	struct ResumeData_Table
	{
		ResumeData_Header h;
		int padding;
		HashTable* m_table;
		hksUint32 m_arrayIndex;
		hksUint32 m_hashIndex;
		hksInt32 m_weakness;
		int padding2;
	};

	struct ResumeData_Closure
	{
		ResumeData_Header h;
		int padding;
		HksClosure* m_closure;
		hksInt32 m_index;
		int padding2;
	};

	struct ResumeData_CClosure
	{
		ResumeData_Header h;
		int padding;
		cclosure* m_cclosure;
		hksInt32 m_upvalueIndex;
		int padding2;
	};

	struct ResumeData_Userdata
	{
		ResumeData_Header h;
		int padding;
		UserData* m_data;
	};

	union ResumeData_Entry
	{
		ResumeData_State State;
		ResumeData_Table HashTable;
		ResumeData_Closure Closure;
		ResumeData_CClosure CClosure;
		ResumeData_Userdata Userdata;
	};

	struct HksGcWeights
	{
		HksGcCost m_removeString;
		HksGcCost m_finalizeUserdataNoMM;
		HksGcCost m_finalizeUserdataGcMM;
		HksGcCost m_cleanCoroutine;
		HksGcCost m_removeWeak;
		HksGcCost m_markObject;
		HksGcCost m_traverseString;
		HksGcCost m_traverseUserdata;
		HksGcCost m_traverseCoroutine;
		HksGcCost m_traverseWeakTable;
		HksGcCost m_freeChunk;
		HksGcCost m_sweepTraverse;
	};

	struct Method : ChunkHeader
	{
		struct hksInstructionArray
		{
			hksUint32 size;
			const hksInstruction* data;
		};

		struct HksObjectArray
		{
			hksUint32 size;
			HksObject* data;
		};

		struct MethodArray
		{
			hksUint32 size;
			Method** data;
		};

		struct LocalInfo
		{
			InternString* name;
			hksInt32 start_pc;
			hksInt32 end_pc;
		};

		struct hksUint32Array
		{
			hksUint32 size;
			hksUint32* data;
		};

		struct LocalInfoArray
		{
			hksUint32 size;
			LocalInfo* data;
		};

		struct InternStringArray
		{
			hksUint32 size;
			InternString** data;
		};

		typedef hksInstructionArray Instructions;
		typedef HksObjectArray Constants;
		typedef MethodArray Children;
		typedef LocalInfoArray Locals;
		typedef hksUint32Array LineInfo;
		typedef InternStringArray UpValueInfo;

		struct DebugInfo
		{
			hksUint32 line_defined;
			hksUint32 last_line_defined;
			LineInfo lineInfo;
			UpValueInfo upvalInfo;
			InternString* source;
			InternString* name;
			Locals localInfo;
		};

		hksUint32 hash;
		hksUshort16 num_upvals;
		hksUshort16 m_numRegisters;
		hksByte num_params;
		hksByte m_flags;
		Instructions instructions;
		Constants constants;
		Children children;
		DebugInfo* m_debug;
	};

	struct UpValue : ChunkHeader
	{
		HksObject m_storage;
		HksObject* loc;
		UpValue* m_next;
	};

	struct cclosure : ChunkHeader
	{
		lua_CFunction m_function;
		HashTable* m_env;
		hksShort16 m_numUpvalues;
		hksShort16 m_flags;
		InternString* m_name;
		HksObject m_upvalues[1];
	};

	struct UserData : ChunkHeader
	{
		HashTable* m_env;
		Metatable* m_meta;
		char m_data[8];
	};

	struct StaticStringCache
	{
		HksObject m_objects[42];
	};

	struct HashTable : ChunkHeader
	{
		struct Node
		{
			HksRegister m_key;
			HksRegister m_value;
		};

		Metatable* m_meta;
		hksUint32 m_mask;
		Node* m_hashPart;
		HksRegister* m_arrayPart;
		hksUint32 m_arraySize;
		Node* m_freeNode;
	};

	struct Metatable
	{
		__int8 gap0[1];
	};

	struct InternString : GenericChunkHeader
	{
		hksSize m_lengthbits;
		hksUint32 m_hash;
		char m_data[30];
		char padding[6];
	};

	struct StringTable
	{
		InternString** m_data;
		hksUint32 m_count;
		hksUint32 m_mask;
		StringPinner* m_pinnedStrings;
	};

	struct StringPinner
	{
		struct Node
		{
			InternString* m_strings[32];
			Node* m_prev;
		};

		lua_State* const m_state;
		StringPinner* const m_prev;
		InternString** m_nextStringsPlace;
		Node m_firstNode;
		Node* m_currentNode;
	};

	struct CallSite
	{
		_jmp_buf m_jumpBuffer;
		CallSite* m_prev;
	};

	struct CallStack
	{
		struct ActivationRecord
		{
			HksObject* m_base;
			const hksInstruction* m_returnAddress;
			hksShort16 m_tailCallDepth;
			hksShort16 m_numVarargs;
			hksInt32 m_numExpectedReturns;
		};

		ActivationRecord* m_records;
		ActivationRecord* m_lastrecord;
		ActivationRecord* m_current;
		const hksInstruction* m_current_lua_pc;
		const hksInstruction* m_hook_return_addr;
		hksInt32 m_hook_level;
		hksInt32 padding;
	};

	struct ApiStack
	{
		HksObject* top;
		HksObject* base;
		HksObject* alloc_top;
		HksObject* bottom;
	};

	struct MemoryManager
	{
		enum ChunkColor : hksInt32
		{
			WHITE = 0x0,
			BLACK = 0x1,
		};

		lua_Alloc m_allocator;
		void* m_allocatorUd;
		ChunkColor m_chunkColor;
		hksSize m_used;
		hksSize m_highwatermark;
		ChunkList m_allocationList;
		ChunkList m_sweepList;
		ChunkHeader* m_lastKeptChunk;
		lua_State* m_state;
	};

	struct GarbageCollector
	{
		struct ResumeStack
		{
			ResumeData_Entry* m_storage;
			hksInt32 m_numEntries;
			hksUint32 m_numAllocated;
		};

		struct GreyStack
		{
			HksObject* m_storage;
			hksSize m_numEntries;
			hksSize m_numAllocated;
		};

		struct RemarkStack
		{
			HashTable** m_storage;
			hksSize m_numAllocated;
			hksSize m_numEntries;
		};

		struct WeakStack_Entry
		{
			hksInt32 m_weakness;
			HashTable* m_table;
		};

		struct WeakStack
		{
			WeakStack_Entry* m_storage;
			hksInt32 m_numEntries;
			hksUint32 m_numAllocated;
		};

		HksGcCost m_target;
		HksGcCost m_stepsLeft;
		HksGcCost m_stepLimit;
		HksGcWeights m_costs;
		HksGcCost m_unit;
		_jmp_buf* m_jumpPoint;
		lua_State* m_mainState;
		lua_State* m_finalizerState;
		MemoryManager* m_memory;
		void* m_emergencyGCMemory;
		hksInt32 m_phase;
		ResumeStack m_resumeStack;
		GreyStack m_greyStack;
		RemarkStack m_remarkStack;
		WeakStack m_weakStack;
		hksBool m_finalizing;
		HksObject m_safeTableValue;
		lua_State* m_startOfStateStackList;
		lua_State* m_endOfStateStackList;
		lua_State* m_currentState;
		HksObject m_safeValue;
		void* m_compiler;
		void* m_bytecodeReader;
		void* m_bytecodeWriter;
		hksInt32 m_pauseMultiplier;
		HksGcCost m_stepMultiplier;
		hksSize m_emergencyMemorySize;
		bool m_stopped;
		lua_CFunction m_gcPolicy;
		hksSize m_pauseTriggerMemoryUsage;
		hksInt32 m_stepTriggerCountdown;
		hksUint32 m_stringTableIndex;
		hksUint32 m_stringTableSize;
		UserData* m_lastBlackUD;
		UserData* m_activeUD;
	};

	struct RuntimeProfileData
	{
		struct Stats
		{
			hksUint64 hksTime;
			hksUint64 callbackTime;
			hksUint64 gcTime;
			hksUint64 cFinalizerTime;
			hksUint64 compilerTime;
			hksUint32 hkssTimeSamples;
			hksUint32 callbackTimeSamples;
			hksUint32 gcTimeSamples;
			hksUint32 compilerTimeSamples;
			hksUint32 num_newuserdata;
			hksUint32 num_tablerehash;
			hksUint32 num_pushstring;
			hksUint32 num_pushcfunction;
			hksUint32 num_newtables;
			hksInt32 padding;
		};

		hksInt64 stackDepth;
		hksInt64 callbackDepth;
		hksUint64 lastTimer;
		Stats frameStats;
		hksUint64 gcStartTime;
		hksUint64 finalizerStartTime;
		hksUint64 compilerStartTime;
		hksUint64 compilerStartGCTime;
		hksUint64 compilerStartGCFinalizerTime;
		hksUint64 compilerCallbackStartTime;
		hksInt64 compilerDepth;
		void* outFile;
		lua_State* rootState;
	};

	struct HksGlobal
	{
		MemoryManager m_memory;
		GarbageCollector m_collector;
		StringTable m_stringTable;
		hksInt64 padding3;
		HksBytecodeSharingMode m_bytecodeSharingMode;
		hksInt32 padding;
		HksObject m_registry;
		ChunkList m_userDataList;
		lua_State* m_root;
		StaticStringCache m_staticStringCache;
		DebugInstance* m_debugger;
		void* m_profiler;
		RuntimeProfileData m_runProfilerData;
		HksCompilerSettings m_compilerSettings;
		lua_CFunction m_panicFunction;
		void* m_luaplusObjectList;
		hksInt32 m_heapAssertionFrequency;
		hksInt32 m_heapAssertionCount;
		HksLogFunc m_logFunction;
		HksEmergencyGCFailFunc m_emergencyGCFailFunction;
		HksBytecodeEndianness m_bytecodeDumpEndianness;
		hksInt32 padding2;
	};

	struct DebugHook
	{
		lua_Hook m_callback;
		hksInt32 m_mask;
		hksInt32 m_count;
		hksInt32 m_counter;
		bool m_inuse;
		const hksInstruction* m_prevPC;
	};

	struct DebugInstance
	{
		struct RuntimeProfilerStats
		{
			hksInt32 hksTime;
			hksInt32 callbackTime;
			hksInt32 gcTime;
			hksInt32 cFinalizerTime;
			hksInt64 heapSize;
			hksInt64 num_newuserdata;
			hksInt64 num_pushstring;
		};

		hksInt32 m_savedObjects;
		hksInt32 m_keepAliveObjects;
		lua_State* m_activeState;
		lua_State* m_mainState;
		void* m_owner;
		hksInt32 m_DebuggerLevel;
		hksInt32 stored_Hook_level;
		bool m_clearHook;
		const hksInstruction* stored_Hook_return_addr;
		hksInt32 m_debugStepLastLine;
		DebugInstance* m_next;
		const hksInstruction* m_activePC;
		hksInt32 runtimeProfileSendBufferWritePosition;
		RuntimeProfilerStats runtimeProfileSendBuffer[30];
	};

	struct HksClosure : ChunkHeader
	{
		struct MethodCache
		{
			const HksObject* consts;
			const hksInstruction* inst;
			hksUshort16 m_numRegisters;
			hksByte m_flags;
			hksByte num_params;
		};

		Method* m_method;
		HashTable* m_env;
		hksByte m_mayHaveUpvalues;
		MethodCache m_cache;
		UpValue* m_upvalues[1];
	};
}

struct lua_State
{
	enum Status : hksInt32
	{
		NEW = 0x1,
		RUNNING = 0x2,
		YIELDED = 0x3,
		DEAD_ERROR = 0x4,
	};

	hks::ChunkHeader baseclass;
	hks::HksGlobal* m_global;
	hks::CallStack m_callStack;
	hks::ApiStack m_apistack;
	hks::UpValue* pending;
	HksObject globals;
	HksObject m_cEnv;
	hks::ErrorHandler m_callsites;
	hksInt32 m_numberOfCCalls;
	void* m_context;
	hks::InternString* m_name;
	lua_State* m_next;
	lua_State* m_nextStateStack;
	Status m_status;
	HksError m_error;
	hks::DebugHook m_debugHook;
};

int lua_pcall(lua_State* s, long nargs, long nresults);
void luaL_register(lua_State* s, const char* libname, const luaL_Reg* l);
void lua_pop(lua_State* s, int n);
void lua_setglobal(lua_State* s, const char* k);
void lua_setfield(lua_State* s, int index, const char* k);
HksNumber lua_tonumber(lua_State* s, int index);
const char* lua_tostring(lua_State* s, int index);
const void* lua_topointer(lua_State* s, int index);
int lua_toboolean(lua_State* s, int index);
void lua_pushnumber(lua_State* s, HksNumber n);
void lua_pushinteger(lua_State* s, int n);
void lua_pushnil(lua_State* s);
void lua_pushboolean(lua_State* s, int b);
void lua_pushvalue(lua_State* s, int index);
void lua_pushlstring(lua_State* s, const char* str, size_t l);
void lua_pushfstring(lua_State* s, const char* fmt, ...);
void lua_pushvfstring(lua_State* s, const char* fmt, va_list* argp);

int hksi_hksL_loadbuffer(lua_State* s, HksCompilerSettings* options, char const* buff, size_t sz, char const* name);
void hksI_openlib(lua_State* s, const char* libname, const luaL_Reg l[], int nup, int isHksFunc);
void hks_pushnamedcclosure(lua_State* s, lua_CFunction fn, int n, const char* functionName, int treatClosureAsFuncForProf);
const char* hksi_lua_pushvfstring(lua_State* s, const char* fmt, va_list* argp);
const char* hks_obj_tolstring(lua_State* s, HksObject* obj, size_t* len);
char Com_Error_(const char* file, int line, ErrorCode code, const char* fmt, ...);
void hksi_luaL_error(lua_State* s, const char* fmt, ...);
void luaL_argerror(lua_State* s, int narg, const char* extramsg);
void Lua_CoD_LuaStateManager_Error(const char* error, lua_State* luaVM);

namespace hks
{
	extern uintptr_t baseAddress;
	void InitializeApi();
	int CheckForHotReload(lua_State* s);
	int InitHotReload(lua_State* s);
	int vm_call_internal(lua_State* s, int nargs, int nresults, const hksInstruction* pc);
}

#if HKS_CURRENT_PLATFORM == PLATFORM_MAC
#define REBASE(x) (((uint64_t)x) - (uint64_t)0x100000000 + ????)
#define PTR_hksi_hksL_loadbuffer REBASE(0x101474566)
#define PTR_hksI_openlib REBASE(0x1014748A1)
#define PTR_hks_pushnamedcclosure REBASE(0x101446BB6)
#define PTR_hksi_lua_pushvfstring REBASE(0x101473EE8)
#define PTR_hks_obj_tolstring REBASE(0x10146D28B)
#define PTR_Com_Error_ REBASE(0x100E71E51)
#define PTR_hksi_luaL_error REBASE(0x10145D2E6)
#define PTR_luaL_argerror REBASE(0x1014731DA)
#define PTR_Lua_CoD_LuaStateManager_Error REBASE(0x101528B3C)
#define PTR_vm_call_internal REBASE(0x101475CE3)
#define PTR_lua_pcall REBASE(0x10147636C)


#define PTR_lua_pushlstring REBASE(0x1014467F4)
#elif HKS_CURRENT_PLATFORM == PLATFORM_WIN64_STEAM
#include <Windows.h>
#include <Winternl.h>
#define REBASE(x) (*(uint64_t*)((uint64_t)(NtCurrentTeb()->ProcessEnvironmentBlock) + 0x10) + (uint64_t)(x))
// find hks::CompilerFunc, up to hks::Compiler, up to correct func
#define PTR_hksi_hksL_loadbuffer REBASE(0x1D4BD80)
// name conflict for module '%s'
#define PTR_hksI_openlib REBASE(0x1D49440)
// find Lua_CoD_GenerateLuaState with "Yield", hks_pushnamedcclosure is the function above the function with "print"
#define PTR_hks_pushnamedcclosure REBASE(0x1D4BA70)
// luaL_error(v4, "name conflict in global table for module \"%s\"", v5); -> va_start, hksi_luaL_where, hksi_lua_pushvfstring
#define PTR_hksi_lua_pushvfstring REBASE(0x1D4E5A0)
// Failed to allocate a chunk of size: %d bytes\n\n, v10 = hks_obj_tolstring(v3, v9, 0i64);
#define PTR_hks_obj_tolstring REBASE(0x1D4B6C0)
// MPUI_ERROR_CAPS
#define PTR_Com_Error_ REBASE(0x20F8170)
// operator %s is not supported for %s %s %s
#define PTR_hksi_luaL_error REBASE(0x1D4D050)
// bad argument #%d (%s)
#define PTR_luaL_argerror REBASE(0x1D4CE50)
// %s (VM: %s):\n%s\n
#define PTR_Lua_CoD_LuaStateManager_Error REBASE(0x1F11DA0)
// Attempt to call a %s value
#define PTR_vm_call_internal REBASE(0x1D70FE0)
// Error processing event '%s' (enclosing if call)
#define PTR_lua_pcall REBASE(0x1D53DB0)

// ddlpairs (mac inlines this ugh)
#define PTR_lua_setfield REBASE(0x1429680)

// %s: %p (mac inlines this too UGH)
#define PTR_lua_topointer REBASE(0x1D4EF90)

// MAC WHY ARE YOU SO DUMB
#define PTR_lua_toboolean REBASE(0x14373D0)

// everything inlines this one :upside_down:
#define PTR_lua_toui64 REBASE(0x1D4C8A0)

// lastlinedefined
#define PTR_lua_pushlstring REBASE(0xA18430)
#elif HKS_CURRENT_PLATFORM == PLATFORM_WIN64_MSTOR
#include <Windows.h>
#include <Winternl.h>
#define REBASE(x) (*(uint64_t*)((uint64_t)(NtCurrentTeb()->ProcessEnvironmentBlock) + 0x10) + (uint64_t)(x))
#define PTR_hksi_hksL_loadbuffer REBASE(0x1E52BB0)
#define PTR_hksI_openlib REBASE(0x1E532A0)
#define PTR_hks_pushnamedcclosure REBASE(0x1E553A0)
#define PTR_hksi_lua_pushvfstring REBASE(0x1E55540)
#define PTR_hks_obj_tolstring REBASE(0x1E410A0)
#define PTR_Com_Error_ REBASE(0x2210B90)
#define PTR_hksi_luaL_error REBASE(0x1E528E0)
#define PTR_luaL_argerror REBASE(0x1E52470)
#define PTR_Lua_CoD_LuaStateManager_Error REBASE(0x20168F0)
#define PTR_vm_call_internal REBASE(0x1E54C00)
#define PTR_lua_pcall REBASE(0x1E54EF0)
#define PTR_lua_setfield REBASE(0x14E3EB0)
#define PTR_lua_topointer REBASE(0x1E18BE0)
#define PTR_lua_toboolean REBASE(0x14F1420)
#define PTR_lua_toui64 REBASE(0) // TODO
#define PTR_lua_pushlstring REBASE(0xAA2280)
#endif