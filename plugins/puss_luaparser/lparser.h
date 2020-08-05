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
  Token* tokens;
  Block block;
} LuaChunk;

// see static int parse(lua_State* L);
// 
#define UPVAL_IDX_STRMAP	lua_upvalueindex(1)
#define UPVAL_IDX_REVERSED	lua_upvalueindex(2)

/*
** Expression and variable descriptor.
** Code generation for variables and expressions can be delayed to allow
** optimizations; An 'expdesc' structure describes a potentially-delayed
** variable/expression. It has a description of its "main" value plus a
** list of conditional jumps that can also produce its value (generated
** by short-circuit operators 'and'/'or').
*/

/* kinds of variables/expressions */
typedef enum {
  VVOID,  /* when 'expdesc' describes the last expression a list,
             this kind means an empty list (so, no expression) */
  VNIL,  /* constant nil */
  VTRUE,  /* constant true */
  VFALSE,  /* constant false */
  VK,  /* constant in 'k'; info = index of constant in 'k' */
  VKFLT,  /* floating constant; nval = numerical float value */
  VKINT,  /* integer constant; ival = numerical integer value */
  VKSTR,  /* string constant; strval = TString address;
             (string is fixed by the lexer) */
  VNONRELOC,  /* expression has its value in a fixed register;
                 info = result register */
  VLOCAL,  /* local variable; var.sidx = stack index (local register);
              var.vidx = relative index in 'actvar.arr'  */
  VUPVAL,  /* upvalue variable; info = index of upvalue in 'upvalues' */
  VCONST,  /* compile-time constant; info = absolute index in 'actvar.arr'  */
  VINDEXED,  /* indexed variable;
                ind.t = table register;
                ind.idx = key's R index */
  VINDEXUP,  /* indexed upvalue;
                ind.t = table upvalue;
                ind.idx = key's K index */
  VINDEXI, /* indexed variable with constant integer;
                ind.t = table register;
                ind.idx = key's value */
  VINDEXSTR, /* indexed variable with literal string;
                ind.t = table register;
                ind.idx = key's K index */
  VJMP,  /* expression is a test/comparison;
            info = pc of corresponding jump instruction */
  VRELOC,  /* expression can put result in any register;
              info = instruction pc */
  VCALL,  /* expression is a function call; info = instruction pc */
  VVARARG  /* vararg expression; info = instruction pc */
} expkind;


#define vkisvar(k)	(VLOCAL <= (k) && (k) <= VINDEXSTR)
#define vkisindexed(k)	(VINDEXED <= (k) && (k) <= VINDEXSTR)


#define NO_JUMP		(-1)


typedef struct expdesc {
  expkind k;
  union {
    lua_Integer ival;    /* for VKINT */
    lua_Number nval;  /* for VKFLT */
    TString *strval;  /* for VKSTR */
    int info;  /* for generic use */
    struct {  /* for indexed variables */
      short idx;  /* index (R or "long" K) */
      lu_byte t;  /* table (register or upvalue) */
    } ind;
    struct {  /* for local variables */
      lu_byte sidx;  /* index in the stack */
      unsigned short vidx;  /* compiler index (in 'actvar.arr')  */
    } var;
  } u;
  int t;  /* patch list of 'exit when true' */
  int f;  /* patch list of 'exit when false' */
  AstNode *expr;
} expdesc;


/* kinds of variables */
#define VDKREG		0   /* regular */
#define RDKCONST	1   /* constant */
#define RDKTOCLOSE	2   /* to-be-closed */
#define RDKCTC		3   /* compile-time constant */

/* description of an active local variable */
typedef union Vardesc {
  struct {
    // TValuefields;  /* constant value (if it is a compile-time constant) */
    lu_byte kind;
    lu_byte sidx;  /* index of the variable in the stack */
    short pidx;  /* index of the variable in the Proto's 'locvars' array */
    TString *name;  /* variable name */
  } vd;
  // TValue k;  /* constant value (if any) */
} Vardesc;



/* description of pending goto statements and label statements */
typedef struct Labeldesc {
  TString *name;  /* label identifier */
  int pc;  /* position in code */
  int line;  /* line where it appeared */
  lu_byte nactvar;  /* number of active variables in that position */
  lu_byte close;  /* goto that escapes upvalues */
} Labeldesc;


/* list of labels or gotos */
typedef struct Labellist {
  Labeldesc *arr;  /* array */
  int n;  /* number of entries in use */
  int size;  /* array size */
} Labellist;


/* dynamic structures used by the parser */
typedef struct Dyndata {
  struct {  /* list of all active local variables */
    Vardesc *arr;
    int n;
    int size;
  } actvar;
  Labellist gt;  /* list of pending gotos */
  Labellist label;   /* list of active labels */
} Dyndata;


/* state needed to generate code for a given function */
typedef struct FuncState {
  struct FuncState *prev;  /* enclosing function */
  struct LexState *ls;  /* lexical state */
  Block *bl;
} FuncState;


LUAI_FUNC void luaY_parser (lua_State *L, LuaChunk *chunk, ZIO *z, Mbuffer *buff,
                                 const char *name, int firstchar);



#endif
