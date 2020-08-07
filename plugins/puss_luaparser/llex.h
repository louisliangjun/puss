/*
** $Id: llex.h $
** Lexical Analyzer
** See Copyright Notice in lua.h
*/

#ifndef llex_h
#define llex_h

#include "lzio.h"
#include "lobject.h"

#define FIRST_RESERVED	257


#if !defined(LUA_ENV)
#define LUA_ENV		"_ENV"
#endif


/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER RESERVED"
*/
enum RESERVED {
  /* terminal symbols denoted by reserved words */
  TK_AND = FIRST_RESERVED, TK_BREAK,
  TK_DO, TK_ELSE, TK_ELSEIF, TK_END, TK_FALSE, TK_FOR, TK_FUNCTION,
  TK_GOTO, TK_IF, TK_IN, TK_LOCAL, TK_NIL, TK_NOT, TK_OR, TK_REPEAT,
  TK_RETURN, TK_THEN, TK_TRUE, TK_UNTIL, TK_WHILE,
  /* other terminal symbols */
  TK_IDIV, TK_CONCAT, TK_DOTS, TK_EQ, TK_GE, TK_LE, TK_NE,
  TK_SHL, TK_SHR,
  TK_DBCOLON, TK_EOS,
  TK_FLT, TK_INT, TK_NAME, TK_STRING,
  TK_COMMENT, TK_ERROR
};

/* number of reserved words */
#define NUM_RESERVED	(cast_int(TK_WHILE-FIRST_RESERVED + 1))


typedef union {
  lua_Number r;
  lua_Integer i;
  TString *ts;    // TK_NAME, TK_STRING, TK_COMMENT, TK_ERROR
} SemInfo;  /* semantics information */


typedef struct Token {
  int token;
  int spos; /* start char offset */
  int epos; /* enc char offset */
  int sline; /* start line */
  int eline; /* end line */
  int slpos; /* start line char offset */
  int elpos; /* end line char start offset */
  SemInfo seminfo;
} Token;


/* state of the lexer plus state of the parser when shared by all
   functions */
typedef struct LexState {
  Token t;  /* current token */
  int ctoken;
  int lookahead;  /* lookahead token */
  struct FuncState *fs;  /* current function (parser) */
  struct lua_State *L;
  int sizetokens;
  int ntokens;
  Token* tokens;
  struct AstNode* freelist;
  TString *source;  /* current source name */

  // only use in lex
  ZIO *z;  /* input stream */
  unsigned int uni; /* unicode - current */
  int lpos; /* unicode - line pos */
  int cpos; /* unicode - charactor pos */
  const char* currentexcept;
  int current;  /* current character (charint) */
  int linenumber;  /* input line counter */
  Mbuffer *buff;  /* buffer for tokens */
} LexState;


LUAI_FUNC void luaX_init (lua_State *L);
LUAI_FUNC void luaX_setinput (lua_State *L, LexState *ls, ZIO *z, TString *source, int firstchar);
#define luaX_newliteral(ls, s)		luaX_newstringreversed(ls, s, (sizeof(s)/sizeof(char))-1, NULL)
#define luaX_newstring(ls, str, l)	luaX_newstringreversed((ls), (str), (l), NULL)
LUAI_FUNC TString *luaX_newstringreversed (LexState *ls, const char *str, size_t l, int *reversed);
LUAI_FUNC int luaX_lookahead (LexState *ls);
LUAI_FUNC void luaX_next (LexState *ls);
LUAI_FUNC const char *luaX_token2str (int token, char _cache[2]);

#endif
