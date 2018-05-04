// puss_debug.inl - inner used, not include directly

#include "lstate.h"
#include "lobject.h"
#include "lstring.h"
#include "ltable.h"
#include "lfunc.h"
#include "ldebug.h"

#define PUSS_DEBUG_NAME	"[PussDebugName]"

#define BP_NOT_SET		0x00
#define BP_ACTIVE		0x01
#define BP_CONDITION	0x02

typedef struct _FileInfo {
	const char*	name;
	char*		script;
	int			line_num;
	char*		bps;
} FileInfo;

typedef struct _MemHead {
	FileInfo*	finfo;
} MemHead;

typedef struct _DebugEnv {
	lua_Alloc				frealloc;
	void*					ud;
	lua_State*				debug_state;
	int						debug_handle;

	void*					main_addr;
	lua_State*				main_state;

	const FileInfo*			breaked_finfo;
	int						breaked_line;
	lua_State*				breaked_state;
	int						breaked_top;
	int						breaked;

	int						continue_signal;
	int						step_signal;

	const FileInfo*			runto_finfo;
	int						runto_line;
} DebugEnv;

static int file_info_free(lua_State* L) {
	FileInfo* finfo = (FileInfo*)lua_touserdata(L, 1);
	if( finfo->script ) {
		free(finfo->script);
		finfo->script = NULL;
	}
	if( finfo->bps ) {
		free(finfo->bps);
		finfo->bps = NULL;
	}
	return 0;
}

static FileInfo* file_info_fetch(DebugEnv* env, const char* fname) {
	lua_State* L = env->debug_state;
	FileInfo* finfo = NULL;
	const char* name;
	if( !fname )
		return NULL;
	if( *fname=='@' || *fname=='=' )
		++fname;
	if( lua_getfield(L, LUA_REGISTRYINDEX, fname)==LUA_TUSERDATA ) {
		finfo = (FileInfo*)lua_touserdata(L, -1);
		return finfo;
	}
	lua_pop(L, 1);
	name = lua_pushstring(L, fname);
	finfo = lua_newuserdata(L, sizeof(FileInfo));
	memset(finfo, 0, sizeof(FileInfo));
	finfo->name = name;
	if( luaL_newmetatable(L, "PussDbgFInfo") ) {
		lua_pushcfunction(L, file_info_free);
		lua_setfield(L, -2, "__gc");
	}
	lua_setmetatable(L, -2);
	lua_rawset(L, LUA_REGISTRYINDEX);
	return finfo;
}

static int file_info_bp_hit_test(DebugEnv* env, FileInfo* finfo, int line){
	unsigned char bp;
	if( !(finfo->bps) )
		return 0;
	if( (line < 1) || (line > finfo->line_num) )
		return 0;
	bp = finfo->bps[line-1];
	if( bp==BP_ACTIVE )
		return 1;
	// TODO : if( bp==BP_CONDITION ) { }
	return 0;
}

static int script_on_breaked(DebugEnv* env, lua_State* L, int currentline, const FileInfo* finfo) {
	int res;
	if( env->breaked ) return 0;
	if( env->debug_handle==LUA_NOREF ) return 0;

	// reset breaked infos
	env->breaked_finfo = finfo;
	env->breaked_line = currentline;
	env->breaked_state = L;
	env->breaked_top = lua_gettop(L);
	env->breaked = 1;

	env->continue_signal = 0;
	env->step_signal = 0;
	env->runto_finfo = NULL;
	env->runto_line = 0;

	lua_rawgeti(env->debug_state, LUA_REGISTRYINDEX, env->debug_handle);
	res = lua_pcall(L, 0, 0, 0);
	if( res ) {
		lua_pop(L, 1);
	}

	// clear breaked infos
	lua_settop(L, env->breaked_top);
	env->breaked = 0;
	env->breaked_top = 0;
	env->breaked_state = NULL;
	env->breaked_line = 0;
	env->breaked_finfo = NULL;
	return 0;
}

static void debug_env_free(DebugEnv* env) {
	if( env ) {
		if( env->debug_state ) {
			lua_close(env->debug_state);
			env->debug_state = NULL;
		}
		free(env);
	}
}

static void *_debug_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {
	DebugEnv* env = (DebugEnv*)ud;
	MemHead* nptr;
	if( nsize ) {
		nsize += sizeof(MemHead);
	}
	if( ptr ) {
		ptr = (void*)( ((MemHead*)ptr) - 1 );
		osize += sizeof(MemHead);
	}
	nptr = (MemHead*)(*(env->frealloc))(ud, ptr, osize, nsize);
	if( nptr ) {
		memset(nptr, 0, sizeof(MemHead));
		if( (env->main_addr==NULL) && (osize==LUA_TTHREAD) ) {
			env->main_addr = nptr;
		}
		++nptr;
	} else if( ptr && (env->main_addr==ptr) ) {
		debug_env_free(env);
	}
	return (void*)nptr;
}

static int script_line_hook(DebugEnv* env, lua_State* L, lua_Debug* ar) {
	int need_break = 0;
	MemHead* hdr;

	// c function, not need check break
	if( !ttisLclosure(ar->i_ci->func) )
		return 0;

	// fetch lua fuction proto MemHead
	hdr = ((MemHead*)(clLvalue(ar->i_ci->func)->p)) - 1;

	lua_getinfo(L, "Sl", ar);

	if( !(hdr->finfo) ) {
		hdr->finfo = file_info_fetch(env, ar->source);
		if( hdr->finfo==NULL )
			return 0;
	}

	if( env->step_signal ) {
		need_break = 1;
	} else if( env->runto_finfo==hdr->finfo && env->runto_line==ar->currentline ) {
		need_break = 1;
	} else if( file_info_bp_hit_test(env, hdr->finfo, ar->currentline) ) {
		need_break = 1;
	}

	return need_break ? script_on_breaked(env, L, ar->currentline, hdr->finfo) : 0;
}

static inline DebugEnv* debug_env_fetch(lua_State* L) {
	DebugEnv* env = NULL;
	if( lua_getallocf(L, (void**)&env) != _debug_alloc ) {
		env = NULL;
	}
	return env;
}

static void script_hook(lua_State* L, lua_Debug* ar) {
	DebugEnv* env = debug_env_fetch(L);
	assert( env );

	if( env->debug_handle==LUA_NOREF ) {
		lua_sethook(L, NULL, 0, 0);
		return;
	}

	switch( ar->event ) {
	case LUA_HOOKLINE:
		script_line_hook(env, L, ar);
		break;
	case LUA_HOOKCOUNT:
		script_on_breaked(env, L, -1, NULL);
		break;
	default:
		break;
	}
}

static int lua_debug_reset(lua_State* L) {
	DebugEnv* env = *(DebugEnv**)luaL_checkudata(L, 1, PUSS_DEBUG_NAME);
	lua_Hook hook = script_hook;
	int hook_count = (int)luaL_optinteger(L, 3, 0);
	int mask = (hook_count > 0) ? (LUA_MASKLINE | LUA_MASKCOUNT) : LUA_MASKLINE;
	luaL_checktype(L, 2, LUA_TFUNCTION);

	if( env->debug_handle!=LUA_NOREF ) {
		luaL_unref(L, LUA_REGISTRYINDEX, env->debug_handle);
		env->debug_handle = LUA_NOREF;
	}
	lua_pushvalue(L, 2);

	env->debug_handle = luaL_ref(L, LUA_REGISTRYINDEX);
	lua_sethook(env->main_state, hook, mask, hook_count);
	return 0;
}

static int lua_debug_continue(lua_State* L) {
	DebugEnv* env = *(DebugEnv**)luaL_checkudata(L, 1, PUSS_DEBUG_NAME);
	env->continue_signal = 1;
	env->runto_finfo = NULL;
	env->runto_line = 0;
	return 0;
}

static int debug_set_bp(DebugEnv* env, const char* fname, int line) {
	FileInfo* finfo = file_info_fetch(env, fname);
	if( !finfo )
		return 0;
	if( line < 1 || line > 100000 )	// not support 10w lines script
		return 0;

	if( line > finfo->line_num ) {
		int num = line + 100;
		finfo->bps = realloc(finfo->bps, sizeof(char) * num);
		if( finfo->bps ) {
			memset(finfo->bps + finfo->line_num, 0, sizeof(char) * (num - finfo->line_num));
			finfo->line_num = num;
		} else {
			finfo->line_num = 0;
			return 0;
		}
	}

	finfo->bps[line-1] = BP_ACTIVE;
	return 1;
}

static int lua_debug_set_bp(lua_State* L) {
	DebugEnv* env = *(DebugEnv**)luaL_checkudata(L, 1, PUSS_DEBUG_NAME);
	const char* script = luaL_checkstring(L, 1);
	int line = (int)luaL_checkinteger(L, 2);
	lua_pushboolean(L, debug_set_bp(env, script, line));
	return 1;
}

static int lua_debug_del_bp(lua_State* L) {
	DebugEnv* env = *(DebugEnv**)luaL_checkudata(L, 1, PUSS_DEBUG_NAME);
	const char* script = luaL_checkstring(L, 1);
	int line = (int)luaL_checkinteger(L, 2);
	FileInfo* finfo = file_info_fetch(env, script);
	if( finfo && line>=1 && line<=finfo->line_num ) {
		finfo->bps[line-1] = BP_NOT_SET;
	}
	return 0;
}

static int lua_debug_step_into(lua_State* L) {
	DebugEnv* env = *(DebugEnv**)luaL_checkudata(L, 1, PUSS_DEBUG_NAME);
	env->step_signal = 1;
	env->continue_signal = 1;
	return 0;
}

static int lua_debug_step_over(lua_State* L) {
	DebugEnv* env = *(DebugEnv**)luaL_checkudata(L, 1, PUSS_DEBUG_NAME);
	env->step_signal = 1;
	env->continue_signal = 1;
	return 0;
}

static int lua_debug_step_out(lua_State* L) {
	DebugEnv* env = *(DebugEnv**)luaL_checkudata(L, 1, PUSS_DEBUG_NAME);
	env->step_signal = 1;
	env->continue_signal = 1;
	return 0;
}

static int lua_debug_run_to(lua_State* L) {
	DebugEnv* env = *(DebugEnv**)luaL_checkudata(L, 1, PUSS_DEBUG_NAME);
	const char* script = luaL_checkstring(L, 1);
	int line = (int)luaL_checkinteger(L, 2);
	FileInfo* finfo = file_info_fetch(env, script);
	env->continue_signal = 1;
	env->runto_finfo = NULL;
	env->runto_line = 0;
	if( finfo && line>=1 && line<=finfo->line_num ) {
		env->runto_finfo = finfo;
		env->runto_line = line;
	}
	return 0;
}

static luaL_Reg puss_debug_methods[] =
	{ {"__index", NULL}
	, {"reset", lua_debug_reset}
	, {"continue", lua_debug_continue}
	, {"set_bp", lua_debug_set_bp}
	, {"del_bp", lua_debug_del_bp}
	, {"step_into", lua_debug_step_into}
	, {"step_over", lua_debug_step_over}
	, {"step_out", lua_debug_step_out}
	, {"run_to", lua_debug_run_to}
	, {NULL, NULL}
	};

static DebugEnv* debug_env_new(lua_Alloc f, void* ud) {
	DebugEnv* env = (DebugEnv*)malloc(sizeof(DebugEnv));
	lua_State* L;
	if( !env ) return NULL;
	memset(env, 0, sizeof(DebugEnv));
	env->frealloc = f;
	env->ud = ud;
	L = luaL_newstate();
	env->debug_state = L;
	env->debug_handle = LUA_NOREF;
	if( luaL_newmetatable(L, PUSS_DEBUG_NAME) ) {
		luaL_setfuncs(L, puss_debug_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_pop(L, 1);
	luaL_openlibs(env->debug_state);
	return env;
}

static int lua_debugger_run(lua_State* hostL) {
	DebugEnv* env = debug_env_fetch(hostL);
	const char* debugger_script = luaL_optstring(hostL, 1, "tools/debugger.lua");
	lua_State* L = env->debug_state;
	if( env->debug_handle!=LUA_NOREF ) {
		luaL_unref(L, LUA_REGISTRYINDEX, env->debug_handle);
		env->debug_handle = LUA_NOREF;
	}
	lua_settop(L, 0);
	lua_gc(L, LUA_GCCOLLECT, 0);

	*((DebugEnv**)lua_newuserdata(L, sizeof(DebugEnv*))) = env;
	luaL_setmetatable(L, PUSS_DEBUG_NAME);
	lua_setglobal(L, "__puss_debug__");

	puss_get_value(L, "puss.trace_dofile");
	lua_pushstring(L, debugger_script);
	if( lua_pcall(L, 1, 0, 0) ) {
		lua_pop(L, 1);
	}
	lua_gc(L, LUA_GCCOLLECT, 0);
	return 0;
}

