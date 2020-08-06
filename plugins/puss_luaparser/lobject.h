/*
** $Id: lobject.h $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/


#ifndef lobject_h
#define lobject_h


#ifdef _MSC_VER
#ifndef inline
#define inline __forceinline
#endif
#endif

typedef const char	TString;


/*
** Union of all Lua values
*/
typedef union Value {
  lua_Integer i;   /* integer numbers */
  lua_Number n;    /* float numbers */
  TString s;    /* string */
} Value;


/*
** Tagged Values. This is the basic representation of values in Lua:
** an actual value plus a tag with its type.
*/
typedef struct TValue {
  union {
    Value value_;
    lua_Integer i;   /* integer numbers */
    lua_Number n;    /* float numbers */
    TString s;    /* string */
  };
  char tt_;
} TValue;

#define setfltvalue(obj,x) \
  { TValue *io=(obj); io->n=(x); io->tt_ = 'n'; }

#define setivalue(obj,x) \
  { TValue *io=(obj); io->i=(x); io->tt_ = 'i'; }


/* size of buffer for 'luaO_utf8esc' function */
#define UTF8BUFFSZ	8

LUAI_FUNC int luaO_utf8esc (char *buff, unsigned long x);
LUAI_FUNC size_t luaO_str2num (const char *s, TValue *o);
LUAI_FUNC int luaO_hexavalue (int c);

#endif

