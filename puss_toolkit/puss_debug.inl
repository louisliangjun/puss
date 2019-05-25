// puss_debug.inl

#include "puss_toolkit.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <time.h>
#endif

#include <ctype.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define luac_c
#define LUA_CORE

#include "lstate.h"
#include "lobject.h"
#include "lstring.h"
#include "ltable.h"
#include "lfunc.h"
#include "ldebug.h"
#include "lopcodes.h"

#define PUSS_DEBUG_NAME	"[PussDebug]"

#define BP_NULL			0x00
#define BP_ACTIVE		0x01
#define BP_COND			0x02

#define SIGSTEP_NULL	0
#define SIGSTEP_INTO	1
#define SIGSTEP_OUT		2
#define SIGSTEP_OVER	3

typedef struct _FileInfo {
	const char*		name;
	char*			script;
	int				line_num;
	char*			bps;
} FileInfo;

typedef struct _MemHead {
	FileInfo*		finfo;
} MemHead;

typedef struct _DebugEnv {
	lua_Alloc		frealloc;
	void*			frealloc_ud;

	int				key_of_bps;		// { ["fname:line"] : state }
	int				key_of_conds;	// { crc32(finfo,line) : cond_expr_function }
	int				key_of_cond_expr_load;	// function(expr) ==> cond_expr_function

	lua_State*		debug_state;
	int				debug_handle;

	void*			main_addr;
	lua_State*		main_state;

	lua_State*		breaked_state;
	const FileInfo*	breaked_finfo;
	int				breaked_line;
	int				breaked_top;
	int				breaked_frame;

	int				step_signal;
	int				step_depth;
	lua_State*		step_state;
	const FileInfo*	runto_finfo;
	int				runto_line;
	int				capture_error;
} DebugEnv;

#ifndef PUSS_DEBUG_HASH
	static const uint32_t crc_table[] = /* CRC polynomial 0xedb88320 */
		{ 0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3
		, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91
		, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7
		, 0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5
		, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b
		, 0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59
		, 0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f
		, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d

		, 0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433
		, 0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01
		, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457
		, 0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65
		, 0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb
		, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9
		, 0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f
		, 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad

		, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683
		, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1
		, 0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7
		, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5
		, 0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b
		, 0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79
		, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f
		, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d

		, 0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713
		, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21
		, 0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777
		, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45
		, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db
		, 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9
		, 0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf
		, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
		};

	#define CRC32C(c,d)	(c=(c>>8) ^ crc_table[(c^(d))&0xFF])

	static uint32_t _puss_debug_crc32(uint32_t c, const void* src, uint32_t len) {
		const uint8_t* ps = (const uint8_t*)src;
		const uint8_t* pe = ps + len;
		uint32_t d;
		for( ; ps<pe; ++ps ) {
			d = *ps;
			CRC32C(c,d);
		}
		return ~c;
	}

	#define PUSS_DEBUG_HASH	_puss_debug_crc32
#endif

static lua_Integer cond_bp_hash(const FileInfo* finfo, int line) {
	return (lua_Integer)PUSS_DEBUG_HASH(PUSS_DEBUG_HASH(~0, &finfo, sizeof(finfo)), &line, sizeof(line));
}

static void debug_handle_unref(DebugEnv* env) {
	lua_State* L = env->debug_state;
	if( env->debug_handle!=LUA_NOREF ) {
		luaL_unref(L, LUA_REGISTRYINDEX, env->debug_handle);
		env->debug_handle = LUA_NOREF;
	}
}

static void debug_handle_invoke(DebugEnv* env, int step) {
	lua_State* L = env->debug_state;
	lua_settop(L, 0);
	lua_rawgeti(L, LUA_REGISTRYINDEX, env->debug_handle);
	lua_pushinteger(L, step);
	if( lua_pcall(L, 1, 0, 0) ) {
		lua_pop(L, 1);
	}
}

static int bps_table_push(lua_State* L, DebugEnv* env) {
	if( lua_rawgetp(L, LUA_REGISTRYINDEX, &(env->key_of_bps))!=LUA_TTABLE ) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_rawsetp(L, LUA_REGISTRYINDEX, &(env->key_of_bps));
	}
	return 1;
}

static int conds_table_push(lua_State* L, DebugEnv* env) {
	if( lua_rawgetp(L, LUA_REGISTRYINDEX, &(env->key_of_conds))!=LUA_TTABLE ) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_rawsetp(L, LUA_REGISTRYINDEX, &(env->key_of_conds));
	}
	return 1;
}

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
	if( lua_getfield(L, LUA_REGISTRYINDEX, fname)==LUA_TUSERDATA ) {
		finfo = (FileInfo*)lua_touserdata(L, -1);
		lua_pop(L, 1);
		return finfo;
	}
	lua_pop(L, 1);
	name = lua_pushstring(L, fname);
	finfo = (FileInfo*)lua_newuserdata(L, sizeof(FileInfo));
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

static int file_info_bp_hit_test(lua_State* L, DebugEnv* env, FileInfo* finfo, int line) {
	unsigned char bp;
	int top;
	int hit;
	if( !(finfo->bps) )
		return 0;
	if( (line < 1) || (line > finfo->line_num) )
		return 0;
	bp = finfo->bps[line-1];
	if( bp==BP_ACTIVE )
		return 1;
	if( bp!=BP_COND )
		return 0;

	top = lua_gettop(L);
	conds_table_push(L, env);
	hit = lua_rawgeti(L, -1, cond_bp_hash(finfo, line))==LUA_TFUNCTION
		&& lua_pcall(L, 0, 1, 0)==LUA_OK
		&& lua_toboolean(L, -1)
		;
	lua_settop(L, top);
	return hit;
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

static void *_debug_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
	DebugEnv* env = (DebugEnv*)ud;
	MemHead* nptr;
	if( nsize ) {
		nsize += sizeof(MemHead);
	}
	if( ptr ) {
		ptr = (void*)( ((MemHead*)ptr) - 1 );
		osize += sizeof(MemHead);
	}
	nptr = (MemHead*)(*(env->frealloc))(env->frealloc_ud, ptr, osize, nsize);
	if( nptr ) {
		memset(nptr, 0, sizeof(MemHead));
		if( (env->main_addr==NULL) && (osize==LUA_TTHREAD) ) {
			env->main_addr = nptr;
		}
		++nptr;
	} else if( ptr && (ptr==env->main_addr) && env->main_state ) {
		debug_env_free(env);
	}
	return (void*)nptr;
}

static const FileInfo* script_line_hook(DebugEnv* env, lua_State* L, lua_Debug* ar) {
	MemHead* hdr;

	// c function, not need check break
	if( !ttisLclosure(ar->i_ci->func) )
		return NULL;

	// fetch lua fuction proto MemHead
	hdr = ((MemHead*)(clLvalue(ar->i_ci->func)->p)) - 1;

	lua_getinfo(L, "Sl", ar);

	if( !(hdr->finfo) ) {
		hdr->finfo = file_info_fetch(env, ar->source);
		if( hdr->finfo==NULL )
			return NULL;
	}

	switch( env->step_signal ) {
	case SIGSTEP_INTO:
		return hdr->finfo;
	case SIGSTEP_OUT:
		if( env->step_depth < 0 )
			return hdr->finfo;
		break;
	case SIGSTEP_OVER:
		if( env->step_depth <= 0 )
			return hdr->finfo;
		break;
	default:
		break;
	}

	if( env->runto_finfo==hdr->finfo && env->runto_line==ar->currentline )
		return hdr->finfo;

	if( file_info_bp_hit_test(L, env, hdr->finfo, ar->currentline) )
		return hdr->finfo;

	return NULL;
}

static int script_on_breaked(DebugEnv* env, lua_State* L, int currentline, const FileInfo* finfo) {
	int frame;
	if( env->breaked_state ) return 0;
	if( env->debug_handle==LUA_NOREF ) return 0;

	// reset breaked infos
	env->breaked_state = L;
	env->breaked_finfo = finfo;
	env->breaked_line = currentline;
	env->breaked_top = lua_gettop(L);

	frame = env->breaked_frame;
	if( finfo ) {
		env->step_signal = SIGSTEP_NULL;
		env->step_depth = 0;
		env->step_state = NULL;
		env->runto_finfo = NULL;
		env->runto_line = 0;
		debug_handle_invoke(env, -1);
		while( frame==env->breaked_frame ) {
#ifdef _WIN32
			Sleep(1);
#else
			usleep(1000);
#endif
			debug_handle_invoke(env, 0);
		}
		debug_handle_invoke(env, 1);
	} else {
		// count hook, not clear step & runto signs 
		debug_handle_invoke(env, 0);
	}

	// clear breaked infos
	lua_settop(L, env->breaked_top);
	env->breaked_state = NULL;
	env->breaked_finfo = NULL;
	env->breaked_line = 0;
	env->breaked_top = 0;
	return 0;
}

#ifndef PUSS_DEBUG_ENV_FETCH
	#define PUSS_DEBUG_ENV_FETCH	debug_env_fetch
	static inline DebugEnv* debug_env_fetch(lua_State* L) {
		DebugEnv* env = NULL;
		if( lua_getallocf(L, (void**)&env) != _debug_alloc ) {
			env = NULL;
		}
		return env;
	}
#endif

static void script_hook(lua_State* L, lua_Debug* ar) {
	DebugEnv* env = PUSS_DEBUG_ENV_FETCH(L);
	const FileInfo* finfo;
	assert( env );

	if( env->debug_handle==LUA_NOREF ) {
		lua_sethook(L, NULL, 0, 0);
		return;
	}

	switch( ar->event ) {
	case LUA_HOOKCOUNT:
		script_on_breaked(env, L, -1, NULL);
		break;
	case LUA_HOOKLINE:
		if( (finfo = script_line_hook(env, L, ar)) != NULL ) {
			env->breaked_frame++;
			script_on_breaked(env, L, ar->currentline, finfo);
		}
		break;
	case LUA_HOOKCALL:
		if( env->step_state==L )
			env->step_depth++;
		break;
	case LUA_HOOKRET:
		if( env->step_state==L )
			env->step_depth--;
		break;

#ifdef LUA_HOOKERROR
	case LUA_HOOKERROR:
		if( env->capture_error && ar->currentline ) {
			env->breaked_frame++;
			script_on_breaked(env, L, ar->currentline, file_info_fetch(env, "<pesudo_error_hook>"));
		}
		break;
#endif

	default:
		break;
	}
}

#ifdef LUA_MASKERROR
	#define DEBUG_HOOK_MASK	(LUA_MASKLINE | LUA_MASKCALL | LUA_MASKRET | LUA_MASKERROR)
#else
	#define DEBUG_HOOK_MASK	(LUA_MASKLINE | LUA_MASKCALL | LUA_MASKRET)
#endif

static int lua_debug_reset(lua_State* L) {
	DebugEnv* env = *(DebugEnv**)luaL_checkudata(L, 1, PUSS_DEBUG_NAME);
	int hook_usage = lua_toboolean(L, 3);
	int hook_count = (int)luaL_optinteger(L, 4, 0);
	debug_handle_unref(env);
	if( lua_isfunction(L, 2) ) {
		int mask = hook_usage ? DEBUG_HOOK_MASK : 0;
		if( hook_count > 0 )	mask |= LUA_MASKCOUNT;
		lua_pushvalue(L, 2);
		env->debug_handle = luaL_ref(L, LUA_REGISTRYINDEX);
		lua_sethook(env->main_state, script_hook, mask, hook_count);
	} else {
		lua_sethook(env->main_state, NULL, 0, 0);
	}
	return 0;
}

typedef struct _HostInvokeUD {
	const char*	func;
	size_t		size;
	void*		args;
} HostInvokeUD;

static int do_host_invoke(lua_State* L) {
	HostInvokeUD* ud = (HostInvokeUD*)lua_touserdata(L, 1);
	lua_settop(L, 0);
	puss_get_value(L, ud->func);
	puss_simple_unpack(L, ud->args, ud->size);
	lua_call(L, lua_gettop(L)-1, LUA_MULTRET);
	return lua_gettop(L);
}

static int lua_debug_host_pcall(lua_State* L) {
	DebugEnv* env = *(DebugEnv**)luaL_checkudata(L, 1, PUSS_DEBUG_NAME);
	lua_State* hostL = env->breaked_state ? env->breaked_state : env->main_state;
	const char* func = luaL_checkstring(L, 2);
	int top = lua_gettop(hostL);
	size_t len = 0;
	void* buf = puss_simple_pack(&len, L, 3, -1);
	HostInvokeUD ud = { func, len, buf };
	int res;
	if( !lua_checkstack(hostL, 8) )
		return luaL_error(L, "host stack overflow");

	lua_pushcfunction(hostL, do_host_invoke);
	lua_pushlightuserdata(hostL, &ud);
	res = lua_pcall(hostL, 1, LUA_MULTRET, 0);
	buf = puss_simple_pack(&len, hostL, top+1, -1);
	lua_settop(hostL, top);

	lua_settop(L, 0);
	lua_pushboolean(L, res==LUA_OK);
	puss_simple_unpack(L, buf, len);
	return lua_gettop(L);
}

static int lua_debug_continue(lua_State* L) {
	DebugEnv* env = *(DebugEnv**)luaL_checkudata(L, 1, PUSS_DEBUG_NAME);
	env->breaked_frame++;
	return 0;
}

static int lua_debug_get_bps(lua_State* L) {
	DebugEnv* env = *(DebugEnv**)luaL_checkudata(L, 1, PUSS_DEBUG_NAME);
	return bps_table_push(L, env);
}

static int debug_cond_bp_test_get(lua_State* L) {
	const char* k = lua_type(L,2)==LUA_TSTRING ? lua_tostring(L,2) : NULL;
	const char* s;
	int i;
	lua_Debug ar;
	if( !lua_getstack(L, 2, &ar) )
		return 0;
	if( !lua_getinfo(L, "f", &ar) )
		return 0;
	if( k ) {
		for( i=1; i<256; ++i ) {
			if( (s = lua_getlocal(L, &ar, i))==NULL )
				break;
			if( s==k || strcmp(s,k)==0 )
				return 1;
			lua_pop(L, 1);
		}
		for( i=1; i<256; ++i ) {
			if( (s = lua_getupvalue(L, -1, i))==NULL )
				break;
			if( s==k || strcmp(s,k)==0 )
				return 1;
			lua_pop(L, 1);
		}
	}
	if ((s = lua_getupvalue(L, -1, 1)) != NULL) {
		if (strcmp(s, "_ENV") == 0) {
			lua_pushvalue(L, 2);
			lua_gettable(L, -2);
			return 1;
		}
	}
	return 0;
}

static int debug_cond_bp_test_set(lua_State* L) {
	return luaL_error(L, "cond can not set value!");
}

static int cond_expr_load(lua_State* L) {
	DebugEnv* env = (DebugEnv*)lua_touserdata(L, 1);
	if( lua_rawgetp(L, LUA_REGISTRYINDEX, &(env->key_of_cond_expr_load))!=LUA_TFUNCTION ) {
		lua_pop(L, 1);
		const char* loader_script = "local getter, setter = ...\n"
			"local expr_env = setmetatable({}, {__index=getter, __newindex=setter})\n"
			"return function(expr)\n"
			"	local f, e = load('return '..expr, expr, 't', expr_env)\n"
			"	if not f then error(e) end\n"
			"	return f\n"
			"end\n"
			;

		if( luaL_loadbuffer(L, loader_script, strlen(loader_script), "<CondExprLoader>") )
			return lua_error(L);
		lua_pushcfunction(L, debug_cond_bp_test_get);
		lua_pushcfunction(L, debug_cond_bp_test_set);
		lua_call(L, 2, 1);
		lua_pushvalue(L, -1);
		lua_rawsetp(L, LUA_REGISTRYINDEX, &(env->key_of_cond_expr_load));
	}
	lua_replace(L, 1);
	lua_call(L, 1, 1);
	return 1;
}

static FileInfo* bp_info_fetch(DebugEnv* env, const char* script, int line) {
	FileInfo* finfo = file_info_fetch(env, script);
	if( !finfo )
		return NULL;
	if( line < 1 || line > 64*1024 )	// not support 64K+ lines script
		return NULL;
	if( line > finfo->line_num ) {
		int num = line + 100;
		char* bps = (char*)malloc(sizeof(char) * num);
		if( !bps )
			return NULL;
		if( finfo->bps )
			memcpy(bps, finfo->bps, sizeof(char) * finfo->line_num);
		memset(bps + finfo->line_num, 0, sizeof(char) * (num - finfo->line_num));
		finfo->bps = bps;
		finfo->line_num = num;
	}
	return finfo;
}

static char parse_bp_type(lua_State* L, int idx, FileInfo* finfo, int line) {
	int tp = lua_type(L, idx);
	if( tp==LUA_TNIL || tp==LUA_TNONE )
		return (finfo==NULL || finfo->bps[line-1]==BP_NULL) ? BP_ACTIVE : BP_NULL;
	if( tp==LUA_TSTRING )
		return BP_COND;
	return lua_toboolean(L, idx) ? BP_ACTIVE : BP_NULL;
}

static int lua_debug_set_bp(lua_State* L) {
	DebugEnv* env = *(DebugEnv**)luaL_checkudata(L, 1, PUSS_DEBUG_NAME);
	const char* script = luaL_checkstring(L, 2);
	int line = (int)luaL_checkinteger(L, 3);
	FileInfo* finfo = bp_info_fetch(env, script, line);
	char bp = parse_bp_type(L, 4, finfo, line);
	lua_settop(L, 4);
	if( bp==BP_NULL ) {
		lua_pushnil(L);
		lua_replace(L, 4);
	} else if( bp==BP_ACTIVE ) {
		lua_pushboolean(L, 1);
		lua_replace(L, 4);
	}
	if( !finfo ) {
		lua_pushstring(L, "create BP failed!");
		return 2;
	}
	if( bp==BP_COND ) {
		size_t len = 0;
		const char* str = lua_tolstring(L, 4, &len);
		lua_State* hostL = env->breaked_state ? env->breaked_state : env->main_state;
		int top = lua_gettop(hostL);
		lua_pushcfunction(hostL, cond_expr_load);
		lua_pushlightuserdata(hostL, env);
		lua_pushlstring(hostL, str, len);
		if( lua_pcall(hostL, 2, 1, 0) ) {
			str = lua_tolstring(L, -1, &len);
			lua_pushlstring(L, str, len);
			lua_settop(hostL, top);
			return 2;
		}
		// conds[crc32(finfo,line)] = expr function
		// 
		conds_table_push(hostL, env);
		lua_pushvalue(hostL, -2);
		lua_rawseti(hostL, -2, cond_bp_hash(finfo, line));
		lua_settop(hostL, top);
	}

	// bps[fname:line] = bp
	// 
	bps_table_push(L, env);
	lua_replace(L, 1);
	lua_pushfstring(L, "%s:%d", finfo->name, line);
	lua_pushvalue(L, 4);
	lua_settable(L, 1);	// set bp
	finfo->bps[line-1] = bp;
	return 1;	// bp
}

static int lua_debug_step_into(lua_State* L) {
	DebugEnv* env = *(DebugEnv**)luaL_checkudata(L, 1, PUSS_DEBUG_NAME);
	env->step_signal = SIGSTEP_INTO;
	env->step_depth = 0;
	env->step_state = NULL;
	env->breaked_frame++;
	return 0;
}

static int lua_debug_step_over(lua_State* L) {
	DebugEnv* env = *(DebugEnv**)luaL_checkudata(L, 1, PUSS_DEBUG_NAME);
	env->step_signal = env->breaked_state ? SIGSTEP_OVER : SIGSTEP_INTO;
	env->step_depth = 0;
	env->step_state = env->breaked_state;
	env->breaked_frame++;
	return 0;
}

static int lua_debug_step_out(lua_State* L) {
	DebugEnv* env = *(DebugEnv**)luaL_checkudata(L, 1, PUSS_DEBUG_NAME);
	env->step_signal = env->breaked_state ? SIGSTEP_OUT : SIGSTEP_INTO;
	env->step_depth = 0;
	env->step_state = env->breaked_state;
	env->breaked_frame++;
	return 0;
}

static int lua_debug_run_to(lua_State* L) {
	DebugEnv* env = *(DebugEnv**)luaL_checkudata(L, 1, PUSS_DEBUG_NAME);
	const char* script = luaL_checkstring(L, 2);
	int line = (int)luaL_checkinteger(L, 3);
	FileInfo* finfo = file_info_fetch(env, script);
	env->breaked_frame++;
	if( finfo && line>=1 && line<=finfo->line_num ) {
		env->runto_finfo = finfo;
		env->runto_line = line;
	} else {
		env->runto_finfo = NULL;
		env->runto_line = 0;
	}
	return 0;
}

static int lua_debug_jmp_to(lua_State* L){
	DebugEnv* env = *(DebugEnv**)luaL_checkudata(L, 1, PUSS_DEBUG_NAME);
	int toline = (int)luaL_checkinteger(L, 2);
	CallInfo* ci = env->breaked_state ? env->breaked_state->ci : NULL;
	int i = 0;
	const int* map;
	Proto* p;

	if( ci==NULL || (ttype(ci->func) != LUA_TLCL) )
		return 0;

	p = clLvalue(ci->func)->p;
	if( toline < p->linedefined || toline > p->lastlinedefined )
		return 0;

	map = p->lineinfo;
	for (; toline != map[i] && i < p->sizelineinfo; i++);
	if (i == p->sizelineinfo)
		return 0;

	ci->u.l.savedpc = p->code + i;

	lua_pushinteger(L, toline);
	return 1;
}

static int lua_debug_capture_error(lua_State* L) {
	DebugEnv* env = *(DebugEnv**)luaL_checkudata(L, 1, PUSS_DEBUG_NAME);
	env->capture_error = lua_isnoneornil(L, 2) ? (!(env->capture_error)) : lua_toboolean(L, 2);
	lua_pushboolean(L, env->capture_error);
	return 1;
}

#ifndef PUSS_DEBUG_NOT_USE_DISASM
	#ifdef VOID
		#undef VOID
	#endif
	#define VOID(p)		((const void*)(p))

	#ifdef _MSC_VER
		#if (_MSC_VER < 1900)
			#ifndef snprintf
				#define snprintf	_snprintf
			#endif
		#endif
	#endif

	typedef struct _PrintEnv {
		luaL_Buffer			B;
		char				str[1024+8];
	} PrintEnv;

	#define printf_decls	PrintEnv* _PENV,
	#define printf_usage	_PENV,
	#define printf(...) do{ \
		int len = snprintf(_PENV->str, 1024, __VA_ARGS__); \
		if( len > 0 ) { luaL_addlstring(&(_PENV->B), _PENV->str, len); } \
	} while(0)

	#define PrintString(ts)	_PrintString(printf_usage (ts))
	static void _PrintString(printf_decls const TString* ts)
	{
		const char* s = getstr(ts);
		size_t i, n = tsslen(ts);
		printf("%c", '"');
		for (i = 0; i < n; i++)
		{
			int c = (int)(unsigned char)s[i];
			switch (c)
			{
			case '"':  printf("\\\""); break;
			case '\\': printf("\\\\"); break;
			case '\a': printf("\\a"); break;
			case '\b': printf("\\b"); break;
			case '\f': printf("\\f"); break;
			case '\n': printf("\\n"); break;
			case '\r': printf("\\r"); break;
			case '\t': printf("\\t"); break;
			case '\v': printf("\\v"); break;
			default:	if (isprint(c)) printf("%c", c); else printf("\\%03d", c);
			}
		}
		printf("%c", '"');
	}

	#define PrintConstant(f,i)	_PrintConstant(printf_usage (f),(i))
	static void _PrintConstant(printf_decls const Proto* f, int i)
	{
		const TValue* o = &f->k[i];
		switch (ttype(o))
		{
		case LUA_TNIL:
			printf("nil");
			break;
		case LUA_TBOOLEAN:
			printf(bvalue(o) ? "true" : "false");
			break;
		case LUA_TNUMFLT:
		{
			char buff[100];
			sprintf(buff, LUA_NUMBER_FMT, fltvalue(o));
			printf("%s", buff);
			if (buff[strspn(buff, "-0123456789")] == '\0') printf(".0");
			break;
		}
		case LUA_TNUMINT:
			printf(LUA_INTEGER_FMT, ivalue(o));
			break;
		case LUA_TSHRSTR: case LUA_TLNGSTR:
			PrintString(tsvalue(o));
			break;
		default:				/* cannot happen */
			printf("? type=%d", ttype(o));
			break;
		}
	}

	#define UPVALNAME(x) ((f->upvalues[x].name) ? getstr(f->upvalues[x].name) : "-")
	#define MYK(x)		(-1-(x))
	
	#define PrintCode(f)	_PrintCode(printf_usage (f))
	static void _PrintCode(printf_decls const Proto* f)
	{
		const Instruction* code = f->code;
		int pc, n = f->sizecode;
		for (pc = 0; pc < n; pc++)
		{
			Instruction i = code[pc];
			OpCode o = GET_OPCODE(i);
			int a = GETARG_A(i);
			int b = GETARG_B(i);
			int c = GETARG_C(i);
			int ax = GETARG_Ax(i);
			int bx = GETARG_Bx(i);
			int sbx = GETARG_sBx(i);
			int line = getfuncline(f, pc);
			printf("%d\t", pc + 1);
			if (line > 0) printf("[%d]\t", line); else printf("[-]\t");
			printf("%-9s\t", luaP_opnames[o]);
			switch (getOpMode(o))
			{
			case iABC:
				printf("%d", a);
				if (getBMode(o) != OpArgN) printf(" %d", ISK(b) ? (MYK(INDEXK(b))) : b);
				if (getCMode(o) != OpArgN) printf(" %d", ISK(c) ? (MYK(INDEXK(c))) : c);
				break;
			case iABx:
				printf("%d", a);
				if (getBMode(o) == OpArgK) printf(" %d", MYK(bx));
				if (getBMode(o) == OpArgU) printf(" %d", bx);
				break;
			case iAsBx:
				printf("%d %d", a, sbx);
				break;
			case iAx:
				printf("%d", MYK(ax));
				break;
			}
			switch (o)
			{
			case OP_LOADK:
				printf("\t; "); PrintConstant(f, bx);
				break;
			case OP_GETUPVAL:
			case OP_SETUPVAL:
				printf("\t; %s", UPVALNAME(b));
				break;
			case OP_GETTABUP:
				printf("\t; %s", UPVALNAME(b));
				if (ISK(c)) { printf(" "); PrintConstant(f, INDEXK(c)); }
				break;
			case OP_SETTABUP:
				printf("\t; %s", UPVALNAME(a));
				if (ISK(b)) { printf(" "); PrintConstant(f, INDEXK(b)); }
				if (ISK(c)) { printf(" "); PrintConstant(f, INDEXK(c)); }
				break;
			case OP_GETTABLE:
			case OP_SELF:
				if (ISK(c)) { printf("\t; "); PrintConstant(f, INDEXK(c)); }
				break;
			case OP_SETTABLE:
			case OP_ADD:
			case OP_SUB:
			case OP_MUL:
			case OP_POW:
			case OP_DIV:
			case OP_IDIV:
			case OP_BAND:
			case OP_BOR:
			case OP_BXOR:
			case OP_SHL:
			case OP_SHR:
			case OP_EQ:
			case OP_LT:
			case OP_LE:
				if (ISK(b) || ISK(c))
				{
					printf("\t; ");
					if (ISK(b)) PrintConstant(f, INDEXK(b)); else printf("-");
					printf(" ");
					if (ISK(c)) PrintConstant(f, INDEXK(c)); else printf("-");
				}
				break;
			case OP_JMP:
			case OP_FORLOOP:
			case OP_FORPREP:
			case OP_TFORLOOP:
				printf("\t; to %d", sbx + pc + 2);
				break;
			case OP_CLOSURE:
				printf("\t; %p", VOID(f->p[bx]));
				break;
			case OP_SETLIST:
				if (c == 0) printf("\t; %d", (int)code[++pc]); else printf("\t; %d", c);
				break;
			case OP_EXTRAARG:
				printf("\t; "); PrintConstant(f, ax);
				break;
	#ifdef _LUA_WITH_INT_
			case OP_INT:
				printf("\t; ");
				break;
	#endif

	#ifdef _LUA_WITH_CONSTS_OPTIMIZE_
			case OP_NOP:
				printf("\t; ");
				if (GET_OPCODE(code[pc+1])==OP_XGETTABUP) {
					PrintConstant(f, INDEXK(b));
					PrintConstant(f, INDEXK(c));
				}
				break;
			case OP_XGETTABUP:
				printf("\t; from %d", bx);
				break;
			case OP_TRYTABUP:
			case OP_TRYLIBUP:
				printf("\t; %s", UPVALNAME(b));
				if (ISK(c)) { printf(" "); PrintConstant(f, INDEXK(c)); }
				break;
			case OP_XGETSELF:
				if (c && ISK(c)) {
					printf("\t; ");
					PrintConstant(f, INDEXK(c));
				}
				else {
					printf("\t; <no-deubg>");
				}
				break;
			case OP_XSELF:
				printf("\t; from %d", bx);
				break;
			case OP_TRYSELF:
				if (ISK(c)) { printf("\t; "); PrintConstant(f, INDEXK(c)); }
				break;
	#endif

			default:
				break;
			}
			printf("\n");
		}
	}

	#define SS(x)	((x==1)?"":"s")
	#define S(x)	(int)(x),SS(x)

	#define PrintHeader(f)	_PrintHeader(printf_usage (f))
	static void _PrintHeader(printf_decls const Proto* f)
	{
		const char* s = f->source ? getstr(f->source) : "=?";
		if (*s == '@' || *s == '=')
			s++;
		else if (*s == LUA_SIGNATURE[0])
			s = "(bstring)";
		else
			s = "(string)";
		printf("\n%s <%s:%d,%d> (%d instruction%s at %p)\n",
			(f->linedefined == 0) ? "main" : "function", s,
			f->linedefined, f->lastlinedefined,
			S(f->sizecode), VOID(f));
		printf("%d%s param%s, %d slot%s, %d upvalue%s, ",
			(int)(f->numparams), f->is_vararg ? "+" : "", SS(f->numparams),
			S(f->maxstacksize), S(f->sizeupvalues));
		printf("%d local%s, %d constant%s, %d function%s\n",
			S(f->sizelocvars), S(f->sizek), S(f->sizep));
	}

	#define PrintDebug(f)	_PrintDebug(printf_usage (f))
	static void _PrintDebug(printf_decls const Proto* f)
	{
		int i, n;
		n = f->sizek;
		printf("constants (%d) for %p:\n", n, VOID(f));
		for (i = 0; i < n; i++)
		{
			printf("\t%d\t", i + 1);
			PrintConstant(f, i);
			printf("\n");
		}
		n = f->sizelocvars;
		printf("locals (%d) for %p:\n", n, VOID(f));
		for (i = 0; i < n; i++)
		{
			printf("\t%d\t%s\t%d\t%d\n",
				i, getstr(f->locvars[i].varname), f->locvars[i].startpc + 1, f->locvars[i].endpc + 1);
		}
		n = f->sizeupvalues;
		printf("upvalues (%d) for %p:\n", n, VOID(f));
		for (i = 0; i < n; i++)
		{
			printf("\t%d\t%s\t%d\t%d\n",
				i, UPVALNAME(i), f->upvalues[i].instack, f->upvalues[i].idx);
		}
	}

	#define PrintFunction(f,full)	_PrintFunction(printf_usage (f),(full))
	static void _PrintFunction(printf_decls const Proto* f, int full)
	{
		//int i, n = f->sizep;
		PrintHeader(f);
		PrintCode(f);
		if (full) PrintDebug(f);
		//for (i = 0; i < n; i++) PrintFunction(f->p[i], full);
	}

	static int lua_debug_fetch_disasm(lua_State* L) {
		PrintEnv PENV;
		PrintEnv* _PENV = &PENV;
		const LClosure* f = NULL;
		const Instruction* pc = NULL;
		DebugEnv* env = *(DebugEnv**)luaL_checkudata(L, 1, PUSS_DEBUG_NAME);
		luaL_buffinit(L, &(PENV.B));
		PENV.str[0] = '\0';

		if( !(env->breaked_state) )
			return 0;

		if( lua_type(L, 2)==LUA_TSTRING ) {
			int top = lua_gettop(env->main_state);
			if( puss_get_value(env->main_state, lua_tostring(L, 2))==LUA_TFUNCTION ) {
				f = (const LClosure*)(lua_iscfunction(env->main_state, -1) ? NULL : lua_topointer(env->main_state, -1));
			}
			lua_settop(env->main_state, top);
		} else {
			int lv = (int)luaL_optinteger(L, 2, 0);
			const CallInfo *ci = env->breaked_state->ci;
			while (ci && ((--lv) > 0)) {
				ci = ci->previous;
			}
			if (ci && (ttype(ci->func) == LUA_TLCL)) {
				f = clLvalue(ci->func);
				pc = ci->u.l.savedpc;
			}
		}
		if (f) {
			Proto *p = f->p;
			PrintFunction(p, 0);
			luaL_pushresult(&(PENV.B));
			lua_pushinteger(L, pc ? (pc - p->code + 1) : 0);
			return 2;
		}
		return 0;
	}

	static int _fetch_disasm(lua_State* L) {
		PrintEnv PENV;
		PrintEnv* _PENV = &PENV;
		const LClosure* f = (const LClosure*)(lua_iscfunction(L, 1) ? NULL : lua_topointer(L, 1));
		luaL_buffinit(L, &(PENV.B));
		PENV.str[0] = '\0';
		if( f ) {
			Proto *p = f->p;
			PrintFunction(p, 0);
		}
		luaL_pushresult(&(PENV.B));
		return 1;
	}

	static int lua_debug_getpid(lua_State* L) {
	#ifdef _WIN32
		lua_pushinteger(L, GetCurrentProcessId());
	#else
		lua_pushinteger(L, getpid());
	#endif
		return 1;
	}
#else
	static int lua_debug_fetch_disasm(lua_State* L) {
		return 0;
	}

	static int _fetch_disasm(lua_State* L) {
		return 0;
	}

	static int lua_debug_getpid(lua_State* L) {
		// TODO : lua_pushinteger(L, getpid());
		lua_pushinteger(L, 0);
		return 1;
	}
#endif

static luaL_Reg puss_debug_methods[] =
	{ {"__index", NULL}
	, {"__reset", lua_debug_reset}
	, {"__host_pcall", lua_debug_host_pcall}
	, {"continue", lua_debug_continue}
	, {"get_bps", lua_debug_get_bps}
	, {"set_bp", lua_debug_set_bp}
	, {"step_into", lua_debug_step_into}
	, {"step_over", lua_debug_step_over}
	, {"step_out", lua_debug_step_out}
	, {"run_to", lua_debug_run_to}
	, {"jmp_to", lua_debug_jmp_to}
	, {"capture_error", lua_debug_capture_error}
	, {"fetch_disasm", lua_debug_fetch_disasm}
	, {"getpid", lua_debug_getpid}
	, {NULL, NULL}
	};

static DebugEnv* lua_debugger_new(lua_Alloc f, void* ud) {
	DebugEnv* env = (DebugEnv*)malloc(sizeof(DebugEnv));
	lua_State* L;
	if( !env ) return NULL;
	memset(env, 0, sizeof(DebugEnv));
	env->frealloc = f;
	env->frealloc_ud = ud;
	L = luaL_newstate();
	env->debug_state = L;
	env->debug_handle = LUA_NOREF;
	if( luaL_newmetatable(L, PUSS_DEBUG_NAME) ) {
		luaL_setfuncs(L, puss_debug_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_pop(L, 1);
	luaL_openlibs(L);
	lua_pushcfunction(L, puss_toolkit_open);
	if (lua_pcall(L, 0, 0, 0)) {
		lua_writestringerror("[Puss] load builtins failed: %s\n", lua_tostring(L, -1));
		lua_close(L);
		L = NULL;
	}
	lua_pop(L, 1);
	return env;
}

static void lua_debugger_clear(DebugEnv* env) {
	lua_State* L = env->debug_state;
	debug_handle_unref(env);
	lua_settop(L, 0);
	lua_gc(L, LUA_GCCOLLECT, 0);
}

#ifndef PUSS_DEBUG_DEFAULT_SCRIPT
	#define PUSS_DEBUG_DEFAULT_SCRIPT	NULL
#endif

static int lua_debugger_debug(lua_State* hostL) {
	DebugEnv* env = PUSS_DEBUG_ENV_FETCH(hostL);
	if( lua_isfunction(hostL, 1) )		// disasm <function>
		return _fetch_disasm(hostL);
	if( lua_toboolean(hostL, 1) ) {		// enable <true>
		lua_State* L = env->debug_state;
		const char* debugger_script = luaL_optstring(hostL, 2, PUSS_DEBUG_DEFAULT_SCRIPT);
		size_t n = 0;
		void* b = puss_simple_pack(&n, hostL, 3, -1);
		int top;
		lua_debugger_clear(env);

		puss_lua_get(L, PUSS_KEY_PUSS);
		lua_assert( lua_istable(L, -1) );

		*((DebugEnv**)lua_newuserdata(L, sizeof(DebugEnv*))) = env;
		luaL_setmetatable(L, PUSS_DEBUG_NAME);
		lua_setfield(L, -2, "_debug_proxy");

		lua_getfield(L, -1, "trace_dofile");
		top = lua_gettop(L);
		if( debugger_script ) {
			lua_pushstring(L, debugger_script);
		} else {
			lua_pushfstring(L, "%s/core/dbgsvr.lua", __puss_config__.app_path);
		}
		lua_pushvalue(L, -1);
		lua_setfield(L, -4, "_debug_filename");
		lua_pushglobaltable(L);
		puss_simple_unpack(L, b, n);
		if( lua_pcall(L, lua_gettop(L)-top, 0, 0) ) {
			lua_pop(L, 1);
		}
		if( env->debug_handle==LUA_NOREF ) {
			lua_debugger_clear(env);
		} else {
			lua_pop(L, 1);
		}
	} else if( lua_isboolean(hostL, 1) ) {	// disable <false>
		lua_debugger_clear(env);
	} else if( env->debug_handle ) {		// update <>
		debug_handle_invoke(env, 0);
	}

	lua_pushboolean(hostL, env->debug_handle!=LUA_NOREF);
	return 1;
}

static void lua_debugger_attach(DebugEnv* env, lua_State* hostL) {
	// support: puss.debug
	if( !env )
		return;

	if( !hostL ) {
		debug_env_free(env);
		return;
	}

	env->main_state = hostL;
	puss_lua_get(hostL, PUSS_KEY_PUSS);
	lua_pushcfunction(hostL, lua_debugger_debug);
	lua_setfield(hostL, -2, "debug");	// puss.debug
	lua_pop(hostL, 1);
}

static int __puss_config_debug_level__ = 0;

static lua_State* puss_lua_debugger_newstate(void) {
	lua_Alloc alloc = __puss_config__.app_alloc_fun;
	void* alloc_tag = __puss_config__.app_alloc_tag;
	DebugEnv* dbg = NULL;
	lua_State* L;
	if( __puss_config_debug_level__ ) {
		dbg = lua_debugger_new(alloc, alloc_tag);
		if (!dbg) return NULL;
		alloc = _debug_alloc;
		alloc_tag = dbg;
	}
	L = lua_newstate(alloc, alloc_tag);
	if( L ) {
		luaL_openlibs(L);
		lua_pushcfunction(L, puss_toolkit_open);
		if( lua_pcall(L, 0, 0, 0) ) {
			lua_writestringerror("[Puss] load builtins failed: %s\n", lua_tostring(L, -1));
			lua_close(L);
			L = NULL;
		}
	}
	lua_debugger_attach(dbg, L);
	return L;
}

static inline void puss_lua_debugger_update(lua_State* hostL) {
	DebugEnv* env = PUSS_DEBUG_ENV_FETCH(hostL);
	if (env && env->debug_handle != LUA_NOREF) {
		debug_handle_invoke(env, 0);
	}
}
