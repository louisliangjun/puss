#ifndef _INC_HEADER_H_
#define _INC_HEADER_H_

#ifndef __TINYC__
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <stdint.h>

#ifdef _WIN32
	#ifndef inline
		#define inline	__inline
	#endif
#endif

#else

// stdnoreturn.h
#define noreturn _Noreturn

// stddef.h
typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ssize_t;
typedef __WCHAR_TYPE__ wchar_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __PTRDIFF_TYPE__ intptr_t;
typedef __SIZE_TYPE__ uintptr_t;

#ifndef __int8_t_defined
#define __int8_t_defined
typedef signed char int8_t;
typedef signed short int int16_t;
typedef signed int int32_t;
#ifdef __LP64__
typedef signed long int int64_t;
#else
typedef signed long long int int64_t;
#endif
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
#ifdef __LP64__
typedef unsigned long int uint64_t;
#else
typedef unsigned long long int uint64_t;
#endif
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

#define offsetof(type, field) ((size_t)&((type *)0)->field)

// float.h
#define FLT_RADIX 2

/* IEEE float */
#define FLT_MANT_DIG 24
#define FLT_DIG 6
#define FLT_ROUNDS 1
#define FLT_EPSILON 1.19209290e-07F
#define FLT_MIN_EXP (-125)
#define FLT_MIN 1.17549435e-38F
#define FLT_MIN_10_EXP (-37)
#define FLT_MAX_EXP 128
#define FLT_MAX 3.40282347e+38F
#define FLT_MAX_10_EXP 38

/* IEEE double */
#define DBL_MANT_DIG 53
#define DBL_DIG 15
#define DBL_EPSILON 2.2204460492503131e-16
#define DBL_MIN_EXP (-1021)
#define DBL_MIN 2.2250738585072014e-308
#define DBL_MIN_10_EXP (-307)
#define DBL_MAX_EXP 1024
#define DBL_MAX 1.7976931348623157e+308
#define DBL_MAX_10_EXP 308

/* horrible intel long double */
#if defined __i386__ || defined __x86_64__

#define LDBL_MANT_DIG 64
#define LDBL_DIG 18
#define LDBL_EPSILON 1.08420217248550443401e-19L
#define LDBL_MIN_EXP (-16381)
#define LDBL_MIN 3.36210314311209350626e-4932L
#define LDBL_MIN_10_EXP (-4931)
#define LDBL_MAX_EXP 16384
#define LDBL_MAX 1.18973149535723176502e+4932L
#define LDBL_MAX_10_EXP 4932

#else

/* same as IEEE double */
#define LDBL_MANT_DIG 53
#define LDBL_DIG 15
#define LDBL_EPSILON 2.2204460492503131e-16
#define LDBL_MIN_EXP (-1021)
#define LDBL_MIN 2.2250738585072014e-308
#define LDBL_MIN_10_EXP (-307)
#define LDBL_MAX_EXP 1024
#define LDBL_MAX 1.7976931348623157e+308
#define LDBL_MAX_10_EXP 308

#endif

// stdlib.h, string.h, memory.h

int memcmp(const void *_Buf1,const void *_Buf2,size_t _Size);
void * memmove(void *_Dst,const void *_Src,size_t _Size);
void * memcpy(void *_Dst,const void *_Src,size_t _Size);
void * memset(void *_Dst,int _Val,size_t _Size);

char * strcpy(char *_Dest,const char *_Source);
char * strcat(char *_Dest,const char *_Source);
int strcmp(const char *_Str1,const char *_Str2);
size_t strlen(const char *_Str);
char * strtok(char *_Str,const char *_Delim);

long int strtol(const char *nptr, char **endptr, int base);
unsigned long int strtoul(const char *nptr, char **endptr, int base);
float strtof (const char * __restrict__, char ** __restrict__);
double strtod(const char *_Str,char **_EndPtr);
int sprintf(char *str, const char *format, ...);

#endif//__TINYC__

#define LUAI_MAXSTACK		1000000

#define LUA_MULTRET	(-1)
#define LUA_REGISTRYINDEX	(-LUAI_MAXSTACK - 1000)

#define lua_upvalueindex(i) (LUA_REGISTRYINDEX - (2+i))

#define LUA_OK		0
#define LUA_YIELD	1

#define LUA_TNONE		(-1)
#define LUA_TNIL		0
#define LUA_TBOOLEAN		1
#define LUA_TLIGHTUSERDATA	2
#define LUA_TNUMBER		3
#define LUA_TSTRING		4
#define LUA_TTABLE		5
#define LUA_TFUNCTION		6
#define LUA_TUSERDATA		7
#define LUA_TTHREAD		8
#define LUA_NUMTAGS		9

#define LUA_MINSTACK	20

#define LUA_RIDX_MAINTHREAD	1
#define LUA_RIDX_GLOBALS	2

typedef double lua_Number;
typedef int64_t lua_Integer;
typedef uint64_t lua_Unsigned;
typedef struct lua_State lua_State;
typedef int (*lua_CFunction) (lua_State *L);

#define LUA_OPADD	0	/* ORDER TM, ORDER OP */
#define LUA_OPSUB	1
#define LUA_OPMUL	2
#define LUA_OPMOD	3
#define LUA_OPPOW	4
#define LUA_OPDIV	5
#define LUA_OPIDIV	6
#define LUA_OPBAND	7
#define LUA_OPBOR	8
#define LUA_OPBXOR	9
#define LUA_OPSHL	10
#define LUA_OPSHR	11
#define LUA_OPUNM	12
#define LUA_OPBNOT	13

#define LUA_OPEQ	0
#define LUA_OPLT	1
#define LUA_OPLE	2

#define lua_tonumber(L,i)	lua_tonumberx(L,(i),NULL)
#define lua_tointeger(L,i)	lua_tointegerx(L,(i),NULL)
#define lua_pop(L,n)		lua_settop(L, -(n)-1)
#define lua_newtable(L)		lua_createtable(L, 0, 0)

#define lua_register(L,n,f) (lua_pushcfunction(L, (f)), lua_setglobal(L, (n)))
#define lua_pushcfunction(L,f)	lua_pushcclosure(L, (f), 0)

#define lua_isfunction(L,n)	(lua_type(L, (n)) == LUA_TFUNCTION)
#define lua_istable(L,n)	(lua_type(L, (n)) == LUA_TTABLE)
#define lua_islightuserdata(L,n)	(lua_type(L, (n)) == LUA_TLIGHTUSERDATA)
#define lua_isnil(L,n)		(lua_type(L, (n)) == LUA_TNIL)
#define lua_isboolean(L,n)	(lua_type(L, (n)) == LUA_TBOOLEAN)
#define lua_isthread(L,n)	(lua_type(L, (n)) == LUA_TTHREAD)
#define lua_isnone(L,n)		(lua_type(L, (n)) == LUA_TNONE)
#define lua_isnoneornil(L, n)	(lua_type(L, (n)) <= 0)

#define lua_pushliteral(L, s)	lua_pushstring(L, "" s)

#define lua_pushglobaltable(L)  \
	((void)lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS))

#define lua_tostring(L,i)	lua_tolstring(L, (i), NULL)
#define lua_insert(L,idx)	lua_rotate(L, (idx), 1)
#define lua_remove(L,idx)	(lua_rotate(L, (idx), -1), lua_pop(L, 1))
#define lua_replace(L,idx)	(lua_copy(L, -1, (idx)), lua_pop(L, 1))

typedef struct luaL_Reg {
	const char *name;
	lua_CFunction func;
} luaL_Reg;

#define LUA_NOREF       (-2)
#define LUA_REFNIL      (-1)

#define luaL_newlibtable(L,l)	\
  lua_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define luaL_argcheck(L, cond,arg,extramsg)	\
		((void)((cond) || luaL_argerror(L, (arg), (extramsg))))
#define luaL_checkstring(L,n)	(luaL_checklstring(L, (n), NULL))
#define luaL_optstring(L,n,d)	(luaL_optlstring(L, (n), (d), NULL))

#define luaL_typename(L,i)	lua_typename(L, lua_type(L,(i)))

#define luaL_dostring(L, s) \
	(luaL_loadstring(L, s) || lua_pcall(L, 0, LUA_MULTRET, 0))

#define luaL_getmetatable(L,n)	(lua_getfield(L, LUA_REGISTRYINDEX, (n)))

#define luaL_opt(L,f,n,d)	(lua_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#define luaL_loadbuffer(L,s,sz,n)	luaL_loadbufferx(L,s,sz,n,NULL)

int            lua_absindex            (lua_State *L, int idx);
int            lua_gettop              (lua_State *L);
void           lua_settop              (lua_State *L, int idx);
void           lua_pushvalue           (lua_State *L, int idx);
void           lua_rotate              (lua_State *L, int idx, int n);
void           lua_copy                (lua_State *L, int fromidx, int toidx);
int            lua_checkstack          (lua_State *L, int n);
void           lua_xmove               (lua_State *from, lua_State *to, int n);
int            lua_isnumber            (lua_State *L, int idx);
int            lua_isstring            (lua_State *L, int idx);
int            lua_iscfunction         (lua_State *L, int idx);
int            lua_isinteger           (lua_State *L, int idx);
int            lua_isuserdata          (lua_State *L, int idx);
int            lua_type                (lua_State *L, int idx);
const char*    lua_typename            (lua_State *L, int tp);
lua_Number     lua_tonumberx           (lua_State *L, int idx, int *isnum);
lua_Integer    lua_tointegerx          (lua_State *L, int idx, int *isnum);
int            lua_toboolean           (lua_State *L, int idx);
const char*    lua_tolstring           (lua_State *L, int idx, size_t *len);
size_t         lua_rawlen              (lua_State *L, int idx);
lua_CFunction  lua_tocfunction         (lua_State *L, int idx);
void	*      lua_touserdata          (lua_State *L, int idx);
lua_State*     lua_tothread            (lua_State *L, int idx);
const void*    lua_topointer           (lua_State *L, int idx);
void           lua_arith               (lua_State *L, int op);
int            lua_rawequal            (lua_State *L, int idx1, int idx2);
int            lua_compare             (lua_State *L, int idx1, int idx2, int op);
void           lua_pushnil             (lua_State *L);
void           lua_pushnumber          (lua_State *L, lua_Number n);
void           lua_pushinteger         (lua_State *L, lua_Integer n);
const char*    lua_pushlstring         (lua_State *L, const char *s, size_t len);
const char*    lua_pushstring          (lua_State *L, const char *s);
const char*    lua_pushfstring         (lua_State *L, const char *fmt, ...);
void           lua_pushboolean         (lua_State *L, int b);
void           lua_pushlightuserdata   (lua_State *L, void *p);
int            lua_pushthread          (lua_State *L);
int            lua_getglobal           (lua_State *L, const char *name);
int            lua_gettable            (lua_State *L, int idx);
int            lua_getfield            (lua_State *L, int idx, const char *k);
int            lua_geti                (lua_State *L, int idx, lua_Integer n);
int            lua_rawget              (lua_State *L, int idx);
int            lua_rawgeti             (lua_State *L, int idx, lua_Integer n);
int            lua_rawgetp             (lua_State *L, int idx, const void *p);
void           lua_createtable         (lua_State *L, int narr, int nrec);
void*          lua_newuserdata         (lua_State *L, size_t sz);
int            lua_getmetatable        (lua_State *L, int objindex);
int            lua_getuservalue        (lua_State *L, int idx);
void           lua_setglobal           (lua_State *L, const char *name);
void           lua_settable            (lua_State *L, int idx);
void           lua_setfield            (lua_State *L, int idx, const char *k);
void           lua_seti                (lua_State *L, int idx, lua_Integer n);
void           lua_rawset              (lua_State *L, int idx);
void           lua_rawseti             (lua_State *L, int idx, lua_Integer n);
void           lua_rawsetp             (lua_State *L, int idx, const void *p);
int            lua_setmetatable        (lua_State *L, int objindex);
void           lua_setuservalue        (lua_State *L, int idx);
void           lua_pushcclosure        (lua_State* L, lua_CFunction f, int nup);
void           lua_call                (lua_State *L, int nargs, int nresults);
int            lua_pcall               (lua_State *L, int nargs, int nresults, int errfunc);
int            lua_status              (lua_State *L);
int            lua_isyieldable         (lua_State *L);
int            lua_error               (lua_State *L);
int            lua_next                (lua_State *L, int idx);
void           lua_concat              (lua_State *L, int n);
void           lua_len                 (lua_State *L, int idx);
size_t         lua_stringtonumber      (lua_State *L, const char *s);
int            luaL_getmetafield       (lua_State *L, int obj, const char *e);
const char*    luaL_tolstring          (lua_State *L, int idx, size_t *len);
int            luaL_argerror           (lua_State *L, int arg, const char *extramsg);
const char*    luaL_checklstring       (lua_State *L, int arg, size_t *l);
const char*    luaL_optlstring         (lua_State *L, int arg, const char *def, size_t *l);
lua_Number     luaL_checknumber        (lua_State *L, int arg);
lua_Number     luaL_optnumber          (lua_State *L, int arg, lua_Number def);
lua_Integer    luaL_checkinteger       (lua_State *L, int arg);
lua_Integer    luaL_optinteger         (lua_State *L, int arg, lua_Integer def);
void           luaL_checkstack         (lua_State *L, int sz, const char *msg);
void           luaL_checktype          (lua_State *L, int arg, int t);
void           luaL_checkany           (lua_State *L, int arg);
int            luaL_newmetatable       (lua_State *L, const char *tname);
void           luaL_setmetatable       (lua_State *L, const char *tname);
void*          luaL_testudata          (lua_State *L, int ud, const char *tname);
void*          luaL_checkudata         (lua_State *L, int ud, const char *tname);
void           luaL_where              (lua_State *L, int lvl);
int            luaL_error              (lua_State *L, const char *fmt, ...);
int            luaL_checkoption        (lua_State *L, int arg, const char *def, const char *const lst[]);
int            luaL_ref                (lua_State *L, int t);
void           luaL_unref              (lua_State *L, int t, int ref);
int            luaL_loadbufferx        (lua_State *L, const char *buff, size_t sz, const char *name, const char *mode);
int            luaL_loadstring         (lua_State *L, const char *s);
lua_Integer    luaL_len                (lua_State *L, int idx);
const char*    luaL_gsub               (lua_State *L, const char *s, const char *p, const char *r);
int            luaL_getsubtable        (lua_State *L, int idx, const char *fname);

#endif//_INC_HEADER_H_
