#include "hks.h"
#include <cstdarg>

int hksi_hksL_loadbuffer(lua_State* s, HksCompilerSettings* options, char const* buff, size_t sz, char const* name)
{
	typedef int hksi_hksL_loadbuffer_t(lua_State*, HksCompilerSettings*, char const*, size_t, char const*);
	auto* f = (hksi_hksL_loadbuffer_t*)(PTR_hksi_hksL_loadbuffer);
	return f(s, options, buff, sz, name);
}

void hksI_openlib(lua_State* s, const char* libname, const luaL_Reg l[], int nup, int isHksFunc)
{
	typedef void hksI_openlib_t(lua_State*, const char*, const luaL_Reg[], int, int);
	auto* f = (hksI_openlib_t*)(PTR_hksI_openlib);
	f(s, libname, l, nup, isHksFunc);
}

void hks_pushnamedcclosure(lua_State* s, lua_CFunction fn, int n, const char* functionName, int treatClosureAsFuncForProf)
{
	typedef void hks_pushnamedcclosure_t(lua_State*, lua_CFunction, int, const char*, int);
	auto* f = (hks_pushnamedcclosure_t*)(PTR_hks_pushnamedcclosure);
	f(s, fn, n, functionName, treatClosureAsFuncForProf);
}

const char* hksi_lua_pushvfstring(lua_State* s, const char* fmt, va_list* argp)
{
	typedef const char* hksi_lua_pushvfstring_t(lua_State*, const char*, va_list*);
	auto f = (hksi_lua_pushvfstring_t*)(PTR_hksi_lua_pushvfstring);
	return f(s, fmt, argp);
}

const char* hks_obj_tolstring(lua_State* s, HksObject* obj, size_t* len)
{
	typedef const char* hks_obj_tolstring_t(lua_State*, HksObject*, size_t*);
	auto f = (hks_obj_tolstring_t*)(PTR_hks_obj_tolstring);
	return f(s, obj, len);
}

char Com_Error_(const char* file, int line, ErrorCode code, const char* fmt, ...)
{
	typedef char Com_Error_t(char const*, int, int, char const*, ...);
	auto f = (Com_Error_t*)(PTR_Com_Error_);
	return f(file, line, code, fmt);
}

void hksi_luaL_error(lua_State* s, const char* fmt, ...)
{
	typedef void hksi_luaL_error_t(lua_State*, const char*, ...);
	auto f = (hksi_luaL_error_t*)(PTR_hksi_luaL_error);
	f(s, fmt);
}

void luaL_argerror(lua_State* s, int narg, const char* extramsg)
{
	typedef void luaL_argerror_t(lua_State*, int, const char*);
	auto f = (luaL_argerror_t*)(PTR_luaL_argerror);
	f(s, narg, extramsg);
}

void Lua_CoD_LuaStateManager_Error(const char* error, lua_State* luaVM)
{
	typedef void Lua_CoD_LuaStateManager_Error_t(const char*, lua_State*);
	auto f = (Lua_CoD_LuaStateManager_Error_t*)(PTR_Lua_CoD_LuaStateManager_Error);
	f(error, luaVM);
}

namespace hks
{
	int vm_call_internal(lua_State* s, int nargs, int nresults, const hksInstruction* pc)
	{
		typedef int vm_call_internal_t(lua_State*, int, int, const hksInstruction*);
		auto f = (vm_call_internal_t*)(PTR_vm_call_internal);
		return f(s, nargs, nresults, pc);
	}
}

int hks_identity_map(const char* filename, int lua_line)
{
	return lua_line;
}


#define LUA_REGISTRYINDEX	(-10000)
#define LUA_ENVIRONINDEX	(-10001)
#define LUA_GLOBALSINDEX	(-10002)

HksObject getObjectForIndex(lua_State* s, int index)
{
    HksNumber result;
    HksObject* object;
    int failure;

    if (index <= LUA_REGISTRYINDEX)
    {
        failure = 0;
        switch (index)
        {
        case LUA_REGISTRYINDEX:
            object = &s->m_global->m_registry;
            break;
        case LUA_GLOBALSINDEX:
            object = &s->globals;
            break;
        case LUA_ENVIRONINDEX:
            s->m_cEnv.v.cClosure = (hks::cclosure*)s->m_apistack.base[-1].v.cClosure->m_env;
            s->m_cEnv.t = 5;
            object = &s->m_cEnv;
            break;
        default:
            object = (HksObject*)(&s->m_apistack.base[-1].v.cClosure->m_numUpvalues + 8 * (LUA_GLOBALSINDEX - index));
            break;
        }
    }
    else if (index <= 0)
    {
        if (index >= 0)
        {
            failure = 1;
            object = 0LL;
        }
        else if (&s->m_apistack.top[index] < s->m_apistack.base)
        {
            failure = 1;
            object = 0LL;
        }
        else
        {
            failure = 0;
            object = &s->m_apistack.top[index];
        }
    }
    else if (&s->m_apistack.base[index - 1] >= s->m_apistack.top)
    {
        failure = 1;
        object = 0LL;
    }
    else
    {
        failure = 0;
        object = &s->m_apistack.base[index - 1];
    }

    return *object;
}

int lua_pcall(lua_State* s, long nargs, long nresults)
{
    typedef int lua_pcall_t(lua_State* luaState, long, long);
    lua_pcall_t* f2 = (lua_pcall_t*)(PTR_lua_pcall);
    return f2(s, nargs, nresults);
}

void luaL_register(lua_State* s, const char* libname, const luaL_Reg* l)
{
    hksI_openlib(s, libname, l, 0, 1);
}

void lua_setfield(lua_State* s, int index, const char* k)
{
    typedef void lua_setfield_t(lua_State* luaState, int, const char*);
    auto f = (lua_setfield_t*)(PTR_lua_setfield);
    f(s, index, k);
}

void lua_setglobal(lua_State* s, const char* k)
{
    lua_setfield(s, LUA_GLOBALSINDEX, k);
}

void lua_pop(lua_State* s, int n)
{
    s->m_apistack.top -= n;
}

HksNumber lua_tonumber(lua_State* s, int index)
{
    auto object = getObjectForIndex(s, index);
    return object.v.number;
}

const char* lua_tostring(lua_State* s, int index)
{
    auto object = getObjectForIndex(s, index);
    return hks_obj_tolstring(s, &object, 0);
}

const void* lua_topointer(lua_State* s, int index)
{
    typedef const void* lua_topointer_t(lua_State* luaState, int);
    auto f = (lua_topointer_t*)(PTR_lua_topointer);
    return f(s, index);
}

int lua_toboolean(lua_State* s, int index)
{
    typedef int lua_toboolean_t(lua_State* luaState, int);
    auto f = (lua_toboolean_t*)(PTR_lua_toboolean);
    return f(s, index);
}

hksUint64 lua_toui64(lua_State* s, int index)
{
    typedef hksUint64 lua_toui64_t(lua_State* luaState, int);
    auto f = (lua_toui64_t*)(PTR_lua_toui64);
    return f(s, index);
}

void lua_pushnumber(lua_State* s, HksNumber n)
{
    auto top = s->m_apistack.top;
    top->v.number = n;
    top->t = TNUMBER;
    s->m_apistack.top = top + 1;
}

void lua_pushinteger(lua_State* s, int n)
{
    auto top = s->m_apistack.top;
    top->v.number = float(n);
    top->t = TNUMBER;
    s->m_apistack.top = top + 1;
}

void lua_pushnil(lua_State* s)
{
    auto top = s->m_apistack.top;
    top->v.number = 0;
    top->t = TNIL;
    s->m_apistack.top = top + 1;
}

void lua_pushboolean(lua_State* s, int b)
{
    auto top = s->m_apistack.top;
    top->v.boolean = b;
    top->t = TBOOLEAN;
    s->m_apistack.top = top + 1;
}

void lua_pushvalue(lua_State* s, int index)
{
    HksObject* st;
    auto object = getObjectForIndex(s, index);
    st = s->m_apistack.top;
    *st = object;
    s->m_apistack.top = st + 1;
}

void lua_pushlstring(lua_State* s, const char* str, size_t l)
{
    typedef void lua_pushlstring_t(lua_State* luaState, const char*, size_t);
    auto f = (lua_pushlstring_t*)(PTR_lua_pushlstring);
    f(s, str, l);
}

void lua_pushfstring(lua_State* s, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    hksi_lua_pushvfstring(s, fmt, &va);
}

void lua_pushvfstring(lua_State* s, const char* fmt, va_list* argp)
{
    hksi_lua_pushvfstring(s, fmt, argp);
}

void lua_getfield(lua_State* s, int index, const char* k)
{
    auto object = getObjectForIndex(s, index);

    const HksRegister v16;

    auto top = --s->m_apistack.top;
    top->v.cClosure = object.v.cClosure;
    s->m_apistack.top = top++;
}

void lua_getglobal(lua_State* s, const char* k)
{
    lua_getfield(s, LUA_GLOBALSINDEX, k);
}