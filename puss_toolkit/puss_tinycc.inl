// puss_tinycc.inl

#ifndef PUSS_TCC_TRACE_ERROR
	#define PUSS_TCC_TRACE_ERROR(err)	fprintf(stderr, "PussTcc: %s\n", err)
#endif

// libtcc header
typedef struct TCCState TCCState;

typedef TCCState * (*tcc_new_t)(void);
typedef void (*tcc_delete_t)(TCCState *s);
typedef void (*tcc_set_lib_path_t)(TCCState *s, const char *path);
typedef void (*tcc_set_error_func_t)(TCCState *s, void *error_opaque, void (*error_func)(void *opaque, const char *msg));
typedef void (*tcc_set_options_t)(TCCState *s, const char *str);
typedef int (*tcc_add_include_path_t)(TCCState *s, const char *pathname);
typedef int (*tcc_add_sysinclude_path_t)(TCCState *s, const char *pathname);
typedef void (*tcc_define_symbol_t)(TCCState *s, const char *sym, const char *value);
typedef void (*tcc_undefine_symbol_t)(TCCState *s, const char *sym);
typedef int (*tcc_add_file_t)(TCCState *s, const char *filename);
typedef int (*tcc_compile_string_t)(TCCState *s, const char *buf);
typedef int (*tcc_set_output_type_t)(TCCState *s, int output_type);

#define TCC_OUTPUT_MEMORY   1 /* output will be run in memory (default) */
#define TCC_OUTPUT_EXE      2 /* executable file */
#define TCC_OUTPUT_DLL      3 /* dynamic library */
#define TCC_OUTPUT_OBJ      4 /* object file */
#define TCC_OUTPUT_PREPROCESS 5 /* only preprocess (used internally) */

typedef int (*tcc_add_library_path_t)(TCCState *s, const char *pathname);
typedef int (*tcc_add_library_t)(TCCState *s, const char *libraryname);
typedef int (*tcc_add_symbol_t)(TCCState *s, const char *name, const void *val);
typedef int (*tcc_output_file_t)(TCCState *s, const char *filename);
typedef int (*tcc_run_t)(TCCState *s, int argc, char **argv);
typedef int (*tcc_relocate_t)(TCCState *s1, void *ptr);

#define TCC_RELOCATE_AUTO (void*)1
typedef void * (*tcc_get_symbol_t)(TCCState *s, const char *name);

#define TCC_SYMBOLS(TRAN) \
	TRAN(tcc_new) \
	TRAN(tcc_delete) \
	TRAN(tcc_set_lib_path) \
	TRAN(tcc_set_error_func) \
	TRAN(tcc_set_options) \
	TRAN(tcc_add_include_path) \
	TRAN(tcc_add_sysinclude_path) \
	TRAN(tcc_define_symbol) \
	TRAN(tcc_undefine_symbol) \
	TRAN(tcc_add_file) \
	TRAN(tcc_compile_string) \
	TRAN(tcc_set_output_type) \
	TRAN(tcc_add_library_path) \
	TRAN(tcc_add_library) \
	TRAN(tcc_add_symbol) \
	TRAN(tcc_output_file) \
	TRAN(tcc_run) \
	TRAN(tcc_relocate) \
	TRAN(tcc_get_symbol)


typedef struct _LibTcc {
	#define TCC_DECL(sym)	sym ## _t	sym;
		TCC_SYMBOLS(TCC_DECL)
	#undef TCC_DECL
} LibTcc;

static lua_CFunction _libtcc_symbol(lua_State* L, const char* tcc_dll, const char* sym) {
	int top = lua_gettop(L);
	lua_CFunction f;
	lua_pushvalue(L, top);	// package.loadlib
	lua_pushstring(L, tcc_dll);
	lua_pushstring(L, sym);
	lua_call(L, 2, 1);
	if( (f = (lua_type(L, -1)==LUA_TFUNCTION ? lua_tocfunction(L, -1) : NULL))==NULL )
		luaL_error(L, "load symbol(%s) failed!", sym);
	lua_settop(L, top);
	return f;
}

static int _libtcc_load(lua_State* L, LibTcc* lib, const char* tcc_dll) {
	luaL_requiref(L, LUA_LOADLIBNAME, luaopen_package, 0);
	assert( lua_type(L, -1)==LUA_TTABLE );
	lua_getfield(L, -1, "loadlib");		// up[2]: package.loadlib
	assert( lua_type(L, -1)==LUA_TFUNCTION );
	lua_remove(L, -2);

	#define TCC_LOAD(sym)	lib->sym = (sym ## _t)_libtcc_symbol(L,tcc_dll, #sym);
		TCC_SYMBOLS(TCC_LOAD)
	#undef TCC_LOAD
	lua_pop(L, 1);
	return 0;
}

#define PUSS_TCC_NAME		"[PussTcc]"

typedef struct _PussTccLua {
	LibTcc*		libtcc;
	TCCState*	s;
	int			relocate;
} PussTccLua;

static void puss_tcc_error(void *opaque, const char *msg) {
	PUSS_TCC_TRACE_ERROR(msg);
}

static int puss_tcc_destroy(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	if( ud->s ) {
		ud->libtcc->tcc_delete(ud->s);
		ud->s = NULL;
	}
	return 0;
}

static int puss_tcc_set_options(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* options = luaL_checkstring(L, 2);
	if( ud->s ) {
		ud->libtcc->tcc_set_options(ud->s, options);
	}
	return 0;
}

static int puss_tcc_add_include_path(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* pathname = luaL_checkstring(L, 2);
	if( ud->s ) {
		ud->libtcc->tcc_add_include_path(ud->s, pathname);
	}
	return 0;
}

static int puss_tcc_add_sysinclude_path(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* pathname = luaL_checkstring(L, 2);
	if( ud->s ) {
		ud->libtcc->tcc_add_sysinclude_path(ud->s, pathname);
	}
	return 0;
}

static int puss_tcc_define_symbol(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* sym = luaL_checkstring(L, 2);
	const char* value = luaL_optstring(L, 3, NULL);
	if( ud->s ) {
		ud->libtcc->tcc_define_symbol(ud->s, sym, value);
	}
	return 0;
}

static int puss_tcc_undefine_symbol(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* sym = luaL_checkstring(L, 2);
	if( ud->s ) {
		ud->libtcc->tcc_undefine_symbol(ud->s, sym);
	}
	return 0;
}

static int puss_tcc_add_file(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* filename = luaL_checkstring(L, 2);
	if( ud->s ) {
		if( ud->libtcc->tcc_add_file(ud->s, filename) ) {
			ud->libtcc->tcc_delete(ud->s);
			ud->s = NULL;
			luaL_error(L, "add file failed!");
		}
	}
	return 0;
}

static int puss_tcc_compile_string(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* str = luaL_checkstring(L, 2);
	if( ud->s ) {
		if( ud->libtcc->tcc_compile_string(ud->s, str) ) {
			ud->libtcc->tcc_delete(ud->s);
			ud->s = NULL;
			luaL_error(L, "compile failed!");
		}
	}
	return 0;
}

static int puss_tcc_add_symbol(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* name = luaL_checkstring(L, 2);
	const void* val = lua_topointer(L, 3);
	if( ud->s ) {
		ud->libtcc->tcc_add_symbol(ud->s, name, val);
	}
	return 0;
}

#ifdef _WIN32
	static const char* except_filter(unsigned long e) {
		switch( e ) {
		case EXCEPTION_ACCESS_VIOLATION:	return "EXCEPTION_ACCESS_VIOLATION";
		case EXCEPTION_INT_DIVIDE_BY_ZERO:	return "EXCEPTION_INT_DIVIDE_BY_ZERO";
		case EXCEPTION_STACK_OVERFLOW:		return "EXCEPTION_STACK_OVERFLOW";
		default:	break;
		}
		return NULL;
	}

	#define __reg_signals()

	static int wrap_cfunction(lua_State* L) {
		lua_CFunction f = lua_tocfunction(L, lua_upvalueindex(1));
		const char* err = NULL;
		int res;
		__try {
			res = f(L);
		}
		__except( ((err = except_filter(GetExceptionCode()))==NULL) ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER) {
			luaL_error(L, err);
		}
		return res;
	}
#else
	#include <signal.h>

	#if defined(__GNUC__)
	#define l_noret		void __attribute__((noreturn))
	#elif defined(_MSC_VER) && _MSC_VER >= 1200
	#define l_noret		void __declspec(noreturn)
	#else
	#define l_noret		void
	#endif

	typedef void (*Pfunc) (lua_State *L, void *ud);
	extern l_noret luaD_throw (lua_State *L, int errcode);
	extern int luaD_rawrunprotected (lua_State *L, Pfunc f, void *ud);

	static __thread lua_State* sigL;

	static void __puss_tcc_sig_handle(int sig) {
		if( sigL ) {
			luaL_error(sigL, "sig error: %d", sig);
		} else {
			char msg[64];
			sprintf(msg, "sig error: %d\n", sig);
			PUSS_TCC_TRACE_ERROR(msg);
			abort();
		}
	}

	static void __reg_signals(void) {
		struct sigaction act;
		memset(&act, 0, sizeof(act));
		act.sa_handler = __puss_tcc_sig_handle;
		act.sa_flags = SA_NODEFER | SA_NOMASK;
		sigaction(SIGFPE, &act, NULL);
		sigaction(SIGBUS, &act, NULL);
		sigaction(SIGSEGV, &act, NULL);
		sigaction(SIGILL, &act, NULL);
	}

	struct WrapUD {
		lua_CFunction	f;
		int				res;
		lua_State*		old;
	};

	static void wrap_pcall(lua_State* L, void* ud) {
		struct WrapUD* r = (struct WrapUD*)ud;
		sigL = L;
		r->res = r->f(L);
	}

	static int wrap_cfunction(lua_State* L) {
		struct WrapUD ud = { lua_tocfunction(L, lua_upvalueindex(1)), 0, sigL };
		int st = luaD_rawrunprotected(L, wrap_pcall, &ud);
		sigL = ud.old;
		if( st ) luaD_throw(L, st);
		return ud.res;
	}
#endif

static void puss_tcc_add_crt(LibTcc* libtcc, TCCState* s) {
#define _ADDSYM(sym)	libtcc->tcc_add_symbol(s, #sym, sym);
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

void puss_tcc_add_lua(LibTcc* libtcc, TCCState* s) {
#define _ADDSYM(sym)	libtcc->tcc_add_symbol(s, #sym, sym);
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
	_ADDSYM(lua_pushvfstring)
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
	_ADDSYM(lua_callk)
	_ADDSYM(lua_pcallk)
	_ADDSYM(lua_yieldk)
	_ADDSYM(lua_resume)
	_ADDSYM(lua_status)
	_ADDSYM(lua_isyieldable)
	_ADDSYM(lua_error)
	_ADDSYM(lua_next)
	_ADDSYM(lua_concat)
	_ADDSYM(lua_len)
	_ADDSYM(lua_stringtonumber)
	_ADDSYM(lua_getupvalue)
	_ADDSYM(lua_setupvalue)
	_ADDSYM(lua_upvalueid)
	_ADDSYM(lua_upvaluejoin)
	_ADDSYM(luaL_getmetafield)
	_ADDSYM(luaL_callmeta)
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
	_ADDSYM(luaL_setfuncs)
	_ADDSYM(luaL_getsubtable)
	_ADDSYM(luaL_traceback)
#undef _ADDSYM
}

#define puss_upvalueindex(i)	lua_upvalueindex(1 + i)

typedef void (*PussPushCClosure)(lua_State* L, lua_CFunction f, int nup);
typedef int (*PussModuleInit)(lua_State* L, PussPushCClosure puss_pushcclosure);

static void _puss_pushcclosure_tcc(lua_State* L, lua_CFunction f, int nup) {
	luaL_checkudata(L, 1, PUSS_TCC_NAME);
	lua_pushcfunction(L, f);
	if( nup > 0 )	lua_insert(L, -(nup+1));
	lua_pushvalue(L, 1);
	lua_pushcclosure(L, wrap_cfunction, nup+2);
}

static int puss_tcc_relocate(lua_State* L) {
	PussTccLua*	ud = (PussTccLua*)luaL_checkudata(L, 1, PUSS_TCC_NAME);
	const char* init_function_name = luaL_checkstring(L, 2);
	if( !ud->s )
		return 0;
	if( ud->relocate )
		return 0;
	ud->relocate = 1;
#ifdef _WIN32
	puss_tcc_add_crt(ud->libtcc, ud->s);
#endif
	puss_tcc_add_lua(ud->libtcc, ud->s);
	if( ud->libtcc->tcc_relocate(ud->s, TCC_RELOCATE_AUTO) != 0 ) {
		ud->libtcc->tcc_delete(ud->s);
		ud->s = NULL;
		luaL_error(L, "relocate failed!");
	}
	if( init_function_name ) {
		PussModuleInit init = (PussModuleInit)(ud->libtcc->tcc_get_symbol(ud->s, init_function_name));
		if( init ) {
			return init(L, _puss_pushcclosure_tcc);
		}
		luaL_error(L, "init function(%s) not found!", init_function_name);
	}
	return 0;
}

static luaL_Reg puss_tcc_methods[] =
	{ {"__gc", puss_tcc_destroy}
	, {"set_options", puss_tcc_set_options}
	, {"add_include_path", puss_tcc_add_include_path}
	, {"add_sysinclude_path", puss_tcc_add_sysinclude_path}
	, {"define_symbol", puss_tcc_define_symbol}
	, {"undefine_symbol", puss_tcc_undefine_symbol}
	, {"add_file", puss_tcc_add_file}
	, {"compile_string", puss_tcc_compile_string}
	, {"add_symbol", puss_tcc_add_symbol}
	, {"relocate", puss_tcc_relocate}
	, {NULL, NULL}
	};

static int _puss_tcc_new(lua_State* L) {
	LibTcc* lib = (LibTcc*)lua_touserdata(L, lua_upvalueindex(1));
	PussTccLua*	ud = (PussTccLua*)lua_newuserdata(L, sizeof(PussTccLua));
	ud->libtcc = lib;
	ud->s = NULL;
	ud->relocate = 0;

	if( luaL_newmetatable(L, PUSS_TCC_NAME) ) {
		luaL_setfuncs(L, puss_tcc_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	ud->s = ud->libtcc->tcc_new();
	ud->libtcc->tcc_set_error_func(ud->s, ud, puss_tcc_error);
	ud->libtcc->tcc_set_output_type(ud->s, TCC_OUTPUT_MEMORY);
	return 1;
}

static int puss_push_libtcc_new(lua_State* L) {
	const char* tcc_dll = luaL_checkstring(L, 1);
	LibTcc* lib = (LibTcc*)lua_newuserdata(L, sizeof(LibTcc));
	_libtcc_load(L, lib, tcc_dll);
	lua_pushcclosure(L, _puss_tcc_new, 1);
	__reg_signals();
	return 1;
}
