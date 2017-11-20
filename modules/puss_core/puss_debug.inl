// puss_debug.inl

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
	lua_Alloc	frealloc;
	void*		ud;
	lua_State*	debug_state;
	void*		debug_plugin;
	void*		main_addr;
	lua_State*	main_state;
	lua_State*	current_state;

	int			breaked;
	int			pause;
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
	if (!fname)
		return NULL;
	if (*fname=='@' || *fname=='=')
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
	// lua_State *L = env->current_state;
	unsigned char bp;
	if( !(finfo->bps) )
		return 0;
	if( (line < 1) || (line > finfo->line_num) )
		return 0;
	bp = finfo->bps[line];
	if( bp==BP_ACTIVE )
		return 1;
	// TODO : if( bp==BP_CONDITION ) { }
	return 0;
}

static int script_on_breaked(DebugEnv* env, lua_State* L, lua_Debug* ar) {
	env->pause = 0;
	if( env->debug_plugin && (!(env->breaked)) ) {
		lua_State* t = env->current_state;
		env->current_state = L;
		env->breaked = 1;
		// env->plugin->breaked();
		env->breaked = FALSE;
		env->current_state = t;
	}
	return 0;
}

static int script_line_hook(DebugEnv* env, lua_State* L, lua_Debug* ar) {
	MemHead* hdr;
	LClosure* cl;
	Proto* p;
	CallInfo* ci = ar->i_ci;
	if (!ttisLclosure(ci->func))
		return 0;

	cl = clLvalue(ci->func);
	p = cl->p;
	hdr = ((MemHead*)p) - 1;

	lua_getinfo(L, "nSlf", ar);

	if( !(hdr->finfo) ) {
		if( (hdr->finfo = file_info_fetch(env, ar->source))==NULL )
			return 0;
	}

	// ar->what: the string "Lua" if the function is a Lua function, "C" if it is a C function, "main" if it is the main part of a chunk.
	if( ar->what[0] == 'C' )
		return 0;

	assert(lua_isfunction(L, -1));
	if( env->pause )
		return script_on_breaked(env, L, ar);

	if( file_info_bp_hit_test(env, hdr->finfo, ar->currentline) )
		return script_on_breaked(env, L, ar);

	return 0;
}

static void script_hook(lua_State* L, lua_Debug* ar) {
	DebugEnv* env = NULL;
	lua_getallocf(L, (void**)&env);
	assert( env );
	if( !(env->debug_plugin) ) {
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
	default:
		break;
	}
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

#ifndef LUA_MASKERROR
	#define LUA_MASKERROR	0
#endif//LUA_MASKERROR

#define DEBUG_HOOK_MASK (LUA_MASKCALL | LUA_MASKRET | LUA_MASKLINE | LUA_MASKCOUNT | LUA_MASKERROR)

static void debug_env_sethook(DebugEnv* env, int enable, int count) {
	if( enable ) {
		lua_sethook(env->main_state, script_hook, DEBUG_HOOK_MASK, count);
	} else {
		lua_sethook(env->main_state, NULL, 0, 0);
	}
}

static int lua_debug_enable(lua_State* L) {
	DebugEnv* env = NULL;
	int enable = lua_toboolean(L, 1);
	int count = (int)luaL_optinteger(L, 2, 4096);
	lua_getallocf(L, (void**)&env);
	assert( env );

	debug_env_sethook(env, enable, count);
	return 0;
}

static luaL_Reg lua_debug_methods[] =
	{ {"enable", lua_debug_enable}
	, {NULL, NULL}
	};

static int debug_env_init(lua_State* L) {
	lua_newtable(L);	// new debug
	luaL_setfuncs(L, lua_debug_methods, 0);
	lua_setfield(L, 1, "debug");	// ks.debug
	return 0;
}

