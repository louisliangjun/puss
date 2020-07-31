/*
** $Id: llex.c $
** Lexical Analyzer
** See Copyright Notice in lua.h
*/

#define llex_c
#define LUA_CORE


#include <string.h>

#include "lua.h"
#include "lauxlib.h"

#include "lctype.h"
#include "lmem.h"
#include "llex.h"
#include "lobject.h"
#include "lparser.h"
#include "lzio.h"


#define luaX_newliteral(ls, s)	luaX_newstringreversed(ls, s, (sizeof(s)/sizeof(char))-1, NULL)


// #define next(ls)	(ls->current = zgetc(ls->z))

static void next(LexState* ls) {
	int c = zgetc(ls->z);
	if( c != EOZ ) {
		ls->current = c;
		/* still have continuation bytes? */
		if (ls->uni & 0x40) {
			/* not a continuation byte? */
			if (c & 0x80) {
				ls->uni <<= 1;
			} else {
				ls->uni = 0;	/* ignore : invalid byte sequence */
			}
		} else {
			ls->uni = (c & 0x80) ? (unsigned char)c : 0;
			ls->cpos++;
		}
	} else {
		ls->uni = 0;
		if( ls->current != EOZ ) {
			ls->current = EOZ;
			ls->cpos++;
		}
	}
}


#define currIsNewline(ls)	(ls->current == '\n' || ls->current == '\r')


/* ORDER RESERVED */
static const char *const luaX_tokens [] = {
    "and", "break", "do", "else", "elseif",
    "end", "false", "for", "function", "goto", "if",
    "in", "local", "nil", "not", "or", "repeat",
    "return", "then", "true", "until", "while",
    "//", "..", "...", "==", ">=", "<=", "~=",
    "<<", ">>", "::", "<eof>",
    "<number>", "<integer>", "<name>", "<string>",
    "<comment>", "<error>"
};


#define save_and_next(ls) (save(ls, ls->current), next(ls))


void luaX_init (lua_State *L) {
  int i;
  for (i=0; i<NUM_RESERVED; i++) {
    lua_pushinteger(L, FIRST_RESERVED+i);
	lua_setfield(L, -2, luaX_tokens[i]);
  }
}


static void save (LexState *ls, int c) {
  Mbuffer *b = ls->buff;
  if (luaZ_bufflen(b) + 1 > luaZ_sizebuffer(b)) {
    size_t newsize;
    if (luaZ_sizebuffer(b) >= MAX_SIZE/2)
      luaL_error(ls->L, "lexical element too long");
    newsize = luaZ_sizebuffer(b) * 2;
    luaZ_resizebuffer(ls->L, b, newsize);
  }
  b->buffer[luaZ_bufflen(b)++] = cast_char(c);
}


/*
** creates a new string and anchors it in scanner's table so that
** it will not be collected until the end of the compilation
** (by that time it should be anchored somewhere)
*/
TString *luaX_newstringreversed (LexState *ls, const char *str, size_t l, int* reversed) {
  lua_State *L = ls->L;
  TString* s = lua_pushlstring(L, str, l);
  if (reversed) {
    lua_pushvalue(L, -1);
    if (lua_rawget(L, UPVAL_IDX_REVERSED)==LUA_TNUMBER) {
      *reversed = (int)lua_tointeger(L, -1);
	}
    lua_pop(L, 1);
  }
  lua_pushvalue(L, -1);
  if (lua_rawget(L, UPVAL_IDX_STRMAP)==LUA_TSTRING) {
    s = lua_tostring(L, -1);
	lua_pop(L, 2);
  } else {
    lua_assert( lua_isnil(L, -1) );
    lua_pop(L, 1);
    lua_pushvalue(L, -1);
    lua_rawset(L, UPVAL_IDX_STRMAP);	/* t[string] = string */
  }
  return s;
}


/*
** increment line number and skips newline sequence (any of
** \n, \r, \n\r, or \r\n)
*/
static void inclinenumber (LexState *ls) {
  int old = ls->current;
  lua_assert(currIsNewline(ls));
  next(ls);  /* skip '\n' or '\r' */
  if (currIsNewline(ls) && ls->current != old)
    next(ls);  /* skip '\n\r' or '\r\n' */
  if (++ls->linenumber >= MAX_INT)
    luaL_error(ls->L, "chunk has too many lines");
  ls->lpos = ls->cpos;
}


void luaX_setinput (lua_State *L, LexState *ls, ZIO *z, TString *source,
                    int firstchar) {
  ls->uni = 0;
  ls->lpos = 0;
  ls->cpos = 0;
  ls->current_except = NULL;
  ls->current_token = &(ls->t);
  ls->freelist = NULL;

  ls->t.token = 0;
  ls->L = L;
  ls->current = firstchar;
  ls->lookahead.token = TK_EOS;  /* no look-ahead token */
  ls->z = z;
  ls->fs = NULL;
  ls->linenumber = 1;
  ls->lastline = 1;
  ls->source = source;
  ls->envn = luaX_newliteral(ls, LUA_ENV);  /* get env name */
  luaZ_resizebuffer(ls->L, ls->buff, LUA_MINBUFFER);  /* initialize buffer */
}


#define savetkpos(ls) do { \
    Token* tk = (ls)->current_token; \
    tk->eline = ls->linenumber; \
    tk->elpos = ls->lpos; \
    tk->epos = ls->cpos; \
  } while(0)


/*
** =======================================================
** LEXICAL ANALYZER
** =======================================================
*/


static int check_next1 (LexState *ls, int c) {
  if (ls->current == c) {
    next(ls);
    return 1;
  }
  else return 0;
}


/*
** Check whether current char is in set 'set' (with two chars) and
** saves it
*/
static int check_next2 (LexState *ls, const char *set) {
  lua_assert(set[2] == '\0');
  if (ls->current == set[0] || ls->current == set[1]) {
    save_and_next(ls);
    return 1;
  }
  else return 0;
}


/* LUA_NUMBER */
/*
** This function is quite liberal in what it accepts, as 'luaO_str2num'
** will reject ill-formed numerals. Roughly, it accepts the following
** pattern:
**
**   %d(%x|%.|([Ee][+-]?))* | 0[Xx](%x|%.|([Pp][+-]?))*
**
** The only tricky part is to accept [+-] only after a valid exponent
** mark, to avoid reading '3-4' or '0xe+1' as a single number.
**
** The caller might have already read an initial dot.
*/
static int read_numeral (LexState *ls, SemInfo *seminfo) {
  TValue obj;
  const char *expo = "Ee";
  int first = ls->current;
  lua_assert(lisdigit(ls->current));
  save_and_next(ls);
  if (first == '0' && check_next2(ls, "xX"))  /* hexadecimal? */
    expo = "Pp";
  for (;;) {
    if (check_next2(ls, expo))  /* exponent mark? */
      check_next2(ls, "-+");  /* optional exponent sign */
    else if (lisxdigit(ls->current) || ls->current == '.')  /* '%x|%.' */
      save_and_next(ls);
    else break;
  }
  if (lislalpha(ls->current))  /* is numeral touching a letter? */
    save_and_next(ls);  /* force an error */
  save(ls, '\0');
  ls->current_token->epos = ls->cpos;
  if (luaO_str2num(luaZ_buffer(ls->buff), &obj) == 0) {  /* format error? */
    seminfo->ts = luaX_newstring(ls, luaZ_buffer(ls->buff), luaZ_bufflen(ls->buff) - 1);
	return TK_ERROR;
  }
  if (obj.tt_=='i') {
    seminfo->i = obj.i;
    return TK_INT;
  }
  else {
    lua_assert(obj.tt_=='n');
    seminfo->r = obj.n;
    return TK_FLT;
  }
}


/*
** reads a sequence '[=*[' or ']=*]', leaving the last bracket.
** If sequence is well formed, return its number of '='s + 2; otherwise,
** return 1 if there is no '='s or 0 otherwise (an unfinished '[==...').
*/
static size_t skip_sep (LexState *ls) {
  size_t count = 0;
  int s = ls->current;
  lua_assert(s == '[' || s == ']');
  save_and_next(ls);
  while (ls->current == '=') {
    save_and_next(ls);
    count++;
  }
  return (ls->current == s) ? count + 2
         : (count == 0) ? 1
         : 0;
}


static void read_long_string (LexState *ls, SemInfo *seminfo, size_t sep, int isstr) {
  int line = ls->linenumber;  /* initial line (for error message) */
  save_and_next(ls);  /* skip 2nd '[' */
  if (currIsNewline(ls))  /* string starts with a newline? */
    inclinenumber(ls);  /* skip it */
  for (;;) {
    switch (ls->current) {
      case EOZ: {  /* error */
        savetkpos(ls);
        seminfo->ts = isstr ? luaX_newliteral(ls, "unfinished long string") : luaX_newliteral(ls, "unfinished long comment");
        break;  /* to avoid warnings */
      }
      case ']': {
        if (skip_sep(ls) == sep) {
          save_and_next(ls);  /* skip 2nd ']' */
          goto endloop;
        }
        break;
      }
      case '\n': case '\r': {
        save(ls, '\n');
        inclinenumber(ls);
        if (!seminfo) luaZ_resetbuffer(ls->buff);  /* avoid wasting space */
        break;
      }
      default: {
        if (seminfo) save_and_next(ls);
        else next(ls);
      }
    }
  } endloop:
  seminfo->ts = luaX_newstring(ls, luaZ_buffer(ls->buff) + sep,
                                   luaZ_bufflen(ls->buff) - 2 * sep);
}


static int esccheck (LexState *ls, int c, const char *msg) {
  if (!c) {
    if (ls->current != EOZ)
      save_and_next(ls);  /* add current to buffer for error message */
    if (ls->current_except == NULL)
      ls->current_except = msg;
    return 0;
  }
  return 1;
}


static int gethexa (LexState *ls) {
  save_and_next(ls);
  return esccheck (ls, lisxdigit(ls->current), "hexadecimal digit expected") ? luaO_hexavalue(ls->current) : 0;
}


static int readhexaesc (LexState *ls) {
  int r = gethexa(ls);
  r = (r << 4) + gethexa(ls);
  luaZ_buffremove(ls->buff, 2);  /* remove saved chars from buffer */
  return r;
}


static unsigned long readutf8esc (LexState *ls) {
  unsigned long r;
  int i = 4;  /* chars to be removed: '\', 'u', '{', and first digit */
  save_and_next(ls);  /* skip 'u' */
  if (!esccheck(ls, ls->current == '{', "missing '{'"))
    return 0;
  r = gethexa(ls);  /* must have at least one digit */
  while (cast_void(save_and_next(ls)), lisxdigit(ls->current)) {
    i++;
    if (esccheck(ls, r <= (0x7FFFFFFFu >> 4), "UTF-8 value too large"))
      r = (r << 4) + luaO_hexavalue(ls->current);
  }
  if (!esccheck(ls, ls->current == '}', "missing '}'")) {
    next(ls);  /* skip '}' */
    luaZ_buffremove(ls->buff, i);  /* remove saved chars from buffer */
  }
  return r;
}


static void utf8esc (LexState *ls) {
  char buff[UTF8BUFFSZ];
  int n = luaO_utf8esc(buff, readutf8esc(ls));
  for (; n > 0; n--)  /* add 'buff' to string */
    save(ls, buff[UTF8BUFFSZ - n]);
}


static int readdecesc (LexState *ls) {
  int i;
  int r = 0;  /* result accumulator */
  for (i = 0; i < 3 && lisdigit(ls->current); i++) {  /* read up to 3 digits */
    r = 10*r + ls->current - '0';
    save_and_next(ls);
  }
  if (esccheck(ls, r <= UCHAR_MAX, "decimal escape too large"))
    luaZ_buffremove(ls->buff, i);  /* remove read digits from buffer */
  return r;
}


static int read_string (LexState *ls, int del, SemInfo *seminfo) {
  ls->current_except = NULL;
  save_and_next(ls);  /* keep delimiter (for error messages) */
  while (ls->current != del) {
    switch (ls->current) {
      case EOZ:
      case '\n':
      case '\r':
        ls->current_token->epos = ls->cpos - 1;
        seminfo->ts = luaX_newliteral(ls, "unfinished string");
        return TK_ERROR;
      case '\\': {  /* escape sequences */
        int c;  /* final character to be saved */
        save_and_next(ls);  /* keep '\\' for error messages */
        switch (ls->current) {
          case 'a': c = '\a'; goto read_save;
          case 'b': c = '\b'; goto read_save;
          case 'f': c = '\f'; goto read_save;
          case 'n': c = '\n'; goto read_save;
          case 'r': c = '\r'; goto read_save;
          case 't': c = '\t'; goto read_save;
          case 'v': c = '\v'; goto read_save;
          case 'x': c = readhexaesc(ls); goto read_save;
          case 'u': utf8esc(ls);  goto no_save;
          case '\n': case '\r':
            inclinenumber(ls); c = '\n'; goto only_save;
          case '\\': case '\"': case '\'':
            c = ls->current; goto read_save;
          case EOZ: goto no_save;  /* will raise an error next loop */
          case 'z': {  /* zap following span of spaces */
            luaZ_buffremove(ls->buff, 1);  /* remove '\\' */
            next(ls);  /* skip the 'z' */
            while (lisspace(ls->current)) {
              if (currIsNewline(ls)) inclinenumber(ls);
              else next(ls);
            }
            goto no_save;
          }
          default: {
            if (esccheck(ls, lisdigit(ls->current), "invalid escape sequence"))
              c = readdecesc(ls);  /* digital escape '\ddd' */
            goto only_save;
          }
        }
       read_save:
         next(ls);
         /* go through */
       only_save:
         luaZ_buffremove(ls->buff, 1);  /* remove '\\' */
         save(ls, c);
         /* go through */
       no_save: break;
      }
      default:
        save_and_next(ls);
    }
  }
  save_and_next(ls);  /* skip delimiter */
  ls->current_token->epos = ls->cpos;
  if (ls->current_except) {
    seminfo->ts = luaX_newstring(ls, ls->current_except, strlen(ls->current_except));
    return TK_ERROR;
  }
  seminfo->ts = luaX_newstring(ls, luaZ_buffer(ls->buff) + 1,
                                   luaZ_bufflen(ls->buff) - 2);
  return TK_STRING;
}


static int llex (LexState *ls, SemInfo *seminfo) {
  Token* ctk = (ls)->current_token;
  luaZ_resetbuffer(ls->buff);
  for (;;) {
    switch (ls->current) {
      case '\n': case '\r': {  /* line breaks */
        inclinenumber(ls);
        continue;
      }
      case ' ': case '\f': case '\t': case '\v': {  /* spaces */
        next(ls);
        continue;
      }
    }
    ctk->sline = ctk->eline = ls->linenumber;
    ctk->slpos = ctk->elpos = ls->lpos;
    ctk->spos = ls->cpos;
    ctk->epos = ls->cpos + 1;
    #define return_more return (ctk->epos = ls->cpos), 

    switch (ls->current) {
      case '-': {  /* '-' or '--' (comment) */
        next(ls);
        if (ls->current != '-') return '-';
        /* else is a comment */
        next(ls);
        if (ls->current == '[') {  /* long comment? */
          size_t sep = skip_sep(ls);
          luaZ_resetbuffer(ls->buff);  /* 'skip_sep' may dirty the buffer */
          if (sep >= 2) {
            read_long_string(ls, seminfo, sep, 0);  /* skip long comment */
            luaZ_resetbuffer(ls->buff);  /* previous call may dirty the buff. */
          }
        }
		else {
          /* else short comment */
          while (!currIsNewline(ls) && ls->current != EOZ)
            next(ls);  /* skip until end of line (or end of file) */
        }
        return_more TK_COMMENT;
      }
      case '[': {  /* long string or simply '[' */
        size_t sep = skip_sep(ls);
        if (sep >= 2) {
          read_long_string(ls, seminfo, sep, 1);
          savetkpos(ls);
          return TK_STRING;
        }
        else if (sep == 0) {  /* '[=...' missing second bracket? */
          seminfo->ts = luaX_newliteral(ls, "invalid long string delimiter");
          savetkpos(ls);
          return TK_ERROR;
        }
        return '[';
      }
      case '=': {
        next(ls);
        if (check_next1(ls, '=')) return_more TK_EQ;
        else return '=';
      }
      case '<': {
        next(ls);
        if (check_next1(ls, '=')) return_more TK_LE;
        else if (check_next1(ls, '<')) return_more TK_SHL;
        else return '<';
      }
      case '>': {
        next(ls);
        if (check_next1(ls, '=')) return_more TK_GE;
        else if (check_next1(ls, '>')) return_more TK_SHR;
        else return '>';
      }
      case '/': {
        next(ls);
        if (check_next1(ls, '/')) return_more TK_IDIV;
        else return '/';
      }
      case '~': {
        next(ls);
        if (check_next1(ls, '=')) return_more TK_NE;
        else return '~';
      }
      case ':': {
        next(ls);
        if (check_next1(ls, ':')) return_more TK_DBCOLON;
        else return ':';
      }
      case '"': case '\'': {  /* short literal strings */
        return read_string(ls, ls->current, seminfo);
      }
      case '.': {  /* '.', '..', '...', or number */
        save_and_next(ls);
        if (check_next1(ls, '.')) {
          if (check_next1(ls, '.'))
            return_more TK_DOTS;   /* '...' */
          else return_more TK_CONCAT;   /* '..' */
        }
        else if (!lisdigit(ls->current)) return '.';
        else return read_numeral(ls, seminfo);
      }
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9': {
        return read_numeral(ls, seminfo);
      }
      case EOZ: {
        return TK_EOS;
      }
      default: {
		if (ls->uni) {
		  int cpos = ls->cpos;
		  while (ls->uni) {
			while (ls->uni & 0x40)
			  save_and_next(ls);
			save_and_next(ls);
		  }
		  return_more TK_ERROR;
		}
        if (lislalpha(ls->current)) {  /* identifier or reserved word? */
          int tk = TK_NAME;
          TString *ts;
          do {
            save_and_next(ls);
          } while (lislalnum(ls->current));
          ts = luaX_newstringreversed(ls, luaZ_buffer(ls->buff),
                                  luaZ_bufflen(ls->buff), &tk);
          seminfo->ts = ts;
          return_more tk;
        }
        else {  /* single-char tokens (+ - / ...) */
          int c = ls->current;
          next(ls);
          return c;
        }
      }
    #undef return_more
    }
  }
}


void luaX_next (LexState *ls) {
  ls->lastline = ls->linenumber;
  if (ls->lookahead.token != TK_EOS) {  /* is there a look-ahead token? */
    ls->t = ls->lookahead;  /* use this one */
    ls->lookahead.token = TK_EOS;  /* and discharge it */
  }
  else {
    ls->t.token = llex(ls, &ls->t.seminfo);  /* read next token */
  }
}


int luaX_lookahead (LexState *ls) {
  lua_assert(ls->lookahead.token == TK_EOS);
  ls->lookahead.token = llex(ls, &ls->lookahead.seminfo);
  return ls->lookahead.token;
}

