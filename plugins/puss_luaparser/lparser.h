/*
** $Id: lparser.h $
** Lua Parser
** See Copyright Notice in lua.h
*/

#ifndef lparser_h
#define lparser_h

#include <assert.h>

#include "llex.h"

typedef enum
  { AST_error = 1
  , AST_emptystat
  , AST_caluse
  , AST_ifstat
  , AST_whilestat
  , AST_dostat
  , AST_fornum
  , AST_forlist
  , AST_repeatstat
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
  int isloop;
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
    int strid;
    TString *msg;
  } _error; // <error-message>
  struct {
    AstNode *cond;
    Block body;
  } _caluse; // [if|elseif|else] <cond> then <body>
  struct {
    AstNodeList caluses;
  } _ifstat; // if cond then ... elseif cond then ... else ... end
  struct {
    AstNode *cond;
    Block body;
  } _whilestat;
  struct {
    Block body;
  } _dostat;
  struct {
    AstNode *var;
    AstNode *from;
    AstNode *to;
    AstNode *step;
    const Token *_do;
    Block body;
  } _fornum;
  struct {
    AstNodeList vars;
    const Token *_in;
    AstNodeList iters;
    const Token *_do;
    Block body;
  } _forlist; // forlist -> NAME {,NAME} IN explist forbody 
  struct {
    AstNode *cond;
    Block body;
  } _repeatstat;
  struct {
    FuncDecl func;
  } _localfunc;
  struct {
    AstNode *expr;
  } _exprstat; // a = b or func()
  struct {
    AstNodeList vars;
    const Token *_assign;
    AstNodeList values;
  } _localstat; // local k1,k2,k3 = v1,v2,v3
  struct {
    AstNodeList vars;
    const Token *_assign;
    AstNodeList values;
  } _assign; // k1,k2,k3 = v1,v2,v3
  struct {
    AstNodeList values;
  } _retstat; // stat -> RETURN [explist] [';'] 
  struct {
    AstNodeList fields;
  } _constructor;

  // value expr
  struct {
    AstNode *parent;
  } _vname;
  struct {
    const Token *attr;
  } _var;
  struct {
    AstNode *k;
    AstNode *v;
  } _field;
  struct {
    AstNode *expr;
  } _unary;
  struct {
    const Token *op;
    AstNode *l;
    AstNode *r;
  } _bin;
  struct {
    const Token *indexer;
    AstNode *name;
    AstNodeList params;
  } _call;
  FuncDecl _func;
 };
};

#define ast(node, T)    ((assert((node)->type==AST_ ## T), node)-> _ ## T)

typedef struct LuaChunk {
  int ntokens;
  Token *tokens;
  AstNode *freelist;
  Block block;
} LuaChunk;

/* state needed to generate code for a given function */
typedef struct FuncState {
  struct FuncState *prev;  /* enclosing function */
  struct LexState *ls;  /* lexical state */
  Block *bl;
} FuncState;


#define IDXOF_STR_MAP	1
#define IDXOF_REVERSED	2

LUAI_FUNC void luaY_parser (lua_State *L, LuaChunk *chunk, ZIO *z, Mbuffer *buff,
                                 const char *name, int firstchar);


#endif
