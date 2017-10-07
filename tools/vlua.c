// vlua.c
// 
// compile : gcc -s -O2 -pthread -Wall -o vlua ./vlua.c -llua -lm -ldl
// 
// compile : gcc -s -O2 -pthread -Wall -DLUA_USE_LINUX -I./3rd/lua53 -o vlua ./vlua.c ./3rd/lua53/*.c -lm -ldl
// 
// msys2 compile :
//  pacman -S lua
//  gcc -static -s -O2 -Wall -o vlua ./vlua.c /mingw64/lib/liblua.a -lm
// 
// mingw compile :
//  cd ../3rd/lua-5.3.4 && make mingw
//  gcc -s -O2 -Wall -I../3rd/lua-5.3.4/src -o vlua ./vlua.c ../3rd/lua-5.3.4/src/liblua.a -lm
// 

#ifdef _WIN32
	#undef _WIN32_WINNT
	#define _WIN32_WINNT 0x0500
	#include <windows.h>
	#include <process.h>
	#include <io.h>
	#define inline __inline

	typedef HANDLE		VLuaThreadID;
	typedef unsigned (__stdcall * VLuaThreadFunc)(void* arg);
	#define VLUA_THREAD_DECLARE(func, arg) unsigned __stdcall func(void* arg)

	static inline int vlua_thread_create(VLuaThreadID* ptid, VLuaThreadFunc func, void* arg) {
		*ptid = (VLuaThreadID)_beginthreadex(NULL, 0, func, arg, 0, NULL);
		return (*ptid)!=0;
	}

	static inline void vlua_thread_destroy(VLuaThreadID ptid) {
		CloseHandle(ptid);
	}

#else
	#include <sys/time.h>
	#include <sys/stat.h>
	#include <unistd.h>
	#include <limits.h>
	#include <dirent.h>
	#include <utime.h>
	#include <pthread.h>
	#include <errno.h>

	typedef pthread_t	VLuaThreadID;
	typedef void* (*VLuaThreadFunc)(void* arg);
	#define VLUA_THREAD_DECLARE(func, arg) void* func(void* arg)

	static inline int vlua_thread_create(VLuaThreadID* ptid, VLuaThreadFunc func, void* arg) {
		int ret = pthread_create(ptid, NULL, func, arg);
		return ret==0;
	}

	static inline void vlua_thread_destroy(VLuaThreadID ptid) {
		void* ret = NULL;
		pthread_join(ptid, &ret);
	}

#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <time.h>
#include <assert.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

static lua_State* vlua_state_new(void);

enum BuiltinLuaRIDX
	{ LUA_RIDX_VLUA = LUA_RIDX_LAST + 1
	, LUA_RIDX_PICKLE_CACHE
	, LUA_RIDX_TEMP_REF_TABLE
	, LUA_RIDX_BUILTIN_MAX	// must last
	};

#define vlua_builtin_ridx_rawget(L,ridx)	lua_rawgeti((L), LUA_REGISTRYINDEX, (ridx))
#define vlua_builtin_ridx_rawset(L,ridx)	lua_rawseti((L), LUA_REGISTRYINDEX, (ridx))

static const char*	vlua_self = NULL;
static const char*	vlua_script = NULL;
static int			vlua_argc = 0;
static const char*	vlua_argv[256];

static inline void* vlua_realloc(lua_State* L, void* p, size_t osize, size_t nsize) {
    void *ud;
	lua_Alloc f = lua_getallocf(L, &ud);
    return f(ud, p, osize, nsize);
}

static int debug_traceback(lua_State* L) {
	const char *msg = lua_tostring(L, 1);
	luaL_traceback(L, L, msg?msg:"(unknown error)", 1);
	return 1;
}

static int vlua_pcall_stacktrace(lua_State* L, int narg, int nres) {
	int status;
	int base = lua_gettop(L) - narg;  /* function index */
	lua_pushcfunction(L, debug_traceback);
	lua_insert(L, base);  /* put it under chunk and args */
	status = lua_pcall(L, narg, nres, base);
	lua_remove(L, base);  /* remove traceback function */
	return status;
}

static int vlua_rawget_ex(lua_State* L, const char* name) {
	int top = lua_gettop(L);
	const char* ps = name;
	const char* pe = ps;
	int tp = LUA_TTABLE;

	lua_pushglobaltable(L);
	for( ; *pe; ++pe ) {
		if( *pe=='.' ) {
			if( tp!=LUA_TTABLE )
				goto not_found;
			lua_pushlstring(L, ps, pe-ps);
			tp = lua_rawget(L, -2);
			lua_remove(L, -2);
			ps = pe+1;
		}
	}

	if( ps!=pe && tp==LUA_TTABLE ) {
		lua_pushlstring(L, ps, pe-ps);
		tp = lua_rawget(L, -2);
		lua_remove(L, -2);
		return tp;
	}

not_found:
	lua_settop(L, top);
	lua_pushnil(L);
	tp = LUA_TNIL;
	return tp;
}

// args

static int lua_fetch_args(lua_State* L) {
	int i;
	lua_createtable(L, vlua_argc, 0);
	for( i=0; i<vlua_argc; ++i ) {
		lua_pushstring(L, vlua_argv[i]);
		lua_rawseti(L, -2, i+1);
	}
	return 1;
}

static int lua_match_arg(lua_State* L) {
	int nret = 0;
	int top, i;
	if( vlua_rawget_ex(L, "string.match")!=LUA_TFUNCTION )
		return luaL_error(L, "string.match missing");

	top = lua_gettop(L);
	for( i=0; i<vlua_argc; ++i ) {
		lua_pushvalue(L, top);
		lua_pushstring(L, vlua_argv[i]);
		lua_pushvalue(L, 1);
		lua_call(L, 2, LUA_MULTRET);
		if( !lua_isnoneornil(L, top+1) ) {
			nret = lua_gettop(L) - top;
			break;
		}
		lua_settop(L, top);
	}
	return nret;
}

// pickle

#define TNIL		0
#define TTRUE		1
#define TFALSE		2
#define TINT		3
#define TNUM		4
#define TSTR		5
#define TTABLE		6
#define TNULL		7
#define TTREF		8

#define PICKLE_DEPTH_MAX	32
#define PICKLE_ARG_MAX		32

// pickle mem buffer

typedef struct _PickleMemBuffer {
	unsigned char*	buf;
	size_t			len;
	size_t			free;
} PickleMemBuffer;

static inline unsigned char* pickle_mem_prepare(lua_State* L, PickleMemBuffer* mb, size_t len) {
	if( mb->free < len ) {
		size_t new_size = (mb->len + len) * 2;
		mb->buf = (unsigned char*)vlua_realloc(L, mb->buf, (mb->len + mb->free), new_size);
		mb->free = new_size - mb->len;
	}
	return mb->buf + mb->len;
}

typedef struct _LPacker {
	PickleMemBuffer	mb;		// MUST first

	lua_State*		L;
	int				depth;

	// table pack
	int				tref;
	uint32_t		nref;
} LPacker;

typedef struct _LUnPacker {
	lua_State*		L;
	const char*		start;
	const char*		end;
	int				depth;
	int				tref;
	uint32_t		nref;
	uint32_t		iref;
} LUnPacker;

static LPacker* _pickle_reset(LPacker* pac, lua_State* L) {
	pac->mb.free += pac->mb.len;
	pac->mb.len = 0;
	pac->L = L;
	pac->depth = 1;
	pac->tref = 0;
	pac->nref = 0;
	return pac;
}

static inline void _pickle_addbuf(lua_State* L, LPacker* pac, const void* buf, size_t len) {
	PickleMemBuffer* mb = &(pac->mb);
	unsigned char* pos = pickle_mem_prepare(L, mb, len);
	if( buf )
	    memcpy(pos, buf, len);
    mb->len += len;
    mb->free -= len;
}

static inline void _pickle_add(lua_State* L, LPacker* pac, unsigned char tp, const void* buf, size_t len) {
	PickleMemBuffer* mb = &(pac->mb);
	size_t sz = 1 + len;
	unsigned char* pos = pickle_mem_prepare(L, mb, sz);
	*pos++ = tp;
	if( buf )
	    memcpy(pos, buf, len);
    mb->len += sz;
    mb->free -= sz;
}

static void _pack(LPacker* pac, int idx);
static const char* _unpack(LUnPacker* upac, const char* pkt);

static int _pickle_mem_buffer_gc(lua_State* L) {
	LPacker* pac = (LPacker*)lua_touserdata(L, 1);
	if( pac ) {
		_pickle_reset(pac, L);
		vlua_realloc(L, pac->mb.buf, pac->mb.free, 0);
		memset(pac, 0, sizeof(LPacker));
	}
	return 0;
}

static int _pickle_mem_buffer_parse(lua_State* L) {
	LPacker* pac = (LPacker*)lua_touserdata(L, 1);
	lua_pushlstring(L, (const char*)(pac->mb.buf), (size_t)(pac->mb.len));
	return 1;
}

static void _pickle_push_ref_table(lua_State* L, int new_table) {
	if( !new_table ) {
		if( vlua_builtin_ridx_rawget(L, LUA_RIDX_TEMP_REF_TABLE)==LUA_TTABLE )
			return;
		lua_pop(L, 1);
	}

	lua_createtable(L, 32, 0);
	lua_pushvalue(L, -1);
	vlua_builtin_ridx_rawset(L, LUA_RIDX_TEMP_REF_TABLE);
}

static PickleMemBuffer* pickle_mem_buffer_push(lua_State* L) {
	LPacker* pac = NULL;
	if( vlua_builtin_ridx_rawget(L, LUA_RIDX_PICKLE_CACHE)==LUA_TUSERDATA ) {
		pac = (LPacker*)lua_touserdata(L, -1);
	} else {
		lua_pop(L, 1);
		pac = (LPacker*)lua_newuserdata(L, sizeof(LPacker));
		memset(pac, 0, sizeof(LPacker));
		lua_newtable(L);
		lua_pushcfunction(L, _pickle_mem_buffer_gc);
		lua_setfield(L, -2, "__gc");
		lua_pushcfunction(L, _pickle_mem_buffer_parse);
		lua_setfield(L, -2, "__call");
		lua_setmetatable(L, -2);
		lua_pushvalue(L, -1);
		vlua_builtin_ridx_rawset(L, LUA_RIDX_PICKLE_CACHE);
	}

	_pickle_push_ref_table(L, 1);
	lua_pop(L, 1);

	assert( (PickleMemBuffer*)pac==&(pac->mb) );
	return (PickleMemBuffer*)_pickle_reset(pac, L);
}

static inline void _pack_tbl(LPacker* pac, int idx) {
	lua_State* L = pac->L;

	_pickle_push_ref_table(L, 0);
	lua_pushvalue(L, idx);
	if( lua_rawget(L, -2)==LUA_TNUMBER ) {
		uint32_t n = (uint32_t)lua_tointeger(L, -1);
		assert( lua_tointeger(L, -1) >= 0 );
		lua_pop(L, 2);
		if( n >= pac->nref )
			pac->nref = n + 1;
		_pickle_add(L, pac, TTREF, &n, 4);

	} else {
		int top, kidx, vidx;
		lua_Integer i, n;
		size_t narr;

		lua_pop(L, 1);
		lua_pushvalue(L, idx);
		lua_pushinteger(L, pac->nref++);
		lua_rawset(L, -3);
		lua_pop(L, 1);

		top = lua_gettop(L);
		kidx = top + 1;
		vidx = top + 2;

		narr = lua_rawlen(L, idx);
		if( narr > 0x7FFFFFFF )
			luaL_error(L, "too large table array!");

		n = (lua_Integer)narr;
		{
			int32_t v = (int32_t)n;
			_pickle_add(L, pac, TTABLE, &v, 4);
		}

		pac->depth++;
		if( pac->depth > PICKLE_DEPTH_MAX ) {
			luaL_error(L, "pack table too depth!");

		} else if( n ) {
			for( i=1; i<=n; ++i ) {
				lua_rawgeti(L, idx, i);
				_pack(pac, kidx);
				lua_settop(L, top);
			}

			lua_pushnil(L);
			while( lua_next(L, idx) ) {
				if( lua_type(L, -2)==LUA_TNUMBER ) {
					if( lua_isinteger(L, -2) ) {
						lua_Integer k = lua_tointeger(L, -2);
						if( k>0 && k<=n ) {
							lua_settop(L, kidx);
							continue;
						}
					}
				}
				_pack(pac, kidx);
				_pack(pac, vidx);
				lua_settop(L, kidx);
			}
			_pickle_add(L, pac, TNIL, NULL, 0);

		} else {
			lua_pushnil(L);
			while( lua_next(L, idx) ) {
				_pack(pac, kidx);
				_pack(pac, vidx);
				lua_settop(L, kidx);
			}
			_pickle_add(L, pac, TNIL, NULL, 0);
		}
		pac->depth--;
	}
}

static void _pack(LPacker* pac, int idx) {
	lua_State* L = pac->L;
	switch( lua_type(L, idx) ) {
	case LUA_TBOOLEAN:
		_pickle_add(L, pac, lua_toboolean(L, idx) ? TTRUE : TFALSE, NULL, 0);
		break;

	case LUA_TLIGHTUSERDATA:
		_pickle_add(L, pac, lua_touserdata(L, idx)==NULL ? TNULL : TNIL, NULL, 0);
		break;

	case LUA_TNUMBER:
		if( lua_isinteger(L, idx) ) {
			int64_t v = (int64_t)lua_tointeger(L, idx);
			_pickle_add(L, pac, TINT, &v, 8);

		} else {
			double v = (double)lua_tonumber(L, idx);
			_pickle_add(L, pac, TNUM, &v, 8);
		}
		break;

	case LUA_TSTRING:
		{
			size_t len = 0;
			const char* str = lua_tolstring(L, idx, &len);
			uint32_t v = (uint32_t)len;
			_pickle_add(L, pac, TSTR, &v, 4);
			_pickle_addbuf(L, pac, str, len);
		}
		break;

	case LUA_TTABLE:
		_pack_tbl(pac, idx);
		break;

	default:
		_pickle_add(L, pac, TNIL, NULL, 0);
		break;
	}
}

static inline const char* _unpack_str(LUnPacker* upac, const char* pkt, size_t len) {
	if( (pkt + len) <= upac->end ) {
		lua_pushlstring(upac->L, pkt, len);
	} else {
		luaL_error(upac->L, "pkt bad size when unpack str");
	}
	return pkt + len;
}

static inline const char* _unpack_tbl(LUnPacker* upac, const char* pkt) {
	lua_State* L = upac->L;
	if( (pkt + 4) <= upac->end ) {
		int32_t narr;
		memcpy(&narr, pkt, 4);
		pkt += 4;

		upac->depth++;
		if( upac->depth > PICKLE_DEPTH_MAX ) {
			luaL_error(L, "unpack table too depth!");

		} else if( narr>=0 ) {
			int n = narr;
			int i;
			lua_createtable(L, n, 0);

			if( upac->iref < upac->nref ) {
				upac->iref++;
				lua_pushvalue(L, -1);
				lua_rawseti(L, upac->tref, (lua_Integer)upac->iref);
			}

			for( i=1; i<=n; ++i ) {
				pkt = _unpack(upac, pkt);
				lua_rawseti(L, -2, i);
			}
			for(;;) {
				pkt = _unpack(upac, pkt);
				if( lua_isnil(L, -1) ) {
					lua_pop(L, 1);
					break;
				}
				pkt = _unpack(upac, pkt);
				lua_rawset(L, -3);
			}

		} else {
			luaL_error(L, "pkt table arr size");
		}
		upac->depth--;

	} else {
		luaL_error(L, "pkt bad size when unpack table arr len");
	}

	return pkt;
}

static inline void _unpack_tref(LUnPacker* upac, uint32_t ref) {
	if( ref < upac->iref ) {
		assert( upac->tref && lua_istable(upac->L, upac->tref) );
		lua_rawgeti(upac->L, upac->tref, (lua_Integer)(ref + 1));

	} else {
		luaL_error(upac->L, "bad tref");
	}
}

static const char* _unpack(LUnPacker* upac, const char* pkt) {
	lua_State* L = upac->L;
	const char* end = upac->end;
	char tp;
	if( pkt >= end )
		luaL_error(L, "pkt bad size when unpack type");
	tp = *pkt++;

	switch( tp ) {
	case TNIL:
		lua_pushnil(L);
		break;
	case TTRUE:
		lua_pushboolean(L, 1);
		break;
	case TFALSE:
		lua_pushboolean(L, 0);
		break;
	case TINT:
		if( (pkt + 8) <= end ) {
			int64_t v;
			memcpy(&v, pkt, 8);
			pkt += 8;
			lua_pushinteger(L, (lua_Integer)v);
		} else {
			luaL_error(L, "pkt bad size when unpack int64");
		}
		break;
	case TNUM:
		if( (pkt + 8) <= end ) {
			double v;
			memcpy(&v, pkt, 8);
			pkt += 8;
			lua_pushnumber(L, (lua_Number)v);
		} else {
			luaL_error(L, "pkt bad size when unpack number");
		}
		break;
	case TSTR:
		if( (pkt + 4) <= end ) {
			uint32_t sz;
			memcpy(&sz, pkt, 4);
			pkt = _unpack_str(upac, pkt+4, sz);
		} else {
			luaL_error(L, "pkt bad size when unpack str len");
		}
		break;
	case TTABLE:
		pkt = _unpack_tbl(upac, pkt);
		break;
	case TNULL:
		lua_pushlightuserdata(L, NULL);
		break;
	case TTREF:
		if( (pkt + 4) <= end ) {
			uint32_t ref;
			memcpy(&ref, pkt, 4);
			pkt += 4;
			_unpack_tref(upac, ref);
		} else {
			luaL_error(L, "pkt bad size when unpack tref len");
		}
		break;
	default:
		luaL_error(L, "pkt bad type(%c)", tp);
		break;
	}

	return pkt;
}

static void _lua_pickle_pack(LPacker* pac, int start, int end) {
	lua_State* L = pac->L;
	int i, n;
#ifdef _DEBUG
	int top = lua_gettop(L);
#endif
	assert( start>=0 && end>=0 );

	if( start > end )
		luaL_error(L, "start > end");
	n = end - start + 1;
	if( n > PICKLE_ARG_MAX )
		luaL_error(L, "arg count MUST <= %d", PICKLE_ARG_MAX);
	luaL_checkstack(L, (8 + 2*PICKLE_DEPTH_MAX), "pickle pack check stack failed!");	// luaL_Buffer, error, every depth(k, v) & keep free 6 space

	_pickle_addbuf(L, pac, NULL, 1 + 4);	// skip n, nref

	for( i=start; i<=end; ++i ) {
#ifdef _DEBUG
		assert( lua_gettop(L)==top );
#endif
		_pack(pac, i);
	}

	pac->mb.buf[0] = (unsigned char)n;
	memcpy(pac->mb.buf+1, &(pac->nref), sizeof(pac->nref));

#ifdef _DEBUG
	assert( lua_gettop(L)==top );
#endif
}

static void* vlua_pickle_pack(size_t* plen, lua_State* L, int start, int end) {
	LPacker* pac;
	start = lua_absindex(L, start);
	end = lua_absindex(L, end);
	pac = (LPacker*)pickle_mem_buffer_push(L);
	lua_pop(L, 1);
	assert( pac );

	if( start <= end )
		_lua_pickle_pack(pac, start, end);

	assert( pac->mb.len );
	*plen = pac->mb.len;
	return pac->mb.buf;
}

static int _pickle_unpack(LUnPacker* upac) {
	lua_State* L = upac->L;
	const char* ps = (const char*)upac->start;
	int i, n;
#ifdef _DEBUG
	int top = lua_gettop(L);
#endif

	if( ps>=upac->end )
		return 0;

	if( (ps + (1 + 4)) >= upac->end )
		luaL_error(L, "pkt bad format, empty");

	n = *ps++;
	memcpy(&(upac->nref), ps, 4);
	ps += 4;

	if( n<0 && n>PICKLE_ARG_MAX )
		luaL_error(L, "pkt bad format, narg out of range");
	luaL_checkstack(L, (n + 1 + (2*PICKLE_DEPTH_MAX + 8)), "pickle unpack check stack failed!");	// narg + refs + every depth(last value) & keep free 8 space
	if( upac->nref ) {
		_pickle_push_ref_table(L, 1);
		upac->tref = lua_gettop(L);
	}

	for( i=0; i<n; ++i ) {
#ifdef _DEBUG
		assert( lua_gettop(L)==((upac->tref ? upac->tref : top) + i) );
#endif
		ps = _unpack(upac, ps);
	}
	if( ps!=upac->end )
		luaL_error(L, "pkt length not match");

	if( upac->nref )
		lua_remove(L, upac->tref);

#ifdef _DEBUG
	assert( lua_gettop(L)==(top + n) );
#endif
	return n;
}

static int vlua_pickle_unpack(lua_State* L, const void* pkt, size_t len) {
	LUnPacker upac;
	upac.L = L;
	upac.start = (const char*)pkt;
	upac.end = upac.start + len;
	upac.depth = 1;
	upac.tref = 0;
	upac.nref = 0;
	upac.iref = 0;
	return _pickle_unpack(&upac);
}

static int lua_pickle_pack(lua_State* L) {
	size_t len = 0;
	void* buf = vlua_pickle_pack(&len, L, 1, -1);
	lua_pushlstring(L, (const char*)buf, len);
	return 1;
}

static int lua_pickle_unpack(lua_State* L) {
	LUnPacker upac;
	size_t len = 0;
	upac.L = L;
	upac.start = "";
	upac.end = upac.start;
	upac.depth = 1;
	upac.tref = 0;
	upac.nref = 0;
	upac.iref = 0;

	if( lua_type(L, 1)==LUA_TSTRING ) {
		upac.start = lua_tolstring(L, 1, &len);
		upac.end = upac.start + len;
	}

	return _pickle_unpack(&upac);
}

// thread

#define VLUA_THREAD_MAX	64

typedef struct _VLuaThreadPool	VLuaThreadPool;
typedef struct _VLuaThreadTask	VLuaThreadTask;

typedef struct _VLuaThreadTask {
	VLuaThreadTask*		next;
	const char*			req;		// req method path
	size_t				key_len;
	const void*			key_buf;	// req key
	size_t				len;
	void*				pkt;		// req & resp packet
	int					resp_res;
} VLuaThreadTask;

static void thread_task_reset_packet(VLuaThreadTask* task, const void* pkt, size_t len) {
	task->len = 0;
	free(task->pkt);
	task->pkt = len ? malloc(len) : NULL;
	if( task->pkt ) {
		memcpy(task->pkt, pkt, len);
		task->len = len;
	}
}

typedef struct _VLuaThreadMQ {
	#ifdef _WIN32
		CRITICAL_SECTION	cs;
		HANDLE				ev;
	#else
		pthread_mutex_t		mutex;
		pthread_cond_t		cond;
	#endif

	VLuaThreadTask*			head;
	VLuaThreadTask*			tail;
} VLuaThreadMQ;

static void vlua_thread_task_mq_init(VLuaThreadMQ* mq) {
	memset(mq, 0, sizeof(VLuaThreadMQ));
	#ifdef _WIN32
		InitializeCriticalSection(&mq->cs);
		mq->ev = CreateEventA(NULL, TRUE, FALSE, NULL);
	#else
		pthread_mutex_init(&mq->mutex, 0);
		pthread_cond_init(&mq->cond, NULL);
	#endif
}

static void vlua_thread_task_mq_uninit(VLuaThreadMQ* mq) {
	#ifdef _WIN32
		CloseHandle(mq->ev);
		DeleteCriticalSection(&mq->cs);
	#else
		pthread_cond_destroy(&mq->cond);
		pthread_mutex_destroy(&mq->mutex);
	#endif
}

static void vlua_thread_task_mq_push(VLuaThreadMQ* mq, VLuaThreadTask* task) {
	int need_event;
	task->next = NULL;

#ifdef _WIN32
	EnterCriticalSection(&mq->cs);
#else
	pthread_mutex_lock(&mq->mutex);
#endif

	if( mq->head ) {
		mq->tail->next = task;
	} else {
		mq->head = task;
	}
	mq->tail = task;

	need_event = (mq->head==task);

#ifdef _WIN32
	LeaveCriticalSection(&mq->cs);
#else
	pthread_mutex_unlock(&mq->mutex);
#endif

	if( need_event ) {
		#ifdef _WIN32
			SetEvent(mq->ev);
		#else
			pthread_cond_broadcast(&mq->cond);
		#endif
	}
}

static VLuaThreadTask* __thread_task_mq_pop(VLuaThreadMQ* mq) {
	VLuaThreadTask* res = NULL;
	if( mq->head ) {
		res = mq->head;
		mq->head = res->next;
		res->next = NULL;

		#ifdef _WIN32
			if( !(mq->head) )
				ResetEvent(mq->ev);
		#endif
	}
	return res;
}

static VLuaThreadTask* vlua_thread_task_mq_pop(VLuaThreadMQ* mq, uint32_t wait_time) {
	VLuaThreadTask* res = NULL;

#ifdef _WIN32
	if( WaitForSingleObject(mq->ev, wait_time)==WAIT_OBJECT_0 ) {
		EnterCriticalSection(&mq->cs);
		res = __thread_task_mq_pop(mq);
		LeaveCriticalSection(&mq->cs);
	}
#else
	pthread_mutex_lock(&mq->mutex);
task_pop_label:
	if( (res = __thread_task_mq_pop(mq)) == NULL ) {
		if( wait_time==0xFFFFFFFF) {
			if( pthread_cond_wait(&mq->cond, &mq->mutex)==0 )
				goto task_pop_label;
		} else {
			struct timespec timeout;
			struct timeval now;
			gettimeofday(&now, NULL);
			now.tv_usec += (wait_time * 1000);
			timeout.tv_sec = now.tv_sec + (now.tv_usec / 1000000);
			timeout.tv_nsec = (now.tv_usec % 1000000) * 1000;
			if( pthread_cond_timedwait(&mq->cond, &mq->mutex, &timeout)!=ETIMEDOUT )
				goto task_pop_label;
		}
	}
	pthread_mutex_unlock(&mq->mutex);
#endif

	return res;
}

typedef struct _VLuaThreadPriv {
	lua_State*		state;
	VLuaThreadPool*	pool;
	VLuaThreadID	id;
} VLuaThreadPriv;

struct _VLuaThreadPool {
	int				stop;
	size_t			remain;
	VLuaThreadMQ	req_mq;
	VLuaThreadMQ	resp_mq;
	size_t			thread_num;
	VLuaThreadPriv	threads[VLUA_THREAD_MAX];
};

static int lua_thread_pool_destroy(lua_State* L) {
	VLuaThreadPool* ud = (VLuaThreadPool*)luaL_testudata(L, 1, "VLuaThreadPool");
	if( !ud )
		return 0;

	if( !ud->stop ) {
		VLuaThreadTask stop_tasks[VLUA_THREAD_MAX];
		VLuaThreadTask* task;
		size_t n = ud->thread_num;
		size_t i;
		ud->stop = 1;
		memset(stop_tasks, 0, sizeof(stop_tasks));

		ud->remain += n;
		for( i=0; i<n; ++i ) {
			vlua_thread_task_mq_push(&(ud->req_mq), &stop_tasks[i]);
		}

		while( ud->remain > 0 ) {
			task = (VLuaThreadTask*)vlua_thread_task_mq_pop(&(ud->resp_mq), 0xFFFFFFFF);
			if( task ) {
				ud->remain--;
				if( task->req ) {
					free(task->pkt);
					free(task);
				}
			}
		}

		for( i=0; i<n; ++i ) {
			vlua_thread_destroy(ud->threads[i].id);
			lua_close(ud->threads[i].state);
		}

		vlua_thread_task_mq_uninit(&(ud->resp_mq));
		vlua_thread_task_mq_uninit(&(ud->req_mq));
	}

	return 0;
}

static int lua_thread_pool_remain(lua_State* L) {
	VLuaThreadPool* ud = (VLuaThreadPool*)luaL_checkudata(L, 1, "VLuaThreadPool");
	lua_pushinteger(L, ud ? ud->remain : 0);
	return 1;
}

static int _lua_task_execute(lua_State* L) {
	VLuaThreadTask* task = (VLuaThreadTask*)lua_touserdata(L, lua_upvalueindex(1));
	size_t n = 0;
	void* s;

	if( vlua_rawget_ex(L, task->req)!=LUA_TFUNCTION )
		return luaL_error(L, "not find req function: %s", task->req);

	vlua_pickle_unpack(L, task->pkt, task->len);

	lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);

	s = vlua_pickle_pack(&n, L, 1, lua_gettop(L));
	thread_task_reset_packet(task, s, n);
	return 0;
}

static VLUA_THREAD_DECLARE(work_thread, _arg) {
	VLuaThreadPriv* arg = (VLuaThreadPriv*)_arg;
	VLuaThreadPool* pool = arg->pool;
	lua_State* L = arg->state;
	VLuaThreadTask* task = NULL;

	while( L ) {
		task = vlua_thread_task_mq_pop(&(pool->req_mq), 0xFFFFFFFF);
		if( !task )
			continue;

		if( task->req ) {
			if( !(pool->stop) ) {
				lua_settop(L, 0);
				lua_pushlightuserdata(L, task);
				lua_pushcclosure(L, _lua_task_execute, 1);
				task->resp_res = vlua_pcall_stacktrace(L, 0, 0);
				if( task->resp_res ) {
					size_t n = 0;
					const char* e = lua_tolstring(L, -1, &n);
					thread_task_reset_packet(task, e, n);
				}
			}
		} else {
			L = NULL;	// used for break
		}
		vlua_thread_task_mq_push(&(pool->resp_mq), task);
	}
	return 0;
}

static int lua_thread_pool_run(lua_State* L) {
	// pool, req, key, ...	
	VLuaThreadPool* ud = (VLuaThreadPool*)luaL_checkudata(L, 1, "VLuaThreadPool");
	int top = lua_gettop(L);
	size_t key_len = 0;
	void* key_buf = (top < 3) ? NULL : vlua_pickle_pack(&key_len, L, 2, 2);
	size_t len = 0;
	const char* req = luaL_checklstring(L, 3, &len);
	VLuaThreadTask* task;
	char* str;

	if( ud->stop )
		return luaL_error(L, "thread pool already destroy!");

	task = (VLuaThreadTask*)malloc(sizeof(VLuaThreadTask) + len + 1 + key_len);
	memset(task, 0, sizeof(VLuaThreadTask));

	// req
	str = (char*)(task + 1);
	memcpy(str, req, len);
	str[len] = '\0';
	task->req = str;

	// key
	str += (len + 1);
	if( key_len ) {
		memcpy(str, key_buf, key_len);
	}
	task->key_len = key_len;
	task->key_buf = str;

	// args
	str = (char*)vlua_pickle_pack(&len, L, 4, top);
	thread_task_reset_packet(task, str, len);

	ud->remain++;
	vlua_thread_task_mq_push(&(ud->req_mq), task);
	return 0;
}

static int lua_thread_pool_dispatch(lua_State* L) {
	VLuaThreadPool* ud = (VLuaThreadPool*)luaL_checkudata(L, 1, "VLuaThreadPool");
	int has_cb = lua_isfunction(L, 2);
	uint32_t wait_timeout = luaL_optinteger(L, 3, 0xFFFFFFFF);
	VLuaThreadTask* task;
	int top;

	if( ud->stop )
		return luaL_error(L, "thread pool already destroy!");

	if( (task = vlua_thread_task_mq_pop(&(ud->resp_mq), wait_timeout))==NULL )
		return 0;

	ud->remain--;

	if( task->resp_res ) {
		lua_pushlstring(L, (const char*)(task->pkt), task->len);
		free(task->pkt);
		free(task);
		return lua_error(L);
	}

	top = lua_gettop(L);
	if( has_cb ) {
		lua_pushvalue(L, 2);
	}
	vlua_pickle_unpack(L, task->key_buf, task->key_len);
	vlua_pickle_unpack(L, task->pkt, task->len);
	free(task->pkt);
	free(task);
	if( has_cb ) {
		lua_call(L, lua_gettop(L) - top - 1, LUA_MULTRET);
	}

	return lua_gettop(L) - top;
}

static int lua_thread_pool_wait(lua_State* L) {
	VLuaThreadPool* ud = (VLuaThreadPool*)luaL_checkudata(L, 1, "VLuaThreadPool");
	int n = lua_gettop(L);
	int i;

	while( ud->remain ) {
		lua_pushcfunction(L, lua_thread_pool_dispatch);
		for( i=1; i<=n; ++i ) {
			lua_pushvalue(L, i);
		}
		lua_call(L, n, 0);
	}
	return 0;
}

static const luaL_Reg thread_pool_methods[] =
	{ {"__gc", lua_thread_pool_destroy}
	, {"destroy", lua_thread_pool_destroy}
	, {"remain", lua_thread_pool_remain}
	, {"run", lua_thread_pool_run}
	, {"dispatch", lua_thread_pool_dispatch}
	, {"wait", lua_thread_pool_wait}
	, {NULL, NULL}
	};

typedef struct _ThreadInitTag {
	size_t		length;
	const char*	script;
	size_t		len;
	const void*	pkt;
} ThreadInitTag;

static int _lua_thread_init(lua_State* L) {
	ThreadInitTag* tag = (ThreadInitTag*)lua_touserdata(L, lua_upvalueindex(1));

	// function vmake_target_add(target, process)
	// function vmake(...) : args : target1, target2 ...
	// 
	const char* thread_init_script =
		"function vmake_target_add(target, process) end\n"
		"function vmake(...) error('not allowed in thread!') end\n"
		;
	if( luaL_loadbuffer(L, thread_init_script, strlen(thread_init_script), "<thread_init>") )
		return lua_error(L);
	lua_call(L, 0, 0);

	if( luaL_loadbuffer(L, tag->script, tag->length, "<thread_init>") )
		return lua_error(L);
	if( tag->pkt )
		vlua_pickle_unpack(L, tag->pkt, tag->len);
	lua_call(L, lua_gettop(L)-1, 0);

	return 0;
}

static int lua_thread_pool_create(lua_State* L) {
	int top = lua_gettop(L);
	size_t thread_num = (size_t)luaL_optinteger(L, 1, 0);
	size_t i;
	VLuaThreadPool* ud = NULL;
	ThreadInitTag init_tag;
	memset(&init_tag, 0, sizeof(init_tag));

	if( thread_num < 1 ) {
		thread_num = 1;
	} else if( thread_num > VLUA_THREAD_MAX ) {
		thread_num = VLUA_THREAD_MAX;
	}

	ud = (VLuaThreadPool*)lua_newuserdata(L, sizeof(VLuaThreadPool));
	memset(ud, 0, sizeof(VLuaThreadPool));

	vlua_thread_task_mq_init(&(ud->req_mq));
	vlua_thread_task_mq_init(&(ud->resp_mq));
	ud->thread_num = thread_num;

	init_tag.length = 0;
	init_tag.script = luaL_optlstring(L, 2, NULL, &init_tag.length);
	init_tag.len = 0;
	init_tag.pkt = (top < 3 ) ? NULL : vlua_pickle_pack(&init_tag.len, L, 3, top);

	for( i=0; i<thread_num; ++i ) {
		lua_State* state = vlua_state_new();
		if( !state ) {
			lua_pushstring(L, "new lua state failed!");
			goto error_return;
		}
		ud->threads[i].state = state;
		ud->threads[i].pool = ud;
		if( !vlua_thread_create(&(ud->threads[i].id), work_thread, &(ud->threads[i])) ) {
			lua_pushstring(L, "thread create failed!");
			goto error_return;
		}
		if( !init_tag.script )
			continue;
		lua_pushlightuserdata(state, &init_tag);
		lua_pushcclosure(state, _lua_thread_init, 1);
		if( vlua_pcall_stacktrace(state, 0, 0) ) {
			size_t len = 0;
			const char* str = lua_tolstring(state, -1, &len);
			lua_pushlstring(L, str, len);
			goto error_return;
		}
	}

	if( luaL_newmetatable(L, "VLuaThreadPool") ) {
		luaL_setfuncs(L, thread_pool_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");	// metatable.__index = metatable
	}
	lua_setmetatable(L, -2);
	return 1;

error_return:
	for( i=0; i<thread_num; ++i ) {
		if( ud->threads[i].state ) {
			lua_close(ud->threads[i].state);
		}
	}
	return lua_error(L);
}

// files

typedef struct _FileStr {
	size_t n;
	const char* s;
} FileStr;

#define is_sep(ch) ((ch)=='/' || (ch)=='\\')

static const char* skip_seps(const char* s) {
	while( is_sep(*s) ) {
		++s;
	}
	return s;
}

static int lua_filename_format(lua_State* L) {
	FileStr strs[258];
	FileStr* start = strs;
	FileStr* cur = strs;
	FileStr* end = cur + 258;
	const char* s = luaL_checkstring(L, 1);
	luaL_Buffer B;

	// root
#ifdef _WIN32
	if( ((s[0]>='a' && s[1]<='z') || (s[0]>='A' && s[1]<='Z')) && (s[1]==':') ) {
		cur->s = s;
		cur->n = 2;
		s += 2;
		if( is_sep(*s) ) {
			++s;
			cur->n++;
		}
		++cur;
		start = cur;
	} else if( is_sep(s[0]) && is_sep(s[1]) ) {
		cur->s = s;
		cur->n = 2;
		s += 2;
		start = ++cur;
		start = cur;
	}
#else
	if( *s=='/' ) {
		cur->s = s;
		cur->n = 1;
		++s;
		start = ++cur;
	}
#endif

	// separate & remove ./ && remove xx/../ 
	// 
	while( *(s = skip_seps(s)) ) {
		if( cur >= end ) {
			FileStr* prev = cur - 1;
			while( *s ) { ++s; }
			prev->n = (s - prev->s);
			break;
		}

		cur->s = s++;
		while( !(*s=='\0' || is_sep(*s)) ) {
			++s;
		}
		cur->n = (s - cur->s);

		if( cur->n==1 && cur->s[0]=='.' ) {
			// remove ./
		} else if( cur->n==2 && cur->s[0]=='.' && cur->s[1]=='.' ) {
			FileStr* prev = cur - 1;
			if( (prev < start) || (prev->n==2 && prev->s[0]=='.' && prev->s[1]=='.') ) {
				++cur;
			} else {
				--cur;	// remove xx/../
			}
		} else {
			++cur;
		}
	}

	luaL_buffinit(L, &B);
	if( start!=strs ) {
		luaL_addlstring(&B, strs[0].s, strs[0].n);	// root C: or C:\ or / 
	}
	if( start < cur ) {
		end = cur;
		cur = start;
		luaL_addlstring(&B, cur->s, cur->n);
		for( ++cur; cur < end; ++cur) {
#ifdef _WIN32
			luaL_addchar(&B, '\\');
#else
			luaL_addchar(&B, '/');
#endif
			luaL_addlstring(&B, cur->s, cur->n);
		}
	}
	luaL_pushresult(&B);
	lua_pushboolean(L, start!=strs);	// is abs path
	return 2;
}

static int lua_file_stat(lua_State* L) {
	const char* filename = luaL_checkstring(L, 1);
	struct stat st;
	if( stat(filename, &st) )
		return 0;
	lua_pushboolean(L, 1);
	lua_pushinteger(L, (lua_Integer)st.st_size);
	lua_pushinteger(L, (lua_Integer)st.st_mtime);
	return 3;
}

static int lua_file_copy(lua_State* L) {
	size_t slen = 0;
	const char* src = luaL_checklstring(L, 1, &slen);
	size_t dlen = 0;
	const char* dst = luaL_checklstring(L, 2, &dlen);
	#ifdef _WIN32
		lua_pushboolean(L, CopyFileA(src, dst, 1));
	#else
		luaL_Buffer B;
		luaL_buffinitsize(L, &B, slen + dlen + 32);
		sprintf(B.b, "cp %s %s", src, dst);
		lua_pushboolean(L, system(B.b)==0);
	#endif
	return 1;
}

static int lua_file_mkdir(lua_State* L) {
	const char* dirpath = luaL_checkstring(L, 1);
	#ifdef _WIN32
		lua_pushboolean(L, CreateDirectoryA(dirpath, NULL));
	#else
		int mode = (int)luaL_optinteger(L, 2, 0777);
		lua_pushboolean(L, mkdir(dirpath, mode)==0);
	#endif
	return 1;
}

static int lua_file_list(lua_State* L) {
	const char* dirpath = luaL_checkstring(L, 1);
	lua_Integer nfile = 0;
	lua_Integer ndir = 0;
#ifdef _WIN32
	HANDLE h = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATAA fdata;
	char findstr[4096];
	lua_newtable(L);	// files
	lua_newtable(L);	// dirs

	snprintf(findstr, 4096, "%s\\*.*", dirpath);
	h = FindFirstFileA(findstr, &fdata);
	if( h == INVALID_HANDLE_VALUE )
		return 2;
	while( h != INVALID_HANDLE_VALUE ) {
		if( fdata.cFileName[0]==L'.' ) {
			if( fdata.cFileName[1]==L'\0' ) {
				goto next_label;
			} else if( fdata.cFileName[1]==L'.' && fdata.cFileName[2]==L'\0' ) {
				goto next_label;
			}
		}

		lua_pushstring(L, fdata.cFileName);
		if( fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
			lua_rawseti(L, -2, ++ndir);
		} else {
			lua_rawseti(L, -3, ++nfile);
		}

	next_label:
		if( !FindNextFileA(h, &fdata) )
			break;
	}
	FindClose(h);
#else
	DIR* dir = opendir(dirpath);
	struct dirent* finfo = NULL;
	lua_newtable(L);	// files
	lua_newtable(L);	// dirs

	if( !dir )
		return 2;
	
	while( (finfo = readdir(dir)) != NULL ) {
		if( finfo->d_name[0]=='.' ) {
			if( finfo->d_name[1]=='\0' )
				continue;
			if( finfo->d_name[1]=='.' && finfo->d_name[2]=='\0' )
				continue;
		}

		lua_pushstring(L, finfo->d_name);
		if( finfo->d_type==DT_DIR ) {
			lua_rawseti(L, -2, ++ndir);
		} else {
			lua_rawseti(L, -3, ++nfile);
		}
    }
	closedir(dir);
#endif
	return 2;
}

// vlua lib for build
// 
static const luaL_Reg vlua_methods[] =
	{ {"fetch_args", lua_fetch_args}
	, {"match_arg", lua_match_arg}
	, {"pack", lua_pickle_pack}
	, {"unpack", lua_pickle_unpack}
	, {"thread_pool_create", lua_thread_pool_create}
	, {"filename_format", lua_filename_format}
	, {"file_stat", lua_file_stat}
	, {"file_copy", lua_file_copy}
	, {"file_mkdir", lua_file_mkdir}
	, {"file_list", lua_file_list}
	, {NULL, NULL}
	};

static void push_exe_path(lua_State* L) {
	char pth[4096];
	int len;
#ifdef _WIN32
	len = (int)GetModuleFileNameA(0, pth, 4096);
#else
	len = readlink("/proc/self/exe", pth, 4096);
#endif
	// try use argv[0]
	if( len <= 0 ) {
		len = strlen(vlua_self); 
		strcpy(pth, vlua_self);
	}

	for( --len; len>0; --len ) {
		if( pth[len]=='\\' || pth[len]=='/' ) {
			pth[len] = '\0';
			break;
		}
	}

	if( len > 0 ) {
		lua_pushlstring(L, pth, len);
	} else {
		lua_pushstring(L, ".");
	}
}

static lua_State* vlua_state_new(void) {
	lua_State* L = luaL_newstate();
	int ridx;

	luaL_openlibs(L);

	for( ridx=LUA_RIDX_LAST+1; ridx<LUA_RIDX_BUILTIN_MAX; ++ridx ) {
		lua_pushboolean(L, 0);
		lua_rawseti(L, LUA_REGISTRYINDEX, ridx);
	}
	luaL_newlib(L, vlua_methods);

	lua_pushvalue(L, -1);
	vlua_builtin_ridx_rawset(L,LUA_RIDX_VLUA);

#ifdef _WIN32
	lua_pushstring(L, "windows");
#else
	lua_pushstring(L, "linux");
#endif
	lua_setfield(L, -2, "OS");

	push_exe_path(L);
	lua_setfield(L, -2, "path");

	lua_pushstring(L, vlua_self);
	lua_setfield(L, -2, "self");

	if( vlua_script ) {
		lua_pushstring(L, vlua_script);
		lua_setfield(L, -2, "script");
	}

	lua_setglobal(L, "vlua");

	return L;
}

static int show_help(void) {
	printf("usage : vlua <filename> [args]\n");
	printf("usage : vlua -e <script> [args]\n");
	return 0;
}

static int run(lua_State* L, const char* script) {
	if( luaL_loadfile(L, script) ) {
		fprintf(stderr, "error : %s\n", lua_tostring(L, -1));
		return 2;
	}

	// vlua.__script
	// 
	lua_getglobal(L, "vlua");
	vlua_rawget_ex(L, "string.dump");
	lua_pushvalue(L, -3);
	lua_call(L, 1, 1);
	lua_setfield(L, -2, "__script");
	lua_pop(L, 1);

	// vlua.thread_pool, main thread arg: -jN, default use 4 thread, -j0 means not use thread_pool
	// 
	// function vmake_target_add(target, process)
	// function vmake(...) : args : target1, target2 ...
	// 
	// add default main()
	// 
	const char* main_thread_init_script =
		"local n = vlua.match_arg('^%-j(%d+)$')\n"
		"n = math.tointeger(n) or 4\n"
		"if n > 0 then\n"
		"	-- delay create thread pool!\n"
		"	vlua.thread_pool = setmetatable({}, {__index=function(t,k)\n"
		"		local pool = rawget(t, '__self')\n"
		"		if not pool then\n"
		"			pool = vlua.thread_pool_create(n, vlua.__script)\n"
		"			rawset(t, '__self', pool)\n"
		"			vlua.thread_pool = pool\n"
		"		end\n"
		"		local elem = pool[k]\n"
		"		if type(elem)~='function' then return elem end\n"
		"		local wrapper = function(self, ...) return elem(pool, ...) end\n"
		"		rawset(t,k,wrapper)\n"
		"		return wrapper\n"
		"	end})\n"
		"end\n"
		"\n"
		"local __targets = {}	-- target : process\n"
		"local __maked_targets = {}\n"
		"\n"
		"function vmake_target_all()\n"
		"	return __targets\n"
		"end\n"
		"\n"
		"function vmake_target_add(target, process)\n"
		"	assert(__targets[target]==nil, 'target('..tostring(target)..') already exist!')\n"
		"	__targets[target] = process\n"
		"end\n"
		"\n"
		"local function _make(target)\n"
		"	local output = __maked_targets[target]\n"
		"	if output then return output end\n"
		"	__maked_targets[target] = '<building>'\n"
		"	local f = __targets[target]\n"
		"	if f then\n"
		"		output = f(target) or '<no-output>'\n"
		"		__maked_targets[target] = output\n"
		"		return output\n"
		"	end\n"
		"	error('unknown target: '..tostring(target))\n"
		"end\n"
		"\n"
		"function vmake(...)\n"
		"	local n = select('#', ...)\n"
		"	if n < 2 then return _make(...) end\n"
		"	local res = {}\n"
		"	for i=1,n do\n"
		"		local t = select(i, ...)\n"
		"		res[i] = _make(t)\n"
		"	end\n"
		"	return res\n"
		"end\n"
		"\n"
		"function main()\n"
		"	local targets = {}\n"
		"	for _,v in ipairs(vlua.fetch_args()) do\n"
		"		local t = v:match('^[_%w]+$')\n"
		"		if t then table.insert(targets, t) end\n"
		"	end\n"
		"	if #targets==0 then table.insert(targets, '') end\n"
		"	vmake(table.unpack(targets))\n"
		"end\n"
		;

	if( luaL_loadbuffer(L, main_thread_init_script, strlen(main_thread_init_script), "<vmake_main>") ) {
		fprintf(stderr, "main thread init error : %s\n", lua_tostring(L, -1));
		return 1;
	}

	lua_call(L, 0, 0);

	if( vlua_pcall_stacktrace(L, 0, 0) ) {
		fprintf(stderr, "run error : %s\n", lua_tostring(L, -1));
		return 4;
	}

	if( lua_getglobal(L, "main")==LUA_TFUNCTION ) {
		if( vlua_pcall_stacktrace(L, 0, 0) ) {
			fprintf(stderr, "run main error : %s\n", lua_tostring(L, -1));
			return 5;
		}
	}

	return 0;
}

static const char* vlua_args_parse(int* is_script_file, int argc, char** argv) {
	int script_arg = 0;
	int is_exec = 0;
	int i;

	for( i=argc-1; i>0; --i ) {
		if( argv[i][0] != '-' ) {
			script_arg = i;
		} else if( strcmp(argv[i], "-e")==0 || strcmp(argv[i], "--execute")==0 ) {
			if( (i+1) < argc ) {
				script_arg = i;
				is_exec = 1;
				break;
			}
		}
	}

	if( script_arg==0 )
		return NULL;

	for( i=1; i<argc; ++i ) {
		if( i==script_arg ) {
			if( is_exec ) {
				++i;
			}
			continue;
		}
		vlua_argv[vlua_argc++] = argv[i];
	}

	if( is_exec ) {
		*is_script_file = 0;
		return argv[script_arg + 1];
	}

	*is_script_file = 1;
	return argv[script_arg];
}

int main(int argc, char** argv) {
	int is_script_file = 0;
	const char* script = vlua_args_parse(&is_script_file, argc, argv);
	lua_State* L;
	int res = 0;

	if( !script )
		return show_help();

	vlua_self = argv[0];
	vlua_script = script;

	L = vlua_state_new();
	if( is_script_file ) {
		res = run(L, script);
	} else if( luaL_loadbuffer(L, script, strlen(script), "<-e>") ) {
		fprintf(stderr, "error : %s\n", lua_tostring(L, -1));
		res = 10;
	} else if( vlua_pcall_stacktrace(L, 0, 0) ) {
		fprintf(stderr, "run error : %s\n", lua_tostring(L, -1));
		res = 11;
	}
	lua_close(L);
	return res;
}

