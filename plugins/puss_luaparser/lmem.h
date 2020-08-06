/*
** $Id: lmem.h $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/

#ifndef lmem_h
#define lmem_h


#include <stddef.h>

#include "llimits.h"
#include "lua.h"


/*
** Computes the minimum between 'n' and 'MAX_SIZET/sizeof(t)', so that
** the result is not larger than 'n' and cannot overflow a 'size_t'
** when multiplied by the size of type 't'. (Assumes that 'n' is an
** 'int' or 'unsigned int' and that 'int' is not larger than 'size_t'.)
*/
#define luaM_limitN(n,t)  \
  ((cast_sizet(n) <= MAX_SIZET/sizeof(t)) ? (n) :  \
     cast_uint((MAX_SIZET/sizeof(t))))


/*
** Arrays of chars do not need any test
*/
#define luaM_reallocvchar(L,b,on,n)  \
  cast_charp(luaM_saferealloc__(L, (b), (on)*sizeof(char), (n)*sizeof(char)))

#define luaM_freemem(L, b, s)	luaM_free__(L, (b), (s))
#define luaM_free(L, b)		luaM_free__(L, (b), sizeof(*(b)))
#define luaM_freearray(L, b, n)   luaM_free__(L, (b), (n)*sizeof(*(b)))

#define luaM_new(L,t)		cast(t*, luaM_malloc__(L, sizeof(t), 0))
#define luaM_newvector(L,n,t)	cast(t*, luaM_malloc__(L, (n)*sizeof(t), 0))

#define luaM_newobject(L,tag,s)	luaM_malloc__(L, (s), tag)

#define luaM_growvector(L,v,nelems,size,t,limit,e) \
	((v)=cast(t *, luaM_growaux__(L,v,nelems,&(size),sizeof(t), \
                         luaM_limitN(limit,t),e)))

#define luaM_shrinkvector(L,v,size,fs,t) \
   ((v)=cast(t *, luaM_shrinkvector__(L, v, &(size), fs, sizeof(t))))

/* not to be called directly */
LUAI_FUNC void *luaM_realloc__ (lua_State *L, void *block, size_t oldsize,
                                                          size_t size);
LUAI_FUNC void *luaM_saferealloc__ (lua_State *L, void *block, size_t oldsize,
                                                              size_t size);
LUAI_FUNC void luaM_free__ (lua_State *L, void *block, size_t osize);
LUAI_FUNC void *luaM_growaux__ (lua_State *L, void *block, int nelems,
                               int *size, int size_elem, int limit,
                               const char *what);
LUAI_FUNC void *luaM_shrinkvector__ (lua_State *L, void *block, int *nelem,
                                    int final_n, int size_elem);
LUAI_FUNC void *luaM_malloc__ (lua_State *L, size_t size, int tag);

#endif

