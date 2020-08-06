// ast_trace.c

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lparser.h"

void ast_trace(AstNode *node, int depth, const char *name);

static void ast_trace_head(AstNode *node, int depth, const char *type, const char *name) {
  for (; depth>0; --depth) printf("  ");
  printf("[%d-%d] [%d-%d] [%s] %s"
    , node->ts->sline, node->te->eline
    , node->ts->spos - node->ts->slpos, node->te->epos - node->te->elpos
    , type ? type : "<unknown>"
    , name ? name : ""
  );
}

void ast_trace_list(AstNodeList *list, int depth, const char* name) {
  AstNode *iter = list ? list->head : NULL;
  int i;
  for (i=0; i<depth; ++i) printf("  ");
  printf("<%s>\n", name);
  for (; iter; iter=iter->_next) {
    ast_trace(iter, depth+1, NULL);
  }
  for (i=0; i<depth; ++i) printf("  ");
  printf("</%s>\n", name);
}

void ast_trace(AstNode *node, int depth, const char *name) {
  if (!node)
    return;
  switch (node->type) {
    case AST_error: {
      ast_trace_head(node, depth, "error", NULL); printf("%s\n", ast(node, error).msg);
      break;
    }
    case AST_emptystat: {
      ast_trace_head(node, depth, "emptystat", NULL); printf("\n");
      break;
    }
    case AST_caluse: {
      ast_trace_head(node, depth, "caluse", NULL); printf("\n");
      ast_trace(ast(node, caluse).cond, depth+1, "cond");
      ast_trace_list(&ast(node, caluse).body.stats, depth+1, "body");
      break;
    }
    case AST_ifstat: {
      ast_trace_head(node, depth, "ifstat", NULL); printf("\n");
      ast_trace_list(&ast(node, ifstat).caluses, depth+1, "caluses");
      break;
    }
    case AST_whilestat: {
      ast_trace_head(node, depth, "whilestat", NULL); printf("\n");
      ast_trace(ast(node, whilestat).cond, depth+1, "cond");
      ast_trace_list(&ast(node, whilestat).body.stats, depth+1, "body");
      break;
    }
    case AST_dostat: {
      ast_trace_head(node, depth, "dostat", NULL); printf("\n");
      ast_trace_list(&ast(node, dostat).body.stats, depth+1, "body");
      break;
    }
    case AST_fornum: {
      ast_trace_head(node, depth, "fornum", NULL); printf("\n");
      ast_trace(ast(node, fornum).var, depth+1, "var");
      ast_trace(ast(node, fornum).from, depth+1, "from");
      ast_trace(ast(node, fornum).to, depth+1, "to");
      ast_trace(ast(node, fornum).step, depth+1, "step");
      ast_trace_list(&ast(node, fornum).body.stats, depth+1, "body");
      break;
    }
    case AST_forlist: {
      ast_trace_head(node, depth, "forlist", NULL); printf("\n");
      ast_trace_list(&ast(node, forlist).vars, depth+1, "vars");
      ast_trace_list(&ast(node, forlist).iters, depth+1, "iters");
      ast_trace_list(&ast(node, forlist).body.stats, depth+1, "body");
      break;
    }
    case AST_repeatstat: {
      ast_trace_head(node, depth, "repeatstat", NULL); printf("\n");
      ast_trace_list(&ast(node, repeatstat).body.stats, depth+1, "body");
      ast_trace(ast(node, repeatstat).cond, depth+1, "cond");
      break;
    }
    case AST_localfunc: {
      ast_trace_head(node, depth, "localfunc", NULL); printf("\n");
      ast_trace(ast(node, localfunc).func.name, depth+1, "name");
      ast_trace_list(&ast(node, localfunc).func.params, depth+1, "params");
      ast_trace_list(&ast(node, localfunc).func.vtypes, depth+1, "vtypes");
      ast_trace_list(&ast(node, localfunc).func.body.stats, depth+1, "body");
      break;
    }
    case AST_localstat: {
      ast_trace_head(node, depth, "localstat", NULL); printf("\n");
      ast_trace_list(&ast(node, localstat).vars, depth+1, "vars");
      ast_trace_list(&ast(node, localstat).values, depth+1, "values");
      break;
    }
    case AST_labelstat: {
      ast_trace_head(node, depth, "labelstat", NULL); printf("\n");
      break;
    }
    case AST_retstat: {
      ast_trace_head(node, depth, "retstat", NULL); printf("\n");
      ast_trace_list(&ast(node, retstat).values, depth+1, "values");
      break;
    }
    case AST_breakstat: {
      ast_trace_head(node, depth, "breakstat", NULL); printf("\n");
      break;
    }
    case AST_gotostat: {
      ast_trace_head(node, depth, "gotostat", NULL); printf("\n");
      break;
    }
    case AST_exprstat: {
      ast_trace_head(node, depth, "exprstat", NULL); printf("\n");
      ast_trace(ast(node, exprstat).expr, depth+1, "expr");
      break;
    }
    case AST_assign: {
      ast_trace_head(node, depth, "assign", NULL); printf("\n");
      ast_trace_list(&ast(node, assign).vars, depth+1, "vars");
      ast_trace_list(&ast(node, assign).values, depth+1, "values");
      break;
    }
    case AST_constructor: {
      ast_trace_head(node, depth, "constructor", NULL); printf("\n");
      ast_trace_list(&ast(node, constructor).fields, depth+1, "fields");
      break;
    }
    case AST_vnil: {
      ast_trace_head(node, depth, "vnil", NULL); printf("\n");
      break;
    }
    case AST_vbool: {
      ast_trace_head(node, depth, "vbool", NULL); printf("\n");
      break;
    }
    case AST_vint: {
      ast_trace_head(node, depth, "vint", NULL); printf("\n");
      break;
    }
    case AST_vflt: {
      ast_trace_head(node, depth, "vflt", NULL); printf("\n");
      break;
    }
    case AST_vstr: {
      ast_trace_head(node, depth, "vstr", NULL); printf("\n");
      break;
    }
    case AST_vname: {
      ast_trace_head(node, depth, "vname", NULL); printf("\n");
      ast_trace(ast(node, vname).parent, depth+1, "parent");
      break;
    }
    case AST_vararg: {
      ast_trace_head(node, depth, "vararg", NULL); printf("\n");
      break;
    }
    case AST_unary: {
      ast_trace_head(node, depth, "unary", NULL); printf("\n");
      ast_trace(ast(node, unary).expr, depth+1, "expr");
      break;
    }
    case AST_bin: {
      ast_trace_head(node, depth, "bin", NULL); printf("\n");
      ast_trace(ast(node, bin).l, depth+1, "l");
      ast_trace(ast(node, bin).r, depth+1, "r");
      break;
    }
    case AST_var: {
      ast_trace_head(node, depth, "var", NULL); printf("\n");
      break;
    }
    case AST_vtype: {
      ast_trace_head(node, depth, "vtype", NULL); printf("\n");
      break;
    }
    case AST_call: {
      ast_trace_head(node, depth, "call", NULL); printf("\n");
      ast_trace(ast(node, call).name, depth+1, "name");
      ast_trace_list(&ast(node, call).params, depth+1, "params");
      break;
    }
    case AST_func: {
      ast_trace_head(node, depth, "func", NULL); printf("\n");
      ast_trace(ast(node, func).name, depth+1, "name");
      ast_trace_list(&ast(node, func).params, depth+1, "params");
      ast_trace_list(&ast(node, func).vtypes, depth+1, "vtypes");
      ast_trace_list(&ast(node, func).body.stats, depth+1, "body");
      break;
    }
    case AST_field: {
      ast_trace_head(node, depth, "field", NULL); printf("\n");
      ast_trace(ast(node, field).k, depth+1, "k");
      ast_trace(ast(node, field).v, depth+1, "v");
      break;
    }
    default: {
      ast_trace_head(node, depth, "<unknown>", NULL); printf("\n");
      break;
    }
  }
}

