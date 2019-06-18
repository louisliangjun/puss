// puss_debug_tinycc.inl

#define PUSS_TCCDBG_NAME		"[PussTccDbg]"

#ifdef _WIN32

typedef struct TccDbg {
	DWORD		dwProcessId;
	HANDLE		hProcess;
	DEBUG_EVENT	ev;
} TccDbg;

static void tccdbg_continue(TccDbg* dbg, DWORD dwContinueStatus) {
	ContinueDebugEvent(dbg->ev.dwProcessId, dbg->ev.dwThreadId, dwContinueStatus);
}

static int _tcc_debug_gc(lua_State* L) {
	TccDbg* dbg = (TccDbg*)luaL_checkudata(L, 1, PUSS_TCCDBG_NAME);
	if( dbg->dwProcessId ) {
		DebugActiveProcessStop(dbg->dwProcessId);
	}
	dbg->dwProcessId = 0;
	dbg->hProcess = NULL;
	return 0;
}

static int _tcc_debug_read(lua_State* L) {
	TccDbg* dbg = (TccDbg*)luaL_checkudata(L, 1, PUSS_TCCDBG_NAME);
	LPCVOID* ptr = lua_islightuserdata(L, 2) ? (LPCVOID*)lua_touserdata(L, 2) : (LPCVOID*)luaL_checkinteger(L, 2);
	SIZE_T len = (SIZE_T)luaL_checkinteger(L, 3);
	if( dbg->hProcess ) {
		luaL_Buffer B;
		SIZE_T sz = 0;
		luaL_buffinitsize(L, &B, len);
		if( ReadProcessMemory(dbg->hProcess, ptr, B.b, len, &sz) ) {
			luaL_pushresultsize(&B, sz);
			return 1;
		}
	}
	return 0;
}

static int _tcc_debug_write(lua_State* L) {
	TccDbg* dbg = (TccDbg*)luaL_checkudata(L, 1, PUSS_TCCDBG_NAME);
	LPVOID* ptr = lua_islightuserdata(L, 2) ? (LPVOID*)lua_touserdata(L, 2) : (LPVOID*)luaL_checkinteger(L, 2);
	size_t len = 0;
	const char* src = luaL_checklstring(L, 3, &len);
	if( dbg->hProcess ) {
		SIZE_T sz = 0;
		if( WriteProcessMemory(dbg->hProcess, ptr, src, len, &sz) ) {
			lua_pushinteger(L, (lua_Integer)sz);
			return 1;
		}
	}
	return 0;
}

static void _tccdbg_on_exception(lua_State* L, TccDbg* dbg, const EXCEPTION_DEBUG_INFO* info) {
	const char* emsg = "exception";
	#define exception_case(e)	case e:	emsg = #e;	break;
	switch (info->ExceptionRecord.ExceptionCode) {
	exception_case(EXCEPTION_ACCESS_VIOLATION)
	exception_case(EXCEPTION_ARRAY_BOUNDS_EXCEEDED)
	exception_case(EXCEPTION_DATATYPE_MISALIGNMENT)
	exception_case(EXCEPTION_FLT_DENORMAL_OPERAND)
	exception_case(EXCEPTION_FLT_DIVIDE_BY_ZERO)
	exception_case(EXCEPTION_FLT_INEXACT_RESULT)
	exception_case(EXCEPTION_FLT_INVALID_OPERATION)
	exception_case(EXCEPTION_FLT_OVERFLOW)
	exception_case(EXCEPTION_FLT_STACK_CHECK)
	exception_case(EXCEPTION_FLT_UNDERFLOW)
	exception_case(EXCEPTION_ILLEGAL_INSTRUCTION)
	exception_case(EXCEPTION_IN_PAGE_ERROR)
	exception_case(EXCEPTION_INT_DIVIDE_BY_ZERO)
	exception_case(EXCEPTION_INT_OVERFLOW)
	exception_case(EXCEPTION_INVALID_DISPOSITION)
	exception_case(EXCEPTION_NONCONTINUABLE_EXCEPTION)
	exception_case(EXCEPTION_PRIV_INSTRUCTION)
	exception_case(EXCEPTION_STACK_OVERFLOW)
	exception_case(DBG_CONTROL_C)
	}
	#undef exception_case

	switch (info->ExceptionRecord.ExceptionCode) {
	case EXCEPTION_BREAKPOINT:
		// First chance: Display the current 
		// instruction and register values. 
		tccdbg_continue(dbg, info->dwFirstChance ? DBG_EXCEPTION_NOT_HANDLED : DBG_CONTINUE);
		break;
	case EXCEPTION_SINGLE_STEP:
		// First chance: Update the display of the 
		// current instruction and register values. 
		tccdbg_continue(dbg, info->dwFirstChance ? DBG_EXCEPTION_NOT_HANDLED : DBG_CONTINUE);
		break;
	default:
		lua_pushstring(L, "exception");
		lua_pushboolean(L, info->dwFirstChance);
		lua_pushstring(L, emsg);
		lua_pushinteger(L, info->ExceptionRecord.ExceptionCode);
		lua_pushlightuserdata(L, info->ExceptionRecord.ExceptionAddress);
		lua_call(L, lua_gettop(L)-1, LUA_MULTRET);
		tccdbg_continue(dbg, info->dwFirstChance ? DBG_EXCEPTION_NOT_HANDLED : DBG_CONTINUE);
		break;
	}
}

static void _tccdbg_on_create_thread(lua_State* L, TccDbg* dbg, const CREATE_THREAD_DEBUG_INFO* info) {
	tccdbg_continue(dbg, DBG_CONTINUE);
}

static void _tccdbg_on_create_process(lua_State* L, TccDbg* dbg, const CREATE_PROCESS_DEBUG_INFO* info) {
	dbg->hProcess = info->hProcess;
	tccdbg_continue(dbg, DBG_CONTINUE);
}

static void _tccdbg_on_thread_exit(lua_State* L, TccDbg* dbg, const EXIT_THREAD_DEBUG_INFO* info) {
	tccdbg_continue(dbg, DBG_CONTINUE);
}

static void _tccdbg_on_process_exit(lua_State* L, TccDbg* dbg, const EXIT_PROCESS_DEBUG_INFO* info) {
	tccdbg_continue(dbg, DBG_CONTINUE);
	dbg->hProcess = NULL;
	dbg->dwProcessId = 0;
}

static void _tccdbg_on_load_dll(lua_State* L, TccDbg* dbg, const LOAD_DLL_DEBUG_INFO* info) {
	tccdbg_continue(dbg, DBG_CONTINUE);
}

static void _tccdbg_on_unload_dll(lua_State* L, TccDbg* dbg, const UNLOAD_DLL_DEBUG_INFO* info) {
	tccdbg_continue(dbg, DBG_CONTINUE);
}

static void _tccdbg_on_debug_string(lua_State* L, TccDbg* dbg, const OUTPUT_DEBUG_STRING_INFO* info) {
	luaL_Buffer B;
	SIZE_T bytesRead = 0;
	lua_pushstring(L, "debug_string");
	luaL_buffinitsize(L, &B, info->nDebugStringLength);
	ReadProcessMemory(dbg->hProcess, info->lpDebugStringData, B.b, info->nDebugStringLength, &bytesRead);
	luaL_pushresultsize(&B, bytesRead);
	lua_call(L, lua_gettop(L)-1, LUA_MULTRET);
	tccdbg_continue(dbg, DBG_CONTINUE);
}

static void _tccdbg_on_rip(lua_State* L, TccDbg* dbg, const RIP_INFO* info) {
	tccdbg_continue(dbg, DBG_CONTINUE);
}

static int _tcc_debug_wait(lua_State* L) {
	TccDbg* dbg = (TccDbg*)luaL_checkudata(L, 1, PUSS_TCCDBG_NAME);
	DWORD dwMilliseconds = (DWORD)luaL_optinteger(L, 3, INFINITE);
	luaL_checktype(L, 2, LUA_TFUNCTION);
	lua_settop(L, 2);
	lua_insert(L, 1);

	if( !dbg->dwProcessId )
		return 0;

	if( !WaitForDebugEvent(&(dbg->ev), dwMilliseconds) ) {
		lua_pushboolean(L, 0);
		return 1;
	}

	switch( dbg->ev.dwDebugEventCode ) {
	case EXCEPTION_DEBUG_EVENT:
		_tccdbg_on_exception(L, dbg, &(dbg->ev.u.Exception));
		break;
	case CREATE_THREAD_DEBUG_EVENT:
		_tccdbg_on_create_thread(L, dbg, &(dbg->ev.u.CreateThread));
		break;
	case CREATE_PROCESS_DEBUG_EVENT:
		_tccdbg_on_create_process(L, dbg, &(dbg->ev.u.CreateProcessInfo));
		break;
	case EXIT_THREAD_DEBUG_EVENT:
		_tccdbg_on_thread_exit(L, dbg, &(dbg->ev.u.ExitThread));
		break;
	case EXIT_PROCESS_DEBUG_EVENT:
		_tccdbg_on_process_exit(L, dbg, &(dbg->ev.u.ExitProcess));
		break;
	case LOAD_DLL_DEBUG_EVENT:
		_tccdbg_on_load_dll(L, dbg, &(dbg->ev.u.LoadDll));
		break;
	case UNLOAD_DLL_DEBUG_EVENT:
		_tccdbg_on_unload_dll(L, dbg, &(dbg->ev.u.UnloadDll));
		break;
	case OUTPUT_DEBUG_STRING_EVENT:
		_tccdbg_on_debug_string(L, dbg, &(dbg->ev.u.DebugString));
		break;
	case RIP_EVENT:
		_tccdbg_on_rip(L, dbg, &(dbg->ev.u.RipInfo));
		break;
	}
	lua_pushboolean(L, 1);
	return 1;
}

static int _stab_code_inited = 0;
static char* _stab_code[256];

static void _stab_code_init(void) {
	if( _stab_code_inited )	return;
	_stab_code_inited = 1;
	memset(_stab_code, 0, sizeof(char*) * 256);
#define __define_stab(NAME, CODE, STRING) _stab_code[CODE] = STRING;
	#include "tinycc/stab.def"
#undef __define_stab
}

static int _tcc_debug_parse_stab(lua_State* L) {
	TccDbg* dbg = (TccDbg*)luaL_checkudata(L, 1, PUSS_TCCDBG_NAME);
	size_t stab_str_len = 0;
	const char* stab_str_mem = luaL_checklstring(L, 2, &stab_str_len);
	TCCStabTbl stab;
	unsigned long i;
	SIZE_T sz;
	SIZE_T bytes;
	TCCStabSym* syms;
	char* strs;
	if( stab_str_len!=sizeof(TCCStabTbl) )
		luaL_error(L, "TCCStabTbl size error");
	if( !dbg->hProcess )
		luaL_error(L, "process not attached!");
	memcpy(&stab, stab_str_mem, stab_str_len);
	sz = sizeof(TCCStabSym) * stab.syms_len;
	syms = (TCCStabSym*)malloc(sz + stab.strs_len + 16);
	strs = (char*)(syms + stab.syms_len);
	if( !syms )
		luaL_error(L, "no memory");
	if( !ReadProcessMemory(dbg->hProcess, stab.syms, syms, sz, &bytes) )
		luaL_error(L, "read syms error");
	if( !ReadProcessMemory(dbg->hProcess, stab.strs, strs, stab.strs_len, &bytes) )
		luaL_error(L, "read strs error");
	_stab_code_init();
	lua_createtable(L, stab.syms_len, 0);
	for( i=1; i<stab.syms_len; ++i ) {
		const TCCStabSym* sym = syms + i;
		const char* code = _stab_code[sym->n_type];

		lua_createtable(L, 4, 0);
		if( code )
			lua_pushstring(L, code);
		else
			lua_pushinteger(L, sym->n_type);
		lua_rawseti(L, -2, 1);

		if( sym->n_strx )
			lua_pushstring(L, strs + sym->n_strx);
		else
			lua_pushnil(L);
		lua_rawseti(L, -2, 2);

		lua_pushinteger(L, sym->n_value);
		lua_rawseti(L, -2, 3);

		lua_pushinteger(L, sym->n_desc);
		lua_rawseti(L, -2, 4);

		lua_rawseti(L, -2, i);
	}
	return 1;
}

static int _tcc_debug_set_data(lua_State* L) {
	luaL_checkudata(L, 1, PUSS_TCCDBG_NAME);
	luaL_checkany(L, 2);
	luaL_checkany(L, 3);
	if( lua_getuservalue(L, 1)!=LUA_TTABLE ) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setuservalue(L, 1);
	}
	lua_replace(L, 1);
	lua_pushvalue(L, 2);
	lua_pushvalue(L, 3);
	lua_settable(L, 1);
	lua_pushvalue(L, 3);
	return 1;
}

static int _tcc_debug_get_data(lua_State* L) {
	luaL_checkudata(L, 1, PUSS_TCCDBG_NAME);
	luaL_checkany(L, 2);
	if( lua_getuservalue(L, 1)!=LUA_TTABLE ) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setuservalue(L, 1);
	}
	lua_replace(L, 1);
	lua_pushvalue(L, 2);
	lua_gettable(L, 1);
	return 1;
}

static luaL_Reg puss_tccdbg_methods[] =
	{ {"__gc", _tcc_debug_gc}
	, {"detach", _tcc_debug_gc}
	, {"read", _tcc_debug_read}
	, {"write", _tcc_debug_write}
	, {"wait", _tcc_debug_wait}
	, {"parse_stab", _tcc_debug_parse_stab}
	, {"set_data", _tcc_debug_set_data}
	, {"get_data", _tcc_debug_get_data}
	, {NULL, NULL}
	};

static int _tcc_debug_attach(lua_State* L) {
	DWORD dwProcessId = (DWORD)luaL_checkinteger(L, 1);
	TccDbg* dbg = (TccDbg*)lua_newuserdata(L, sizeof(TccDbg));
	memset(dbg, 0, sizeof(TccDbg));
	if( luaL_newmetatable(L, PUSS_TCCDBG_NAME) ) {
		luaL_setfuncs(L, puss_tccdbg_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);

	if( !DebugActiveProcess(dwProcessId) )
		return luaL_error(L, "DebugActiveProcess error: %d", (int)GetLastError());
	dbg->dwProcessId = dwProcessId;
	return 1;
}

#else

static int _tcc_debug_attach(lua_State* L) {
	return 0;
}

#endif

