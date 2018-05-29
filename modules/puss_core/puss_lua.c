// puss_lua.c

const char builtin_scripts[] = "-- puss_builtin.lua\n\n\n"
	"local function puss_loadfile(name, env)\n"
	"	local f, err = loadfile(name, 'bt', env or _ENV)\n"
	"	if not f then return f, string.format('load script(%s) failed: %s', name, err) end\n"
	"	return f\n"
	"end\n"
	"\n"
	"local function puss_dofile(name, env, ...)\n"
	"	local f, err = puss_loadfile(name, env)\n"
	"	if not f then error(err) end\n"
	"	return f(...)\n"
	"end\n"
	"\n"
	"local function puss_dostring(script, name, env, ...)\n"
	"	local f, err = load(script, name, 'bt', env or _ENV)\n"
	"	if f==nil then error(err) end\n"
	"	return f(...)\n"
	"end\n"
	"\n"
	"puss.loadfile = puss_loadfile\n"
	"puss.dofile = puss_dofile\n"
	"puss.dostring = puss_dostring\n"
	"\n"
	"local logerr = function(err) print(debug.traceback(err,2)); return err; end\n"
	"puss.logerr_handle = function(h) if h then logerr=h end; return logerr end\n"
	"puss.trace_pcall = function(f, ...) return xpcall(f, logerr, ...) end\n"
	"puss.trace_dofile = function(name, env, ...) return xpcall(puss_dofile, logerr, name, env, ...) end\n"
	"puss.trace_dostring = function(script, name, env, ...) return xpcall(puss_dostring, logerr, script, name, env, ...) end\n"
	"\n"
	;

#define _PUSS_MODULE_IMPLEMENT
#include "puss_module.h"
#include "puss_lua.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>

#ifdef _WIN32
	#include <windows.h>
	#define PATH_SEP_STR	"\\"
#else
	#include <unistd.h>
	#define PATH_SEP_STR	"/"
#endif

#include "puss_debug.inl"

#define PUSS_NAMESPACE(name)			"[" #name "]"

#define PUSS_NAMESPACE_PUSS				PUSS_NAMESPACE(puss)
#define PUSS_NAMESPACE_MODULES_LOADED	PUSS_NAMESPACE(modules)
#define PUSS_NAMESPACE_INTERFACES		PUSS_NAMESPACE(interfaces)
#define PUSS_NAMESPACE_APP_PATH			PUSS_NAMESPACE(app_path)
#define PUSS_NAMESPACE_APP_NAME			PUSS_NAMESPACE(app_name)
#define PUSS_NAMESPACE_MODULE_SUFFIX	PUSS_NAMESPACE(module_suffix)
#define PUSS_NAMESPACE_PICKLE_CACHE		PUSS_NAMESPACE(pickle_cache)

int puss_get_value(lua_State* L, const char* name) {
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

static const char* lua_getfield_str(lua_State* L, const char* ns, const char* def) {
	const char* str = def;
	if( lua_getfield(L, LUA_REGISTRYINDEX, ns)==LUA_TSTRING ) {
		str = lua_tostring(L, -1);
	}
	lua_pop(L, 1);
	return str;
}

static int module_init_wrapper(lua_State* L);

static void puss_module_require(lua_State* L, const char* name) {
	if( !name ) {
		luaL_error(L, "puss_module_require, module name MUST exist!");
	}
	lua_getfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_MODULES_LOADED);
	if( lua_getfield(L, -1, name)==LUA_TNIL ) {
		lua_pop(L, 1);
		lua_pushstring(L, name);
		lua_pushcclosure(L, module_init_wrapper, 1);
		lua_call(L, 0, 1);
	}
}

static void puss_interface_register(lua_State* L, const char* name, void* iface) {
	int top = lua_gettop(L);
	if( !(name && iface) ) {
		luaL_error(L, "puss_interface_register(%s), name and iface MUST exist!", name);
	}
	lua_getfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_INTERFACES);
	if( lua_getfield(L, -1, name)==LUA_TNIL ) {
		lua_pop(L, 1);
		lua_pushlightuserdata(L, iface);
		lua_setfield(L, -2, name);
	} else if( lua_touserdata(L, -1)!=iface ) {
		luaL_error(L, "puss_interface_register(%s), iface already exist with different address!", name);
	}
	lua_settop(L, top);
}

static void* puss_interface_check(lua_State* L, const char* name) {
	void* iface = NULL;
	if( name ) {
		lua_getfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_INTERFACES);
		lua_getfield(L, -1, name);
		iface = lua_touserdata(L, -1);
		lua_pop(L, 2);
	}
	if( !iface ) {
		luaL_error(L, "puss_interface_check(%s), iface not found!", name);
	}
	return iface;
}

static void puss_push_const_table(lua_State* L) {
#ifdef LUA_USE_OPTIMIZATION_WITH_CONST
	if( lua_getfield(L, LUA_REGISTRYINDEX, LUA_USE_OPTIMIZATION_WITH_CONST)!=LUA_TTABLE ) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setfield(L, LUA_REGISTRYINDEX, LUA_USE_OPTIMIZATION_WITH_CONST);
	}
#else
	lua_pushglobaltable(L);
#endif
}

static const char* puss_app_path(lua_State* L) {
	return lua_getfield_str(L, PUSS_NAMESPACE_APP_PATH, ".");
}

// pickle

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

// pickle mem buffer

typedef struct _PickleMemBuffer {
	unsigned char*	buf;
	size_t			len;
	size_t			free;
} PickleMemBuffer;

static inline unsigned char* pickle_mem_prepare(lua_State* L, PickleMemBuffer* mb, size_t len) {
	if( mb->free < len ) {
		size_t new_size = (mb->len + len) * 2;
		mb->buf = (unsigned char*)realloc(mb->buf, new_size);
		mb->free = new_size - mb->len;
	}
	return mb->buf + mb->len;
}

typedef struct _LPacker {
	PickleMemBuffer	mb;		// MUST first
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
		assert( FALSE && "bad logic" );
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

void* puss_pickle_pack(size_t* plen, lua_State* L, int start, int end) {
	LPacker* pac;
	start = lua_absindex(L, start);
	end = lua_absindex(L, end);
	if( lua_getfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_PICKLE_CACHE)==LUA_TUSERDATA ) {
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
		lua_setfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_PICKLE_CACHE);
	}
	_pickle_reset(pac, L);
	lua_pop(L, 1);
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

int puss_pickle_unpack(lua_State* L, const void* pkt, size_t len) {
	LUnPacker upac;
	upac.L = L;
	upac.start = (const char*)pkt;
	upac.end = upac.start + len;
	upac.depth = 1;
	return _pickle_unpack(&upac);
}

typedef struct _FileStr {
	size_t n;
	char* s;
} FileStr;

#ifdef _WIN32
	#define is_sep(ch) ((ch)=='/' || (ch)=='\\')
#else
	#define is_sep(ch) ((ch)=='/')
#endif

static inline char* skip_seps(char* s) {
	while( is_sep(*s) ) {
		++s;
	}
	return s;
}

static inline char* copy_filestr(char* dst, char* src, size_t n) {
	if( dst != src ) {
		size_t i;
		for( i=0; i<n; ++i ) {
			dst[i] = src[i];
		}
	}
	return dst + n;
}

size_t puss_filename_format(char* fname) {
	FileStr strs[258];
	FileStr* start = strs;
	FileStr* cur = strs;
	FileStr* end = cur + 258;
	char* s = fname;

	// root
#ifdef _WIN32
	if( ((s[0]>='a' && s[1]<='z') || (s[0]>='A' && s[1]<='Z')) && (s[1]==':') ) {
		cur->s = s;
		cur->n = 2;
		s += 2;
		if( is_sep(*s) ) {
			s[0] = '\\';
			++s;
			cur->n++;
		}
		start = ++cur;
	} else if( is_sep(s[0]) && is_sep(s[1]) ) {
		cur->s = s;
		cur->n = 2;
		s[0] = '\\';
		s[1] = '\\';
		s += 2;
		start = ++cur;
	}
#else
	if( is_sep(*s) ) {
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

	end = cur;
	s = fname;
	if( start!=strs ) {	// root C: or C:\ or \\ or / 
		s = copy_filestr(s, strs[0].s, strs[0].n);
	}
	if( start < end ) {
		s = copy_filestr(s, start->s, start->n);
		for( cur=start+1; cur < end; ++cur) {
			s = copy_filestr(s, PATH_SEP_STR, 1);
			s = copy_filestr(s, cur->s, cur->n);
		}
	}
	*s = '\0';
	return (size_t)(s - fname);
}

static PussInterface puss_iface =
	{ puss_module_require
	, puss_interface_register
	, puss_interface_check
	, puss_push_const_table
	, puss_pickle_pack
	, puss_pickle_unpack
	, puss_app_path
	, puss_get_value
	, puss_filename_format
	};

static int module_init_wrapper(lua_State* L) {
	const char* name = (const char*)lua_tostring(L, lua_upvalueindex(1));
	const char* app_path = lua_getfield_str(L, PUSS_NAMESPACE_APP_PATH, ".");
	const char* module_suffix = lua_getfield_str(L, PUSS_NAMESPACE_MODULE_SUFFIX, ".so");
	PussModuleInit f = NULL;
	assert( lua_gettop(L)==0 );

	// local f, err = package.loadlib(module_filename, '__puss_module_init__')
	// 
	puss_get_value(L, "package.loadlib");
	{
		luaL_Buffer B;
		luaL_buffinit(L, &B);
		luaL_addstring(&B, app_path);
		luaL_addstring(&B, PATH_SEP_STR "modules" PATH_SEP_STR);
		luaL_addstring(&B, name);
		luaL_addstring(&B, module_suffix);
		luaL_pushresult(&B);
	}
	lua_pushstring(L, "__puss_module_init__");
	lua_call(L, 2, 2);
	if( lua_type(L, -2)!=LUA_TFUNCTION )
		lua_error(L);
	f = (PussModuleInit)lua_tocfunction(L, -2);
	if( !f )
		luaL_error(L, "load module fetch init function failed!");
	lua_pop(L, 2);
	assert( lua_gettop(L)==0 );

	// __puss_module_init__()
	// puss_module_loaded[name] = <last return value> or true
	// 
	if( (f(L, &puss_iface)<=0) || lua_isnoneornil(L, -1) ) {
		lua_settop(L, 0);
		lua_pushboolean(L, 1);
	}

	lua_getfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_MODULES_LOADED);
	lua_pushvalue(L, -2);
	lua_setfield(L, -2, name);
	lua_pop(L, 1);

	return 1;
}

static int puss_lua_module_require(lua_State* L) {
	const char* name = luaL_checkstring(L, 1);
	lua_getfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_MODULES_LOADED);
	if( lua_getfield(L, -1, name) != LUA_TNIL )
		return 1;
	lua_settop(L, 1);
	lua_pushcclosure(L, module_init_wrapper, 1);
	lua_call(L, 0, 1);
	return 1;
}

static int puss_lua_pickle_pack(lua_State* L) {
	size_t len = 0;
	void* buf = puss_pickle_pack(&len, L, 1, -1);
	lua_pushlstring(L, (const char*)buf, len);
	return 1;
}

static int puss_lua_pickle_unpack(lua_State* L) {
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

static int puss_lua_filename_format(lua_State* L) {
	size_t n = 0;
	const char* s = luaL_checklstring(L, 1, &n);
	luaL_Buffer B;
	luaL_buffinitsize(L, &B, n+1);
	memcpy(B.b, s, n+1);
	n = puss_filename_format(B.b);
	luaL_pushresultsize(&B, n);
	return 1;
}

#ifdef _WIN32
	static int _lua_mbcs2wch(lua_State* L, int stridx, UINT code_page, const char* code_page_name) {
		size_t len = 0;
		const char* str = luaL_checklstring(L, stridx, &len);
		int wlen = MultiByteToWideChar(code_page, 0, str, (int)len, NULL, 0);
		if( wlen > 0 ) {
			luaL_Buffer B;
			wchar_t* wstr = (wchar_t*)luaL_buffinitsize(L, &B, (size_t)(wlen<<1));
			MultiByteToWideChar(code_page, 0, str, (int)len, wstr, wlen);
			luaL_addsize(&B, (size_t)(wlen<<1));
			luaL_pushresult(&B);
		} else if( wlen==0 ) {
			lua_pushliteral(L, "");
		} else {
			luaL_error(L, "%s to utf16 convert failed!", code_page_name);
		}
		return 1;
	}

	static int _lua_wch2mbcs(lua_State* L, int stridx, UINT code_page, const char* code_page_name) {
		size_t wlen = 0;
		const wchar_t* wstr = (const wchar_t*)luaL_checklstring(L, stridx, &wlen);
		int len;
		wlen >>= 1;
		len = WideCharToMultiByte(code_page, 0, wstr, (int)wlen, NULL, 0, NULL, NULL);
		if( len > 0 ) {
			luaL_Buffer B;
			char* str = luaL_buffinitsize(L, &B, (size_t)len);
			WideCharToMultiByte(code_page, 0, wstr, (int)wlen, str, len, NULL, NULL);
			luaL_addsize(&B, len);
			luaL_pushresult(&B);
		} else if( len==0 ) {
			lua_pushliteral(L, "");
		} else {
			luaL_error(L, "utf16 to %s convert failed!", code_page_name);
		}
		return 1;
	}

	static int puss_lua_local_to_utf8(lua_State* L) {
		_lua_mbcs2wch(L, 1, 0, "utf8");
		return _lua_wch2mbcs(L, -1, CP_UTF8, "local");
	}

	static int puss_lua_utf8_to_local(lua_State* L) {
		_lua_mbcs2wch(L, 1, CP_UTF8, "utf8");
		return _lua_wch2mbcs(L, -1, 0, "local");
	}
#else
	static int puss_lua_local_to_utf8(lua_State* L) {
		// TODO if need, now locale==UTF8 
		luaL_checkstring(L, 1);
		lua_settop(L, 1);
		return 1;
	}

	static int puss_lua_utf8_to_locale(lua_State* L) {
		// TODO if need, now locale==UTF8 
		luaL_checkstring(L, 1);
		lua_settop(L, 1);
		return 1;
	}
#endif//_WIN32

static luaL_Reg puss_methods[] =
	{ {"require",			puss_lua_module_require}
	, {"pickle_pack",		puss_lua_pickle_pack}
	, {"pickle_unpack",		puss_lua_pickle_unpack}
	, {"filename_format",	puss_lua_filename_format}
	, {"local_to_utf8",		puss_lua_local_to_utf8}
	, {"utf8_to_local",		puss_lua_utf8_to_local}
	, {NULL, NULL}
	};

static void puss_module_setup(lua_State* L, const char* app_path, const char* app_name, const char* module_suffix) {
	// fprintf(stderr, "!!!puss_module_setup %s %s %s\n", app_path, app_name, module_suffix);

	lua_getfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_PUSS);
	assert( lua_type(L, -1)==LUA_TTABLE );

	puss_push_const_table(L);
	lua_setfield(L, -2, "_consts");	// puss._consts

	lua_pushstring(L, app_path ? app_path : ".");
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_APP_PATH);
	lua_setfield(L, -2, "_path");	// puss._path

	lua_pushstring(L, app_name ? app_name : "puss");
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_APP_NAME);
	lua_setfield(L, -2, "_self");	// puss._self

	lua_pushstring(L, module_suffix ? module_suffix : ".so");
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_MODULE_SUFFIX);
	lua_setfield(L, -2, "_module_suffix");	// puss._module_suffix

	lua_pushstring(L, PATH_SEP_STR);
	lua_setfield(L, -2, "_sep");	// puss._sep
}

static void *_default_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud; (void)osize;  /* not used */
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  else
    return realloc(ptr, nsize);
}

static int _default_panic (lua_State *L) {
  lua_writestringerror("PANIC: unprotected error in call to Lua API (%s)\n",
                        lua_tostring(L, -1));
  return 0;  /* return to Lua to abort */
}

lua_State* puss_lua_newstate(int debug, lua_Alloc f, void* ud) {
	DebugEnv* env = NULL;
	lua_State* L;
	if( debug ) {
		env = debug_env_new(f ? f : _default_alloc, ud);
		if( !env ) return NULL;
		ud = env;
		f = _debug_alloc;
	} else {
		f = f ? f : _default_alloc;
	}
	L = lua_newstate(f, ud);
	if( L ) {
		lua_atpanic(L, &_default_panic);
	}
	if( env ) {
		env->main_state = L;
	}
	return L;
}

static void puss_lua_init(lua_State* L, const char* app_path, const char* app_name, const char* module_suffix) {
	// puss namespace init
	lua_newtable(L);	// puss
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_PUSS);
	puss_module_setup(L, app_path, app_name, module_suffix);

	// puss modules["puss"] = puss-module
	lua_newtable(L);
	{
		lua_pushvalue(L, -2);
		lua_setfield(L, -2, "puss");
	}
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_MODULES_LOADED);

	// puss interfaces["PussInterface"] = puss_iface
	lua_newtable(L);
	{
		lua_pushlightuserdata(L, &puss_iface);
		lua_setfield(L, -2, "PussInterface");
	}
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_INTERFACES);

	// set to _G.puss
	lua_pushvalue(L, -1);
	lua_setglobal(L, "puss");

	// puss.NULL
	lua_pushlightuserdata(L, NULL);
	lua_setfield(L, -2, "NULL");

	luaL_setfuncs(L, puss_methods, 0);

	// setup builtins
	if( luaL_loadbuffer(L, builtin_scripts, sizeof(builtin_scripts)-1, "@puss_builtin.lua") ) {
		lua_error(L);
	}
	lua_call(L, 0, 0);
}

static int _lua_proxy_inited = 0;

void puss_lua_open(lua_State* L, const char* app_path, const char* app_name, const char* module_suffix) {
	DebugEnv* env;

	// check already open
	if( lua_getfield(L, LUA_REGISTRYINDEX, PUSS_NAMESPACE_PUSS)==LUA_TTABLE ) {
		lua_setglobal(L, "puss");
		return;
	}
	lua_pop(L, 1);

	if( !_lua_proxy_inited ) {
		_lua_proxy_inited = 1;
	#define __LUAPROXY_SYMBOL(sym)	puss_iface.luaproxy.sym = sym;
		#include "luaproxy.symbols"
	#undef __LUAPROXY_SYMBOL
	}

	puss_lua_init(L, app_path, app_name, module_suffix);

	// debugger
	env = debug_env_fetch(L);
	if( env ) {
		puss_lua_init(env->debug_state, app_path, app_name, module_suffix);

		lua_pushcfunction(L, lua_debugger_run);
		lua_setfield(L, 1, "debug");	// puss.debug
	}

	lua_pop(L, 1);
}

void puss_lua_open_default(lua_State* L, const char* arg0, const char* module_suffix) {
	char pth[4096];
	int len;
	const char* app_path = NULL;
	const char* app_name = NULL;
#ifdef _WIN32
	len = (int)GetModuleFileNameA(0, pth, 4096);
#else
	len = readlink("/proc/self/exe", pth, 4096);
#endif
	if( len > 0 ) {
		pth[len] = '\0';
	} else {
		// try use argv[0]
		len = (int)strlen(arg0); 
		strcpy(pth, arg0);
	}

	for( --len; len>0; --len ) {
		if( pth[len]=='\\' || pth[len]=='/' ) {
			pth[len] = '\0';
			break;
		}
	}

	if( len > 0 ) {
		app_path = pth;
		app_name = pth+len+1;
	}

	puss_lua_open(L, app_path, app_name, module_suffix);
}

