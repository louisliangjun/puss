/*
** $Id: lmem.c $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/

#define lmem_c
#define LUA_CORE

#include "lprefix.h"

#include <stdlib.h>
#include <string.h>

#include "puss_plugin.h"

#include "lmem.h"


/*
** Minimum size for arrays during parsing, to avoid overhead of
** reallocating to size 1, then 2, and then 4. All these arrays
** will be reallocated to exact sizes or erased when parsing ends.
*/
#define MINSIZEARRAY	4


void *luaM_growaux__ (lua_State *L, void *block, int nelems, int *psize,
                     int size_elems, int limit, const char *what) {
  void *newblock;
  int size = *psize;
  if (nelems + 1 <= size)  /* does one extra element still fit? */
    return block;  /* nothing to be done */
  if (size >= limit / 2) {  /* cannot double it? */
    if (unlikely(size >= limit))  /* cannot grow even a little? */
      luaL_error(L, "luaM_growaux__ error!");
    size = limit;  /* still have at least one free place */
  }
  else {
    size *= 2;
    if (size < MINSIZEARRAY)
      size = MINSIZEARRAY;  /* minimum size */
  }
  lua_assert(nelems + 1 <= size && size <= limit);
  /* 'limit' ensures that multiplication will not overflow */
  newblock = luaM_saferealloc__(L, block, cast_sizet(*psize) * size_elems,
                                         cast_sizet(size) * size_elems);
  *psize = size;  /* update only when everything else is OK */
  return newblock;
}


/*
** In prototypes, the size of the array is also its number of
** elements (to save memory). So, if it cannot shrink an array
** to its number of elements, the only option is to raise an
** error.
*/
void *luaM_shrinkvector__ (lua_State *L, void *block, int *size,
                          int final_n, int size_elem) {
  void *newblock;
  size_t oldsize = cast_sizet((*size) * size_elem);
  size_t newsize = cast_sizet(final_n * size_elem);
  lua_assert(newsize <= oldsize);
  newblock = luaM_saferealloc__(L, block, oldsize, newsize);
  *size = final_n;
  return newblock;
}

/* }================================================================== */

/*
** Free memory
*/
void luaM_free__ (lua_State *L, void *block, size_t osize) {
  free(block);
}



/*
** Generic allocation routine.
** If allocation fails while shrinking a block, do not try again; the
** GC shrinks some blocks and it is not reentrant.
*/
void *luaM_realloc__ (lua_State *L, void *block, size_t osize, size_t nsize) {
  void *newblock;
  if (nsize==0) {
    free(block);
    return NULL;
  }
  if (!(newblock = realloc(block, nsize)))
    return NULL;
  if (nsize > osize)
    memset(((char*)newblock)+osize, 0, nsize-osize);
  return newblock;
}


void *luaM_saferealloc__ (lua_State *L, void *block, size_t osize,
                                                    size_t nsize) {
  void *newblock = luaM_realloc__(L, block, osize, nsize);
  if (unlikely(newblock == NULL && nsize > 0))  /* allocation failed? */
    luaL_error(L, "luaM_saferealloc__ error!");
  return newblock;
}


void *luaM_malloc__ (lua_State *L, size_t size, int tag) {
  void *p;
  if (size == 0)
    return NULL;  /* that's all */
  if ((p = realloc(NULL, size))!=NULL)
    memset(p, 0, size);
  return p;
}
