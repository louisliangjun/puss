local puss_tcc_header_h = [[#line 1 "core/c/tinycc.lua"
#ifndef _STDDEF_H
#define _STDDEF_H

typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ssize_t;
typedef __WCHAR_TYPE__ wchar_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __PTRDIFF_TYPE__ intptr_t;
typedef __SIZE_TYPE__ uintptr_t;

#if __STDC_VERSION__ >= 201112L
typedef union { long long __ll; long double __ld; } max_align_t;
#endif

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

#endif

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

int printf(char const* const format, ...);
int sprintf(char *str, const char *format, ...);

#define LUAI_MAXSTACK		1000000

#define LUA_MULTRET	(-1)
#define LUA_REGISTRYINDEX	(-LUAI_MAXSTACK - 1000)
#define lua_upvalueindex(i)	(LUA_REGISTRYINDEX - (i))

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
typedef void* lua_KContext;
typedef struct lua_State lua_State;
typedef int (*lua_CFunction) (lua_State *L);
typedef int (*lua_KFunction) (lua_State *L, int status, lua_KContext ctx);

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

#define lua_call(L,n,r)		lua_callk(L, (n), (r), 0, NULL)
#define lua_pcall(L,n,r,f)	lua_pcallk(L, (n), (r), (f), 0, NULL)
#define lua_yield(L,n)		lua_yieldk(L, (n), 0, NULL)

#define lua_tonumber(L,i)	lua_tonumberx(L,(i),NULL)
#define lua_tointeger(L,i)	lua_tointegerx(L,(i),NULL)
#define lua_pop(L,n)		lua_settop(L, -(n)-1)
#define lua_newtable(L)		lua_createtable(L, 0, 0)

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

#define luaL_newlib(L,l)  \
  (luaL_checkversion(L), luaL_newlibtable(L,l), luaL_setfuncs(L,l,0))

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
void           lua_callk               (lua_State *L, int nargs, int nresults, lua_KContext ctx, lua_KFunction k);
int            lua_pcallk              (lua_State *L, int nargs, int nresults, int errfunc, lua_KContext ctx, lua_KFunction k);
int            lua_yieldk              (lua_State *L, int nresults, lua_KContext ctx, lua_KFunction k);
int            lua_resume              (lua_State *L, lua_State *from, int narg);
int            lua_status              (lua_State *L);
int            lua_isyieldable         (lua_State *L);
int            lua_error               (lua_State *L);
int            lua_next                (lua_State *L, int idx);
void           lua_concat              (lua_State *L, int n);
void           lua_len                 (lua_State *L, int idx);
size_t         lua_stringtonumber      (lua_State *L, const char *s);
const char*    lua_getupvalue          (lua_State *L, int funcindex, int n);
const char*    lua_setupvalue          (lua_State *L, int funcindex, int n);
void*          lua_upvalueid           (lua_State *L, int fidx, int n);
void           lua_upvaluejoin         (lua_State *L, int fidx1, int n1, int fidx2, int n2);
int            luaL_getmetafield       (lua_State *L, int obj, const char *e);
int            luaL_callmeta           (lua_State *L, int obj, const char *e);
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
void           luaL_setfuncs           (lua_State *L, const luaL_Reg *l, int nup);
int            luaL_getsubtable        (lua_State *L, int idx, const char *fname);
void           luaL_traceback          (lua_State *L, lua_State *L1, const char *msg, int level);

#define	puss_upvalueindex(i)	lua_upvalueindex(2+i)
void	puss_pushcclosure(lua_State* L, lua_CFunction f, int nup);

]]

local libtcc1_c = [[#line 1 "libtcc1.c"
/* TCC runtime library. 
   Parts of this code are (c) 2002 Fabrice Bellard 

   Copyright (C) 1987, 1988, 1992, 1994, 1995 Free Software Foundation, Inc.

This file is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

In addition to the permissions in the GNU General Public License, the
Free Software Foundation gives you unlimited permission to link the
compiled version of this file into combinations with other programs,
and to distribute those combinations without any restriction coming
from the use of this file.  (The General Public License restrictions
do apply in other respects; for example, they cover modification of
the file, and distribution when not linked into a combine
executable.)

This file is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  
*/

#define W_TYPE_SIZE   32
#define BITS_PER_UNIT 8

typedef int Wtype;
typedef unsigned int UWtype;
typedef unsigned int USItype;
typedef long long DWtype;
typedef unsigned long long UDWtype;

struct DWstruct {
    Wtype low, high;
};

typedef union
{
  struct DWstruct s;
  DWtype ll;
} DWunion;

typedef long double XFtype;
#define WORD_SIZE (sizeof (Wtype) * BITS_PER_UNIT)
#define HIGH_WORD_COEFF (((UDWtype) 1) << WORD_SIZE)

/* the following deal with IEEE single-precision numbers */
#define EXCESS		126
#define SIGNBIT		0x80000000
#define HIDDEN		(1 << 23)
#define SIGN(fp)	((fp) & SIGNBIT)
#define EXP(fp)		(((fp) >> 23) & 0xFF)
#define MANT(fp)	(((fp) & 0x7FFFFF) | HIDDEN)
#define PACK(s,e,m)	((s) | ((e) << 23) | (m))

/* the following deal with IEEE double-precision numbers */
#define EXCESSD		1022
#define HIDDEND		(1 << 20)
#define EXPD(fp)	(((fp.l.upper) >> 20) & 0x7FF)
#define SIGND(fp)	((fp.l.upper) & SIGNBIT)
#define MANTD(fp)	(((((fp.l.upper) & 0xFFFFF) | HIDDEND) << 10) | \
				(fp.l.lower >> 22))
#define HIDDEND_LL	((long long)1 << 52)
#define MANTD_LL(fp)	((fp.ll & (HIDDEND_LL-1)) | HIDDEND_LL)
#define PACKD_LL(s,e,m)	(((long long)((s)+((e)<<20))<<32)|(m))

/* the following deal with x86 long double-precision numbers */
#define EXCESSLD	16382
#define EXPLD(fp)	(fp.l.upper & 0x7fff)
#define SIGNLD(fp)	((fp.l.upper) & 0x8000)

/* only for x86 */
union ldouble_long {
    long double ld;
    struct {
        unsigned long long lower;
        unsigned short upper;
    } l;
};

union double_long {
    double d;
#if 1
    struct {
        unsigned int lower;
        int upper;
    } l;
#else
    struct {
        int upper;
        unsigned int lower;
    } l;
#endif
    long long ll;
};

union float_long {
    float f;
    unsigned int l;
};

/* XXX: we don't support several builtin supports for now */
#if !defined __x86_64__ && !defined __arm__

/* XXX: use gcc/tcc intrinsic ? */
#if defined __i386__
#define sub_ddmmss(sh, sl, ah, al, bh, bl) \
  __asm__ ("subl %5,%1\n\tsbbl %3,%0"					\
	   : "=r" ((USItype) (sh)),					\
	     "=&r" ((USItype) (sl))					\
	   : "0" ((USItype) (ah)),					\
	     "g" ((USItype) (bh)),					\
	     "1" ((USItype) (al)),					\
	     "g" ((USItype) (bl)))
#define umul_ppmm(w1, w0, u, v) \
  __asm__ ("mull %3"							\
	   : "=a" ((USItype) (w0)),					\
	     "=d" ((USItype) (w1))					\
	   : "%0" ((USItype) (u)),					\
	     "rm" ((USItype) (v)))
#define udiv_qrnnd(q, r, n1, n0, dv) \
  __asm__ ("divl %4"							\
	   : "=a" ((USItype) (q)),					\
	     "=d" ((USItype) (r))					\
	   : "0" ((USItype) (n0)),					\
	     "1" ((USItype) (n1)),					\
	     "rm" ((USItype) (dv)))
#define count_leading_zeros(count, x) \
  do {									\
    USItype __cbtmp;							\
    __asm__ ("bsrl %1,%0"						\
	     : "=r" (__cbtmp) : "rm" ((USItype) (x)));			\
    (count) = __cbtmp ^ 31;						\
  } while (0)
#else
#error unsupported CPU type
#endif

/* most of this code is taken from libgcc2.c from gcc */

static UDWtype __udivmoddi4 (UDWtype n, UDWtype d, UDWtype *rp)
{
  DWunion ww;
  DWunion nn, dd;
  DWunion rr;
  UWtype d0, d1, n0, n1, n2;
  UWtype q0, q1;
  UWtype b, bm;

  nn.ll = n;
  dd.ll = d;

  d0 = dd.s.low;
  d1 = dd.s.high;
  n0 = nn.s.low;
  n1 = nn.s.high;

#if !defined(UDIV_NEEDS_NORMALIZATION)
  if (d1 == 0)
    {
      if (d0 > n1)
	{
	  /* 0q = nn / 0D */

	  udiv_qrnnd (q0, n0, n1, n0, d0);
	  q1 = 0;

	  /* Remainder in n0.  */
	}
      else
	{
	  /* qq = NN / 0d */

	  if (d0 == 0)
	    d0 = 1 / d0;	/* Divide intentionally by zero.  */

	  udiv_qrnnd (q1, n1, 0, n1, d0);
	  udiv_qrnnd (q0, n0, n1, n0, d0);

	  /* Remainder in n0.  */
	}

      if (rp != 0)
	{
	  rr.s.low = n0;
	  rr.s.high = 0;
	  *rp = rr.ll;
	}
    }

#else /* UDIV_NEEDS_NORMALIZATION */

  if (d1 == 0)
    {
      if (d0 > n1)
	{
	  /* 0q = nn / 0D */

	  count_leading_zeros (bm, d0);

	  if (bm != 0)
	    {
	      /* Normalize, i.e. make the most significant bit of the
		 denominator set.  */

	      d0 = d0 << bm;
	      n1 = (n1 << bm) | (n0 >> (W_TYPE_SIZE - bm));
	      n0 = n0 << bm;
	    }

	  udiv_qrnnd (q0, n0, n1, n0, d0);
	  q1 = 0;

	  /* Remainder in n0 >> bm.  */
	}
      else
	{
	  /* qq = NN / 0d */

	  if (d0 == 0)
	    d0 = 1 / d0;	/* Divide intentionally by zero.  */

	  count_leading_zeros (bm, d0);

	  if (bm == 0)
	    {
	      /* From (n1 >= d0) /\ (the most significant bit of d0 is set),
		 conclude (the most significant bit of n1 is set) /\ (the
		 leading quotient digit q1 = 1).

		 This special case is necessary, not an optimization.
		 (Shifts counts of W_TYPE_SIZE are undefined.)  */

	      n1 -= d0;
	      q1 = 1;
	    }
	  else
	    {
	      /* Normalize.  */

	      b = W_TYPE_SIZE - bm;

	      d0 = d0 << bm;
	      n2 = n1 >> b;
	      n1 = (n1 << bm) | (n0 >> b);
	      n0 = n0 << bm;

	      udiv_qrnnd (q1, n1, n2, n1, d0);
	    }

	  /* n1 != d0...  */

	  udiv_qrnnd (q0, n0, n1, n0, d0);

	  /* Remainder in n0 >> bm.  */
	}

      if (rp != 0)
	{
	  rr.s.low = n0 >> bm;
	  rr.s.high = 0;
	  *rp = rr.ll;
	}
    }
#endif /* UDIV_NEEDS_NORMALIZATION */

  else
    {
      if (d1 > n1)
	{
	  /* 00 = nn / DD */

	  q0 = 0;
	  q1 = 0;

	  /* Remainder in n1n0.  */
	  if (rp != 0)
	    {
	      rr.s.low = n0;
	      rr.s.high = n1;
	      *rp = rr.ll;
	    }
	}
      else
	{
	  /* 0q = NN / dd */

	  count_leading_zeros (bm, d1);
	  if (bm == 0)
	    {
	      /* From (n1 >= d1) /\ (the most significant bit of d1 is set),
		 conclude (the most significant bit of n1 is set) /\ (the
		 quotient digit q0 = 0 or 1).

		 This special case is necessary, not an optimization.  */

	      /* The condition on the next line takes advantage of that
		 n1 >= d1 (true due to program flow).  */
	      if (n1 > d1 || n0 >= d0)
		{
		  q0 = 1;
		  sub_ddmmss (n1, n0, n1, n0, d1, d0);
		}
	      else
		q0 = 0;

	      q1 = 0;

	      if (rp != 0)
		{
		  rr.s.low = n0;
		  rr.s.high = n1;
		  *rp = rr.ll;
		}
	    }
	  else
	    {
	      UWtype m1, m0;
	      /* Normalize.  */

	      b = W_TYPE_SIZE - bm;

	      d1 = (d1 << bm) | (d0 >> b);
	      d0 = d0 << bm;
	      n2 = n1 >> b;
	      n1 = (n1 << bm) | (n0 >> b);
	      n0 = n0 << bm;

	      udiv_qrnnd (q0, n1, n2, n1, d1);
	      umul_ppmm (m1, m0, q0, d0);

	      if (m1 > n1 || (m1 == n1 && m0 > n0))
		{
		  q0--;
		  sub_ddmmss (m1, m0, m1, m0, d1, d0);
		}

	      q1 = 0;

	      /* Remainder in (n1n0 - m1m0) >> bm.  */
	      if (rp != 0)
		{
		  sub_ddmmss (n1, n0, n1, n0, m1, m0);
		  rr.s.low = (n1 << b) | (n0 >> bm);
		  rr.s.high = n1 >> bm;
		  *rp = rr.ll;
		}
	    }
	}
    }

  ww.s.low = q0;
  ww.s.high = q1;
  return ww.ll;
}

#define __negdi2(a) (-(a))

long long __divdi3(long long u, long long v)
{
    int c = 0;
    DWunion uu, vv;
    DWtype w;
    
    uu.ll = u;
    vv.ll = v;
    
    if (uu.s.high < 0) {
        c = ~c;
        uu.ll = __negdi2 (uu.ll);
    }
    if (vv.s.high < 0) {
        c = ~c;
        vv.ll = __negdi2 (vv.ll);
    }
    w = __udivmoddi4 (uu.ll, vv.ll, (UDWtype *) 0);
    if (c)
        w = __negdi2 (w);
    return w;
}

long long __moddi3(long long u, long long v)
{
    int c = 0;
    DWunion uu, vv;
    DWtype w;
    
    uu.ll = u;
    vv.ll = v;
    
    if (uu.s.high < 0) {
        c = ~c;
        uu.ll = __negdi2 (uu.ll);
    }
    if (vv.s.high < 0)
        vv.ll = __negdi2 (vv.ll);
    
    __udivmoddi4 (uu.ll, vv.ll, (UDWtype *) &w);
    if (c)
        w = __negdi2 (w);
    return w;
}

unsigned long long __udivdi3(unsigned long long u, unsigned long long v)
{
    return __udivmoddi4 (u, v, (UDWtype *) 0);
}

unsigned long long __umoddi3(unsigned long long u, unsigned long long v)
{
    UDWtype w;
    
    __udivmoddi4 (u, v, &w);
    return w;
}

/* XXX: fix tcc's code generator to do this instead */
long long __ashrdi3(long long a, int b)
{
#ifdef __TINYC__
    DWunion u;
    u.ll = a;
    if (b >= 32) {
        u.s.low = u.s.high >> (b - 32);
        u.s.high = u.s.high >> 31;
    } else if (b != 0) {
        u.s.low = ((unsigned)u.s.low >> b) | (u.s.high << (32 - b));
        u.s.high = u.s.high >> b;
    }
    return u.ll;
#else
    return a >> b;
#endif
}

/* XXX: fix tcc's code generator to do this instead */
unsigned long long __lshrdi3(unsigned long long a, int b)
{
#ifdef __TINYC__
    DWunion u;
    u.ll = a;
    if (b >= 32) {
        u.s.low = (unsigned)u.s.high >> (b - 32);
        u.s.high = 0;
    } else if (b != 0) {
        u.s.low = ((unsigned)u.s.low >> b) | (u.s.high << (32 - b));
        u.s.high = (unsigned)u.s.high >> b;
    }
    return u.ll;
#else
    return a >> b;
#endif
}

/* XXX: fix tcc's code generator to do this instead */
long long __ashldi3(long long a, int b)
{
#ifdef __TINYC__
    DWunion u;
    u.ll = a;
    if (b >= 32) {
        u.s.high = (unsigned)u.s.low << (b - 32);
        u.s.low = 0;
    } else if (b != 0) {
        u.s.high = ((unsigned)u.s.high << b) | ((unsigned)u.s.low >> (32 - b));
        u.s.low = (unsigned)u.s.low << b;
    }
    return u.ll;
#else
    return a << b;
#endif
}

#endif /* !__x86_64__ */

/* XXX: fix tcc's code generator to do this instead */
float __floatundisf(unsigned long long a)
{
    DWunion uu; 
    XFtype r;

    uu.ll = a;
    if (uu.s.high >= 0) {
        return (float)uu.ll;
    } else {
        r = (XFtype)uu.ll;
        r += 18446744073709551616.0;
        return (float)r;
    }
}

double __floatundidf(unsigned long long a)
{
    DWunion uu; 
    XFtype r;

    uu.ll = a;
    if (uu.s.high >= 0) {
        return (double)uu.ll;
    } else {
        r = (XFtype)uu.ll;
        r += 18446744073709551616.0;
        return (double)r;
    }
}

long double __floatundixf(unsigned long long a)
{
    DWunion uu; 
    XFtype r;

    uu.ll = a;
    if (uu.s.high >= 0) {
        return (long double)uu.ll;
    } else {
        r = (XFtype)uu.ll;
        r += 18446744073709551616.0;
        return (long double)r;
    }
}

unsigned long long __fixunssfdi (float a1)
{
    register union float_long fl1;
    register int exp;
    register unsigned long l;

    fl1.f = a1;

    if (fl1.l == 0)
	return (0);

    exp = EXP (fl1.l) - EXCESS - 24;

    l = MANT(fl1.l);
    if (exp >= 41)
	return (unsigned long long)-1;
    else if (exp >= 0)
        return (unsigned long long)l << exp;
    else if (exp >= -23)
        return l >> -exp;
    else
        return 0;
}

long long __fixsfdi (float a1)
{
    long long ret; int s;
    ret = __fixunssfdi((s = a1 >= 0) ? a1 : -a1);
    return s ? ret : -ret;
}

unsigned long long __fixunsdfdi (double a1)
{
    register union double_long dl1;
    register int exp;
    register unsigned long long l;

    dl1.d = a1;

    if (dl1.ll == 0)
	return (0);

    exp = EXPD (dl1) - EXCESSD - 53;

    l = MANTD_LL(dl1);

    if (exp >= 12)
	return (unsigned long long)-1;
    else if (exp >= 0)
        return l << exp;
    else if (exp >= -52)
        return l >> -exp;
    else
        return 0;
}

long long __fixdfdi (double a1)
{
    long long ret; int s;
    ret = __fixunsdfdi((s = a1 >= 0) ? a1 : -a1);
    return s ? ret : -ret;
}

#ifndef __arm__
unsigned long long __fixunsxfdi (long double a1)
{
    register union ldouble_long dl1;
    register int exp;
    register unsigned long long l;

    dl1.ld = a1;

    if (dl1.l.lower == 0 && dl1.l.upper == 0)
	return (0);

    exp = EXPLD (dl1) - EXCESSLD - 64;

    l = dl1.l.lower;

    if (exp > 0)
	return (unsigned long long)-1;
    else if (exp >= -63) 
        return l >> -exp;
    else
        return 0;
}

long long __fixxfdi (long double a1)
{
    long long ret; int s;
    ret = __fixunsxfdi((s = a1 >= 0) ? a1 : -a1);
    return s ? ret : -ret;
}
#endif /* !ARM */
]]

-- tinycc.lua

local TCC_OPTIONS = '-Wall -Werror -nostdinc -nostdlib -I.'
if DEBUG then
	TCC_OPTIONS = '-g ' .. TCC_OPTIONS
end

__exports.compile = function(src, ...)
	local f = io.open(puss._path..'/'..src, 'r')
	if not f then return false end
	local ctx = f:read('*all')
	f:close()
	if not ctx then return false end

	print( puss.tcc_new )

	local tcc = puss.tcc_new()
	tcc:set_options(TCC_OPTIONS)
	tcc:set_output_type('memory')
	tcc:compile_string(libtcc1_c)
	local files = { puss_tcc_header_h, '#line 1 "'..src..'"', ctx }
	tcc:compile_string( table.concat(files, '\n') )
	return tcc:relocate(...)
end

