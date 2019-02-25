// simple_pickle.c

#include "puss_toolkit.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

#define PUSS_KEY_PICKLE_CACHE	"PussSimplePickleCache"

#define TNIL		0
#define TTRUE		1
#define TFALSE		2
#define TINT		3
#define TNUM		4
#define TSTR		5
#define TTABLE		6
#define TLIGHTUD	7
#define TMAXCOUNT	8

#define PICKLE_DEPTH_MAX	32
#define PICKLE_ARG_MAX		32

typedef struct _PickleBuffer {
	unsigned char*	buf;
	size_t			len;
	size_t			free;
} PickleBuffer;

static inline unsigned char* pickle_mem_prepare(lua_State* L, PickleBuffer* mb, size_t len) {
	if( mb->free < len ) {
		size_t new_size = (mb->len + len) * 2;
		mb->buf = (unsigned char*)realloc(mb->buf, new_size);
		mb->free = new_size - mb->len;
	}
	return mb->buf + mb->len;
}

typedef struct _LPacker {
	PickleBuffer	mb;		// MUST first
	lua_State*		L;
	int				depth;
} LPacker;

typedef struct _LUnPacker {
	lua_State*		L;
	const char*		start;
	const char*		end;
	int				depth;
} LUnPacker;

static LPacker* _pickle_reset(LPacker* pac, lua_State* L) {
	pac->mb.free += pac->mb.len;
	pac->mb.len = 0;
	pac->L = L;
	pac->depth = 1;
	return pac;
}

static inline void _pickle_addbuf(lua_State* L, LPacker* pac, const void* buf, size_t len) {
	PickleBuffer* mb = &(pac->mb);
	unsigned char* pos = pickle_mem_prepare(L, mb, len);
	if( buf )
	    memcpy(pos, buf, len);
    mb->len += len;
    mb->free -= len;
}

static inline void _pickle_add(lua_State* L, LPacker* pac, unsigned char tp, const void* buf, size_t len) {
	PickleBuffer* mb = &(pac->mb);
	size_t sz = 1 + len;
	unsigned char* pos = pickle_mem_prepare(L, mb, sz);
	*pos++ = tp;
	if( buf )
	    memcpy(pos, buf, len);
    mb->len += sz;
    mb->free -= sz;
}

static int _pickle_mem_buffer_gc(lua_State* L) {
	LPacker* pac = (LPacker*)lua_touserdata(L, 1);
	if( pac ) {
		_pickle_reset(pac, L);
		free(pac->mb.buf);
		memset(pac, 0, sizeof(LPacker));
	}
	return 0;
}

static void _pack(LPacker* pac, int idx) {
	lua_State* L = pac->L;
	int top;
	int64_t q;
	uint64_t Q;
	double d;
	uint32_t sz;

	switch( lua_type(L, idx) ) {
	case LUA_TBOOLEAN:
		_pickle_add(L, pac, lua_toboolean(L, idx) ? TTRUE : TFALSE, NULL, 0);
		break;
	case LUA_TNUMBER:
		if( lua_isinteger(L, idx) ) {
			q = (int64_t)lua_tointeger(L, idx);
			_pickle_add(L, pac, TINT, &q, 8);
		} else {
			d = (double)lua_tonumber(L, idx);
			_pickle_add(L, pac, TNUM, &d, 8);
		}
		break;
	case LUA_TSTRING:
		{
			size_t len = 0;
			const char* str = lua_tolstring(L, idx, &len);
			sz = (uint32_t)len;
			_pickle_add(L, pac, TSTR, &sz, 4);
			_pickle_addbuf(L, pac, str, len);
		}
		break;
	case LUA_TTABLE:
		pac->depth++;
		if( pac->depth > PICKLE_DEPTH_MAX )
			luaL_error(L, "pack table too depth!");
		_pickle_add(L, pac, TTABLE, NULL, 0);
		top = lua_gettop(L);
		lua_pushnil(L);
		while( lua_next(L, idx) ) {
			_pack(pac, top+1);
			_pack(pac, top+2);
			lua_settop(L, top+1);
		}
		_pickle_add(L, pac, TNIL, NULL, 0);
		pac->depth--;
		break;
	case LUA_TLIGHTUSERDATA:
		Q = (uint64_t)lua_touserdata(L, idx);
		_pickle_add(L, pac, TLIGHTUD, &Q, 8);
		break;
	default:
		_pickle_add(L, pac, TNIL, NULL, 0);
		break;
	}
}

static const size_t _pickle_type_size[TMAXCOUNT] =
	{ 0	// TNIL
	, 0	// TTRUE
	, 0	// TFALSE
	, 0	// TINT
	, 8	// TNUM
	, 4	// TSTR
	, 0	// TTABLE
	, 8	// TLIGHTUD
	};

static const char* _unpack(LUnPacker* upac, const char* pkt) {
	lua_State* L = upac->L;
	const char* end = upac->end;
	uint8_t tp;
	int64_t q;
	uint64_t Q;
	double d;
	uint32_t sz;

	if( pkt >= end )
		luaL_error(L, "pkt bad size when unpack type");
	tp = (uint8_t)*pkt++;
	if( tp >= TMAXCOUNT )
		luaL_error(L, "pkt bad type(%d)", (int)tp);
	if( (pkt + _pickle_type_size[tp]) > end )
		luaL_error(L, "pkt bad size when unpack");
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
		memcpy(&q, pkt, 8);
		pkt += 8;
		lua_pushinteger(L, (lua_Integer)q);
		break;
	case TNUM:
		memcpy(&d, pkt, 8);
		pkt += 8;
		lua_pushnumber(L, (lua_Number)d);
		break;
	case TSTR:
		memcpy(&sz, pkt, 4);
		pkt += 4;
		if( (pkt + sz) > upac->end )
			luaL_error(upac->L, "pkt bad size when unpack str");
		lua_pushlstring(upac->L, pkt, sz);
		pkt += sz;
		break;
	case TTABLE:
		upac->depth++;
		if( upac->depth > PICKLE_DEPTH_MAX )
			luaL_error(L, "unpack table too depth!");
		lua_newtable(L);
		pkt = _unpack(upac, pkt);
		while( !lua_isnil(L, -1) ) {
			pkt = _unpack(upac, pkt);
			lua_rawset(L, -3);
			pkt = _unpack(upac, pkt);
		}
		lua_pop(L, 1);
		upac->depth--;
		break;
	case TLIGHTUD:
		memcpy(&Q, pkt, 8);
		pkt += 8;
		lua_pushlightuserdata(L, (void*)Q);
		break;
	default:
		assert( 0 && "bad logic" );
		break;
	}

	return pkt;
}

static void _lua_pickle_pack(LPacker* pac, int start, int end) {
	lua_State* L = pac->L;
	int i, n;
	assert( start>=0 && end>=0 );
	if( start > end )
		luaL_error(L, "start > end");
	n = end - start + 1;
	if( n > PICKLE_ARG_MAX )
		luaL_error(L, "arg count MUST <= %d", PICKLE_ARG_MAX);
	luaL_checkstack(L, (8 + 2*PICKLE_DEPTH_MAX), "pickle pack check stack failed!");	// luaL_Buffer, error, every depth(k, v) & keep free 6 space
	_pickle_addbuf(L, pac, NULL, 1);	// skip n
	for( i=start; i<=end; ++i ) {
		_pack(pac, i);
	}
	pac->mb.buf[0] = (unsigned char)n;
}

static LPacker* packer_ensure(lua_State* L) {
	LPacker* pac;
	if( lua_getfield(L, LUA_REGISTRYINDEX, PUSS_KEY_PICKLE_CACHE)==LUA_TUSERDATA ) {
		pac = (LPacker*)lua_touserdata(L, -1);
	} else {
		lua_pop(L, 1);
		pac = (LPacker*)lua_newuserdata(L, sizeof(LPacker));
		memset(pac, 0, sizeof(LPacker));
		lua_newtable(L);
		lua_pushcfunction(L, _pickle_mem_buffer_gc);
		lua_setfield(L, -2, "__gc");
		lua_setmetatable(L, -2);
		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, PUSS_KEY_PICKLE_CACHE);
	}
	_pickle_reset(pac, L);
	lua_pop(L, 1);
	return pac;
}

void* puss_simple_pack(size_t* plen, lua_State* L, int start, int end) {
	LPacker* pac = packer_ensure(L);
	start = lua_absindex(L, start);
	end = lua_absindex(L, end);
	assert( pac );

	if( start <= end )
		_lua_pickle_pack(pac, start, end);

	*plen = pac->mb.len;
	return pac->mb.buf;
}

static int _pickle_unpack(LUnPacker* upac) {
	lua_State* L = upac->L;
	const char* ps = (const char*)upac->start;
	int i, n;
	if( ps>=upac->end )
		return 0;

	if( (ps + 1) >= upac->end )
		luaL_error(L, "pkt bad format, empty");

	n = *ps++;
	if( n<0 && n>PICKLE_ARG_MAX )
		luaL_error(L, "pkt bad format, narg out of range");
	luaL_checkstack(L, (n + 1 + (2*PICKLE_DEPTH_MAX + 8)), "pickle unpack check stack failed!");	// narg + refs + every depth(last value) & keep free 8 space

	for( i=0; i<n; ++i )
		ps = _unpack(upac, ps);
	if( ps!=upac->end )
		luaL_error(L, "pkt length not match");

	return n;
}

int puss_simple_unpack(lua_State* L, const void* pkt, size_t len) {
	LUnPacker upac;
	upac.L = L;
	upac.start = (const char*)pkt;
	upac.end = upac.start + len;
	upac.depth = 1;
	return _pickle_unpack(&upac);
}

static int puss_lua_simple_pack(lua_State* L) {
	size_t len = 0;
	void* buf = puss_simple_pack(&len, L, 1, -1);
	lua_pushlstring(L, (const char*)buf, len);
	return 1;
}

static int puss_lua_simple_unpack(lua_State* L) {
	LUnPacker upac;
	size_t len = 0;
	upac.L = L;
	upac.start = "";
	upac.end = upac.start;
	upac.depth = 1;

	if( lua_type(L, 1)==LUA_TSTRING ) {
		upac.start = lua_tolstring(L, 1, &len);
		upac.end = upac.start + len;
	}

	return _pickle_unpack(&upac);
}

static luaL_Reg simple_pickle_methods[] =
	{ {"simple_pack", puss_lua_simple_pack}
	, {"simple_unpack", puss_lua_simple_unpack}
	, {NULL, NULL}
	};

int puss_reg_simple_pickle(lua_State* L) {
	luaL_setfuncs(L, simple_pickle_methods, 0);
	return 0;
}
