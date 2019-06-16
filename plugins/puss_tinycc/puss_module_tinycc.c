// puss_socket.c

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <io.h>
#else
	#include <sys/types.h>
	#include <unistd.h>
	#include <fcntl.h>
#endif

#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>

#include "puss_plugin.h"

#include "lstate.h"
#include "puss_tinycc.inl"
#include "puss_tccdbg.inl"

#define PUSS_TCCLIB_NAME	"[PussTinyccLib]"

#ifdef _MSC_VER
#define open	_open
#define close	_close
#define lseek	_lseek
#define read	_read
#endif

typedef struct _MemFile {
	const char* cur;
	const char* end;
	char		buf[1];
} MemFile;

static MemFile** _tcc_file_arr = NULL;
static int _tcc_file_cap = 0;
static int _tcc_file_last = -1;
static lua_State* _tcc_current_L = NULL;

static void _tcc_cleanup(void) {
	MemFile** arr = _tcc_file_arr;
	int n = _tcc_file_last + 1;
	int i;
	_tcc_file_cap = 0;
	_tcc_file_last = -1;
	_tcc_file_arr = NULL;

	for( i=0; i<n; ++i ) {
		MemFile* fp = arr[i];
		free(fp);
	}
	free(arr);
}

static int _tcc_file_load(lua_State* L) {
	const char* fname = (const char*)lua_touserdata(L, 1);
	MemFile* fp = NULL;
	size_t len = 0;
	const char* str = NULL;
	if( lua_getfield(L, LUA_REGISTRYINDEX, PUSS_TCCLIB_NAME)!=LUA_TTABLE )
		return 0;
	if( lua_getfield(L, -1, "tcc_load_file")!=LUA_TFUNCTION )
		return 0;
	lua_pushstring(L, fname);
	lua_pushvalue(L, 2);
	lua_call(L, 2, 1);
	str = lua_isstring(L, -1) ? lua_tolstring(L, -1, &len) : NULL;
	if( str && (fp = (MemFile*)malloc(sizeof(MemFile) + len))!=NULL ) {
		fp->cur = fp->buf;
		fp->end = fp->buf + len;
		memcpy(fp->buf, str, len+1);
	}
	lua_pushlightuserdata(L, fp);
	return 1;
}

static int _tcc_file_open(const char* fname, int flags) {
	lua_State* L = _tcc_current_L;
	MemFile* fp = NULL;
	int fd = _tcc_file_last + 1;
	if( !L )
		return -1;

	while( fd >= _tcc_file_cap) {
		_tcc_file_cap *= 2;
		if( _tcc_file_cap < 32 )
			_tcc_file_cap = 32;
	}
	_tcc_file_arr = (MemFile**)realloc(_tcc_file_arr, sizeof(MemFile*) * _tcc_file_cap);

	lua_pushcfunction(L, _tcc_file_load);
	lua_pushlightuserdata(L, (void*)fname);
	lua_pushinteger(L, flags);
	if( lua_pcall(L, 2, 1, 0) ) {
		PUSS_TCC_TRACE_ERROR(lua_tostring(L, -1));
	} else {
		fp = lua_touserdata(L, -1);
	}
	lua_pop(L, 1);
	if( !fp )
		return -1;

	for( fd=0; fd<=_tcc_file_last; ++fd ) {
		if( _tcc_file_arr[fd]==NULL ) {
			_tcc_file_arr[fd] = fp;
			return fd;
		}
	}
	_tcc_file_arr[fd] = fp;
	_tcc_file_last = fd;
	return fd;
}

static int _tcc_file_close(int fd) {
	if( fd>=0 && fd <= _tcc_file_last ) {
		MemFile* fp = _tcc_file_arr[fd];
		_tcc_file_arr[fd] = NULL;
		free(fp);
		if( _tcc_file_last==fd )
			--_tcc_file_last;
	}
	return 0;
}

static long _tcc_file_lseek(int fd, long offset, int mode) {
	if( fd>=0 && fd <= _tcc_file_last ) {
		MemFile* fp = _tcc_file_arr[fd];
		if( fp ) {
			if( mode==SEEK_SET ) {
				fp->cur = fp->buf;
			} else if( mode==SEEK_END ) {
				fp->cur = fp->end;
			}
			fp->cur += offset;
			return (long)(fp->cur - fp->buf);
		}
	}
	return -1;
}

static int _tcc_file_read(int fd, void *buf, unsigned int len) {
	if( fd>=0 && fd <= _tcc_file_last ) {
		MemFile* fp = _tcc_file_arr[fd];
		if( fp && (fp->cur >= fp->buf) && (fp->cur < fp->end) ) {
			size_t sz = fp->end - fp->cur;
			if( sz > (size_t)len )
				sz = len;
			memcpy(buf, fp->cur, sz);
			fp->cur += sz;
			return (int)sz;
		}
	}
	return 0;
}

static TCCHook _tcc_file_hook = { _tcc_file_open, _tcc_file_close, _tcc_file_lseek, _tcc_file_read };

static int _tcc_load_module(lua_State* L) {
	PussTccLua* ud;
	int top, err;
	luaL_checktype(L, 1, LUA_TFUNCTION);

	ud = (PussTccLua*)lua_newuserdata(L, sizeof(PussTccLua));
	ud->s = NULL;
	if( luaL_newmetatable(L, PUSS_TCC_NAME) ) {
		luaL_setfuncs(L, puss_tcc_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	ud->s = tcc_new();
	tcc_set_error_func(ud->s, ud, puss_tcc_error);
	top = lua_gettop(L);
	if( top > 2 )
		lua_insert(L, 2);

	_tcc_current_L = L;
	err = lua_pcall(L, top-1, LUA_MULTRET, 0);
	_tcc_current_L = NULL;
	_tcc_cleanup();
	if( err )
		luaL_error(L, "puss tcc load module error: %s", lua_tostring(L, -1));
	return lua_gettop(L);
}

static int _tcc_load_file_default(lua_State* L) {
	const char* fname = luaL_checkstring(L, 1);
	int flags = (int)luaL_checkinteger(L, 2);
	int fd = open(fname, flags);
	int res = 0;
	void* buf;
	long len;
	if( fd < 0 )
		return 0;
	len = lseek(fd, 0, SEEK_END);
	if( len < 0 )
		return 0;
	buf = malloc((size_t)len);
	lseek(fd, 0, SEEK_SET);
	if( buf && (len == (long)read(fd, buf, (unsigned int)len)) ) {
		lua_pushlstring(L, (const char*)buf, (size_t)len);
		res = 1;
	}
	free(buf);
	close(fd);
	return res;
}

static const luaL_Reg tinycc_lib_methods[] =
	{ {"tcc_load_module", _tcc_load_module}
	, {"tcc_load_file", _tcc_load_file_default}
#ifdef _WIN32
	, {"debug_attach", _tcc_debug_attach}
#endif
	, {NULL, NULL}
	};

PussInterface* __puss_iface__ = NULL;

PUSS_PLUGIN_EXPORT int __puss_plugin_init__(lua_State* L, PussInterface* puss) {
	__puss_iface__ = puss;
	if( lua_getfield(L, LUA_REGISTRYINDEX, PUSS_TCCLIB_NAME)==LUA_TTABLE ) {
		return 1;
	}
	lua_pop(L, 1);

	lua_newtable(L);
	lua_pushvalue(L, -1);
	lua_setfield(L, LUA_REGISTRYINDEX, PUSS_TCCLIB_NAME);

	tcc_setup_hook(&_tcc_file_hook);
	puss_tcc_reg_signals();

	luaL_setfuncs(L, tinycc_lib_methods, 0);

	return 1;
}

