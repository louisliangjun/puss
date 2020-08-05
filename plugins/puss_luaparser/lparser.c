/*
** $Id: lparser.c $
** Lua Parser
** See Copyright Notice in lua.h
*/

#define lparser_c
#define LUA_CORE

#include "lprefix.h"


#include <limits.h>
#include <string.h>
#include <assert.h>

#include "puss_plugin.h"

#include "lmem.h"
#include "llex.h"
#include "lparser.h"

#define CTK         (ls->tokens + ls->ctoken)


static AstNode *ast_node_new(LexState *ls, AstNodeType tp, const Token *ts, const Token *te) {
  AstNode *node = luaM_new(ls->L, AstNode);
  if (!node) luaL_error(ls->L, "memory error");
  node->type = tp;
  node->ts = ts;
  node->te = te;
  node->_freelist = ls->freelist;
  ls->freelist = node;
  return node;
}


static AstNode *ast_error_new(LexState *ls, const char* msg, size_t len, const Token *ts, const Token *te) {
  AstNode *err = ast_node_new(ls, AST_error, ts, te);
  err->error.msg = luaX_newstring(ls, msg, len);
  return err;
}


#define ast_error_newliteral(ls, msg, ts, te)  ast_error_new((ls), (msg), (sizeof(msg)/sizeof(char))-1, (ts), (te))


static inline void ast_nodelist_append(AstNodeList* list, AstNode *node) {
  if (list->head)
    list->tail->_next = node;
  else
    list->head = node;
  list->tail = node;
}


#define stats_append(ls, stat)  ast_nodelist_append(&((ls)->fs->bl->stats), (stat))


/*
** prototypes for recursive non-terminal functions
*/
static void statement (LexState *ls);
static void expr (LexState *ls, AstNode **v);


/*
** Test whether next token is 'c'; if so, skip it.
*/
static inline int testnext (LexState *ls, int c) {
  if (ls->t.token == c) {
    luaX_next(ls);
    return 1;
  }
  else return 0;
}


/*
** Check that next token is 'c'.
*/
#define check(ls,c)      (ls->t.token == (c))


/*
** Check that next token is 'c' and skip it.
*/
#define checknext(ls,c)  testnext((ls),(c))


static void skip_vartype (LexState *ls) {
  if (!testnext(ls, TK_NAME)) {
    // luaX_syntaxerror(ls, "var type expected");
  }
  if (!testnext(ls, '<'))
	return;
  skip_vartype(ls);
  while (testnext(ls, ','))
    skip_vartype(ls);
  switch (ls->t.token) {
    case TK_SHR: ls->t.token = '>'; break;
    case TK_GE:  ls->t.token = '='; break;
    default:
      if (!testnext(ls, '>')) {
        stats_append(ls, ast_error_newliteral(ls, "template type MUST endswith '>'", CTK, CTK));
      }
  }
}


static void test_funtype (LexState *ls, FuncDecl *e) {
  AstNode *vtype;
  if (!testnext(ls, ':'))
    return;
  vtype = ast_node_new(ls, AST_vtype, CTK, CTK);
  skip_vartype(ls);
  vtype->te = CTK;
  ast_nodelist_append(&(e->vtypes), vtype);
  while (testnext(ls, ',')) {
    vtype = ast_node_new(ls, AST_vtype, CTK, CTK);
    skip_vartype(ls);
    vtype->te = CTK;
	ast_nodelist_append(&(e->vtypes), vtype);
  }
}


static inline void enterblock (FuncState *fs, Block *bl, lu_byte isloop) {
  bl->previous = fs->bl;
  bl->isloop = isloop;
  fs->bl = bl;
}


static inline void leaveblock (FuncState *fs) {
  fs->bl = fs->bl->previous;
}


static void open_func (LexState *ls, FuncState *fs, Block *bl) {
  fs->prev = ls->fs;  /* linked list of funcstates */
  fs->ls = ls;
  ls->fs = fs;
  fs->bl = NULL;
  enterblock(fs, bl, 0);
}


static void close_func (LexState *ls) {
  FuncState *fs = ls->fs;
  leaveblock(fs);
  lua_assert(fs->bl == NULL);
  ls->fs = fs->prev;
}



/*============================================================*/
/* GRAMMAR RULES */
/*============================================================*/


/*
** check whether current token is in the follow set of a block.
** 'until' closes syntactical blocks, but do not close scope,
** so it is handled in separate.
*/
static int block_follow (LexState *ls, int withuntil) {
  switch (ls->t.token) {
    case TK_ELSE: case TK_ELSEIF:
    case TK_END: case TK_EOS:
      return 1;
    case TK_UNTIL: return withuntil;
    default: return 0;
  }
}


static void statlist (LexState *ls) {
  /* statlist -> { stat [';'] } */
  while (!block_follow(ls, 1)) {
    if (ls->t.token == TK_RETURN) {
      statement(ls);
      return;  /* 'return' must be last statement */
    }
    statement(ls);
  }
}


static void fieldsel (LexState *ls, AstNode **v) {
  /* fieldsel -> ['.' | ':'] NAME */
  AstNode *exp;
  luaX_next(ls);  /* skip the dot or colon */
  exp = ast_node_new(ls, AST_vname, CTK, CTK);
  exp->vname.parent = *v;
  *v = exp;
  checknext(ls, TK_NAME);
}


static void yindex (LexState *ls, AstNode **v) {
  /* index -> '[' expr ']' */
  luaX_next(ls);  /* skip the '[' */
  expr(ls, v);
  checknext(ls, ']');
}


/*
** {======================================================================
** Rules for Constructors
** =======================================================================
*/

static void recfield (LexState *ls, AstNodeList *fields) {
  /* recfield -> (NAME | '['exp']') = exp */
  AstNode *stat = ast_node_new(ls, AST_field, CTK, CTK);
  if (ls->t.token == TK_NAME) {
    stat->field.k = ast_node_new(ls, AST_var, CTK, CTK);
    if (testnext(ls, ':')) {
      skip_vartype(ls);
      stat->field.k->te = CTK;
    }
  }
  else { /* ls->t.token == '[' */
    yindex(ls, &(stat->field.k));
  }
  checknext(ls, '=');
  expr(ls, &(stat->field.v));
}


static void listfield (LexState *ls, AstNodeList *fields) {
  /* listfield -> exp */
  AstNode *v = NULL;
  expr(ls, &v);
  ast_nodelist_append(fields, v);
}


static void field (LexState *ls, AstNodeList *fields) {
  /* field -> listfield | recfield */
  switch(ls->t.token) {
    case TK_NAME: {  /* may be 'listfield' or 'recfield' */
      if (luaX_lookahead(ls) != '=')  /* expression? */
        listfield(ls, fields);
      else
        recfield(ls, fields);
      break;
    }
    case '[': {
      recfield(ls, fields);
      break;
    }
    default: {
      listfield(ls, fields);
      break;
    }
  }
}


static void constructor (LexState *ls, AstNode **t) {
  /* constructor -> '{' [ field { sep field } [sep] ] '}'
     sep -> ',' | ';' */
  *t = ast_node_new(ls, AST_constructor, CTK, CTK);
  checknext(ls, '{');
  do {
    if (ls->t.token == '}') break;
    field(ls, &((*t)->constructor.fields));
  } while (testnext(ls, ',') || testnext(ls, ';'));
  if (!testnext(ls, '}')) {
    stats_append(ls, ast_error_newliteral(ls, "match {} failed", CTK, CTK));
  }
}

/* }====================================================================== */


static void parlist (LexState *ls, FuncDecl *e) {
  /* parlist -> [ param { ',' param } ] */
  int isvararg = 0;
  AstNode *param;
  if (ls->t.token != ')') {  /* is 'parlist' not empty? */
    do {
      switch (ls->t.token) {
        case TK_NAME: {  /* param -> NAME */
          param = ast_node_new(ls, AST_var, CTK, CTK);
          luaX_next(ls);
          if (testnext(ls, ':')) {
            skip_vartype(ls);
            param->te = CTK;
          }
          ast_nodelist_append(&(e->params), param);
          break;
        }
        case TK_DOTS: {  /* param -> '...' */
          param = ast_node_new(ls, AST_var, CTK, CTK);
          luaX_next(ls);
          isvararg = 1;
          ast_nodelist_append(&(e->params), param);
          break;
        }
        default: {
          stats_append(ls, ast_error_newliteral(ls, "<name> or '...' expected", CTK, CTK));
        }
      }
    } while (!isvararg && testnext(ls, ','));
  }
}


static void body (LexState *ls, FuncDecl *e, int ismethod, int line) {
  /* body ->  '(' parlist ')' block END */
  FuncState new_fs;
  open_func(ls, &new_fs, &(e->body));
  checknext(ls, '(');
  e->ismethod = ismethod;
  parlist(ls, e);
  checknext(ls, ')');
  test_funtype(ls, e);
  statlist(ls);
  if (!testnext(ls, TK_END))
    stats_append(ls, ast_error_newliteral(ls, "match FUNCTION END failed", CTK, CTK));
  close_func(ls);
}


static void explist (LexState *ls, AstNodeList *list) {
  /* explist -> expr { ',' expr } */
  AstNode *v = NULL;
  expr(ls, &v);
  ast_nodelist_append(list, v);
  while (testnext(ls, ',')) {
    expr(ls, &v);
    ast_nodelist_append(list, v);
  }
}


static int funcargs (LexState *ls, AstNodeList *params) {
  switch (ls->t.token) {
    case '(': {  /* funcargs -> '(' [ explist ] ')' */
      luaX_next(ls);
      if (ls->t.token != ')')  /* arg list is empty? */
        explist(ls, params);
      if (!testnext(ls, ')'))
        stats_append(ls, ast_error_newliteral(ls, "match () failed", CTK, CTK));
      break;
    }
    case '{': {  /* funcargs -> constructor */
      AstNode *v = NULL;
      constructor(ls, &v);
      ast_nodelist_append(params, v);
      break;
    }
    case TK_STRING: {  /* funcargs -> STRING */
      ast_nodelist_append(params, ast_node_new(ls, AST_vstr, CTK, CTK));
      luaX_next(ls);  /* must use 'seminfo' before 'next' */
      break;
    }
    default: {
      ast_nodelist_append(params, ast_error_newliteral(ls, "unexpected symbol", CTK, CTK));
    }
  }
  return 0;
}




/*
** {======================================================================
** Expression parsing
** =======================================================================
*/


static void primaryexp (LexState *ls, AstNode **v) {
  /* primaryexp -> NAME | '(' expr ')' */
  switch (ls->t.token) {
    case '(': {
      luaX_next(ls);
      expr(ls, v);
      if (!testnext(ls, ')'))
        stats_append(ls, ast_error_newliteral(ls, "match () failed", CTK, CTK));
      break;
    }
    case TK_NAME: {
      *v = ast_node_new(ls, AST_vname, CTK, CTK);
      luaX_next(ls);
      break;
    }
    default: {
      *v = ast_error_newliteral(ls, "unexpected symbol", CTK, CTK);
      luaX_next(ls);
      break;
    }
  }
}


static void suffixedexp (LexState *ls, AstNode **v) {
  /* suffixedexp ->
       primaryexp { '.' NAME | '[' exp ']' | ':' NAME funcargs | funcargs } */
  primaryexp(ls, v);
  for (;;) {
    switch (ls->t.token) {
      case '.': {  /* fieldsel */
        fieldsel(ls, v);
        break;
      }
      case '[': {  /* '[' exp ']' */
        yindex(ls, v);
        break;
      }
      case ':': {  /* ':' NAME funcargs */
        AstNode *exp = ast_node_new(ls, AST_call, CTK, CTK);
        exp->call.indexer = CTK;
        luaX_next(ls);
        exp->call.name = ast_node_new(ls, AST_vname, CTK, CTK);
		exp->call.name->vname.parent = *v;
		*v = exp;
		if (check(ls, TK_NAME))
          luaX_next(ls);
        funcargs(ls, &(exp->call.params));
        break;
      }
      case '(': case TK_STRING: case '{': {  /* funcargs */
        AstNode *exp = ast_node_new(ls, AST_call, CTK, CTK);
        exp->call.name = *v;
		*v = exp;
        funcargs(ls, &(exp->call.params));
        break;
      }
      default: return;
    }
  }
}


static void simpleexp (LexState *ls, AstNode **v) {
  /* simpleexp -> FLT | INT | STRING | NIL | TRUE | FALSE | ... |
                  constructor | FUNCTION body | suffixedexp */
  switch (ls->t.token) {
    case TK_FLT: {
      *v = ast_node_new(ls, AST_vflt, CTK, CTK);
      break;
    }
    case TK_INT: {
      *v = ast_node_new(ls, AST_vint, CTK, CTK);
      break;
    }
    case TK_STRING: {
      *v = ast_node_new(ls, AST_vstr, CTK, CTK);
      break;
    }
    case TK_NIL: {
      *v = ast_node_new(ls, AST_vnil, CTK, CTK);
      break;
    }
    case TK_TRUE: {
      *v = ast_node_new(ls, AST_vbool, CTK, CTK);
      break;
    }
    case TK_FALSE: {
      *v = ast_node_new(ls, AST_vbool, CTK, CTK);
      break;
    }
    case TK_DOTS: {  /* vararg */
      *v = ast_node_new(ls, AST_vararg, CTK, CTK);
      break;
    }
    case '{': {  /* constructor */
      constructor(ls, v);
      return;
    }
    case TK_FUNCTION: {
      AstNode *exp = ast_node_new(ls, AST_func, CTK, CTK);
      *v = exp;
      luaX_next(ls);
      body(ls, &((*v)->func), 0, ls->linenumber);
      exp->te = CTK;
      return;
    }
    default: {
      suffixedexp(ls, v);
      return;
    }
  }
  luaX_next(ls);
}


typedef enum UnOpr { OPR_MINUS, OPR_BNOT, OPR_NOT, OPR_LEN, OPR_NOUNOPR } UnOpr;


/*
** grep "ORDER OPR" if you change these enums  (ORDER OP)
*/
typedef enum BinOpr {
  /* arithmetic operators */
  OPR_ADD, OPR_SUB, OPR_MUL, OPR_MOD, OPR_POW,
  OPR_DIV, OPR_IDIV,
  /* bitwise operators */
  OPR_BAND, OPR_BOR, OPR_BXOR,
  OPR_SHL, OPR_SHR,
  /* string operator */
  OPR_CONCAT,
  /* comparison operators */
  OPR_EQ, OPR_LT, OPR_LE,
  OPR_NE, OPR_GT, OPR_GE,
  /* logical operators */
  OPR_AND, OPR_OR,
  OPR_NOBINOPR
} BinOpr;


static UnOpr getunopr (int op) {
  switch (op) {
    case TK_NOT: return OPR_NOT;
    case '-': return OPR_MINUS;
    case '~': return OPR_BNOT;
    case '#': return OPR_LEN;
    default: return OPR_NOUNOPR;
  }
}


static BinOpr getbinopr (int op) {
  switch (op) {
    case '+': return OPR_ADD;
    case '-': return OPR_SUB;
    case '*': return OPR_MUL;
    case '%': return OPR_MOD;
    case '^': return OPR_POW;
    case '/': return OPR_DIV;
    case TK_IDIV: return OPR_IDIV;
    case '&': return OPR_BAND;
    case '|': return OPR_BOR;
    case '~': return OPR_BXOR;
    case TK_SHL: return OPR_SHL;
    case TK_SHR: return OPR_SHR;
    case TK_CONCAT: return OPR_CONCAT;
    case TK_NE: return OPR_NE;
    case TK_EQ: return OPR_EQ;
    case '<': return OPR_LT;
    case TK_LE: return OPR_LE;
    case '>': return OPR_GT;
    case TK_GE: return OPR_GE;
    case TK_AND: return OPR_AND;
    case TK_OR: return OPR_OR;
    default: return OPR_NOBINOPR;
  }
}


/*
** Priority table for binary operators.
*/
static const struct {
  lu_byte left;  /* left priority for each binary operator */
  lu_byte right; /* right priority */
} priority[] = {  /* ORDER OPR */
   {10, 10}, {10, 10},           /* '+' '-' */
   {11, 11}, {11, 11},           /* '*' '%' */
   {14, 13},                  /* '^' (right associative) */
   {11, 11}, {11, 11},           /* '/' '//' */
   {6, 6}, {4, 4}, {5, 5},   /* '&' '|' '~' */
   {7, 7}, {7, 7},           /* '<<' '>>' */
   {9, 8},                   /* '..' (right associative) */
   {3, 3}, {3, 3}, {3, 3},   /* ==, <, <= */
   {3, 3}, {3, 3}, {3, 3},   /* ~=, >, >= */
   {2, 2}, {1, 1}            /* and, or */
};

#define UNARY_PRIORITY	12  /* priority for unary operators */


/*
** subexpr -> (simpleexp | unop subexpr) { binop subexpr }
** where 'binop' is any binary operator with a priority higher than 'limit'
*/
static BinOpr subexpr (LexState *ls, AstNode **v, int limit) {
  BinOpr op;
  UnOpr uop;
  uop = getunopr(ls->t.token);
  if (uop != OPR_NOUNOPR) {  /* prefix (unary) operator? */
    AstNode *exp = ast_node_new(ls, AST_unary, CTK, CTK);
    luaX_next(ls);  /* skip operator */
    subexpr(ls, &(exp->unary.expr), UNARY_PRIORITY);
    *v = exp;
  }
  else simpleexp(ls, v);
  /* expand while operators have priorities higher than 'limit' */
  op = getbinopr(ls->t.token);
  while (op != OPR_NOBINOPR && priority[op].left > limit) {
    AstNode *exp = ast_node_new(ls, AST_bin, (*v)->ts, CTK);
    BinOpr nextop;
	exp->bin.op = CTK;
	exp->bin.l = *v;
    luaX_next(ls);  /* skip operator */
    // luaK_infix(ls->fs, op, v);
    /* read sub-expression with higher priority */
    nextop = subexpr(ls, &(exp->bin.r), priority[op].right);
    op = nextop;
  }
  return op;  /* return first untreated operator */
}


static void expr (LexState *ls, AstNode **v) {
  subexpr(ls, v, 0);
}

/* }==================================================================== */



/*
** {======================================================================
** Rules for Statements
** =======================================================================
*/


static void block (LexState *ls, Block *b) {
  /* block -> statlist */
  FuncState *fs = ls->fs;
  enterblock(fs, b, 0);
  statlist(ls);
  leaveblock(fs);
}


/*
** structure to chain all variables in the left-hand side of an
** assignment
*/
struct LHS_assign {
  struct LHS_assign *prev;
  AstNode *v;  /* variable (global, local, upvalue, or indexed) */
};


/*
** Parse and compile a multiple assignment. The first "variable"
** (a 'suffixedexp') was already read by the caller.
**
** assignment -> suffixedexp restassign
** restassign -> ',' suffixedexp restassign | '=' explist
*/
static void restassign (LexState *ls, struct LHS_assign *lh, int nvars, AstNode *stat) {
  assert( stat->type==AST_assign );
  if (testnext(ls, ',')) {  /* restassign -> ',' suffixedexp restassign */
    struct LHS_assign nv;
    nv.prev = lh;
    suffixedexp(ls, &nv.v);
    ast_nodelist_append(&(stat->assign.vars), nv.v);
    restassign(ls, &nv, nvars+1, stat);
  }
  else {  /* restassign -> '=' explist */
    checknext(ls, '=');
    explist(ls, &(stat->assign.explist));
  }
}


static void gotostat (LexState *ls, const Token *tk) {
  if (check(ls, TK_NAME)) {
    stats_append(ls, ast_node_new(ls, AST_gotostat, tk, CTK));
    luaX_next(ls);
  } else {
    stats_append(ls, ast_error_newliteral(ls, "gotostat need name", CTK, CTK));
  }
}


/*
** Break statement. Semantically equivalent to "goto break".
*/
static void breakstat (LexState *ls) {
  stats_append(ls, ast_node_new(ls, AST_breakstat, CTK, CTK));
  luaX_next(ls);  /* skip break */
}


static void labelstat (LexState *ls, const Token *tk) {
  AstNode *stat;
  /* label -> '::' NAME '::' */
  if (!check(ls, TK_NAME)) {
    stats_append(ls, ast_error_newliteral(ls, "labelstat need name", CTK, CTK));
    return;
  }
  luaX_next(ls);
  stat = ast_node_new(ls, AST_labelstat, tk, CTK);
  stats_append(ls, stat);
  checknext(ls, TK_DBCOLON);  /* skip double colon */
}


static void whilestat (LexState *ls, int line) {
  /* whilestat -> WHILE cond DO block END */
  FuncState *fs = ls->fs;
  AstNode *stat = ast_node_new(ls, AST_whilestat, CTK, CTK);
  luaX_next(ls);  /* skip WHILE */
  expr(ls, &(stat->whilestat.cond));  /* read condition */
  enterblock(fs, &(stat->whilestat.body), 1);
  checknext(ls, TK_DO);
  statlist(ls);
  if (!testnext(ls, TK_END))
    stats_append(ls, ast_error_newliteral(ls, "match WHILE END failed", CTK, CTK));
  leaveblock(fs);
}


static void repeatstat (LexState *ls, int line) {
  /* repeatstat -> REPEAT block UNTIL cond */
  FuncState *fs = ls->fs;
  AstNode *stat = ast_node_new(ls, AST_repeatstat, CTK, CTK);
  enterblock(fs, &(stat->repeatstat.body), 1);
  luaX_next(ls);  /* skip REPEAT */
  statlist(ls);
  if (!testnext(ls, TK_UNTIL))
    stats_append(ls, ast_error_newliteral(ls, "match REPEAT UNTIL failed", CTK, CTK));
  expr(ls, &(stat->repeatstat.cond));  /* read condition (inside scope block) */
  leaveblock(fs);  /* finish loop */
}


/*
** Generate code for a 'for' loop.
*/
static void forbody (LexState *ls, Block *body) {
  /* forbody -> DO block */
  FuncState *fs = ls->fs;
  checknext(ls, TK_DO);
  enterblock(fs, body, 1);  /* scope for declared variables */
  statlist(ls);
  leaveblock(fs);  /* end of scope for declared variables */
}


static void fornum (LexState *ls, AstNode *var, int line, const Token *tk) {
  /* fornum -> NAME = exp,exp[,exp] forbody */
  AstNode *stat = ast_node_new(ls, AST_fornum, tk, CTK);
  stat->fornum.var = var;
  checknext(ls, '=');
  expr(ls, &(stat->fornum.from)); /* initial value */
  checknext(ls, ',');
  expr(ls, &(stat->fornum.to));   /* limit */
  if (testnext(ls, ','))
    expr(ls, &(stat->fornum.step));  /* optional step */
  forbody(ls, &(stat->fornum.body));
  stat->te = CTK;
}


static void forlist (LexState *ls, AstNode *var, const Token *tk) {
  /* forlist -> NAME {,NAME} IN explist forbody */
  AstNode *stat = ast_node_new(ls, AST_forlist, tk, CTK);
  ast_nodelist_append(&(stat->forlist.vars), var);
  while (testnext(ls, ',')) {
    if (!check(ls, TK_NAME)) {
      stats_append(ls, ast_error_newliteral(ls, "forlist need name", CTK, CTK));
      continue;
    }
    var = ast_node_new(ls, AST_var, CTK, CTK);
    ast_nodelist_append(&(stat->forlist.vars), var);
    luaX_next(ls);
    if (testnext(ls, ':')) {
      skip_vartype(ls);
      var->te = CTK;
      ast_nodelist_append(&(stat->forlist.vars), var);
    }
  }
  stat->forlist._in = CTK;
  checknext(ls, TK_IN);

  explist(ls, &(stat->forlist.iters));
  forbody(ls, &(stat->forlist.body));
  stat->te = CTK;
}


static void forstat (LexState *ls, int line, const Token *tk) {
  /* forstat -> FOR (fornum | forlist) END */
  FuncState *fs = ls->fs;
  AstNode *var = NULL;
  enterblock(fs, ls->fs->bl, 1);  /* scope for loop and control variables */
  luaX_next(ls);  /* skip 'for' */
  if (!check(ls, TK_NAME)) {
    stats_append(ls, ast_error_newliteral(ls, "forstat need name", tk, CTK));
    return;
  }
  var = ast_node_new(ls, AST_var, CTK, CTK);
  if (testnext(ls, ':')) {
    skip_vartype(ls);
    var->te = CTK;
  }
  switch (ls->t.token) {
    case '=':
      fornum(ls, var, line, tk);
      break;
    case ',': case TK_IN:
      forlist(ls, var, tk);
      break;
    default:
      stats_append(ls, ast_error_newliteral(ls, "'=' or 'in' expected", tk, CTK));
      return;
  }
  if (!testnext(ls, TK_END))
    stats_append(ls, ast_error_newliteral(ls, "match FOR END failed", tk, CTK));
  leaveblock(fs);  /* loop scope ('break' jumps to this point) */
}


static void test_then_block (LexState *ls, AstNodeList *caluses) {
  /* test_then_block -> [IF | ELSEIF] cond THEN block */
  FuncState *fs = ls->fs;
  AstNode *exp = ast_node_new(ls, AST_caluse, CTK, CTK);
  ast_nodelist_append(caluses, exp);
  luaX_next(ls);  /* skip IF or ELSEIF */
  expr(ls, &(exp->caluse.cond));  /* read condition */
  checknext(ls, TK_THEN);
  enterblock(fs, &(exp->caluse.body), 0);
  statlist(ls);  /* 'then' part */
  leaveblock(fs);
}


static void ifstat (LexState *ls, int line) {
  /* ifstat -> IF cond THEN block {ELSEIF cond THEN block} [ELSE block] END */
  AstNode *stat = ast_node_new(ls, AST_ifstat, CTK, CTK);
  test_then_block(ls, &(stat->ifstat.caluses));  /* IF cond THEN block */
  while (ls->t.token == TK_ELSEIF)
    test_then_block(ls, &(stat->ifstat.caluses));  /* ELSEIF cond THEN block */
  if (testnext(ls, TK_ELSE)) {
    AstNode *exp = ast_node_new(ls, AST_caluse, CTK, CTK);
    ast_nodelist_append(&(stat->ifstat.caluses), exp);
    block(ls, &(exp->caluse.body));  /* 'else' part */
    exp->te = CTK;
  }
  if (!testnext(ls, TK_END))
    stats_append(ls, ast_error_newliteral(ls, "match IF END failed", CTK, CTK));
  stat->te = CTK;
}


static void localfunc (LexState *ls, const Token *tk) {
  AstNode *stat;
  if (!check(ls, TK_NAME)) {
    stats_append(ls, ast_error_newliteral(ls, "localfunc need name", CTK, CTK));
    return;
  }
  stat = ast_node_new(ls, AST_localfunc, tk, CTK);
  stats_append(ls, stat);
  stat->localfunc.func.name = ast_node_new(ls, AST_vname, CTK, CTK);
  luaX_next(ls);
  body(ls, &(stat->localfunc.func), 0, ls->linenumber);  /* function created in next register */
  stat->te = CTK;
}


static void getlocalattribute (LexState *ls, AstNode *v) {
  /* ATTRIB -> ['<' Name '>'] */
  if (testnext(ls, '<')) {
    v->var.attr = CTK;
    luaX_next(ls);
    checknext(ls, TK_NAME);
    checknext(ls, '>');
  }
}


static void localstat (LexState *ls, const Token *tk) {
  /* stat -> LOCAL ATTRIB NAME {',' ATTRIB NAME} ['=' explist] */
  AstNode *stat = ast_node_new(ls, AST_localstat, tk, CTK);
  do {
    if (!check(ls, TK_NAME)) {
      stats_append(ls, ast_error_newliteral(ls, "localstat need name", CTK, CTK));
    } else {
      AstNode *v = ast_node_new(ls, AST_var, CTK, CTK);
      luaX_next(ls);
      if (testnext(ls, ':')) {
        skip_vartype(ls);
        v->te = CTK;
      }
      ast_nodelist_append(&(stat->localstat.vars), v);
      getlocalattribute(ls, v);
    }
  } while (testnext(ls, ','));
  if (testnext(ls, '='))
    explist(ls, &(stat->localstat.explist));
}


static int funcname (LexState *ls, AstNode **v, const Token *tk) {
  /* funcname -> NAME {fieldsel} [':' NAME] */
  int ismethod = 0;
  if (check(ls, TK_NAME)) {
    *v = ast_node_new(ls, AST_var, tk, CTK);
    luaX_next(ls);
  }
  else {
    stats_append(ls, ast_error_newliteral(ls, "funcname need name", tk, CTK));
    return ismethod;
  }
  while (ls->t.token == '.') {
    fieldsel(ls, v);
  }
  if (ls->t.token == ':') {
    ismethod = 1;
    fieldsel(ls, v);
  }
  return ismethod;
}


static void funcstat (LexState *ls, int line, const Token *tk) {
  /* funcstat -> FUNCTION funcname body */
  int ismethod;
  AstNode *f = ast_node_new(ls, AST_func, CTK, CTK);
  luaX_next(ls);  /* skip FUNCTION */
  ismethod = funcname(ls, &(f->func.name), tk);
  body(ls, &(f->func), ismethod, line);
}


static void exprstat (LexState *ls) {
  /* stat -> func | assignment */
  struct LHS_assign v;
  const Token *ts = CTK;
  suffixedexp(ls, &v.v);
  if (ls->t.token == '=' || ls->t.token == ',') { /* stat -> assignment ? */
    AstNode *stat = ast_node_new(ls, AST_assign, ts, CTK);
    stat->exprstat.expr = v.v;
    v.prev = NULL;
    restassign(ls, &v, 1, stat);
    stat->te = CTK;
  }
  else {  /* stat -> func */
    if (v.v && v.v->type == AST_call) {
      AstNode *stat = ast_node_new(ls, AST_exprstat, ts, CTK);
      stat->exprstat.expr = v.v;
      stats_append(ls, stat);
    } else {
      stats_append(ls, ast_error_newliteral(ls, "syntax error", CTK, CTK));
    }
  }
}


static void retstat (LexState *ls, const Token *tk) {
  /* stat -> RETURN [explist] [';'] */
  AstNode *stat = ast_node_new(ls, AST_retstat, tk, tk);
  if (block_follow(ls, 1)) {
    /* return no values */
  }
  else if (ls->t.token == ';') {
    /* return no values */
    stat->te = CTK;
  }
  else {
    explist(ls, &(stat->retstat.explist));  /* optional return values */
  }
  testnext(ls, ';');  /* skip optional semicolon */
}


static void statement (LexState *ls) {
  const Token *tk = CTK;
  int line = ls->linenumber;  /* may be needed for error messages */
  switch (ls->t.token) {
    case ';': {  /* stat -> ';' (empty statement) */
      stats_append(ls, ast_node_new(ls, AST_empty, tk, tk));
      luaX_next(ls);  /* skip ';' */
      break;
    }
    case TK_IF: {  /* stat -> ifstat */
      ifstat(ls, line);
      break;
    }
    case TK_WHILE: {  /* stat -> whilestat */
      whilestat(ls, line);
      break;
    }
    case TK_DO: {  /* stat -> DO block END */
      AstNode *stat = ast_node_new(ls, AST_dostat, tk, tk);
      luaX_next(ls);  /* skip DO */
      block(ls, &(stat->dostat.body));
      if (!testnext(ls, TK_END))
        stats_append(ls, ast_error_newliteral(ls, "match DO END failed", CTK, CTK));
      break;
    }
    case TK_FOR: {  /* stat -> forstat */
      forstat(ls, line, tk);
      break;
    }
    case TK_REPEAT: {  /* stat -> repeatstat */
      repeatstat(ls, line);
      break;
    }
    case TK_FUNCTION: {  /* stat -> funcstat */
      funcstat(ls, line, tk);
      break;
    }
    case TK_LOCAL: {  /* stat -> localstat */
      luaX_next(ls);  /* skip LOCAL */
      if (testnext(ls, TK_FUNCTION)) /* local function? */
        localfunc(ls, tk);
	  else
        localstat(ls, tk);
      break;
    }
    case TK_DBCOLON: {  /* stat -> label */
      luaX_next(ls);  /* skip double colon */
      labelstat(ls, tk);
      break;
    }
    case TK_RETURN: {  /* stat -> retstat */
      luaX_next(ls);  /* skip RETURN */
      retstat(ls, tk);
      break;
    }
    case TK_BREAK: {  /* stat -> breakstat */
      breakstat(ls);
      break;
    }
    case TK_GOTO: {  /* stat -> 'goto' NAME */
      luaX_next(ls);  /* skip 'goto' */
      gotostat(ls, tk);
      break;
    }
    default: {  /* stat -> func | assignment */
      exprstat(ls);
      break;
    }
  }
}

/* }====================================================================== */


/*
** compiles the main function, which is a regular vararg function with an
** upvalue named LUA_ENV
*/
static void mainfunc (LexState *ls, FuncState *fs, Block* block) {
  open_func(ls, fs, block);
  luaX_next(ls);
  for (;;) {
    statlist(ls);  /* parse main body */
    if (check(ls, TK_EOS))
      break;
    stats_append(ls, ast_error_newliteral(ls, "unfinished block", CTK, CTK));
  }
  close_func(ls);
}


void luaY_parser (lua_State *L, LuaChunk *chunk, ZIO *z, Mbuffer *buff,
                       const char *name, int firstchar) {
  LexState lexstate;
  FuncState funcstate;
  memset(&lexstate, 0, sizeof(LexState));
  lexstate.buff = buff;
  luaX_setinput(L, &lexstate, z, name, firstchar);
  mainfunc(&lexstate, &funcstate, &(chunk->block));
  chunk->ntokens = lexstate.ntokens;
  chunk->tokens = lexstate.tokens;
}

