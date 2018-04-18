// puss_debug.inl - inner used, not include directly

#include "lstate.h"
#include "lobject.h"
#include "lstring.h"
#include "ltable.h"
#include "lfunc.h"
#include "ldebug.h"

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
	PussDebugEventHandle	debug_event_handle;

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

static int script_on_breaked(DebugEnv* env, lua_State* L, lua_Debug* ar, const FileInfo* finfo) {
	if( env->breaked ) return 0;
	if( !(env->debug_event_handle) ) return 0;

	env->breaked_finfo = finfo;
	env->breaked_line = ar->currentline;
	env->breaked_state = L;
	env->breaked_top = lua_gettop(L);
	env->breaked = 1;
	env->continue_signal = 0;
	env->step_signal = 0;
	env->runto_finfo = NULL;
	env->runto_line = 0;

	env->debug_event_handle(L, PUSS_DEBUG_EVENT_BREAKED_BEGIN);

	while( env->debug_event_handle && env->continue_signal==0 ) {
		env->debug_event_handle(L, PUSS_DEBUG_EVENT_BREAKED_UPDATE);
	}

	if( env->debug_event_handle ) {
		env->debug_event_handle(L, PUSS_DEBUG_EVENT_BREAKED_END);
	}

	env->breaked = 0;
	env->breaked_state = NULL;
	lua_settop(L, env->breaked_top);
	return 0;
}

static DebugEnv* debug_env_new(lua_Alloc f, void* ud) {
	DebugEnv* env = (DebugEnv*)malloc(sizeof(DebugEnv));
	if( !env ) return NULL;
	memset(env, 0, sizeof(DebugEnv));
	env->frealloc = f;
	env->ud = ud;
	env->debug_state = luaL_newstate();
	return env;
}

static void debug_env_free(DebugEnv* env) {
	if( env ) {
		if( env->debug_state ) {
			lua_close(env->debug_state);
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

	lua_getinfo(L, "nSlf", ar);

	if( !(hdr->finfo) ) {
		if( (hdr->finfo = file_info_fetch(env, ar->source))==NULL )
			return 0;
	}

	if( env->step_signal ) {
		need_break = 1;
	} else if( env->runto_finfo==hdr->finfo && env->runto_line==ar->currentline ) {
		need_break = 1;
	} else if( file_info_bp_hit_test(env, hdr->finfo, ar->currentline) ) {
		need_break = 1;
	}

	return need_break ? script_on_breaked(env, L, ar, hdr->finfo) : 0;
}

static int script_count_hook(DebugEnv* env, lua_State* L, lua_Debug* ar) {
	env->breaked_finfo = NULL;
	env->breaked_line = 0;
	env->breaked_state = L;
	env->breaked_top = lua_gettop(L);
	env->breaked = 1;

	env->debug_event_handle(L, PUSS_DEBUG_EVENT_HOOK_COUNT);

	env->breaked = 0;
	env->breaked_state = NULL;
	lua_settop(L, env->breaked_top);
	return 0;
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

	if( !(env->debug_event_handle) ) {
		lua_sethook(L, NULL, 0, 0);
		return;
	}

	switch( ar->event ) {
	case LUA_HOOKLINE:
		script_line_hook(env, L, ar);
		break;
	case LUA_HOOKCALL:
		break;
	case LUA_HOOKRET:
		break;
	case LUA_HOOKCOUNT:
		script_count_hook(env, L, ar);
		break;
	default:
		break;
	}
}

#ifndef LUA_MASKERROR
	#define LUA_MASKERROR	0
#endif//LUA_MASKERROR

#define DEBUG_HOOK_MASK (LUA_MASKCALL | LUA_MASKRET | LUA_MASKLINE | LUA_MASKERROR)

static void debug_env_sethook(DebugEnv* env, int enable, int hook_count) {
	lua_Hook hook = NULL;
	int mask = 0;
	int count = 0;
	if( enable ) {
		hook = script_hook;
		mask = DEBUG_HOOK_MASK;
		if( hook_count > 0 ) {
			mask |= LUA_MASKCOUNT;
			count = hook_count;
		}
	}
	lua_sethook(env->main_state, hook, mask, count);
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

static int debug_del_bp(DebugEnv* env, const char* fname, int line) {
	FileInfo* finfo = file_info_fetch(env, fname);
	if( finfo && line>=1 && line<=finfo->line_num ) {
		finfo->bps[line-1] = BP_NOT_SET;
	}
	return 0;
}

static void check_invoke_debug_event_handle(DebugEnv* env, lua_State* L, enum PussDebugEvent ev) {
	if( env && env->debug_event_handle ) {
		lua_Hook hook = lua_gethook(L);
		int mask = lua_gethookmask(L);
		int count = lua_gethookcount(L);
		lua_sethook(L, NULL, 0, 0);

		env->debug_event_handle(L, ev);

		if( lua_gethook(L)==NULL ) {
			lua_sethook(L, hook, mask, count);
		}
	}
}

static int puss_debug_command(lua_State* L, PussDebugCmd cmd, const void* p, int n) {
	DebugEnv* env = debug_env_fetch(L);
	if( !env )	return 0;

	switch( cmd ) {
	case PUSS_DEBUG_CMD_RESET:
		env->debug_event_handle = (PussDebugEventHandle)p;
		debug_env_sethook(env, p ? 1 : 0, n);
		check_invoke_debug_event_handle(env, L, PUSS_DEBUG_EVENT_ATTACHED);
		break;

	case PUSS_DEBUG_CMD_UPDATE:
		check_invoke_debug_event_handle(env, L, PUSS_DEBUG_EVENT_UPDATE);
		break;

	case PUSS_DEBUG_CMD_BP_SET:		// s=filename, n=line, return 0 if set failed
		return debug_set_bp(env, (const char*)p, n);

	case PUSS_DEBUG_CMD_BP_DEL:		// s=filename, n=line
		return debug_del_bp(env, (const char*)p, n);

	case PUSS_DEBUG_CMD_STEP_INTO:
		env->step_signal = 1;
		env->continue_signal = 1;
		break;

	case PUSS_DEBUG_CMD_STEP_OVER:
		env->step_signal = 1;
		env->continue_signal = 1;
		// TODO : 
		break;

	case PUSS_DEBUG_CMD_STEP_OUT:
		env->step_signal = 1;
		env->continue_signal = 1;
		// TODO : 
		break;

	case PUSS_DEBUG_CMD_CONTINUE:
		env->continue_signal = 1;
		env->runto_finfo = NULL;
		env->runto_line = 0;
		break;
		
	case PUSS_DEBUG_CMD_RUNTO:	// s=filename or NULL, n=line
		env->continue_signal = 1;
		env->runto_finfo = NULL;
		env->runto_line = 0;
		if( p && (n > 0) ) {
			env->runto_finfo = file_info_fetch(env, (const char*)p);
			env->runto_line = n;
		}
		break;

	default:
		break;
	}

	return 0;
}

#define PUSS_BUILTIN_DEBUG_EVENT_HANDLE_NAME	"[PussDebugEventHandle]"

static void builtin_debug_event_handle(lua_State* L, enum PussDebugEvent ev) {
	DebugEnv* env = debug_env_fetch(L);
	puss_rawget_ex(L, "puss.trace_pcall");
	if( lua_getfield(L, LUA_REGISTRYINDEX, PUSS_BUILTIN_DEBUG_EVENT_HANDLE_NAME)!=LUA_TFUNCTION ) {
		lua_pop(L, 1);
		env->debug_event_handle = NULL;
		debug_env_sethook(env, 0, 0);
	}

	lua_pushinteger(L, ev);
	if( lua_pcall(L, 2, 0, 0) ) {
		fprintf(stderr, "builtin_debug_event_handle error: %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
	}
}

static int lua_debug_cmd_reset(lua_State* L) {
	PussDebugEventHandle h = NULL;
	int hook_count = 0;
	if( lua_isnoneornil(L, 1) ) {
		lua_pushnil(L);
	} else {
		luaL_checktype(L, 1, LUA_TFUNCTION);
		h = builtin_debug_event_handle;
		hook_count = (int)luaL_optinteger(L, 2, 0);
		lua_pushvalue(L, 1);
	}
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_BUILTIN_DEBUG_EVENT_HANDLE_NAME);
	puss_debug_command(L, PUSS_DEBUG_CMD_RESET, h, hook_count);
	return 0;
}

static int lua_debug_cmd_update(lua_State* L) {
	puss_debug_command(L, PUSS_DEBUG_CMD_UPDATE, NULL, 0);
	return 0;
}

static int lua_debug_cmd_bp_set(lua_State* L) {
	const char* fname = luaL_checkstring(L, 1);
	int line = (int)luaL_checkinteger(L, 2);
	puss_debug_command(L, PUSS_DEBUG_CMD_BP_SET, fname, line);
	return 0;
}

static int lua_debug_cmd_bp_del(lua_State* L) {
	const char* fname = luaL_checkstring(L, 1);
	int line = (int)luaL_checkinteger(L, 2);
	puss_debug_command(L, PUSS_DEBUG_CMD_BP_DEL, fname, line);
	return 0;
}

static int lua_debug_cmd_step_into(lua_State* L) {
	puss_debug_command(L, PUSS_DEBUG_CMD_STEP_INTO, NULL, 0);
	return 0;
}

static int lua_debug_cmd_step_over(lua_State* L) {
	puss_debug_command(L, PUSS_DEBUG_CMD_STEP_OVER, NULL, 0);
	return 0;
}

static int lua_debug_cmd_step_out(lua_State* L) {
	puss_debug_command(L, PUSS_DEBUG_CMD_STEP_OUT, NULL, 0);
	return 0;
}

static int lua_debug_cmd_continue(lua_State* L) {
	puss_debug_command(L, PUSS_DEBUG_CMD_CONTINUE, NULL, 0);
	return 0;
}

static luaL_Reg lua_debug_methods[] =
	{ {"reset", lua_debug_cmd_reset}
	, {"update", lua_debug_cmd_update}
	, {"bp_set", lua_debug_cmd_bp_set}
	, {"bp_del", lua_debug_cmd_bp_del}
	, {"step_into", lua_debug_cmd_step_into}
	, {"step_over", lua_debug_cmd_step_over}
	, {"step_out", lua_debug_cmd_step_out}
	, {"continue", lua_debug_cmd_continue}
	, {NULL, NULL}
	};

