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

#include "lua.h"
#include "lauxlib.h"

#include "lmem.h"
#include "llex.h"
#include "lparser.h"

#define CTK      ls->tokens[ls->t]

#define MAXUPVAL	255
#define MAXARG_Bx	65535

/* maximum number of local variables per function (must be smaller
   than 250, due to the bytecode format) */
#define MAXVARS		200


#define hasmultret(k)		((k) == VCALL || (k) == VVARARG)


/* because all strings are unified by the scanner, the parser
   can use pointer equality for string equality */
#define eqstr(a,b)	((a) == (b))


#define getstr(s)	(s)


static AstNode *ast_node_create(LexState *ls, AstNodeType tp, const Token *ts, const Token *te) {
  AstNode *node = luaM_new(ls->L, AstNode);
  if (!node) luaL_error(ls->L, "memory error");
  node->type = tp;
  node->ts = ts;
  node->te = te;
  node->_freelist = ls->freelist;
  ls->freelist = node;
  return node;
}


static inline void ast_nodelist_append(AstNodeList* list, AstNode *node) {
  if (list->head)
    list->tail->_next = node;
  else
    list->head = node;
  list->tail = node;
}


#define stats_append(ls, stat)  ast_nodelist_append(&((ls)->fs->bl->block->stats), (stat))


/*
** nodes for block list (list of active blocks)
*/
typedef struct BlockCnt {
  struct BlockCnt *previous;  /* chain */
  int firstlabel;  /* index of first label in this block */
  int firstgoto;  /* index of first pending goto in this block */
  lu_byte nactvar;  /* # active locals outside the block */
  lu_byte upval;  /* true if some variable in the block is an upvalue */
  lu_byte isloop;  /* true if 'block' is a loop */
  lu_byte insidetbc;  /* true if inside the scope of a to-be-closed var. */
  Block *block;
} BlockCnt;



/*
** prototypes for recursive non-terminal functions
*/
static void statement (LexState *ls);
static void expr (LexState *ls, expdesc *v);


static void error_expected (LexState *ls, int token) {
  /*
  luaX_syntaxerror(ls,
       luaO_pushfstring(ls->L, "%s expected", luaX_token2str(ls, token)));
  */
}


static void errorlimit (FuncState *fs, int limit, const char *what) {
  /*
  lua_State *L = fs->ls->L;
  const char *msg;
  int line = fs->f->linedefined;
  const char *where = (line == 0)
                      ? "main function"
                      : luaO_pushfstring(L, "function at line %d", line);
  msg = luaO_pushfstring(L, "too many %s (limit is %d) in %s",
                             what, limit, where);
  luaX_syntaxerror(fs->ls, msg);
  */
}


static void checklimit (FuncState *fs, int v, int l, const char *what) {
  if (v > l) errorlimit(fs, l, what);
}


/*
** Test whether next token is 'c'; if so, skip it.
*/
static int testnext (LexState *ls, int c) {
  if (CTK.token == c) {
    luaX_next(ls);
    return 1;
  }
  else return 0;
}


/*
** Check that next token is 'c'.
*/
static int check (LexState *ls, int c) {
  if (CTK.token == c)
    return 1;
  error_expected(ls, c);
  return 0;
}


/*
** Check that next token is 'c' and skip it.
*/
static int checknext (LexState *ls, int c) {
  if (!check(ls, c))
    return 0;
  luaX_next(ls);
  return 1;
}


static int check_condition(LexState *ls, int c, const char *msg) {
  if (c)
    return 1;
  // luaX_syntaxerror(ls, msg);
  return 0;
}


/*
** Check that next token is 'what' and skip it. In case of error,
** raise an error that the expected 'what' should match a 'who'
** in line 'where' (if that is not the current line).
*/
static void check_match (LexState *ls, int what, int who, int where) {
  if (unlikely(!testnext(ls, what))) {
    if (where == ls->tokens[ls->t].sline)  /* all in the same line? */
      error_expected(ls, what);  /* do not need a complex message */
    else {
      /*
      luaX_syntaxerror(ls, luaO_pushfstring(ls->L,
             "%s expected (to close %s at line %d)",
              luaX_token2str(ls, what), luaX_token2str(ls, who), where));
      */
    }
  }
}


static TString *str_checkname (LexState *ls) {
  TString *ts;
  check(ls, TK_NAME);
  ts = CTK.seminfo.ts;
  luaX_next(ls);
  return ts;
}


static void skip_vartype (LexState *ls) {
  if (!testnext(ls, TK_NAME)) {
    // luaX_syntaxerror(ls, "var type expected");
  }
  if (!testnext(ls, '<'))
	return;
  skip_vartype(ls);
  while (testnext(ls, ','))
    skip_vartype(ls);
  switch (CTK.token) {
  case TK_SHR:	CTK.token = '>';	break;
  case TK_GE:   CTK.token = '=';	break;
  default:	checknext(ls, '>');		break;
  }
}


static void test_funtype (LexState *ls, FuncDecl *e) {
  AstNode *vtype;
  if (!testnext(ls, ':'))
    return;
  vtype = ast_node_create(ls, AST_vtype, &CTK, &CTK);
  skip_vartype(ls);
  vtype->te = &CTK;
  ast_nodelist_append(&(e->vtypes), vtype);
  while (testnext(ls, ',')) {
    vtype = ast_node_create(ls, AST_vtype, &CTK, &CTK);
    skip_vartype(ls);
    vtype->te = &CTK;
	ast_nodelist_append(&(e->vtypes), vtype);
  }
}


static void init_exp (expdesc *e, expkind k, int i) {
  e->f = e->t = NO_JUMP;
  e->k = k;
  e->u.info = i;
}


static void codestring (expdesc *e, TString *s) {
  e->f = e->t = NO_JUMP;
  e->k = VKSTR;
  e->u.strval = s;
}


static void codename (LexState *ls, expdesc *e) {
  codestring(e, str_checkname(ls));
}


/*
** Register a new local variable in the active 'Proto' (for debug
** information).
*/
static int registerlocalvar (LexState *ls, FuncState *fs, TString *varname) {
  Proto *f = fs->f;
  int oldsize = f->sizelocvars;
  luaM_growvector(ls->L, f->locvars, fs->ndebugvars, f->sizelocvars,
                  LocVar, SHRT_MAX, "local variables");
  // while (oldsize < f->sizelocvars)
  //   f->locvars[oldsize++].varname = NULL;
  f->locvars[fs->ndebugvars].varname = varname;
  f->locvars[fs->ndebugvars].startpc = fs->pc;
  return fs->ndebugvars++;
}


/*
** Create a new local variable with the given 'name'. Return its index
** in the function.
*/
static int new_localvar (LexState *ls, TString *name) {
  lua_State *L = ls->L;
  FuncState *fs = ls->fs;
  Dyndata *dyd = ls->dyd;
  Vardesc *var;
  checklimit(fs, dyd->actvar.n + 1 - fs->firstlocal,
                 MAXVARS, "local variables");
  luaM_growvector(L, dyd->actvar.arr, dyd->actvar.n + 1,
                  dyd->actvar.size, Vardesc, USHRT_MAX, "local variables");
  var = &dyd->actvar.arr[dyd->actvar.n++];
  var->vd.kind = VDKREG;  /* default */
  var->vd.name = name;
  return dyd->actvar.n - 1 - fs->firstlocal;
}

#define new_localvarliteral(ls,v) \
    new_localvar(ls,  \
      luaX_newstring(ls, "" v, (sizeof(v)/sizeof(char)) - 1));



/*
** Return the "variable description" (Vardesc) of a given variable.
** (Unless noted otherwise, all variables are referred to by their
** compiler indices.)
*/
static Vardesc *getlocalvardesc (FuncState *fs, int vidx) {
  return &fs->ls->dyd->actvar.arr[fs->firstlocal + vidx];
}


/*
** Convert 'nvar', a compiler index level, to it corresponding
** stack index level. For that, search for the highest variable
** below that level that is in the stack and uses its stack
** index ('sidx').
*/
static int stacklevel (FuncState *fs, int nvar) {
  while (nvar-- > 0) {
    Vardesc *vd = getlocalvardesc(fs, nvar);  /* get variable */
    if (vd->vd.kind != RDKCTC)  /* is in the stack? */
      return vd->vd.sidx + 1;
  }
  return 0;  /* no variables in the stack */
}


/*
** Return the number of variables in the stack for function 'fs'
*/
static int luaY_nvarstack (FuncState *fs) {
  return stacklevel(fs, fs->nactvar);
}


/*
** Get the debug-information entry for current variable 'vidx'.
*/
static LocVar *localdebuginfo (FuncState *fs, int vidx) {
  Vardesc *vd = getlocalvardesc(fs,  vidx);
  if (vd->vd.kind == RDKCTC)
    return NULL;  /* no debug info. for constants */
  else {
    int idx = vd->vd.pidx;
    lua_assert(idx < fs->ndebugvars);
    return &fs->f->locvars[idx];
  }
}


/*
** Create an expression representing variable 'vidx'
*/
static void init_var (FuncState *fs, expdesc *e, int vidx) {
  e->f = e->t = NO_JUMP;
  e->k = VLOCAL;
  e->u.var.vidx = vidx;
  e->u.var.sidx = getlocalvardesc(fs, vidx)->vd.sidx;
}


/*
** Raises an error if variable described by 'e' is read only
*/
static void check_readonly (LexState *ls, expdesc *e) {
  FuncState *fs = ls->fs;
  TString *varname = NULL;  /* to be set if variable is const */
  switch (e->k) {
    case VCONST: {
      varname = ls->dyd->actvar.arr[e->u.info].vd.name;
      break;
    }
    case VLOCAL: {
      Vardesc *vardesc = getlocalvardesc(fs, e->u.var.vidx);
      if (vardesc->vd.kind != VDKREG)  /* not a regular variable? */
        varname = vardesc->vd.name;
      break;
    }
    case VUPVAL: {
      Upvaldesc *up = &fs->f->upvalues[e->u.info];
      if (up->kind != VDKREG)
        varname = up->name;
      break;
    }
    default:
      return;  /* other cases cannot be read-only */
  }
  if (varname) {
    AstNode *err = ast_node_create(ls, AST_error, e->expr->ts, e->expr->te);
    err->error.msg = luaX_newliteral(ls, "attempt to assign to const variable");
    stats_append(ls, err);
  }
}


/*
** Start the scope for the last 'nvars' created variables.
*/
static void adjustlocalvars (LexState *ls, int nvars) {
  FuncState *fs = ls->fs;
  int stklevel = luaY_nvarstack(fs);
  int i;
  for (i = 0; i < nvars; i++) {
    int vidx = fs->nactvar++;
    Vardesc *var = getlocalvardesc(fs, vidx);
    var->vd.sidx = stklevel++;
    var->vd.pidx = registerlocalvar(ls, fs, var->vd.name);
  }
}


/*
** Close the scope for all variables up to level 'tolevel'.
** (debug info.)
*/
static void removevars (FuncState *fs, int tolevel) {
  fs->ls->dyd->actvar.n -= (fs->nactvar - tolevel);
  while (fs->nactvar > tolevel) {
    LocVar *var = localdebuginfo(fs, --fs->nactvar);
    if (var)  /* does it have debug information? */
      var->endpc = fs->pc;
  }
}


/*
** Search the upvalues of the function 'fs' for one
** with the given 'name'.
*/
static int searchupvalue (FuncState *fs, TString *name) {
  int i;
  Upvaldesc *up = fs->f->upvalues;
  for (i = 0; i < fs->nups; i++) {
    if (eqstr(up[i].name, name)) return i;
  }
  return -1;  /* not found */
}


static Upvaldesc *allocupvalue (FuncState *fs) {
  Proto *f = fs->f;
  int oldsize = f->sizeupvalues;
  checklimit(fs, fs->nups + 1, MAXUPVAL, "upvalues");
  luaM_growvector(fs->ls->L, f->upvalues, fs->nups, f->sizeupvalues,
                  Upvaldesc, MAXUPVAL, "upvalues");
  // while (oldsize < f->sizeupvalues)
  //   f->upvalues[oldsize++].name = NULL;
  return &f->upvalues[fs->nups++];
}


static int newupvalue (FuncState *fs, TString *name, expdesc *v) {
  Upvaldesc *up = allocupvalue(fs);
  FuncState *prev = fs->prev;
  if (v->k == VLOCAL) {
    up->instack = 1;
    up->idx = v->u.var.sidx;
    up->kind = getlocalvardesc(prev, v->u.var.vidx)->vd.kind;
    lua_assert(eqstr(name, getlocalvardesc(prev, v->u.var.vidx)->vd.name));
  }
  else {
    up->instack = 0;
    up->idx = cast_byte(v->u.info);
    up->kind = prev->f->upvalues[v->u.info].kind;
    lua_assert(eqstr(name, prev->f->upvalues[v->u.info].name));
  }
  up->name = name;
  return fs->nups - 1;
}


/*
** Look for an active local variable with the name 'n' in the
** function 'fs'. If found, initialize 'var' with it and return
** its expression kind; otherwise return -1.
*/
static int searchvar (FuncState *fs, TString *n, expdesc *var) {
  int i;
  for (i = cast_int(fs->nactvar) - 1; i >= 0; i--) {
    Vardesc *vd = getlocalvardesc(fs, i);
    if (eqstr(n, vd->vd.name)) {  /* found? */
      if (vd->vd.kind == RDKCTC)  /* compile-time constant? */
        init_exp(var, VCONST, fs->firstlocal + i);
      else  /* real variable */
        init_var(fs, var, i);
      return var->k;
    }
  }
  return -1;  /* not found */
}


/*
** Mark block where variable at given level was defined
** (to emit close instructions later).
*/
static void markupval (FuncState *fs, int level) {
  BlockCnt *bl = fs->bl;
  while (bl->nactvar > level)
    bl = bl->previous;
  bl->upval = 1;
  fs->needclose = 1;
}


/*
** Find a variable with the given name 'n'. If it is an upvalue, add
** this upvalue into all intermediate functions. If it is a global, set
** 'var' as 'void' as a flag.
*/
static void singlevaraux (FuncState *fs, TString *n, expdesc *var, int base) {
  if (fs == NULL)  /* no more levels? */
    init_exp(var, VVOID, 0);  /* default is global */
  else {
    int v = searchvar(fs, n, var);  /* look up locals at current level */
    if (v >= 0) {  /* found? */
      if (v == VLOCAL && !base)
        markupval(fs, var->u.var.vidx);  /* local will be used as an upval */
    }
    else {  /* not found as local at current level; try upvalues */
      int idx = searchupvalue(fs, n);  /* try existing upvalues */
      if (idx < 0) {  /* not found? */
        singlevaraux(fs->prev, n, var, 0);  /* try upper levels */
        if (var->k == VLOCAL || var->k == VUPVAL)  /* local or upvalue? */
          idx  = newupvalue(fs, n, var);  /* will be a new upvalue */
        else  /* it is a global or a constant */
          return;  /* don't need to do anything at this level */
      }
      init_exp(var, VUPVAL, idx);  /* new or old upvalue */
    }
  }
}


/*
** Find a variable with the given name 'n', handling global variables
** too.
*/
static void singlevar (LexState *ls, expdesc *var) {
  TString *varname = str_checkname(ls);
  FuncState *fs = ls->fs;
  AstNode *v = ast_node_create(ls, AST_var, &CTK, &CTK);
  singlevaraux(fs, varname, var, 1);
  if (var->k == VVOID) {  /* global name? */
    // expdesc key;
    singlevaraux(fs, ls->envn, var, 1);  /* get environment variable */
    lua_assert(var->k != VVOID);  /* this one must exist */
    // codestring(&key, varname);  /* key is variable name */
    // luaK_indexed(fs, var, &key);  /* env[varname] */
  }
  var->expr = v;
}


static void enterblock (FuncState *fs, BlockCnt *bl, lu_byte isloop, Block *b) {
  LexState *ls = fs->ls;
  b->isloop = isloop;
  bl->isloop = isloop;
  bl->nactvar = fs->nactvar;
  bl->firstlabel = fs->ls->dyd->label.n;
  bl->firstgoto = fs->ls->dyd->gt.n;
  bl->upval = 0;
  bl->insidetbc = (fs->bl != NULL && fs->bl->insidetbc);
  bl->previous = fs->bl;
  bl->block = b;
  fs->bl = bl;
  lua_assert(fs->freereg == luaY_nvarstack(fs));
}


static void leaveblock (FuncState *fs) {
  BlockCnt *bl = fs->bl;
  LexState *ls = fs->ls;
  int hasclose = 0;
  int stklevel = stacklevel(fs, bl->nactvar);  /* level outside the block */
  fs->bl = bl->previous;
}


/*
** adds a new prototype into list of prototypes
*/
static Proto *addprototype (LexState *ls) {
  Proto *clp;
  lua_State *L = ls->L;
  FuncState *fs = ls->fs;
  Proto *f = fs->f;  /* prototype of current function */
  if (fs->np >= f->sizep) {
    int oldsize = f->sizep;
    luaM_growvector(L, f->p, fs->np, f->sizep, Proto *, MAXARG_Bx, "functions");
    // while (oldsize < f->sizep)
    //   f->p[oldsize++] = NULL;
  }
  f->p[fs->np++] = clp = luaF_newproto(L);
  luaC_objbarrier(L, f, clp);
  return clp;
}


static void open_func (LexState *ls, FuncState *fs, BlockCnt *bl, Block *b) {
  Proto *f = fs->f;
  fs->prev = ls->fs;  /* linked list of funcstates */
  fs->ls = ls;
  ls->fs = fs;
  fs->pc = 0;
  fs->previousline = f->linedefined;
  fs->iwthabs = 0;
  fs->lasttarget = 0;
  fs->freereg = 0;
  fs->nk = 0;
  fs->nabslineinfo = 0;
  fs->np = 0;
  fs->nups = 0;
  fs->ndebugvars = 0;
  fs->nactvar = 0;
  fs->needclose = 0;
  fs->firstlocal = ls->dyd->actvar.n;
  fs->firstlabel = ls->dyd->label.n;
  fs->bl = NULL;
  f->source = ls->source;
  f->maxstacksize = 2;  /* registers 0/1 are always valid */
  enterblock(fs, bl, 0, b);
}


static void close_func (LexState *ls) {
  lua_State *L = ls->L;
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  luaK_ret(fs, luaY_nvarstack(fs), 0);  /* final return */
  leaveblock(fs);
  lua_assert(fs->bl == NULL);
  luaK_finish(fs);
  luaM_shrinkvector(L, f->code, f->sizecode, fs->pc, Instruction);
  luaM_shrinkvector(L, f->abslineinfo, f->sizeabslineinfo,
                       fs->nabslineinfo, AbsLineInfo);
  // luaM_shrinkvector(L, f->k, f->sizek, fs->nk, TValue);
  luaM_shrinkvector(L, f->p, f->sizep, fs->np, Proto *);
  luaM_shrinkvector(L, f->locvars, f->sizelocvars, fs->ndebugvars, LocVar);
  luaM_shrinkvector(L, f->upvalues, f->sizeupvalues, fs->nups, Upvaldesc);
  ls->fs = fs->prev;
  luaC_checkGC(L);
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
  switch (CTK.token) {
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
    if (CTK.token == TK_RETURN) {
      statement(ls);
      return;  /* 'return' must be last statement */
    }
    statement(ls);
  }
}


static void fieldsel (LexState *ls, expdesc *v) {
  /* fieldsel -> ['.' | ':'] NAME */
  FuncState *fs = ls->fs;
  expdesc key;
  luaK_exp2anyregup(fs, v);
  luaX_next(ls);  /* skip the dot or colon */
  codename(ls, &key);
  luaK_indexed(fs, v, &key);
}


static void yindex (LexState *ls, expdesc *v) {
  /* index -> '[' expr ']' */
  luaX_next(ls);  /* skip the '[' */
  expr(ls, v);
  luaK_exp2val(ls->fs, v);
  checknext(ls, ']');
}


/*
** {======================================================================
** Rules for Constructors
** =======================================================================
*/


typedef struct ConsControl {
  expdesc v;  /* last list item read */
  expdesc *t;  /* table descriptor */
  int nh;  /* total number of 'record' elements */
  int na;  /* number of array elements already stored */
  int tostore;  /* number of array elements pending to be stored */
} ConsControl;


static void recfield (LexState *ls, ConsControl *cc) {
  /* recfield -> (NAME | '['exp']') = exp */
  FuncState *fs = ls->fs;
  int reg = ls->fs->freereg;
  expdesc tab, key, val;
  if (CTK.token == TK_NAME) {
    checklimit(fs, cc->nh, MAX_INT, "items in a constructor");
    codename(ls, &key);
  }
  else  /* CTK.token == '[' */
    yindex(ls, &key);
  cc->nh++;
  checknext(ls, '=');
  tab = *cc->t;
  luaK_indexed(fs, &tab, &key);
  expr(ls, &val);
  luaK_storevar(fs, &tab, &val);
  fs->freereg = reg;  /* free registers */
}


static void closelistfield (FuncState *fs, ConsControl *cc) {
  if (cc->v.k == VVOID) return;  /* there is no list item */
  luaK_exp2nextreg(fs, &cc->v);
  cc->v.k = VVOID;
  // if (cc->tostore == LFIELDS_PER_FLUSH) {
  //   luaK_setlist(fs, cc->t->u.info, cc->na, cc->tostore);  /* flush */
  //   cc->na += cc->tostore;
  //   cc->tostore = 0;  /* no more items pending */
  // }
}


static void lastlistfield (FuncState *fs, ConsControl *cc) {
  if (cc->tostore == 0) return;
  if (hasmultret(cc->v.k)) {
    luaK_setmultret(fs, &cc->v);
    luaK_setlist(fs, cc->t->u.info, cc->na, LUA_MULTRET);
    cc->na--;  /* do not count last expression (unknown number of elements) */
  }
  else {
    if (cc->v.k != VVOID)
      luaK_exp2nextreg(fs, &cc->v);
    luaK_setlist(fs, cc->t->u.info, cc->na, cc->tostore);
  }
  cc->na += cc->tostore;
}


static void listfield (LexState *ls, ConsControl *cc) {
  /* listfield -> exp */
  expr(ls, &cc->v);
  cc->tostore++;
}


static void field (LexState *ls, ConsControl *cc) {
  /* field -> listfield | recfield */
  switch(CTK.token) {
    case TK_NAME: {  /* may be 'listfield' or 'recfield' */
      if (luaX_lookahead(ls) != '=')  /* expression? */
        listfield(ls, cc);
      else
        recfield(ls, cc);
      break;
    }
    case '[': {
      recfield(ls, cc);
      break;
    }
    default: {
      listfield(ls, cc);
      break;
    }
  }
}


static void constructor (LexState *ls, expdesc *t) {
  /* constructor -> '{' [ field { sep field } [sep] ] '}'
     sep -> ',' | ';' */
  FuncState *fs = ls->fs;
  int line = ls->tokens[ls->t].sline;
  ConsControl cc;
  fs->pc++; // luaK_codeABC(fs, OP_NEWTABLE, 0, 0, 0);
  // luaK_code(fs, 0);  /* space for extra arg. */
  cc.na = cc.nh = cc.tostore = 0;
  cc.t = t;
  init_exp(t, VNONRELOC, fs->freereg);  /* table will be at stack top */
  luaK_reserveregs(fs, 1);
  init_exp(&cc.v, VVOID, 0);  /* no value (yet) */
  checknext(ls, '{');
  do {
    lua_assert(cc.v.k == VVOID || cc.tostore > 0);
    if (CTK.token == '}') break;
    closelistfield(fs, &cc);
    field(ls, &cc);
  } while (testnext(ls, ',') || testnext(ls, ';'));
  check_match(ls, '}', '{', line);
  lastlistfield(fs, &cc);
  // luaK_settablesize(fs, pc, t->u.info, cc.na, cc.nh);
}

/* }====================================================================== */


static void setvararg (FuncState *fs, int nparams) {
  fs->f->is_vararg = 1;
  fs->pc++; // luaK_codeABC(fs, OP_VARARGPREP, nparams, 0, 0);
}


static void parlist (LexState *ls, FuncDecl *e) {
  /* parlist -> [ param { ',' param } ] */
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  int nparams = 0;
  int isvararg = 0;
  AstNode *param;
  if (CTK.token != ')') {  /* is 'parlist' not empty? */
    do {
      switch (CTK.token) {
        case TK_NAME: {  /* param -> NAME */
          param = ast_node_create(ls, AST_var, &CTK, &CTK);
          new_localvar(ls, str_checkname(ls));
          if (testnext(ls, ':')) {
            skip_vartype(ls);
            param->te = &CTK;
          }
          nparams++;
          ast_nodelist_append(&(e->params), param);
          break;
        }
        case TK_DOTS: {  /* param -> '...' */
          param = ast_node_create(ls, AST_var, &CTK, &CTK);
          luaX_next(ls);
          isvararg = 1;
          ast_nodelist_append(&(e->params), param);
          break;
        }
        default: {
          AstNode *err = ast_node_create(ls, AST_error, &CTK, &CTK);
          err->error.msg = luaX_newliteral(ls, "<name> or '...' expected");
          stats_append(ls, err);
        }
      }
    } while (!isvararg && testnext(ls, ','));
  }
  adjustlocalvars(ls, nparams);
  f->numparams = cast_byte(fs->nactvar);
  if (isvararg)
    setvararg(fs, f->numparams);  /* declared vararg */
  luaK_reserveregs(fs, fs->nactvar);  /* reserve registers for parameters */
}


static void body (LexState *ls, FuncDecl *e, int ismethod, int line) {
  /* body ->  '(' parlist ')' block END */
  FuncState new_fs;
  BlockCnt bl;
  new_fs.f = addprototype(ls);
  new_fs.f->linedefined = line;
  open_func(ls, &new_fs, &bl, &(e->body));
  checknext(ls, '(');
  if (ismethod) {
    new_localvarliteral(ls, "self");  /* create 'self' parameter */
    adjustlocalvars(ls, 1);
  }
  parlist(ls, e);
  checknext(ls, ')');
  test_funtype(ls, e);
  statlist(ls);
  check_match(ls, TK_END, TK_FUNCTION, line);
  close_func(ls);
}


static int explist (LexState *ls, expdesc *v, AstNodeList *list) {
  /* explist -> expr { ',' expr } */
  int n = 1;  /* at least one expression */
  expr(ls, v);
  ast_nodelist_append(list, v->expr);
  while (testnext(ls, ',')) {
    luaK_exp2nextreg(ls->fs, v);
    expr(ls, v, list);
    ast_nodelist_append(list, v->expr);
    n++;
  }
  return n;
}


static void funcargs (LexState *ls, expdesc *f, int line, AstNode *call) {
  FuncState *fs = ls->fs;
  expdesc args;
  int base, nparams;
  switch (CTK.token) {
    case '(': {  /* funcargs -> '(' [ explist ] ')' */
      luaX_next(ls);
      if (CTK.token == ')')  /* arg list is empty? */
        args.k = VVOID;
      else {
        explist(ls, &args, &(call->call.params));
        if (hasmultret(args.k))
          luaK_setmultret(fs, &args);
      }
      check_match(ls, ')', '(', line);
      break;
    }
    case '{': {  /* funcargs -> constructor */
      constructor(ls, &args);
      break;
    }
    case TK_STRING: {  /* funcargs -> STRING */
      codestring(&args, CTK.seminfo.ts);
      luaX_next(ls);  /* must use 'seminfo' before 'next' */
      break;
    }
    default: {
      luaX_syntaxerror(ls, "function arguments expected");
    }
  }
  lua_assert(f->k == VNONRELOC);
  base = f->u.info;  /* base register for call */
  if (hasmultret(args.k))
    nparams = LUA_MULTRET;  /* open call */
  else {
    if (args.k != VVOID)
      luaK_exp2nextreg(fs, &args);  /* close last argument */
    nparams = fs->freereg - (base+1);
  }
  init_exp(f, VCALL, fs->pc++); // luaK_codeABC(fs, OP_CALL, base, nparams+1, 2));
  luaK_fixline(fs, line);
  fs->freereg = base+1;  /* call remove function and arguments and leaves
                            (unless changed) one result */
}




/*
** {======================================================================
** Expression parsing
** =======================================================================
*/


static void primaryexp (LexState *ls, expdesc *v) {
  /* primaryexp -> NAME | '(' expr ')' */
  switch (CTK.token) {
    case '(': {
      int line = ls->linenumber;
      luaX_next(ls);
      expr(ls, v);
      check_match(ls, ')', '(', line);
      return;
    }
    case TK_NAME: {
      singlevar(ls, v);
      return;
    }
    default: {
      AstNode *err = ast_node_create(ls, AST_error, &CTK, &CTK);
      err->error.msg = luaX_newliteral(ls, "unexpected symbol");
      stats_append(ls, err);
    }
  }
}


static void suffixedexp (LexState *ls, expdesc *v) {
  /* suffixedexp ->
       primaryexp { '.' NAME | '[' exp ']' | ':' NAME funcargs | funcargs } */
  FuncState *fs = ls->fs;
  int line = ls->linenumber;
  primaryexp(ls, v);
  for (;;) {
    switch (CTK.token) {
      case '.': {  /* fieldsel */
        fieldsel(ls, v);
        break;
      }
      case '[': {  /* '[' exp ']' */
        expdesc key;
        luaK_exp2anyregup(fs, v);
        yindex(ls, &key);
        luaK_indexed(fs, v, &key);
        break;
      }
      case ':': {  /* ':' NAME funcargs */
        expdesc key;
        AstNode *exp = ast_node_create(ls, AST_call, &CTK, &CTK);
        exp->call.obj = v->expr;
        exp->call.indexer = &CTK;
        luaX_next(ls);
		if (check(ls, TK_NAME)) {
          exp->call.name = ast_node_create(ls, AST_vname, &CTK, &CTK);
          luaX_next(ls);
        }
        funcargs(ls, v, line, exp);
        break;
      }
      case '(': case TK_STRING: case '{': {  /* funcargs */
        AstNode *exp = ast_node_create(ls, AST_call, &CTK, &CTK);
        exp->call.name = v->expr;
        funcargs(ls, v, line, exp);
        break;
      }
      default: return;
    }
  }
}


static void simpleexp (LexState *ls, expdesc *v) {
  /* simpleexp -> FLT | INT | STRING | NIL | TRUE | FALSE | ... |
                  constructor | FUNCTION body | suffixedexp */
  switch (CTK.token) {
    case TK_FLT: {
      init_exp(v, VKFLT, 0);
      v->u.nval = CTK.seminfo.r;
      v->expr = ast_node_create(ls, AST_vflt, &CTK, &CTK);
      break;
    }
    case TK_INT: {
      init_exp(v, VKINT, 0);
      v->u.ival = CTK.seminfo.i;
      v->expr = ast_node_create(ls, AST_vint, &CTK, &CTK);
      break;
    }
    case TK_STRING: {
      codestring(v, CTK.seminfo.ts);
      v->expr = ast_node_create(ls, AST_vstr, &CTK, &CTK);
      break;
    }
    case TK_NIL: {
      init_exp(v, VNIL, 0);
      v->expr = ast_node_create(ls, AST_vnil, &CTK, &CTK);
      break;
    }
    case TK_TRUE: {
      init_exp(v, VTRUE, 0);
      v->expr = ast_node_create(ls, AST_vbool, &CTK, &CTK);
      break;
    }
    case TK_FALSE: {
      init_exp(v, VFALSE, 0);
      v->expr = ast_node_create(ls, AST_vbool, &CTK, &CTK);
      break;
    }
    case TK_DOTS: {  /* vararg */
      FuncState *fs = ls->fs;
      // check_condition(ls, fs->f->is_vararg,
      //                 "cannot use '...' outside a vararg function");
      init_exp(v, VVARARG, fs->pc++); // luaK_codeABC(fs, OP_VARARG, 0, 0, 1));
      v->expr = ast_node_create(ls, AST_vararg, &CTK, &CTK);
      break;
    }
    case '{': {  /* constructor */
      constructor(ls, v);
      return;
    }
    case TK_FUNCTION: {
      v->expr = ast_node_create(ls, AST_func, &CTK, &CTK);
      luaX_next(ls);
      body(ls, &(v->expr->func), 0, ls->linenumber);
      v->expr->te = &CTK;
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
static BinOpr subexpr (LexState *ls, expdesc *v, int limit) {
  BinOpr op;
  UnOpr uop;
  uop = getunopr(CTK.token);
  if (uop != OPR_NOUNOPR) {  /* prefix (unary) operator? */
    AstNode *exp = ast_node_create(ls, AST_unary, &CTK, &CTK);
    luaX_next(ls);  /* skip operator */
    subexpr(ls, v, UNARY_PRIORITY);
	exp->unary.expr = v->expr;
	v->expr = expr;
  }
  else simpleexp(ls, v);
  /* expand while operators have priorities higher than 'limit' */
  op = getbinopr(CTK.token);
  while (op != OPR_NOBINOPR && priority[op].left > limit) {
    AstNode *exp = ast_node_create(ls, AST_bin, v->expr->ts, &CTK);
	expdesc v2;
    BinOpr nextop;
	exp->bin.op = &CTK;
	exp->bin.l = v->expr;
    luaX_next(ls);  /* skip operator */
    // luaK_infix(ls->fs, op, v);
    /* read sub-expression with higher priority */
    nextop = subexpr(ls, &v2, priority[op].right);
	exp->bin.r = v2.expr;
    op = nextop;
  }
  return op;  /* return first untreated operator */
}


static void expr (LexState *ls, expdesc *v) {
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
  BlockCnt bl;
  enterblock(fs, &bl, 0, b);
  statlist(ls);
  leaveblock(fs);
}


/*
** structure to chain all variables in the left-hand side of an
** assignment
*/
struct LHS_assign {
  struct LHS_assign *prev;
  expdesc v;  /* variable (global, local, upvalue, or indexed) */
};


/*
** check whether, in an assignment to an upvalue/local variable, the
** upvalue/local variable is begin used in a previous assignment to a
** table. If so, save original upvalue/local value in a safe place and
** use this safe copy in the previous assignment.
*/
static void check_conflict (LexState *ls, struct LHS_assign *lh, expdesc *v) {
  FuncState *fs = ls->fs;
  int extra = fs->freereg;  /* eventual position to save local variable */
  int conflict = 0;
  for (; lh; lh = lh->prev) {  /* check all previous assignments */
    if (vkisindexed(lh->v.k)) {  /* assignment to table field? */
      if (lh->v.k == VINDEXUP) {  /* is table an upvalue? */
        if (v->k == VUPVAL && lh->v.u.ind.t == v->u.info) {
          conflict = 1;  /* table is the upvalue being assigned now */
          lh->v.k = VINDEXSTR;
          lh->v.u.ind.t = extra;  /* assignment will use safe copy */
        }
      }
      else {  /* table is a register */
        if (v->k == VLOCAL && lh->v.u.ind.t == v->u.var.sidx) {
          conflict = 1;  /* table is the local being assigned now */
          lh->v.u.ind.t = extra;  /* assignment will use safe copy */
        }
        /* is index the local being assigned? */
        if (lh->v.k == VINDEXED && v->k == VLOCAL &&
            lh->v.u.ind.idx == v->u.var.sidx) {
          conflict = 1;
          lh->v.u.ind.idx = extra;  /* previous assignment will use safe copy */
        }
      }
    }
  }
  if (conflict) {
    /* copy upvalue/local value to a temporary (in position 'extra') */
    if (v->k == VLOCAL)
      fs->pc++; // luaK_codeABC(fs, OP_MOVE, extra, v->u.var.sidx, 0);
    else
      fs->pc++; // luaK_codeABC(fs, OP_GETUPVAL, extra, v->u.info, 0);
    luaK_reserveregs(fs, 1);
  }
}

/*
** Parse and compile a multiple assignment. The first "variable"
** (a 'suffixedexp') was already read by the caller.
**
** assignment -> suffixedexp restassign
** restassign -> ',' suffixedexp restassign | '=' explist
*/
static void restassign (LexState *ls, struct LHS_assign *lh, int nvars, AstNode *stat) {
  expdesc e;
  assert( stat->type==AST_assign );
  check_condition(ls, vkisvar(lh->v.k), "syntax error");
  check_readonly(ls, &lh->v);
  if (testnext(ls, ',')) {  /* restassign -> ',' suffixedexp restassign */
    struct LHS_assign nv;
    const Token *ts = &CTK;
    nv.prev = lh;
    suffixedexp(ls, &nv.v);
    ast_nodelist_append(&(stat->assign.vars), nv.v.expr);
    if (!vkisindexed(nv.v.k))
      check_conflict(ls, lh, &nv.v);
    restassign(ls, &nv, nvars+1, stat);
  }
  else {  /* restassign -> '=' explist */
    int nexps;
    checknext(ls, '=');
    nexps = explist(ls, &e, &(stat->assign.explist));
  }
}


static void gotostat (LexState *ls, const Token *tk) {
  FuncState *fs = ls->fs;
  int line = ls->linenumber;
  AstNode *stat = ast_node_create(ls, AST_gotostat, tk, &CTK);
  TString *name = str_checkname(ls);  /* label's name */
  stats_append(ls, stat);
}


/*
** Break statement. Semantically equivalent to "goto break".
*/
static void breakstat (LexState *ls) {
  stats_append(ls, ast_node_create(ls, AST_breakstat, &CTK, &CTK));
  luaX_next(ls);  /* skip break */
}


static void labelstat (LexState *ls, TString *name, int line, const Token *tk) {
  AstNode *stat;
  /* label -> '::' NAME '::' */
  stat = ast_node_create(ls, AST_labelstat, tk, &CTK);
  stats_append(ls, stat);
  checknext(ls, TK_DBCOLON);  /* skip double colon */
  // while (CTK.token == ';' || CTK.token == TK_DBCOLON)
  //   statement(ls);  /* skip other no-op statements */
}


static void whilestat (LexState *ls, int line) {
  /* whilestat -> WHILE cond DO block END */
  FuncState *fs = ls->fs;
  int whileinit;
  BlockCnt bl;
  AstNode *stat = ast_node_create(ls, AST_whilestat, &CTK, &CTK);
  expdesc v;
  luaX_next(ls);  /* skip WHILE */
  whileinit = luaK_getlabel(fs);
  expr(ls, &v);  /* read condition */
  if (v.k == VNIL) v.k = VFALSE;  /* 'falses' are all equal here */
  stat->whilestat.cond = v.expr;
  enterblock(fs, &bl, 1, &(stat->whilestat.body));
  checknext(ls, TK_DO);
  block(ls, &(stat->whilestat.body));
  luaK_jumpto(fs, whileinit);
  check_match(ls, TK_END, TK_WHILE, line);
  leaveblock(fs);
}


static void repeatstat (LexState *ls, int line) {
  /* repeatstat -> REPEAT block UNTIL cond */
  FuncState *fs = ls->fs;
  int repeat_init = luaK_getlabel(fs);
  AstNode *stat = ast_node_create(ls, AST_repeatstat, &CTK, &CTK);
  BlockCnt bl1, bl2;
  expdesc v;
  enterblock(fs, &bl1, 1, &(stat->repeatstat.body));  /* loop block */
  enterblock(fs, &bl2, 0, &(stat->repeatstat.body));  /* scope block */
  luaX_next(ls);  /* skip REPEAT */
  statlist(ls);
  check_match(ls, TK_UNTIL, TK_REPEAT, line);
  expr(ls, &v);  /* read condition (inside scope block) */
  if (v.k == VNIL) v.k = VFALSE;  /* 'falses' are all equal here */
  stat->repeatstat.cond = v.expr;
  leaveblock(fs);  /* finish scope */
  if (bl2.upval) {  /* upvalues? */
    fs->pc++; // luaK_codeABC(fs, OP_CLOSE, stacklevel(fs, bl2.nactvar), 0, 0);
  }
  leaveblock(fs);  /* finish loop */
}


/*
** Read an expression and generate code to put its results in next
** stack slot.
**
*/
static void exp1 (LexState *ls) {
  expdesc e;
  expr(ls, &e);
  luaK_exp2nextreg(ls->fs, &e);
  lua_assert(e.k == VNONRELOC);
}


/*
** Fix for instruction at position 'pc' to jump to 'dest'.
** (Jump addresses are relative in Lua). 'back' true means
** a back jump.
*/
static void fixforjump (FuncState *fs, int pc, int dest, int back) {
  Instruction *jmp = &fs->f->code[pc];
  int offset = dest - (pc + 1);
  if (back)
    offset = -offset;
  if (unlikely(offset > MAXARG_Bx))
    luaX_syntaxerror(fs->ls, "control structure too long");
  SETARG_Bx(*jmp, offset);
}


/*
** Generate code for a 'for' loop.
*/
static void forbody (LexState *ls, int base, int line, int nvars, int isgen, Block *body) {
  /* forbody -> DO block */
  // static const OpCode forprep[2] = {OP_FORPREP, OP_TFORPREP};
  // static const OpCode forloop[2] = {OP_FORLOOP, OP_TFORLOOP};
  BlockCnt bl;
  FuncState *fs = ls->fs;
  int prep, endfor;
  checknext(ls, TK_DO);
  prep = fs->pc++; // luaK_codeABx(fs, forprep[isgen], base, 0);
  enterblock(fs, &bl, 0, body);  /* scope for declared variables */
  adjustlocalvars(ls, nvars);
  luaK_reserveregs(fs, nvars);
  block(ls, body);
  leaveblock(fs);  /* end of scope for declared variables */
  fixforjump(fs, prep, luaK_getlabel(fs), 0);
  if (isgen) {  /* generic for? */
    fs->pc++; // luaK_codeABC(fs, OP_TFORCALL, base, 0, nvars);
    luaK_fixline(fs, line);
  }
  endfor = fs->pc++; // luaK_codeABx(fs, forloop[isgen], base, 0);
  fixforjump(fs, endfor, prep + 1, 1);
  luaK_fixline(fs, line);
}


static void fornum (LexState *ls, TString *varname, AstNode *var, int line) {
  /* fornum -> NAME = exp,exp[,exp] forbody */
  FuncState *fs = ls->fs;
  int base = fs->freereg;
  AstNode *stat = ast_node_create(ls, AST_fornum, var->ts-1, &CTK);
  expdesc e;
  stat->fornum.var = var;
  new_localvarliteral(ls, "(for state)");
  new_localvarliteral(ls, "(for state)");
  new_localvarliteral(ls, "(for state)");
  new_localvar(ls, varname);
  checknext(ls, '=');
  expr(ls, &e); /* initial value */
  stat->fornum.from = e.expr;
  checknext(ls, ',');
  expr(ls, &e);   /* limit */
  stat->fornum.to = e.expr;
  if (testnext(ls, ',')) {
    expr(ls, &e);  /* optional step */
    stat->fornum.step = e.expr;
  }
  adjustlocalvars(ls, 3);  /* control variables */
  forbody(ls, base, line, 1, 0, &(stat->fornum.body));
  stat->te = &CTK;
}


static void forlist (LexState *ls, TString *indexname, AstNode *var, const Token *tk) {
  /* forlist -> NAME {,NAME} IN explist forbody */
  FuncState *fs = ls->fs;
  expdesc e;
  int nvars = 5;  /* gen, state, control, toclose, 'indexname' */
  int line;
  int base = fs->freereg;
  AstNode *stat = ast_node_create(ls, AST_forlist, tk, &CTK);
  /* create control variables */
  new_localvarliteral(ls, "(for state)");
  new_localvarliteral(ls, "(for state)");
  new_localvarliteral(ls, "(for state)");
  new_localvarliteral(ls, "(for state)");
  /* create declared variables */
  new_localvar(ls, indexname);
  ast_nodelist_append(&(stat->forlist.vars), var);
  while (testnext(ls, ',')) {
    new_localvar(ls, str_checkname(ls, &e));
    var = ast_node_create(ls, AST_var, &CTK, &CTK);
    if (testnext(ls, ':')) {
      skip_vartype(ls);
      var->te = &CTK;
      ast_nodelist_append(&(stat->forlist.vars), var);
    }
    nvars++;
  }
  stat->forlist._in = &CTK;
  checknext(ls, TK_IN);

  line = ls->linenumber;
  explist(ls, &e, &(stat->forlist.iters));
  forbody(ls, base, line, nvars - 4, 1, &(stat->forlist.body));
  stat->te = &CTK;
}


static void forstat (LexState *ls, int line, const Token *tk) {
  /* forstat -> FOR (fornum | forlist) END */
  FuncState *fs = ls->fs;
  TString *varname;
  AstNode *var = NULL;
  BlockCnt bl;
  const Token *ts = &CTK;
  enterblock(fs, &bl, 1, ls->fs->bl->block);  /* scope for loop and control variables */
  luaX_next(ls);  /* skip 'for' */
  varname = str_checkname(ls);  /* first variable name */
  var = ast_node_create(ls, AST_var, &CTK, &CTK);
  if (testnext(ls, ':')) {
    skip_vartype(ls);
    var->te = &CTK;
  }
  switch (CTK.token) {
    case '=':
      fornum(ls, varname, var, line, tk);
      break;
    case ',': case TK_IN:
      forlist(ls, varname, var, tk);
      break;
    default:
      luaX_syntaxerror(ls, "'=' or 'in' expected");
      break;
  }
  check_match(ls, TK_END, TK_FOR, line);
  leaveblock(fs);  /* loop scope ('break' jumps to this point) */
}


/*
** Check whether next instruction is a single jump (a 'break', a 'goto'
** to a forward label, or a 'goto' to a backward label with no variable
** to close). If so, set the name of the 'label' it is jumping to
** ("break" for a 'break') or to where it is jumping to ('target') and
** return true. If not a single jump, leave input unchanged, to be
** handled as a regular statement.
*/
static int issinglejump (LexState *ls, TString **label, int *target) {
  if (testnext(ls, TK_BREAK)) {  /* a break? */
    *label = luaS_newliteral(ls->L, "break");
    return 1;
  }
  else if (CTK.token != TK_GOTO || luaX_lookahead(ls) != TK_NAME)
    return 0;  /* not a valid goto */
  else {
    TString *lname = ls->tokens[ls->t+1].seminfo.ts;  /* label's id */
    Labeldesc *lb = findlabel(ls, lname);
    if (lb) {  /* a backward jump? */
      /* does it need to close variables? */
      if (luaY_nvarstack(ls->fs) > stacklevel(ls->fs, lb->nactvar))
        return 0;  /* not a single jump; cannot optimize */
      *target = lb->pc;
    }
    else  /* jump forward */
      *label = lname;
    luaX_next(ls);  /* skip goto */
    luaX_next(ls);  /* skip name */
    return 1;
  }
}


static void test_then_block (LexState *ls, int *escapelist, AstNodeList *caluses) {
  /* test_then_block -> [IF | ELSEIF] cond THEN block */
  BlockCnt bl;
  int line;
  FuncState *fs = ls->fs;
  TString *jlb = NULL;
  int target = NO_JUMP;
  expdesc v;
  int jf;  /* instruction to skip 'then' code (if condition is false) */
  AstNode *exp = ast_node_create(ls, AST_caluse, &CTK, &CTK);
  ast_nodelist_append(caluses, exp);
  luaX_next(ls);  /* skip IF or ELSEIF */
  expr(ls, &v);  /* read condition */
  exp->caluse.cond = v.expr;
  checknext(ls, TK_THEN);
  line = ls->linenumber;
  if (issinglejump(ls, &jlb, &target)) {  /* 'if x then goto' ? */
    luaK_goiffalse(ls->fs, &v);  /* will jump to label if condition is true */
    // TODO : bl.block = ifstat->block ;
    enterblock(fs, &bl, 0, &(exp->caluse.body));  /* must enter block before 'goto' */
    if (jlb != NULL)  /* forward jump? */
      newgotoentry(ls, jlb, line, v.t);  /* will be resolved later */
    else  /* backward jump */
      luaK_patchlist(fs, v.t, target);  /* jump directly to 'target' */
    while (testnext(ls, ';')) {}  /* skip semicolons */
    if (block_follow(ls, 0)) {  /* jump is the entire block? */
      leaveblock(fs);
      return;  /* and that is it */
    }
    else  /* must skip over 'then' part if condition is false */
      jf = luaK_jump(fs);
  }
  else {  /* regular case (not a jump) */
    luaK_goiftrue(ls->fs, &v);  /* skip over block if condition is false */
    enterblock(fs, &bl, 0, &(exp->caluse.body));
    jf = v.f;
  }
  statlist(ls);  /* 'then' part */
  leaveblock(fs);
}


static void ifstat (LexState *ls, int line) {
  /* ifstat -> IF cond THEN block {ELSEIF cond THEN block} [ELSE block] END */
  FuncState *fs = ls->fs;
  int escapelist = NO_JUMP;  /* exit list for finished parts */
  AstNode *stat = ast_node_create(ls, AST_ifstat, &CTK, &CTK);
  test_then_block(ls, &escapelist, &(stat->ifstat.caluses));  /* IF cond THEN block */
  while (CTK.token == TK_ELSEIF)
    test_then_block(ls, &escapelist, &(stat->ifstat.caluses));  /* ELSEIF cond THEN block */
  if (testnext(ls, TK_ELSE)) {
    AstNode *exp = ast_node_create(ls, AST_caluse, &CTK, &CTK);
    ast_nodelist_append(&(stat->ifstat.caluses), exp);
    block(ls, &(exp->caluse.body));  /* 'else' part */
    exp->te = &CTK;
  }
  check_match(ls, TK_END, TK_IF, line);
  luaK_patchtohere(fs, escapelist);  /* patch escape list to 'if' end */
  stat->te = &CTK;
}


static void localfunc (LexState *ls, const Token *tk) {
  FuncState *fs = ls->fs;
  int fvar = fs->nactvar;  /* function's variable index */
  AstNode *stat;
  new_localvar(ls, str_checkname(ls));  /* new local variable */
  stat = ast_node_create(ls, AST_localfunc, tk, &CTK);
  adjustlocalvars(ls, 1);  /* enter its scope */
  stats_append(ls, stat);
  body(ls, &(stat->localfunc.func), 0, ls->linenumber);  /* function created in next register */
  stat->te = &CTK;
}


static void getlocalattribute (LexState *ls, AstNode *v) {
  /* ATTRIB -> ['<' Name '>'] */
  if (testnext(ls, '<')) {
    const char *attr;
    v->var.attr = &CTK;
    attr = getstr(str_checkname(ls));
    checknext(ls, '>');
    if (strcmp(attr, "const") == 0)
      v->var.isconst = 1;  /* read-only variable */
  }
}


static void localstat (LexState *ls, const Token *tk) {
  /* stat -> LOCAL ATTRIB NAME {',' ATTRIB NAME} ['=' explist] */
  FuncState *fs = ls->fs;
  int toclose = -1;  /* index of to-be-closed variable (if any) */
  int vidx, kind;  /* index and kind of last variable */
  expdesc e;
  AstNode *v;
  AstNode *stat = ast_node_create(ls, AST_localstat, tk, &CTK);
  do {
    vidx = new_localvar(ls, str_checkname(ls));
	v = ast_node_create(ls, AST_var, &CTK, &CTK);
    if (testnext(ls, ':')) {
      skip_vartype(ls);
      v->te = &CTK;
    }
    ast_nodelist_append(&(stat->localstat.vars), v);
    getlocalattribute(ls, v);
  } while (testnext(ls, ','));
  if (testnext(ls, '='))
    explist(ls, &e, &(stat->localstat.explist));
  checktoclose(ls, toclose);
}


static int funcname (LexState *ls, expdesc *v) {
  /* funcname -> NAME {fieldsel} [':' NAME] */
  int ismethod = 0;
  singlevar(ls, v);
  while (CTK.token == '.')
    fieldsel(ls, v);
  if (CTK.token == ':') {
    ismethod = 1;
    fieldsel(ls, v);
  }
  return ismethod;
}


static void funcstat (LexState *ls, int line) {
  /* funcstat -> FUNCTION funcname body */
  int ismethod;
  expdesc v;
  AstNode *f = ast_node_create(ls, AST_func, &CTK, &CTK);
  luaX_next(ls);  /* skip FUNCTION */
  ismethod = funcname(ls, &v);
  f->func.name = v.expr;
  body(ls, &(f->func), ismethod, line);
}


static void exprstat (LexState *ls) {
  /* stat -> func | assignment */
  FuncState *fs = ls->fs;
  struct LHS_assign v;
  const Token *ts = &CTK;
  suffixedexp(ls, &v.v);
  if (CTK.token == '=' || CTK.token == ',') { /* stat -> assignment ? */
    AstNode *stat = ast_node_create(ls, AST_assign, ts, &CTK);
    stat->exprstat.expr = v.v.expr;
    v.prev = NULL;
    restassign(ls, &v, 1, stat);
    stat->te = &CTK;
  }
  else {  /* stat -> func */
    if (v.v.k == VCALL) {
      AstNode *stat = ast_node_create(ls, AST_exprstat, ts, &CTK);
      stat->exprstat.expr = v.v.expr;
      stats_append(ls, stat);
    } else {
      AstNode *err = ast_node_create(ls, AST_error, ts, &CTK);
      err->error.msg = luaX_newliteral(ls, "syntax error");
      stats_append(ls, err);
    }
  }
}


static void retstat (LexState *ls, const Token *tk) {
  /* stat -> RETURN [explist] [';'] */
  FuncState *fs = ls->fs;
  expdesc e;
  AstNode *stat = ast_node_create(ls, AST_retstat, tk, tk);
  if (block_follow(ls, 1)) {
    /* return no values */
  }
  else if (CTK.token == ';') {
    /* return no values */
    stat->te = &CTK;
  }
  else {
    explist(ls, &e, &(stat->retstat.explist));  /* optional return values */
  }
  testnext(ls, ';');  /* skip optional semicolon */
}


static void statement (LexState *ls) {
  const Token *tk = &CTK;
  int line = ls->linenumber;  /* may be needed for error messages */
  switch (tk->token) {
    case ';': {  /* stat -> ';' (empty statement) */
      stats_append(ls, ast_node_create(ls, AST_empty, tk, tk));
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
      AstNode *stat = ast_node_create(ls, AST_dostat, tk, tk);
      luaX_next(ls);  /* skip DO */
      block(ls, &(stat->dostat.body));
      check_match(ls, TK_END, TK_DO, line);
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
      labelstat(ls, str_checkname(ls), line, tk);
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
  lua_assert(ls->fs->f->maxstacksize >= ls->fs->freereg &&
             ls->fs->freereg >= luaY_nvarstack(ls->fs));
  ls->fs->freereg = luaY_nvarstack(ls->fs);  /* free registers */
}

/* }====================================================================== */


/*
** compiles the main function, which is a regular vararg function with an
** upvalue named LUA_ENV
*/
static void mainfunc (LexState *ls, FuncState *fs, Block* block) {
  BlockCnt bl;
  Upvaldesc *env;
  open_func(ls, fs, &bl, block);
  setvararg(fs, 0);  /* main function is always declared vararg */
  env = allocupvalue(fs);  /* ...set environment upvalue */
  env->instack = 1;
  env->idx = 0;
  env->kind = VDKREG;
  env->name = ls->envn;
  luaX_next(ls);
  for (;;) {
    statlist(ls);  /* parse main body */
    if (!check(ls, TK_EOS)) {
      AstNode *err = ast_node_create(ls, AST_error, &CTK, &CTK);
      err->error.msg = luaX_newliteral(ls, "unfinished block");
      stats_append(ls, err);
    }
  }
  close_func(ls);
}


void luaY_parser (lua_State *L, LuaChunk *chunk, ZIO *z, Mbuffer *buff,
                       Dyndata *dyd, const char *name, int firstchar) {
  LexState lexstate;
  FuncState funcstate;
  funcstate.f = luaM_new(L, Proto);
  funcstate.f->source = luaX_newstring(&lexstate, name, strlen(name));
  lexstate.buff = buff;
  lexstate.dyd = dyd;
  dyd->actvar.n = dyd->gt.n = dyd->label.n = 0;
  luaX_setinput(L, &lexstate, z, funcstate.f->source, firstchar);
  mainfunc(&lexstate, &funcstate, &(chunk->block));
  chunk->ntokens = lexstate.ntokens;
  chunk->tokens = lexstate.tokens;
  lua_assert(!funcstate.prev && funcstate.nups == 1 && !lexstate.fs);
  /* all scopes should be correctly finished */
  lua_assert(dyd->actvar.n == 0 && dyd->gt.n == 0 && dyd->label.n == 0);
}

