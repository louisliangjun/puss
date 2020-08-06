// puss_module_system.c

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "puss_plugin.h"

#include "lctype.h"
#include "lmem.h"
#include "llex.h"
#include "lparser.h"
#include "lzio.h"

#define PUSS_LUAPARSER_LIB_NAME	"[PussLuaParserLib]"

/*
** Execute a protected parser.
*/
struct SParser {  /* data to 'f_parser' */
  ZIO *z;
  Mbuffer buff;  /* dynamic structure used by the scanner */
  const char *name;
  LuaChunk *chunk;
};


static int f_parser (lua_State *L) {
  struct SParser *p = cast(struct SParser *, lua_touserdata(L, lua_upvalueindex(3)));
  int c = zgetc(p->z);  /* read first character */
  luaY_parser(L, p->chunk, p->z, &p->buff, p->name, c);
  lua_assert(cl->nupvalues == cl->p->sizeupvalues);
  return 0;
}


typedef struct LoadS {
  const char *s;
  size_t size;
} LoadS;


static const char *getS (lua_State *L, void *ud, size_t *size) {
  LoadS *ls = (LoadS *)ud;
  (void)L;  /* not used */
  if (ls->size == 0) return NULL;
  *size = ls->size;
  ls->size = 0;
  return ls->s;
}

#define PCHUNK_NAME	"PussLuaPChunk"

static int chunk_gc(lua_State *L) {
  LuaChunk* ud = luaL_checkudata(L, 1, PCHUNK_NAME);
  AstNode *l = ud->freelist;
  ud->freelist = NULL;
  ud->block.stats.head = ud->block.stats.tail = NULL;
  while (l) {
    AstNode *t = l;
    l = t->_freelist;
	luaM_free(L, t);
  }
  if (ud->tokens) {
    luaM_freearray(L, ud->tokens, ud->ntokens);
    ud->tokens = NULL;
    ud->ntokens = 0;
  }
  lua_pushnil(L);
  lua_setuservalue(L, 1);
  return 0;
}

void ast_trace_list(AstNodeList *list, int depth, const char* name);

static int chunk_trace(lua_State *L) {
  LuaChunk* ud = luaL_checkudata(L, 1, PCHUNK_NAME);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  ast_trace_list(&(ud->block.stats), 0, "chunk");
  return 0;
}

static luaL_Reg chunk_mt[] =
  { {"__index", NULL}
  , {"__gc", chunk_gc}
  , {"trace", chunk_trace}
  , {NULL, NULL}
  };

static int parse(lua_State* L) {
  int status;
  ZIO z;
  LoadS ls;
  struct SParser p;
  LuaChunk *ud;
  const char* chunkname = luaL_checkstring(L, 1);
  ls.s = luaL_checklstring(L, 2, &ls.size);
  ud = (LuaChunk*)lua_newuserdata(L, sizeof(LuaChunk));
  memset(ud, 0, sizeof(LuaChunk));
  if (luaL_newmetatable(L, PCHUNK_NAME)) {
    luaL_setfuncs(L, chunk_mt, 0);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
  }
  lua_setmetatable(L, -2);
  lua_newtable(L);	/* lua_upvalueindex(1) == temp strs */
  lua_pushvalue(L, -1); lua_setuservalue(L, -3); /* chunk.<uv> = temp strs */
  luaZ_init(L, &z, getS, &ls);
  p.z = &z; p.name = chunkname;
  p.chunk = ud;
  luaZ_initbuffer(L, &p.buff);
  lua_pushvalue(L, lua_upvalueindex(1)); /* RESERVED */
  lua_pushlightuserdata(L, &p);
  lua_pushcclosure(L, f_parser, 3);
  status = lua_pcall(L, 0, 0, 0);
  luaZ_freebuffer(L, &p.buff);
  return status ? 0 : 1;
}


PussInterface* __puss_iface__ = NULL;

PUSS_PLUGIN_EXPORT int __puss_plugin_init__(lua_State* L, PussInterface* puss) {
  __puss_iface__ = puss;
  if( lua_getfield(L, LUA_REGISTRYINDEX, PUSS_LUAPARSER_LIB_NAME)==LUA_TTABLE )
    return 1;
  lua_pop(L, 1);
  
  lua_newtable(L);
  lua_pushvalue(L, -1);
  lua_setfield(L, LUA_REGISTRYINDEX, PUSS_LUAPARSER_LIB_NAME);
  
  lua_newtable(L); /* RESERVED */
  luaX_init(L);
  lua_pushcclosure(L, parse, 1);	lua_setfield(L, -2, "parse");
  return 1;
}

