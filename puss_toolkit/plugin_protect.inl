// plugin_protect.inl

// #include <setjmp.h>
// #include "lstate.h"

#if (LUA_VERSION_NUM < 504)
#define NCCALLS_TYPE	unsigned short
#else
#define NCCALLS_TYPE	l_uint32
#endif

#ifndef _PUSS_PPCALL_BEGIN
  #define _PUSS_PPCALL_BEGIN(L) \
	NCCALLS_TYPE oldnCcalls = (L)->nCcalls; \
	struct lua_longjmp *previous = (L)->errorJmp;
#endif

#ifndef _PUSS_PPCALL_END
  #define _PUSS_PPCALL_END(L) \
	(L)->errorJmp = previous; \
	(L)->nCcalls = oldnCcalls;
#endif

typedef struct PussPluginProtectLJ	PussPluginProtectLJ;

#ifdef _WIN32
	static _declspec(thread) PussPluginProtectLJ* _ppp_sigLJ;
#else
	static __thread PussPluginProtectLJ* _ppp_sigLJ;
#endif

static lua_CFunction ppp_wrap_fetch_raw_cfunction(lua_State* L) {
	lua_CFunction f = NULL;
	lua_Debug ar;
	if( lua_getstack(L, 0, &ar) == 0 ||
		lua_getinfo(L, "u", &ar) == 0 ||
		ar.nups == 0 ||
		(f = lua_tocfunction(L, lua_upvalueindex(ar.nups)))==NULL )
		luaL_error(L, "fetch_raw_cfunction failed");
	return f;
}

#ifdef _WIN32
	struct PussPluginProtectLJ {
		const char*		e;
		lua_CFunction	f;
	};

	static DWORD WINAPI except_filter(EXCEPTION_POINTERS *ex_info) {
		if( _ppp_sigLJ ) {
			// MessageBoxA(NULL, "XX", "YYY", MB_OK);	// used for debug
			if( _ppp_sigLJ->e==0 ) {
				// char msg[64];
				switch (ex_info->ExceptionRecord->ExceptionCode) {
				case EXCEPTION_ACCESS_VIOLATION:	_ppp_sigLJ->e = "EXCEPTION_ACCESS_VIOLATION";	break;
				case EXCEPTION_INT_DIVIDE_BY_ZERO:	_ppp_sigLJ->e = "EXCEPTION_INT_DIVIDE_BY_ZERO";	break;
				case EXCEPTION_STACK_OVERFLOW:		_ppp_sigLJ->e = "EXCEPTION_STACK_OVERFLOW";		break;
				default:							_ppp_sigLJ->e = "Exception";						break;
				}
				// Trace RT: _ppp_sigLJ->f, ex_info->ContextRecord
				// sprintf(msg, "%s(%x)\n", _ppp_sigLJ->e, ex_info->ExceptionRecord->ExceptionCode);
				// MessageBoxA(NULL, msg, "PluginProtect Error", MB_OK | MB_ICONERROR);	// used for debug
			}
			return EXCEPTION_EXECUTE_HANDLER;
		}
		return EXCEPTION_CONTINUE_SEARCH;
	}

	static int ppp_wrap_cfunction(lua_State* L) {
		// see luaD_rawrunprotected
		PussPluginProtectLJ* oldsigLJ = _ppp_sigLJ;
		PussPluginProtectLJ lj = {0, ppp_wrap_fetch_raw_cfunction(L)};
		int ret = 0;
		_PUSS_PPCALL_BEGIN(L);
		_ppp_sigLJ = &lj;
		__try {
			ret = (*(lj.f))(L);
		}
		__except( except_filter(GetExceptionInformation()) ) {
			// handled
		}
		_ppp_sigLJ = oldsigLJ;
		if( lj.e==0 )
			return ret;
		_PUSS_PPCALL_END(L);
		return luaL_error(L, "PluginError: %s\n", lj.e);
	}
#else
	struct PussPluginProtectLJ {
		int				e;
		lua_CFunction	f;
		jmp_buf			b;
	};

	static void __puss_ppp_sig_handle(int sig, siginfo_t *info, void *uc) {
		if( _ppp_sigLJ ) {
			if( _ppp_sigLJ->e==0 ) {
				// char msg[64];
				_ppp_sigLJ->e = sig ? sig : -1;
				// Trace RT: _ppp_sigLJ->f, uc
				// sprintf(msg, "sig error: %d\n", sig);
				fprintf(stderr, "sig error: %d\n", sig);
			}
			longjmp(_ppp_sigLJ->b, 1);
		}
		signal(sig, SIG_DFL);
	}

	static int ppp_wrap_cfunction(lua_State* L) {
		// see luaD_rawrunprotected
		PussPluginProtectLJ* oldsigLJ = _ppp_sigLJ;
		PussPluginProtectLJ lj = {0, ppp_wrap_fetch_raw_cfunction(L)};
		int ret = 0;
		_PUSS_PPCALL_BEGIN(L);
		_ppp_sigLJ = &lj;
		if( setjmp(lj.b)==0 )
			ret = (*(lj.f))(L);
		_ppp_sigLJ = oldsigLJ;
		if( lj.e==0 )
			return ret;
		_PUSS_PPCALL_END(L);
		return luaL_error(L, "sig error: %d", lj.e);
	}
#endif

static void puss_protect_lua_callk(lua_State *L, int nargs, int nresults, lua_KContext ctx, lua_KFunction k) {
	if( k ) {
		lua_callk(L, nargs, nresults, ctx, k);
	} else {
		PussPluginProtectLJ* oldsigLJ = _ppp_sigLJ;
		_ppp_sigLJ = NULL;
		lua_call(L, nargs, nresults);
		_ppp_sigLJ = oldsigLJ;
	}
}

static int puss_protect_lua_pcallk(lua_State *L, int nargs, int nresults, int errfunc, lua_KContext ctx, lua_KFunction k) {
	int ret;
	if( k ) {
		ret = lua_pcallk(L, nargs, nresults, errfunc, ctx, k);
	} else {
		PussPluginProtectLJ* oldsigLJ = _ppp_sigLJ;
		_ppp_sigLJ = NULL;
		ret = lua_pcall(L, nargs, nresults, errfunc);
		_ppp_sigLJ = oldsigLJ;
	}
	return ret;
}

static void puss_protect_lua_pushcclosure(lua_State* L, lua_CFunction f, int nup) {
	lua_pushcfunction(L, f);
	lua_pushcclosure(L, ppp_wrap_cfunction, nup+1);
}

static void puss_protect_luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup) {
	luaL_checkstack(L, nup+2, "too many upvalues");
	for (; l->name != NULL; l++) {  /* fill the table with given functions */
		int i;
		for (i = 0; i < nup; i++)  /* copy upvalues to the top */
			lua_pushvalue(L, -nup);
		puss_protect_lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
		lua_setfield(L, -(nup + 2), l->name);
	}
	lua_pop(L, nup);  /* remove upvalues */
}

static PussInterface puss_protect_interface;

static void puss_protect_ensure(void) {
#ifndef _WIN32
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = __puss_ppp_sig_handle;
	act.sa_flags = SA_NODEFER | SA_NOMASK | SA_SIGINFO;
	sigaction(SIGFPE, &act, NULL);
	sigaction(SIGBUS, &act, NULL);
	sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGILL, &act, NULL);
#endif
	puss_protect_interface = puss_interface;
	puss_protect_interface.lua_proxy.lua_callk = puss_protect_lua_callk;
	puss_protect_interface.lua_proxy.lua_pcallk = puss_protect_lua_pcallk;
	puss_protect_interface.lua_proxy.lua_pushcclosure = puss_protect_lua_pushcclosure;
	puss_protect_interface.lua_proxy.luaL_setfuncs = puss_protect_luaL_setfuncs;
}

