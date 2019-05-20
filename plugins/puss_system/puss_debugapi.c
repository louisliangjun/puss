// puss_socket.c

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#else
#endif

#include "puss_plugin.h"

#ifdef _WIN32
static int lua_IsDebuggerPresent(lua_State* L) {
	lua_pushboolean(L, IsDebuggerPresent());
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

static int lua_WaitForDebugEvent(lua_State* L) {
	DWORD dwMilliseconds = (DWORD)luaL_optinteger(L, 1, INFINITE);
	DWORD dwContinueStatus = DBG_CONTINUE;
	DEBUG_EVENT DebugEv;
	luaL_checktype(L, 2, LUA_TFUNCTION);
	luaL_checktype(L, 3, LUA_TFUNCTION);
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
	lua_pushvalue(L, 3);
	lua_replace(L, 1);
	lua_pushinteger(L, DebugEv.dwDebugEventCode);
	lua_replace(L, 3);
	lua_pushboolean(L, (lua_pcall(L, lua_gettop(L)-2, LUA_MULTRET, 1)==LUA_OK));
	ContinueDebugEvent(DebugEv.dwProcessId, DebugEv.dwThreadId, dwContinueStatus);
	return 0;
}

static int lua_DebugActiveProcess(lua_State* L) {
	DWORD dwProcessId = (DWORD)luaL_checkinteger(L, 1);
	BOOL bActive = DebugActiveProcess(dwProcessId);
	lua_pushboolean(L, bActive);
	lua_pushinteger(L, GetLastError());
	return 2;
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

static const luaL_Reg debug_lib_methods[] =
	{ {"IsDebuggerPresent", lua_IsDebuggerPresent}
	, {"DebugBreak", lua_DebugBreak}
	, {"OutputDebugString", lua_OutputDebugStringA}
	, {"WaitForDebugEvent", lua_WaitForDebugEvent}
	, {"DebugActiveProcess", lua_DebugActiveProcess}
	, {"DebugActiveProcessStop", lua_DebugActiveProcessStop}
	, {"DebugSetProcessKillOnExit", lua_DebugSetProcessKillOnExit}
	, {NULL, NULL}
	};

void puss_debugapi_reg(lua_State* L) {
	luaL_setfuncs(L, debug_lib_methods, 0);
}

#else

void puss_debugapi_reg(lua_State* L) {
}

#endif
