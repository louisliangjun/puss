// puss_debug_tinycc.inl

#define PUSS_TCCDBG_NAME		"[PussTccDbg]"

#ifdef _WIN32

typedef struct TccDbg {
	DWORD	dwProcessId;
	HANDLE	hProcess;
} TccDbg;

/*
static int lua_IsDebuggerPresent(lua_State* L) {
	lua_pushboolean(L, IsDebuggerPresent());
	return 1;
}

static int lua_DebugActiveProcessStop(lua_State* L) {
	DWORD dwProcessId = (DWORD)luaL_checkinteger(L, 1);
	lua_pushboolean(L, DebugActiveProcessStop(dwProcessId));
	return 1;
}

static int lua_DebugSetProcessKillOnExit(lua_State* L) {
	BOOL KillOnExit = lua_toboolean(L, 1);
	lua_pushboolean(L, DebugSetProcessKillOnExit(KillOnExit));
	return 1;
}

static int lua_DebugBreak(lua_State* L) {
	DebugBreak();
	return 0;
}

static int lua_OutputDebugStringA(lua_State* L) {
	const char* str = luaL_checkstring(L, 1);
	OutputDebugStringA(str);
	return 0;
}
*/
static int _tcc_debug_wait(lua_State* L) {
	TccDbg* ud = (TccDbg*)luaL_checkudata(L, 1, PUSS_TCCDBG_NAME);
	DWORD dwMilliseconds = (DWORD)luaL_optinteger(L, 2, INFINITE);
	DWORD dwContinueStatus = DBG_CONTINUE;
	DEBUG_EVENT DebugEv;
	if( !ud->dwProcessId )
		return 0;
	if( !WaitForDebugEvent(&DebugEv, dwMilliseconds) )
		return 0;

	switch (DebugEv.dwDebugEventCode) {
	case EXCEPTION_DEBUG_EVENT:
		// Process the exception code. When handling 
		// exceptions, remember to set the continuation 
		// status parameter (dwContinueStatus). This value 
		// is used by the ContinueDebugEvent function. 
		// 
		switch (DebugEv.u.Exception.ExceptionRecord.ExceptionCode) {
		case EXCEPTION_ACCESS_VIOLATION:
			// First chance: Pass this on to the system. 
			// Last chance: Display an appropriate error. 
			if( DebugEv.u.Exception.dwFirstChance ) {
			} else {
			}
			dwContinueStatus = DBG_EXCEPTION_NOT_HANDLED;
			break;

		case EXCEPTION_BREAKPOINT:
			// First chance: Display the current 
			// instruction and register values. 
			break;

		case EXCEPTION_DATATYPE_MISALIGNMENT:
			// First chance: Pass this on to the system. 
			// Last chance: Display an appropriate error. 
			dwContinueStatus = DBG_EXCEPTION_NOT_HANDLED;
			break;

		case EXCEPTION_SINGLE_STEP:
			// First chance: Update the display of the 
			// current instruction and register values. 
			break;

		case DBG_CONTROL_C:
			// First chance: Pass this on to the system. 
			// Last chance: Display an appropriate error. 
			break;

		default:
			// Handle other exceptions. 
			dwContinueStatus = DBG_EXCEPTION_NOT_HANDLED;
			break;
		}
		break;

	case CREATE_THREAD_DEBUG_EVENT:
		// As needed, examine or change the thread's registers 
		// with the GetThreadContext and SetThreadContext functions; 
		// and suspend and resume thread execution with the 
		// SuspendThread and ResumeThread functions. 

		// dwContinueStatus = OnCreateThreadDebugEvent(DebugEv);
		break;

	case CREATE_PROCESS_DEBUG_EVENT:
		// As needed, examine or change the registers of the
		// process's initial thread with the GetThreadContext and
		// SetThreadContext functions; read from and write to the
		// process's virtual memory with the ReadProcessMemory and
		// WriteProcessMemory functions; and suspend and resume
		// thread execution with the SuspendThread and ResumeThread
		// functions. Be sure to close the handle to the process image
		// file with CloseHandle.

		// dwContinueStatus = OnCreateProcessDebugEvent(DebugEv);
		break;

	case EXIT_THREAD_DEBUG_EVENT:
		// Display the thread's exit code. 

		// dwContinueStatus = OnExitThreadDebugEvent(DebugEv);
		break;

	case EXIT_PROCESS_DEBUG_EVENT:
		// Display the process's exit code. 

		// dwContinueStatus = OnExitProcessDebugEvent(DebugEv);
		break;

	case LOAD_DLL_DEBUG_EVENT:
		// Read the debugging information included in the newly 
		// loaded DLL. Be sure to close the handle to the loaded DLL 
		// with CloseHandle.

		// dwContinueStatus = OnLoadDllDebugEvent(DebugEv);
		break;

	case UNLOAD_DLL_DEBUG_EVENT:
		// Display a message that the DLL has been unloaded. 

		// dwContinueStatus = OnUnloadDllDebugEvent(DebugEv);
		break;

	case OUTPUT_DEBUG_STRING_EVENT:
		// Display the output debugging string. 

		// dwContinueStatus = OnOutputDebugStringEvent(DebugEv);
		break;

	case RIP_EVENT:
		// dwContinueStatus = OnRipEvent(DebugEv);
		break;
	}
	ContinueDebugEvent(DebugEv.dwProcessId, DebugEv.dwThreadId, dwContinueStatus);
	return 0;
}

static int _tcc_debug_gc(lua_State* L) {
	TccDbg* ud = (TccDbg*)luaL_checkudata(L, 1, PUSS_TCCDBG_NAME);
	if( ud->dwProcessId ) {
		DebugActiveProcessStop(ud->dwProcessId);
		ud->dwProcessId = 0;
	}
	if( ud->hProcess ) {
		CloseHandle(ud->hProcess);
		ud->hProcess = NULL;
	}
	return 0;
}

static luaL_Reg puss_tccdbg_methods[] =
	{ {"__gc", _tcc_debug_gc}
	, {"detach", _tcc_debug_gc}
	, {"wait", _tcc_debug_wait}
	, {NULL, NULL}
	};

static int _tcc_debug_attach(lua_State* L) {
	DWORD dwProcessId = (DWORD)luaL_checkinteger(L, 1);
	TccDbg* ud = (TccDbg*)lua_newuserdata(L, sizeof(TccDbg));
	memset(ud, 0, sizeof(TccDbg));
	if( luaL_newmetatable(L, PUSS_TCCDBG_NAME) ) {
		luaL_setfuncs(L, puss_tccdbg_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);

	if( !(ud->hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId)) )
		return luaL_error(L, "OpenProcess error: %d", (int)GetLastError());

	if( !DebugActiveProcess(dwProcessId) )
		return luaL_error(L, "DebugActiveProcess error: %d", (int)GetLastError());
	ud->dwProcessId = dwProcessId;

	return 1;
}

#else

#endif

