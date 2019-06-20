// puss_tinycc.inl

#ifndef PUSS_TCC_TRACE_ERROR
	#define PUSS_TCC_TRACE_ERROR(err)	fprintf(stderr, "PussTcc: %s\n", err)
	#define PUSS_TCC_TRACE_ERROR(err)	fprintf(stderr, "PussTcc: %s\n", err)
#endif

#ifndef PUSS_TCC_RT_TRACE_BEGIN
	#define PUSS_TCC_RT_TRACE_BEGIN()
#endif

#ifndef PUSS_TCC_RT_TRACE
	static void _puss_rt_trace(const char* msg) { fprintf(stderr, "%s", msg); }
	#define PUSS_TCC_RT_TRACE			_puss_rt_trace
#endif

#ifndef PUSS_TCC_RT_TRACE_END
	#define PUSS_TCC_RT_TRACE_END()
#endif

#ifndef PUSS_TCC_LUA_PCALL_BEGIN
  #define PUSS_TCC_LUA_PCALL_BEGIN(L) \
	unsigned short oldnCcalls = (L)->nCcalls; \
	struct lua_longjmp *previous = (L)->errorJmp;
#endif

#ifndef PUSS_TCC_LUA_PCALL_END
  #define PUSS_TCC_LUA_PCALL_END(L) \
	(L)->errorJmp = previous; \
	(L)->nCcalls = oldnCcalls;
#endif

#include <signal.h>
#include <setjmp.h>

#include "tinycc/libtcc.h"

#define PUSS_TCC_NAME		"[PussTcc]"

typedef struct _PussTccLua {
	TCCState*	s;
} PussTccLua;

static void puss_tcc_error(void *opaque, const char *msg) {
	PUSS_TCC_TRACE_ERROR(msg);
}

static int puss_tcc_destroy(lua_State* L) {
	PussTccLua* ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	if( ud->s ) {
		tcc_delete(ud->s);
		ud->s = NULL;
	}
	return 0;
}

static int puss_tcc_set_lib_path(lua_State* L) {
	PussTccLua* ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* path = luaL_checkstring(L, 2);
	if( ud->s ) {
		tcc_set_lib_path(ud->s, path);
	}
	return 0;
}

static int puss_tcc_set_options(lua_State* L) {
	PussTccLua* ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* options = luaL_checkstring(L, 2);
	if( ud->s ) {
		tcc_set_options(ud->s, options);
	}
	return 0;
}

static int puss_tcc_add_include_path(lua_State* L) {
	PussTccLua* ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* pathname = luaL_checkstring(L, 2);
	if( ud->s ) {
		tcc_add_include_path(ud->s, pathname);
	}
	return 0;
}

static int puss_tcc_add_sysinclude_path(lua_State* L) {
	PussTccLua* ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* pathname = luaL_checkstring(L, 2);
	if( ud->s ) {
		tcc_add_sysinclude_path(ud->s, pathname);
	}
	return 0;
}

static int puss_tcc_define_symbol(lua_State* L) {
	PussTccLua* ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* sym = luaL_checkstring(L, 2);
	const char* value = luaL_optstring(L, 3, NULL);
	if( ud->s ) {
		tcc_define_symbol(ud->s, sym, value);
	}
	return 0;
}

static int puss_tcc_undefine_symbol(lua_State* L) {
	PussTccLua* ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* sym = luaL_checkstring(L, 2);
	if( ud->s ) {
		tcc_undefine_symbol(ud->s, sym);
	}
	return 0;
}

static int puss_tcc_add_file(lua_State* L) {
	PussTccLua* ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* filename = luaL_checkstring(L, 2);
	if( ud->s ) {
		if( tcc_add_file(ud->s, filename) ) {
			tcc_delete(ud->s);
			ud->s = NULL;
			luaL_error(L, "add file failed!");
		}
	}
	return 0;
}

static int puss_tcc_compile_string(lua_State* L) {
	PussTccLua* ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* str = luaL_checkstring(L, 2);
	if( ud->s ) {
		if( tcc_compile_string(ud->s, str) ) {
			tcc_delete(ud->s);
			ud->s = NULL;
			luaL_error(L, "compile failed!");
		}
	}
	return 0;
}

static int puss_tcc_set_output_type(lua_State* L) {
	PussTccLua* ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* tp = luaL_checkstring(L, 2);
	int output_type = TCC_OUTPUT_MEMORY;
	if( strcmp(tp, "exe")==0 ) {
		output_type = TCC_OUTPUT_EXE;
	} else if( strcmp(tp, "dll")==0 ) {
		output_type = TCC_OUTPUT_DLL;
	} else if( strcmp(tp, "obj")==0 ) {
		output_type = TCC_OUTPUT_OBJ;
	} else if( strcmp(tp, "preprocess")==0 ) {
		output_type = TCC_OUTPUT_PREPROCESS;
	}
	if( ud->s ) {
		tcc_set_output_type(ud->s, output_type);
	}
	return 0;
}

static int puss_tcc_add_library_path(lua_State* L) {
	PussTccLua* ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* pathname = luaL_checkstring(L, 2);
	if( ud->s ) {
		tcc_add_library_path(ud->s, pathname);
	}
	return 0;
}

static int puss_tcc_add_library(lua_State* L) {
	PussTccLua* ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* libraryname = luaL_checkstring(L, 2);
	if( ud->s ) {
		tcc_add_library(ud->s, libraryname);
	}
	return 0;
}

static int puss_tcc_add_symbol(lua_State* L) {
	PussTccLua* ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* name = luaL_checkstring(L, 2);
	const void* val = lua_topointer(L, 3);
	if( ud->s ) {
		tcc_add_symbol(ud->s, name, val);
	}
	return 0;
}

static int puss_tcc_add_symbols(lua_State* L) {
	PussTccLua* ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	luaL_checktype(L, 2, LUA_TTABLE);
	lua_pushnil(L);
	while( lua_next(L, 2) ) {
		if( lua_isstring(L, -2) && lua_islightuserdata(L, -1) ) {
			tcc_add_symbol(ud->s, lua_tostring(L, -2), lua_touserdata(L, -1));
		}
		lua_pop(L, 1);
	}
	return 0;
}

static int puss_tcc_output_file(lua_State* L) {
	PussTccLua* ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* filename = luaL_checkstring(L, 2);
	if( ud->s ) {
		tcc_output_file(ud->s, filename);
		tcc_delete(ud->s);
		ud->s = NULL;
	}
	return 0;
}

static int puss_tcc_run(lua_State* L) {
	PussTccLua* ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	int argc = lua_gettop(L) - 1;
	#define ARGS_MAX	64
	const char* argv[ARGS_MAX];
	int i;
	if( argc > ARGS_MAX )
		argc = ARGS_MAX;
	for( i=0; i<argc; ++i )
		argv[i] = luaL_checkstring(L, i+2);
	if( ud->s ) {
		tcc_run(ud->s, argc, (char**)argv);
	}
	return 0;
}

typedef struct TccLJ	TccLJ;

struct TccLJ {
	lua_State*		L;
#ifdef _WIN32
	const char*		e;
#else
	int				e;
#endif
	lua_CFunction	f;
	TCCState*		s;
	jmp_buf			b;
};

static TccLJ* sigLJ = NULL;

#ifdef _WIN32
	static DWORD except_handle_tid = 0;

	static LONG NTAPI except_handle(EXCEPTION_POINTERS *ex_info) {
		DWORD tid = GetCurrentThreadId();
		if( except_handle_tid==tid && sigLJ ) {
			// MessageBoxA(NULL, "XX", except_handle_tid==tid ? "XXX" : "YYY", MB_OK);	// used for debug
			if( sigLJ->e==NULL ) {
				switch (ex_info->ExceptionRecord->ExceptionCode) {
				case EXCEPTION_ACCESS_VIOLATION:	sigLJ->e = "EXCEPTION_ACCESS_VIOLATION";	break;
				case EXCEPTION_INT_DIVIDE_BY_ZERO:	sigLJ->e = "EXCEPTION_INT_DIVIDE_BY_ZERO";	break;
				case EXCEPTION_STACK_OVERFLOW:		sigLJ->e = "EXCEPTION_STACK_OVERFLOW";		break;
				}
				PUSS_TCC_RT_TRACE_BEGIN();
				PUSS_TCC_RT_TRACE("Except");
				{
					char emsg[64];
					sprintf(emsg, "(0x%x):", ex_info->ExceptionRecord->ExceptionCode);
					PUSS_TCC_RT_TRACE(emsg);
				}
				tcc_debug_rt_error(sigLJ->s, sigLJ->f, 16, ex_info->ContextRecord, PUSS_TCC_RT_TRACE);
				PUSS_TCC_RT_TRACE_END();
			}
			longjmp(sigLJ->b, 1);
		}
		return EXCEPTION_CONTINUE_SEARCH;
	}
#else
	static void __puss_tcc_sig_handle(int sig, siginfo_t *info, void *uc) {
		if( sigLJ ) {
			if( sigLJ->e==0 ) {
				char msg[64];
				sigLJ->e = sig ? sig : -1;
				sprintf(msg, "sig error: %d\n", sig);
				PUSS_TCC_RT_TRACE_BEGIN();
				PUSS_TCC_RT_TRACE(msg);
				tcc_debug_rt_error(sigLJ->s, sigLJ->f, 16, uc, PUSS_TCC_RT_TRACE);
				PUSS_TCC_RT_TRACE_END();
			}
			longjmp(sigLJ->b, 1);
		}
		signal(sig, SIG_DFL);
	}
#endif

static void puss_tcc_reg_signals(void) {
  #ifdef _WIN32
	if( except_handle_tid==0 ) {
		except_handle_tid = GetCurrentThreadId();
		AddVectoredExceptionHandler(TRUE, except_handle);
	}
  #else
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = __puss_tcc_sig_handle;
	act.sa_flags = SA_NODEFER | SA_NOMASK | SA_SIGINFO;
	sigaction(SIGFPE, &act, NULL);
	sigaction(SIGBUS, &act, NULL);
	sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGILL, &act, NULL);
  #endif
}

static int wrap_cfunction(lua_State* L) {
	// see luaD_rawrunprotected
	TccLJ* oldsigLJ = sigLJ;
	TccLJ lj = { L, 0, lua_tocfunction(L,lua_upvalueindex(1)), ((PussTccLua*)lua_touserdata(L,lua_upvalueindex(2)))->s };
	int ret = 0;
	PUSS_TCC_LUA_PCALL_BEGIN(L);
	sigLJ = &lj;
	if( setjmp(lj.b)==0 )
		ret = (*(lj.f))(L);
	sigLJ = oldsigLJ;
	if( lj.e==0 )
		return ret;
	PUSS_TCC_LUA_PCALL_END(L);
#ifdef _WIN32
	return luaL_error(L, lj.e);
#else
	return luaL_error(L, "sig error: %d", lj.e);
#endif
}

static void puss_lua_call(lua_State *L, int nargs, int nresults) {
	TccLJ* oldsigLJ = sigLJ;
	sigLJ = NULL;
	lua_call(L, nargs, nresults);
	sigLJ = oldsigLJ;
}

static int puss_lua_pcall(lua_State *L, int nargs, int nresults, int errfunc) {
	TccLJ* oldsigLJ = sigLJ;
	int ret;
	sigLJ = NULL;
	ret = lua_pcall(L, nargs, nresults, errfunc);
	sigLJ = oldsigLJ;
	return ret;
}

#ifdef _WIN32
static void puss_tcc_add_crt(TCCState* s) {
#define _ADDSYM(sym)	tcc_add_symbol(s, #sym, sym);
	_ADDSYM(memcmp);
	_ADDSYM(memmove);
	_ADDSYM(memcpy);
	_ADDSYM(memset);

	_ADDSYM(strcpy);
	_ADDSYM(strcat);
	_ADDSYM(strcmp);
	_ADDSYM(strlen);
	_ADDSYM(strtok);

	_ADDSYM(strtol);
	_ADDSYM(strtoul);
	_ADDSYM(strtof);
	_ADDSYM(strtod);

	_ADDSYM(sprintf);
#undef _ADDSYM
}
#endif

void puss_tcc_add_lua(TCCState* s) {
#define _ADDSYM(sym)	tcc_add_symbol(s, #sym, sym);
	_ADDSYM(lua_absindex)
	_ADDSYM(lua_gettop)
	_ADDSYM(lua_settop)
	_ADDSYM(lua_pushvalue)
	_ADDSYM(lua_rotate)
	_ADDSYM(lua_copy)
	_ADDSYM(lua_checkstack)
	_ADDSYM(lua_xmove)
	_ADDSYM(lua_isnumber)
	_ADDSYM(lua_isstring)
	_ADDSYM(lua_iscfunction)
	_ADDSYM(lua_isinteger)
	_ADDSYM(lua_isuserdata)
	_ADDSYM(lua_type)
	_ADDSYM(lua_typename)
	_ADDSYM(lua_tonumberx)
	_ADDSYM(lua_tointegerx)
	_ADDSYM(lua_toboolean)
	_ADDSYM(lua_tolstring)
	_ADDSYM(lua_rawlen)
	_ADDSYM(lua_tocfunction)
	_ADDSYM(lua_touserdata)
	_ADDSYM(lua_tothread)
	_ADDSYM(lua_topointer)
	_ADDSYM(lua_arith)
	_ADDSYM(lua_rawequal)
	_ADDSYM(lua_compare)
	_ADDSYM(lua_pushnil)
	_ADDSYM(lua_pushnumber)
	_ADDSYM(lua_pushinteger)
	_ADDSYM(lua_pushlstring)
	_ADDSYM(lua_pushstring)
	_ADDSYM(lua_pushfstring)
	_ADDSYM(lua_pushboolean)
	_ADDSYM(lua_pushlightuserdata)
	_ADDSYM(lua_pushthread)
	_ADDSYM(lua_getglobal)
	_ADDSYM(lua_gettable)
	_ADDSYM(lua_getfield)
	_ADDSYM(lua_geti)
	_ADDSYM(lua_rawget)
	_ADDSYM(lua_rawgeti)
	_ADDSYM(lua_rawgetp)
	_ADDSYM(lua_createtable)
	_ADDSYM(lua_newuserdata)
	_ADDSYM(lua_getmetatable)
	_ADDSYM(lua_getuservalue)
	_ADDSYM(lua_setglobal)
	_ADDSYM(lua_settable)
	_ADDSYM(lua_setfield)
	_ADDSYM(lua_seti)
	_ADDSYM(lua_rawset)
	_ADDSYM(lua_rawseti)
	_ADDSYM(lua_rawsetp)
	_ADDSYM(lua_setmetatable)
	_ADDSYM(lua_setuservalue)
	tcc_add_symbol(s, "lua_call", puss_lua_call);
	tcc_add_symbol(s, "lua_pcall", puss_lua_pcall);
	_ADDSYM(puss_lua_pcall)
	_ADDSYM(lua_status)
	_ADDSYM(lua_isyieldable)
	_ADDSYM(lua_error)
	_ADDSYM(lua_next)
	_ADDSYM(lua_concat)
	_ADDSYM(lua_len)
	_ADDSYM(lua_stringtonumber)
	_ADDSYM(luaL_getmetafield)
	_ADDSYM(luaL_tolstring)
	_ADDSYM(luaL_argerror)
	_ADDSYM(luaL_checklstring)
	_ADDSYM(luaL_optlstring)
	_ADDSYM(luaL_checknumber)
	_ADDSYM(luaL_optnumber)
	_ADDSYM(luaL_checkinteger)
	_ADDSYM(luaL_optinteger)
	_ADDSYM(luaL_checkstack)
	_ADDSYM(luaL_checktype)
	_ADDSYM(luaL_checkany)
	_ADDSYM(luaL_newmetatable)
	_ADDSYM(luaL_setmetatable)
	_ADDSYM(luaL_testudata)
	_ADDSYM(luaL_checkudata)
	_ADDSYM(luaL_where)
	_ADDSYM(luaL_error)
	_ADDSYM(luaL_checkoption)
	_ADDSYM(luaL_ref)
	_ADDSYM(luaL_unref)
	_ADDSYM(luaL_loadbufferx)
	_ADDSYM(luaL_loadstring)
	_ADDSYM(luaL_len)
	_ADDSYM(luaL_gsub)
	_ADDSYM(luaL_getsubtable)
#undef _ADDSYM
}

#define puss_lua_upvalueindex(i)	lua_upvalueindex(2+i)

static void puss_lua_pushcclosure(lua_State* L, lua_CFunction f, int nup) {
	luaL_checkudata(L, 1, PUSS_TCC_NAME);
	lua_pushcfunction(L, f);
	lua_pushvalue(L, 1);
	if( nup > 0 )
		lua_rotate(L, -(2+nup), 2);
	lua_pushcclosure(L, wrap_cfunction, 2+nup);
}

static int puss_tcc_relocate(lua_State* L) {
	PussTccLua* ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* init_function_name = luaL_checkstring(L, 2);
	if( !ud->s )
		return 0;
#ifdef _WIN32
	puss_tcc_add_crt(ud->s);
#endif
	puss_tcc_add_lua(ud->s);
	tcc_add_symbol(ud->s, "lua_pushcclosure", puss_lua_pushcclosure);
	if( tcc_relocate(ud->s, TCC_RELOCATE_AUTO) != 0 ) {
		tcc_delete(ud->s);
		ud->s = NULL;
		luaL_error(L, "relocate failed!");
	}
	if( init_function_name ) {
		lua_CFunction init = (lua_CFunction)tcc_get_symbol(ud->s, init_function_name);
		if( init ) {
			return init(L);
		}
		luaL_error(L, "init function(%s) not found!", init_function_name);
	}
	return 0;
}

static int puss_tcc_get_symbol(lua_State* L) {
	PussTccLua* ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* name = luaL_checkstring(L, 2);
	if( ud->s ) {
		void* sym = tcc_get_symbol(ud->s, name);
		if( sym ) {
			lua_pushlightuserdata(L, sym);
			return 1;
		}
	}
	return 0;
}

static int puss_tcc_call(lua_State* L) {
	PussTccLua* ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* name = luaL_checkstring(L, 2);
	lua_CFunction f;
	if( !ud->s )
		luaL_error(L, "tcc module not exist!");
	if( (f = (lua_CFunction)tcc_get_symbol(ud->s, name))==NULL )
		luaL_error(L, "tcc function not exist!");

	puss_lua_pushcclosure(L, f, 0);
	lua_replace(L, 2);
	lua_call(L, lua_gettop(L)-2, LUA_MULTRET);
	return lua_gettop(L) - 1;
}

static int puss_tcc_fetch_stab(lua_State* L) {
	PussTccLua* ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	TCCStabTbl stab;
	if( !ud->s )
		luaL_error(L, "tcc module not exist!");
	tcc_fetch_stab(ud->s, &stab);
	lua_pushlstring(L, (const char*)&stab, sizeof(TCCStabTbl));
	return 1;
}

static luaL_Reg puss_tcc_methods[] =
	{ {"__gc", puss_tcc_destroy}
	, {"set_lib_path", puss_tcc_set_lib_path}
	, {"set_options", puss_tcc_set_options}
	, {"add_include_path", puss_tcc_add_include_path}
	, {"add_sysinclude_path", puss_tcc_add_sysinclude_path}
	, {"define_symbol", puss_tcc_define_symbol}
	, {"undefine_symbol", puss_tcc_undefine_symbol}
	, {"add_file", puss_tcc_add_file}
	, {"compile_string", puss_tcc_compile_string}
	, {"add_symbol", puss_tcc_add_symbol}
	, {"add_symbols", puss_tcc_add_symbols}
	, {"set_output_type", puss_tcc_set_output_type}
	, {"add_library_path", puss_tcc_add_library_path}
	, {"add_library", puss_tcc_add_library}
	, {"output_file", puss_tcc_output_file}
	, {"run", puss_tcc_run}
	, {"relocate", puss_tcc_relocate}
	, {"get_symbol", puss_tcc_get_symbol}
	, {"call", puss_tcc_call}
	, {"fetch_stab", puss_tcc_fetch_stab}
	, {NULL, NULL}
	};

