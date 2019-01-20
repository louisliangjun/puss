// simple_pickle.h

#ifndef _INC_PUSSTOOLKIT_SIMPLE_PICKLE_H_
#define _INC_PUSSTOOLKIT_SIMPLE_PICKLE_H_

#include "puss_lua.h"

PUSS_DECLS_BEGIN

typedef struct _PickleBuffer {
	unsigned char*	buf;
	size_t			len;
	size_t			free;
} PickleBuffer;

void*	puss_simple_pack(size_t* plen, lua_State* L, int start, int end);
int		puss_simple_unpack(lua_State* L, const void* pkt, size_t len);

int		puss_lua_simple_pack(lua_State* L);
int		puss_lua_simple_unpack(lua_State* L);

PUSS_DECLS_END

#endif//_INC_PUSSTOOLKIT_SIMPLE_PICKLE_H_
