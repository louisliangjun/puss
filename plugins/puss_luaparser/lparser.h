/*
** $Id: lparser.h $
** Lua Parser
** See Copyright Notice in lua.h
*/

#ifndef lparser_h
#define lparser_h

#include "llimits.h"
#include "lzio.h"
#include "lobject.h"

typedef enum
  { AST_block
  , AST_empty
  , AST_error
  , AST_caluse
  , AST_ifstat
  , AST_whilestat
  , AST_dostat
  , AST_fornum
  , AST_forlist
  , AST_repeatstat
  , AST_funcstat
  , AST_localfunc
  , AST_localstat
  , AST_labelstat
  , AST_retstat
  , AST_breakstat
  , AST_gotostat
  , AST_exprstat
  , AST_assign
  , AST_constructor
  , AST_vnil
  , AST_vbool
  , AST_vint
  , AST_vflt
  , AST_vstr
  , AST_vname
  , AST_vararg
  , AST_unary
  , AST_bin
  , AST_var
  , AST_vtype
  , AST_call
  , AST_func
  , AST_field
} AstNodeType;

typedef struct AstNode	AstNode;

typedef struct AstNodeList {
  AstNode *head;
  AstNode *tail;
} AstNodeList;

typedef struct Block {
  struct Block *previous;
  AstNodeList stats;
  lu_byte isloop;
} Block;

typedef struct FuncDecl {
  AstNodeList params;
  AstNodeList vtypes;
  AstNode *name;
  int ismethod;
  Block body;
} FuncDecl;

struct AstNode {
  // node base
  AstNodeType type;
  const Token *ts; // range start token
  const Token *te; // range end token
  AstNode *_freelist;
  AstNode *_next; // [stat|exp|vtype] next
 union {
  struct {
    TString *msg;
  } error; // <error-message>
  struct {
    AstNode *cond;
    Block body;
  } caluse; // [if|elseif|else] <cond> then <body>
  struct {
    AstNodeList caluses;
  } ifstat; // if cond then ... elseif cond then ... else ... end
  struct {
    AstNode *cond;
    Block body;
  } whilestat;
  struct {
    Block body;
  } dostat;
  struct {
    AstNode *var;
    AstNode *from;
    AstNode *to;
    AstNode *step;
    const Token *_do;
    Block body;
  } fornum;
  struct {
    AstNodeList vars;
    const Token *_in;
    AstNodeList iters;
    const Token *_do;
    Block body;
  } forlist; // forlist -> NAME {,NAME} IN explist forbody 
  struct {
    AstNode *cond;
    Block body;
  } repeatstat;
  struct {
    FuncDecl func;
  } localfunc;
  struct {
    AstNode *expr;
  } exprstat; // a = b or func()
  struct {
    AstNodeList vars;
    const Token *_assign;
    AstNodeList explist;
  } localstat; // local k1,k2,k3 = v1,v2,v3
  struct {
    AstNodeList vars;
    const Token *_assign;
    AstNodeList explist;
  } assign; // k1,k2,k3 = v1,v2,v3
  struct {
    AstNodeList explist;
  } retstat; // stat -> RETURN [explist] [';'] 
  struct {
    AstNodeList fields;
  } constructor;

  // value expr
  struct {
    AstNode *parent;
  } vname;
  struct {
    const Token *attr;
  } var;
  struct {
    AstNode *k;
    AstNode *v;
  } field;
  struct {
    AstNode *expr;
  } unary;
  struct {
    const Token *op;
    AstNode *l;
    AstNode *r;
  } bin;
  struct {
    const Token *indexer;
    AstNode *name;
    AstNodeList params;
  } call;
  FuncDecl func;
 };
};

typedef struct LuaChunk {
  int ntokens;
  Token *tokens;
  AstNode *freelist;
  Block block;
} LuaChunk;

// see static int parse(lua_State* L);
// 
#define UPVAL_IDX_STRMAP	lua_upvalueindex(1)
#define UPVAL_IDX_REVERSED	lua_upvalueindex(2)

/* state needed to generate code for a given function */
typedef struct FuncState {
  struct FuncState *prev;  /* enclosing function */
  struct LexState *ls;  /* lexical state */
  Block *bl;
} FuncState;


LUAI_FUNC void luaY_parser (lua_State *L, LuaChunk *chunk, ZIO *z, Mbuffer *buff,
                                 const char *name, int firstchar);



#endif
