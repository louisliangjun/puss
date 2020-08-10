// puss_module_system.c

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

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
  struct SParser *p = cast(struct SParser *, lua_touserdata(L, 3));
  int c = zgetc(p->z);  /* read first character */
  luaY_parser(L, p->chunk, p->z, &p->buff, p->name, c);
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
  AstNode *list = ud->freelist;
  AstNode *t;
  ud->freelist = NULL;
  while (list) {
   t = list;
   list = t->_freelist;
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

static const char *ast_type_names[] =
  { "unknown"
  , "error"
  , "emptystat"
  , "caluse"
  , "ifstat"
  , "whilestat"
  , "dostat"
  , "fornum"
  , "forlist"
  , "repeatstat"
  , "localfunc"
  , "localstat"
  , "labelstat"
  , "retstat"
  , "breakstat"
  , "gotostat"
  , "exprstat"
  , "assign"
  , "constructor"
  , "vnil"
  , "vbool"
  , "vint"
  , "vflt"
  , "vstr"
  , "vname"
  , "vararg"
  , "unary"
  , "bin"
  , "var"
  , "vtype"
  , "call"
  , "func"
  , "field"
  };

static inline void push_asttype(lua_State *L, AstNodeType type) {
  lua_getiuservalue(L, 1, 2);
  if (lua_rawgeti(L, -1, type)!=LUA_TSTRING) {
    lua_pushstring(L, ast_type_names[type]);
    lua_copy(L, -1, -2);
    lua_rawseti(L, -3, type);
  }
  lua_replace(L, -2);
}

void ast_trace_list(AstNodeList *list, int depth, const char* name);

static int chunk_trace(lua_State *L) {
  LuaChunk* ud = luaL_checkudata(L, 1, PCHUNK_NAME);
  ast_trace_list(&(ud->block.stats), 0, "chunk");
  return 0;
}

static void ast_return(lua_State *L, LuaChunk *chunk, AstNode *node);

static int _ast_node(lua_State *L) {
  AstNode *node = (AstNode *)lua_touserdata(L, lua_upvalueindex(1));
  lua_pushvalue(L, lua_upvalueindex(2));
  if (lua_gettop(L) > 1)
    lua_insert(L, 1);
  ast_return(L, (LuaChunk *)lua_touserdata(L, 1), node);
  return lua_gettop(L);
}

static inline void push_astnode(lua_State *L, AstNode *node) {
  if (node) {
    lua_pushlightuserdata(L, node);
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, _ast_node, 2);
  }
  else
    lua_pushnil(L);
}

static int _ast_iter(lua_State *L) {
  AstNodeList *list = (AstNodeList *)lua_touserdata(L, lua_upvalueindex(1));
  LuaChunk *chunk = (LuaChunk *)lua_touserdata(L, lua_upvalueindex(2));
  AstNode *iter;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  if (!list) return 0;
  lua_settop(L, 1);
  lua_pushvalue(L, 1);
  lua_pushvalue(L, lua_upvalueindex(2));
  lua_replace(L, 1);
  for (iter=list->head; iter; iter=iter->_next) {
    lua_pushvalue(L, 2);
	lua_pushvalue(L, 1);
    ast_return(L, chunk, iter);
    lua_call(L, lua_gettop(L)-3, 0);
  }
  return 0;
}

static void push_astlist(lua_State *L, AstNodeList *list) {
  lua_pushlightuserdata(L, list);
  lua_pushvalue(L, 1);
  lua_pushcclosure(L, _ast_iter, 2);
  if (list) {
    lua_Integer n = 0;
    AstNode *iter;
    for (iter=list->head; iter; iter=iter->_next)
      ++n;
    lua_pushinteger(L, n);
  }
  else
    lua_pushinteger(L, 0);
}

static void ast_return(lua_State *L, LuaChunk *chunk, AstNode *node) {
  assert(node);
  push_asttype(L, node->type);
  lua_pushinteger(L, (lua_Integer)(node->ts - chunk->tokens));
  lua_pushinteger(L, (lua_Integer)(node->te - chunk->tokens));
  switch (node->type) {
    case AST_error: {
      lua_getiuservalue(L, 1, 1);
      lua_rawgeti(L, -1, ast(node, error).strid);
      lua_replace(L, -2);
      break;
    }
    case AST_caluse: {
      push_astnode(L, ast(node, caluse).cond);
      push_astlist(L, &ast(node, caluse).body.stats);
      break;
    }
    case AST_ifstat: {
      push_astlist(L, &ast(node, ifstat).caluses);
      break;
    }
    case AST_whilestat: {
      push_astnode(L, ast(node, whilestat).cond);
      push_astlist(L, &ast(node, whilestat).body.stats);
      break;
    }
    case AST_dostat: {
      push_astlist(L, &ast(node, dostat).body.stats);
      break;
    }
    case AST_fornum: {
      push_astnode(L, ast(node, fornum).var);
      push_astnode(L, ast(node, fornum).from);
      push_astnode(L, ast(node, fornum).to);
      push_astnode(L, ast(node, fornum).step);
      push_astlist(L, &ast(node, fornum).body.stats);
      break;
    }
    case AST_forlist: {
      push_astlist(L, &ast(node, forlist).vars);
      push_astlist(L, &ast(node, forlist).iters);
      push_astlist(L, &ast(node, forlist).body.stats);
      break;
    }
    case AST_repeatstat: {
      push_astlist(L, &ast(node, repeatstat).body.stats);
      push_astnode(L, ast(node, repeatstat).cond);
      break;
    }
    case AST_localfunc: {
      push_astnode(L, ast(node, localfunc).func.name);
      lua_pushboolean(L, ast(node, localfunc).func.ismethod);
      push_astlist(L, &ast(node, localfunc).func.params);
      push_astlist(L, &ast(node, localfunc).func.vtypes);
      push_astlist(L, &ast(node, localfunc).func.body.stats);
      break;
    }
    case AST_localstat: {
      push_astlist(L, &ast(node, localstat).vars);
      push_astlist(L, &ast(node, localstat).values);
      break;
    }
    case AST_retstat: {
      push_astlist(L, &ast(node, retstat).values);
      break;
    }
    case AST_exprstat: {
      push_astnode(L, ast(node, exprstat).expr);
      break;
    }
    case AST_assign: {
      push_astlist(L, &ast(node, assign).vars);
      push_astlist(L, &ast(node, assign).values);
      break;
    }
    case AST_constructor: {
      push_astlist(L, &ast(node, constructor).fields);
      break;
    }
    case AST_vnil: {
      lua_pushnil(L);
      break;
    }
    case AST_vbool: {
      lua_pushboolean(L, node->ts->token==TK_TRUE);
      break;
    }
    case AST_vint: {
      lua_pushinteger(L, node->ts->i);
      break;
    }
    case AST_vflt: {
      lua_pushnumber(L, node->ts->n);
      break;
    }
    case AST_vstr: {
      lua_getiuservalue(L, 1, 1);
      lua_rawgeti(L, -1, node->ts->strid);
      lua_replace(L, -2);
      break;
    }
    case AST_vname: {
      push_astnode(L, ast(node, vname).parent);
      break;
    }
    case AST_unary: {
      push_astnode(L, ast(node, unary).expr);
      break;
    }
    case AST_bin: {
      push_astnode(L, ast(node, bin).l);
      push_astnode(L, ast(node, bin).r);
      break;
    }
    case AST_call: {
      push_astnode(L, ast(node, call).name);
      push_astlist(L, &ast(node, call).params);
      break;
    }
    case AST_func: {
      push_astnode(L, ast(node, func).name);
      lua_pushboolean(L, ast(node, func).ismethod);
      push_astlist(L, &ast(node, func).params);
      push_astlist(L, &ast(node, func).vtypes);
      push_astlist(L, &ast(node, func).body.stats);
      break;
    }
    case AST_field: {
      push_astnode(L, ast(node, field).k);
      push_astnode(L, ast(node, field).v);
      break;
    }
    default: {
      break;
    }
  }
}

static int chunk_iter(lua_State *L) {
  LuaChunk* ud = luaL_checkudata(L, 1, PCHUNK_NAME);
  push_astlist(L, &(ud->block.stats));
  return 2;
}

static int chunk_token(lua_State *L) {
  LuaChunk* ud = luaL_checkudata(L, 1, PCHUNK_NAME);
  int i = (int)luaL_checkinteger(L, 2);
  char _cache[2] = { '\0', '\0' };
  if (i>0 && i<ud->ntokens) {
    const Token *tk = ud->tokens+i;
    if (lua_rawgeti(L, lua_upvalueindex(1), tk->token)!=LUA_TSTRING) {
      const char* s = luaX_token2str(tk->token, _cache);
      lua_pushstring(L, s ? s : "<unknown>");
      lua_copy(L, -1, -2);
      lua_rawseti(L, lua_upvalueindex(1), tk->token);
    }
    switch (tk->token) {
    case TK_NIL: lua_pushnil(L); break;
    case TK_FLT: lua_pushnumber(L, tk->n); break;
    case TK_INT: lua_pushinteger(L, tk->i); break;
    case TK_EOS: lua_pushnil(L); break;
    case TK_NAME: case TK_STRING: case TK_COMMENT: case TK_ERROR:
      lua_getiuservalue(L, 1, 1);
      lua_rawgeti(L, -1, tk->strid);
      lua_replace(L, -2);
      break;
    default:
      assert(tk->token < TK_EOS);
      lua_pushvalue(L, -1);
      break;
    }
    lua_pushinteger(L, tk->spos); /* start char offset */
    lua_pushinteger(L, tk->epos); /* enc char offset */
    lua_pushinteger(L, tk->sline); /* start line */
    lua_pushinteger(L, tk->eline); /* end line */
    lua_pushinteger(L, tk->slpos); /* start line char offset */
    lua_pushinteger(L, tk->elpos); /* end line char start offset */
    return 8;
  }
  return 0;
}

static luaL_Reg chunk_mt[] =
  { {"__index", NULL}
  , {"__gc", chunk_gc}
  , {"trace", chunk_trace}
  , {"iter", chunk_iter}
  , {"token", NULL}
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
  ud = (LuaChunk*)lua_newuserdatauv(L, sizeof(LuaChunk), 2);
  memset(ud, 0, sizeof(LuaChunk));
  if (luaL_newmetatable(L, PCHUNK_NAME)) {
    luaL_setfuncs(L, chunk_mt, 0);

    lua_newtable(L); /* tokens */
	lua_pushcclosure(L, chunk_token, 1);
	lua_setfield(L, -2, "token");

	lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
  }
  lua_setmetatable(L, -2);
  lua_pushcfunction(L, f_parser);
  lua_newtable(L);	/* 1: strs */
  lua_pushvalue(L, -1); lua_setiuservalue(L, -4, 1); /* chunk.<uv1> = temp strs */
  lua_pushvalue(L, lua_upvalueindex(2)); lua_setiuservalue(L, -4, 2); /* chunk.<uv2> = ast node types */

  luaZ_init(L, &z, getS, &ls);
  p.z = &z; p.name = chunkname;
  p.chunk = ud;
  luaZ_initbuffer(L, &p.buff);

  lua_pushvalue(L, lua_upvalueindex(1)); /* 2: RESERVED */
  lua_pushlightuserdata(L, &p); /* 3: parser */
  status = lua_pcall(L, 3, 0, 0);
  luaZ_freebuffer(L, &p.buff);
  if (status) lua_error(L);
  return 1;
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
  lua_newtable(L); /* ast node types */
  lua_pushcclosure(L, parse, 2);	lua_setfield(L, -2, "parse");
  return 1;
}

