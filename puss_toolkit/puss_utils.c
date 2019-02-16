// conv_utils.c

#include "puss_toolkit.h"

#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <wchar.h>

	static int _lua_mbcs2wch(lua_State* L, int stridx, UINT code_page, const char* code_page_name) {
		size_t len = 0;
		const char* str = luaL_checklstring(L, stridx, &len);
		int wlen = MultiByteToWideChar(code_page, 0, str, (int)len, NULL, 0);
		if( wlen > 0 ) {
			luaL_Buffer B;
			wchar_t* wstr = (wchar_t*)luaL_buffinitsize(L, &B, (size_t)(wlen<<1));
			MultiByteToWideChar(code_page, 0, str, (int)len, wstr, wlen);
			luaL_addsize(&B, (size_t)(wlen<<1));
			luaL_addchar(&B, '\0');	// add more one byte for \u0000
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
#else
	#include <unistd.h>
	#include <dirent.h>
	#include <sys/time.h>
#endif

typedef struct _FStr {
	size_t n;
	char* s;
} FStr;

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

static inline char* copy_str(char* dst, char* src, size_t n) {
	if( dst != src ) {
		size_t i;
		for( i=0; i<n; ++i ) {
			dst[i] = src[i];
		}
	}
	return dst + n;
}

size_t puss_format_filename(char* fname) {
	FStr strs[258];
	FStr* start = strs;
	FStr* cur = strs;
	FStr* end = cur + 258;
	char* s = fname;
	char sep = '/';
#ifdef _WIN32
	if( ((s[0]>='a' && s[1]<='z') || (s[0]>='A' && s[1]<='Z')) && (s[1]==':') ) {
		cur->s = s;
		cur->n = 2;
		s += 2;
		if( is_sep(*s) ) {
			s[0] = sep;
			++s;
			cur->n++;
		}
		start = ++cur;
	} else if( is_sep(s[0]) && is_sep(s[1]) ) {
		cur->s = s;
		cur->n = 2;
		s[0] = sep;
		s[1] = sep;
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
			FStr* prev = cur - 1;
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
			FStr* prev = cur - 1;
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
	if( start!=strs ) {	// root C: or C:/ or // or / 
		s = copy_str(s, strs[0].s, strs[0].n);
	}
	if( start < end ) {
		s = copy_str(s, start->s, start->n);
		for( cur=start+1; cur < end; ++cur) {
			*s++ = sep;
			s = copy_str(s, cur->s, cur->n);
		}
	}
	*s = '\0';
	return (size_t)(s - fname);
}

static int puss_lua_filename_format(lua_State* L) {
	size_t n = 0;
	const char* s = luaL_checklstring(L, 1, &n);
	luaL_Buffer B;
	luaL_buffinitsize(L, &B, n+1);
	memcpy(B.b, s, n+1);
	n = puss_format_filename(B.b);
	luaL_pushresultsize(&B, n);
	return 1;
}

#ifdef _WIN32
	static int _lua_utf8_to_utf16(lua_State* L) { return _lua_mbcs2wch(L, 1, CP_UTF8, "utf8"); }
	static int _lua_utf16_to_utf8(lua_State* L) { return _lua_wch2mbcs(L, 1, CP_UTF8, "utf8"); }
	static int _lua_local_to_utf16(lua_State* L) { return _lua_mbcs2wch(L, 1, 0, "local"); }
	static int _lua_utf16_to_local(lua_State* L) { return _lua_wch2mbcs(L, 1, 0, "local"); }

	static int puss_lua_file_list(lua_State* L) {
		BOOL utf8 = lua_toboolean(L, 2);
		const WCHAR* wpath;
		lua_Integer nfile = 0;
		lua_Integer ndir = 0;
		HANDLE h = INVALID_HANDLE_VALUE;
		WIN32_FIND_DATAW fdata;

		// replace arg1 with(append \\*.* & convert to utf16)
		lua_pushcfunction(L, utf8 ? _lua_utf8_to_utf16 : _lua_local_to_utf16);
		lua_pushvalue(L, 1);
		lua_pushstring(L, "\\*.*");
		lua_concat(L, 2);
		lua_call(L, 1, 1);
		lua_replace(L, 1);
		wpath = (const WCHAR*)luaL_checkstring(L, 1);

		lua_newtable(L);	// files
		lua_newtable(L);	// dirs

		h = FindFirstFileW(wpath, &fdata);
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

			lua_pushcfunction(L, utf8 ? _lua_utf16_to_utf8 : _lua_utf16_to_local);
			lua_pushlstring(L, (const char*)(fdata.cFileName), wcslen(fdata.cFileName)*2 + 1);	// add more one byte for \u0000
			lua_call(L, 1, 1);

			if( fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) {
				lua_rawseti(L, -2, ++ndir);
			} else {
				lua_rawseti(L, -3, ++nfile);
			}

		next_label:
			if( !FindNextFileW(h, &fdata) )
				break;
		}
		FindClose(h);
		return 2;
	}

	static int puss_lua_local_to_utf8(lua_State* L) {
		_lua_mbcs2wch(L, 1, 0, "local");
		return _lua_wch2mbcs(L, -1, CP_UTF8, "utf8");
	}

	static int puss_lua_utf8_to_local(lua_State* L) {
		_lua_mbcs2wch(L, 1, CP_UTF8, "utf8");
		return _lua_wch2mbcs(L, -1, 0, "local");
	}
#else
	static int puss_lua_file_list(lua_State* L) {
		const char* dirpath = luaL_checkstring(L, 1);
		lua_Integer nfile = 0;
		lua_Integer ndir = 0;
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
		return 2;
	}

	static int puss_lua_local_to_utf8(lua_State* L) {
		// TODO if need, now locale==UTF8 
		luaL_checkstring(L, 1);
		lua_settop(L, 1);
		return 1;
	}

	static int puss_lua_utf8_to_local(lua_State* L) {
		// TODO if need, now locale==UTF8 
		luaL_checkstring(L, 1);
		lua_settop(L, 1);
		return 1;
	}
#endif

int puss_get_value(lua_State* L, const char* path) {
	int top = lua_gettop(L);
	const char* ps = path;
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

static int puss_lua_timestamp(lua_State* L) {
#ifdef _WIN32
	#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
		#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
	#else
		#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
	#endif
	FILETIME ft;
	unsigned __int64 ret = 0;
	GetSystemTimeAsFileTime(&ft);
	ret |= ft.dwHighDateTime;
	ret <<= 32;
	ret |= ft.dwLowDateTime;

	// converting file time to unix epoch
	ret -= DELTA_EPOCH_IN_MICROSECS;
	ret /= 10;  // convert into microseconds
	lua_pushinteger(L, (lua_Integer)(ret / 1000));
	lua_pushinteger(L, (lua_Integer)(ret / 1000000UL));
	lua_pushinteger(L, (lua_Integer)(ret % 1000000UL));
#else
	struct timeval tv;
	lua_Unsigned ret;
	gettimeofday(&tv, 0);
	ret = tv.tv_sec;
	ret *= 1000;
	ret += (tv.tv_usec / 1000);
	lua_pushinteger(L, (lua_Integer)ret);
	lua_pushinteger(L, (lua_Integer)tv.tv_sec);
	lua_pushinteger(L, (lua_Integer)tv.tv_usec);
#endif
	return 3;
}

static int puss_lua_sleep(lua_State* L) {
	lua_Integer ms = luaL_checkinteger(L, 1);
#ifdef _WIN32
	Sleep((DWORD)ms);
#else
	usleep(ms*1000);
#endif
	return 3;
}

static luaL_Reg puss_utils_methods[] =
	{ {"filename_format", puss_lua_filename_format}
	, {"file_list", puss_lua_file_list}
	, {"local_to_utf8", puss_lua_local_to_utf8}
	, {"utf8_to_local", puss_lua_utf8_to_local}
	, {"timestamp", puss_lua_timestamp}
	, {"sleep", puss_lua_sleep}
	, {NULL, NULL}
	};

void puss_reg_puss_utils(lua_State* L) {
	luaL_setfuncs(L, puss_utils_methods, 0);
}
