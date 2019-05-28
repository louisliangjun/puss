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

// libtcc header
typedef struct TCCState TCCState;

typedef struct TCCHook {
    int  (*open)(const char* filename, int flag);
    void (*close)(int fd);
    long (*lseek)(int fd, long offset, int origin);
	int  (*read)(int fd, void *buf, unsigned int len);
} TCCHook;

typedef void (*tcc_setup_hook_t)(const TCCHook *hook);
typedef void (*tcc_debug_rt_error_t)(TCCState *s, void *rt_main, int max_level, void* uc, void (*trace)(const char* msg));
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
	TRAN(setup_hook) \
	TRAN(debug_rt_error) \
	TRAN(new) \
	TRAN(delete) \
	TRAN(set_lib_path) \
	TRAN(set_error_func) \
	TRAN(set_options) \
	TRAN(add_include_path) \
	TRAN(add_sysinclude_path) \
	TRAN(define_symbol) \
	TRAN(undefine_symbol) \
	TRAN(add_file) \
	TRAN(compile_string) \
	TRAN(set_output_type) \
	TRAN(add_library_path) \
	TRAN(add_library) \
	TRAN(add_symbol) \
	TRAN(output_file) \
	TRAN(run) \
	TRAN(relocate) \
	TRAN(get_symbol)


typedef struct _LibTcc {
	#define TCC_DECL(sym)	tcc_ ## sym ## _t	_ ## sym;
		TCC_SYMBOLS(TCC_DECL)
	#undef TCC_DECL
} LibTcc;

#ifdef _PUSS_TCC_USE_STATIC_LIB
extern void tcc_setup_hook(const TCCHook *hook);
extern void tcc_debug_rt_error(TCCState *s, void *rt_main, int max_level, void* uc, void (*trace)(const char* msg));
extern TCCState * tcc_new(void);
extern void tcc_delete(TCCState *s);
extern void tcc_set_lib_path(TCCState *s, const char *path);
extern void tcc_set_error_func(TCCState *s, void *error_opaque, void (*error_func)(void *opaque, const char *msg));
extern void tcc_set_options(TCCState *s, const char *str);
extern int tcc_add_include_path(TCCState *s, const char *pathname);
extern int tcc_add_sysinclude_path(TCCState *s, const char *pathname);
extern void tcc_define_symbol(TCCState *s, const char *sym, const char *value);
extern void tcc_undefine_symbol(TCCState *s, const char *sym);
extern int tcc_add_file(TCCState *s, const char *filename);
extern int tcc_compile_string(TCCState *s, const char *buf);
extern int tcc_set_output_type(TCCState *s, int output_type);
extern int tcc_add_library_path(TCCState *s, const char *pathname);
extern int tcc_add_library(TCCState *s, const char *libraryname);
extern int tcc_add_symbol(TCCState *s, const char *name, const void *val);
extern int tcc_output_file(TCCState *s, const char *filename);
extern int tcc_run(TCCState *s, int argc, char **argv);
extern int tcc_relocate(TCCState *s1, void *ptr);
extern void * tcc_get_symbol(TCCState *s1, const char *name);

static int _libtcc_load(lua_State* L, LibTcc* lib, const char* tcc_dll, const TCCHook* tcc_hook) {
	#define TCC_LOAD(sym)   lib->sym = tcc_ ## sym;
		TCC_SYMBOLS(TCC_LOAD)
	#undef TCC_LOAD
	if( tcc_hook ) {
		tcc_setup_hook(tcc_hook);
	}
	return 0;
}
#else
static lua_CFunction _libtcc_symbol(lua_State* L, const char* tcc_dll, const char* sym) {
	int top = lua_gettop(L);
	lua_CFunction f;
	lua_pushvalue(L, top);	// package.loadlib
	lua_pushstring(L, tcc_dll);
	lua_pushstring(L, sym);
	lua_call(L, 2, 1);
	f = lua_type(L, -1)==LUA_TFUNCTION ? lua_tocfunction(L, -1) : NULL;
	lua_settop(L, top);
	return f;
}

static int _libtcc_load(lua_State* L, LibTcc* lib, const char* tcc_dll, const TCCHook* tcc_hook) {
	luaL_requiref(L, LUA_LOADLIBNAME, luaopen_package, 0);
	assert( lua_type(L, -1)==LUA_TTABLE );
	lua_getfield(L, -1, "loadlib");		// up[2]: package.loadlib
	assert( lua_type(L, -1)==LUA_TFUNCTION );
	lua_remove(L, -2);

	#define TCC_LOAD(sym)	if( (lib->_ ## sym = (tcc_ ## sym ## _t)_libtcc_symbol(L, tcc_dll, "tcc_" #sym))==NULL ) luaL_error(L, "load symbol(tcc_" #sym ") failed!");
		TCC_SYMBOLS(TCC_LOAD)
	#undef TCC_LOAD

	if( tcc_hook ) {
		lib->_setup_hook(tcc_hook);
	}
	lua_pop(L, 1);
	return 0;
}

#define tcc_setup_hook			ud->libtcc->_setup_hook
#define tcc_debug_rt_error      ud->libtcc->_debug_rt_error
#define tcc_new                 ud->libtcc->_new
#define tcc_delete              ud->libtcc->_delete
#define tcc_set_lib_path        ud->libtcc->_set_lib_path
#define tcc_set_error_func      ud->libtcc->_set_error_func
#define tcc_set_options         ud->libtcc->_set_options
#define tcc_add_include_path    ud->libtcc->_add_include_path
#define tcc_add_sysinclude_path ud->libtcc->_add_sysinclude_path
#define tcc_define_symbol       ud->libtcc->_define_symbol
#define tcc_undefine_symbol     ud->libtcc->_undefine_symbol
#define tcc_add_file            ud->libtcc->_add_file
#define tcc_compile_string      ud->libtcc->_compile_string
#define tcc_set_output_type     ud->libtcc->_set_output_type
#define tcc_add_library_path    ud->libtcc->_add_library_path
#define tcc_add_library         ud->libtcc->_add_library
#define tcc_add_symbol          ud->libtcc->_add_symbol
#define tcc_output_file         ud->libtcc->_output_file
#define tcc_run                 ud->libtcc->_run
#define tcc_relocate            ud->libtcc->_relocate
#define tcc_get_symbol          ud->libtcc->_get_symbol

#endif

#define PUSS_TCC_NAME		"[PussTcc]"

typedef struct _PussTccLua {
	LibTcc*		libtcc;
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

#ifdef _WIN32
	static DWORD WINAPI except_filter(EXCEPTION_POINTERS *ex_info, PussTccLua *ud, void* f) {
		const char* err = NULL;
		switch (ex_info->ExceptionRecord->ExceptionCode) {
		case EXCEPTION_ACCESS_VIOLATION:	err = "EXCEPTION_ACCESS_VIOLATION";		break;
		case EXCEPTION_INT_DIVIDE_BY_ZERO:	err = "EXCEPTION_INT_DIVIDE_BY_ZERO";	break;
		case EXCEPTION_STACK_OVERFLOW:		err = "EXCEPTION_STACK_OVERFLOW";		break;
		}
		PUSS_TCC_RT_TRACE_BEGIN();
		PUSS_TCC_RT_TRACE("Except");
		{
			char emsg[64];
			sprintf(emsg, "(0x%x):", ex_info->ExceptionRecord->ExceptionCode);
			PUSS_TCC_RT_TRACE(emsg);
		}
		PUSS_TCC_RT_TRACE(err ? err : "exception");
		PUSS_TCC_RT_TRACE("\n");
		tcc_debug_rt_error(ud->s, f, 16, ex_info->ContextRecord, PUSS_TCC_RT_TRACE);
		PUSS_TCC_RT_TRACE_END();
		return err ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
	}

	static void __reg_signals(void) {
	}

	static int wrap_cfunction(lua_State* L) {
		lua_CFunction f = lua_tocfunction(L, lua_upvalueindex(1));
		PussTccLua* ud = (PussTccLua*)lua_touserdata(L, lua_upvalueindex(2));
		int res;

		__try {
			res = f(L);
		}
		__except( except_filter(GetExceptionInformation(), ud, (void*)f) ) {
			luaL_error(L, "C-exception");
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

	typedef struct WrapUD	WrapUD;

	struct WrapUD {
		lua_State*		L;
		lua_CFunction	f;
		PussTccLua*		t;
		int				res;
		WrapUD*			old;
	};

	static __thread WrapUD* sigUD;

	static void __puss_tcc_sig_handle(int sig, siginfo_t *info, void *uc) {
		fprintf(stderr, "[PussTCC] error(%d)\n", sig);
		char msg[64];
		sprintf(msg, "sig error: %d\n", sig);
		if( sigUD ) {
			PussTccLua* ud = sigUD->t;
			PUSS_TCC_RT_TRACE_BEGIN();
			PUSS_TCC_RT_TRACE(msg);
			tcc_debug_rt_error(sigUD->t->s, sigUD->f, 16, uc, PUSS_TCC_RT_TRACE);
			PUSS_TCC_RT_TRACE_END();
			luaL_error(sigUD->L, "sig error: %d", sig);
		} else {
			char msg[64];
			sprintf(msg, "sig error: %d\n", sig);
			PUSS_TCC_TRACE_ERROR(msg);
			signal(sig, SIG_DFL);
		}
	}

	static void __reg_signals(void) {
		struct sigaction act;
		memset(&act, 0, sizeof(act));
		act.sa_sigaction = __puss_tcc_sig_handle;
		act.sa_flags = SA_NODEFER | SA_NOMASK | SA_SIGINFO;
		sigaction(SIGFPE, &act, NULL);
		sigaction(SIGBUS, &act, NULL);
		sigaction(SIGSEGV, &act, NULL);
		sigaction(SIGILL, &act, NULL);
	}

	static void wrap_pcall(lua_State* L, void* ud) {
		struct WrapUD* r = (struct WrapUD*)ud;
		sigUD = r;
		r->res = r->f(L);
	}

	static int wrap_cfunction(lua_State* L) {
		struct WrapUD ud = { L, lua_tocfunction(L, lua_upvalueindex(1)), (PussTccLua*)lua_touserdata(L, lua_upvalueindex(2)), 0, sigUD };
		int st = luaD_rawrunprotected(L, wrap_pcall, &ud);
		sigUD = ud.old;
		if( st ) luaD_throw(L, st);
		return ud.res;
	}
#endif

#ifdef _WIN32
static void puss_tcc_add_crt(PussTccLua* ud) {
#define _ADDSYM(sym)	tcc_add_symbol(ud->s, #sym, sym);
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

	_ADDSYM(printf);
	_ADDSYM(sprintf);
#undef _ADDSYM
}
#endif

void puss_tcc_add_lua(PussTccLua* ud) {
#define _ADDSYM(sym)	tcc_add_symbol(ud->s, #sym, sym);
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
	_ADDSYM(luaL_getsubtable)
#undef _ADDSYM
}

#define puss_upvalueindex(i)	lua_upvalueindex(2+i)

static void puss_pushcclosure(lua_State* L, lua_CFunction f, int nup) {
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
	puss_tcc_add_crt(ud);
#endif
	puss_tcc_add_lua(ud);
	tcc_add_symbol(ud->s, "puss_pushcclosure", puss_pushcclosure);
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

	puss_pushcclosure(L, f, 0);
	lua_replace(L, 2);
	lua_call(L, lua_gettop(L)-2, LUA_MULTRET);
	return lua_gettop(L) - 1;
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
	, {NULL, NULL}
	};

static int _puss_tcc_new(lua_State* L) {
	LibTcc* lib = (LibTcc*)lua_touserdata(L, lua_upvalueindex(1));
	PussTccLua* ud = (PussTccLua*)lua_newuserdata(L, sizeof(PussTccLua));
	ud->libtcc = lib;
	ud->s = NULL;

	if( luaL_newmetatable(L, PUSS_TCC_NAME) ) {
		luaL_setfuncs(L, puss_tcc_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	ud->s = tcc_new();
	tcc_set_error_func(ud->s, ud, puss_tcc_error);
	return 1;
}

static int puss_push_libtcc_new(lua_State* L) {
	const char* tcc_dll = luaL_checkstring(L, 1);
	const TCCHook* tcc_hook = (const TCCHook*)lua_touserdata(L, 2);
	LibTcc* lib = (LibTcc*)lua_newuserdata(L, sizeof(LibTcc));
	_libtcc_load(L, lib, tcc_dll, tcc_hook);
	lua_pushcclosure(L, _puss_tcc_new, 1);
	__reg_signals();
	return 1;
}
