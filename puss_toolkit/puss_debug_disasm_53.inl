// puss_debug_disasm_53.inl

#ifdef _MSC_VER
	#if (_MSC_VER < 1900)
		#ifndef snprintf
			#define snprintf	_snprintf
		#endif
	#endif
#endif

typedef struct _PrintEnv {
	luaL_Buffer			B;
	char				str[1024+8];
} PrintEnv;

#define printf_decls	PrintEnv* _PENV,
#define printf_usage	_PENV,
#define printf(...) do{ \
	int len = snprintf(_PENV->str, 1024, __VA_ARGS__); \
	if( len > 0 ) { luaL_addlstring(&(_PENV->B), _PENV->str, len); } \
} while(0)

#define PrintString(ts)	_PrintString(printf_usage (ts))
static void _PrintString(printf_decls const TString* ts) {
	const char* s = getstr(ts);
	size_t i, n = tsslen(ts);
	printf("%c", '"');
	for (i = 0; i < n; i++)
	{
		int c = (int)(unsigned char)s[i];
		switch (c)
		{
		case '"':  printf("\\\""); break;
		case '\\': printf("\\\\"); break;
		case '\a': printf("\\a"); break;
		case '\b': printf("\\b"); break;
		case '\f': printf("\\f"); break;
		case '\n': printf("\\n"); break;
		case '\r': printf("\\r"); break;
		case '\t': printf("\\t"); break;
		case '\v': printf("\\v"); break;
		default:	if (isprint(c)) printf("%c", c); else printf("\\%03d", c);
		}
	}
	printf("%c", '"');
}

#define PrintConstant(f,i)	_PrintConstant(printf_usage (f),(i))
static void _PrintConstant(printf_decls const Proto* f, int i)
{
	const TValue* o = &f->k[i];
	switch (ttype(o))
	{
	case LUA_TNIL:
		printf("nil");
		break;
	case LUA_TBOOLEAN:
		printf(bvalue(o) ? "true" : "false");
		break;
	case LUA_TNUMFLT:
	{
		char buff[100];
		sprintf(buff, LUA_NUMBER_FMT, fltvalue(o));
		printf("%s", buff);
		if (buff[strspn(buff, "-0123456789")] == '\0') printf(".0");
		break;
	}
	case LUA_TNUMINT:
		printf(LUA_INTEGER_FMT, ivalue(o));
		break;
	case LUA_TSHRSTR: case LUA_TLNGSTR:
		PrintString(tsvalue(o));
		break;
	default:				/* cannot happen */
		printf("? type=%d", ttype(o));
		break;
	}
}

#define UPVALNAME(x) ((f->upvalues[x].name) ? getstr(f->upvalues[x].name) : "-")
#define MYK(x)		(-1-(x))
	
#define PrintCode(f)	_PrintCode(printf_usage (f))
static void _PrintCode(printf_decls const Proto* f)
{
	const Instruction* code = f->code;
	int pc, n = f->sizecode;
	for (pc = 0; pc < n; pc++)
	{
		Instruction i = code[pc];
		OpCode o = GET_OPCODE(i);
		int a = GETARG_A(i);
		int b = GETARG_B(i);
		int c = GETARG_C(i);
		int ax = GETARG_Ax(i);
		int bx = GETARG_Bx(i);
		int sbx = GETARG_sBx(i);
		int line = getfuncline(f, pc);
		printf("%d\t", pc + 1);
		if (line > 0) printf("[%d]\t", line); else printf("[-]\t");
		printf("%-9s\t", luaP_opnames[o]);
		switch (getOpMode(o))
		{
		case iABC:
			printf("%d", a);
			if (getBMode(o) != OpArgN) printf(" %d", ISK(b) ? (MYK(INDEXK(b))) : b);
			if (getCMode(o) != OpArgN) printf(" %d", ISK(c) ? (MYK(INDEXK(c))) : c);
			break;
		case iABx:
			printf("%d", a);
			if (getBMode(o) == OpArgK) printf(" %d", MYK(bx));
			if (getBMode(o) == OpArgU) printf(" %d", bx);
			break;
		case iAsBx:
			printf("%d %d", a, sbx);
			break;
		case iAx:
			printf("%d", MYK(ax));
			break;
		}
		switch (o)
		{
		case OP_LOADK:
			printf("\t; "); PrintConstant(f, bx);
			break;
		case OP_GETUPVAL:
		case OP_SETUPVAL:
			printf("\t; %s", UPVALNAME(b));
			break;
		case OP_GETTABUP:
			printf("\t; %s", UPVALNAME(b));
			if (ISK(c)) { printf(" "); PrintConstant(f, INDEXK(c)); }
			break;
		case OP_SETTABUP:
			printf("\t; %s", UPVALNAME(a));
			if (ISK(b)) { printf(" "); PrintConstant(f, INDEXK(b)); }
			if (ISK(c)) { printf(" "); PrintConstant(f, INDEXK(c)); }
			break;
		case OP_GETTABLE:
		case OP_SELF:
			if (ISK(c)) { printf("\t; "); PrintConstant(f, INDEXK(c)); }
			break;
		case OP_SETTABLE:
		case OP_ADD:
		case OP_SUB:
		case OP_MUL:
		case OP_POW:
		case OP_DIV:
		case OP_IDIV:
		case OP_BAND:
		case OP_BOR:
		case OP_BXOR:
		case OP_SHL:
		case OP_SHR:
		case OP_EQ:
		case OP_LT:
		case OP_LE:
			if (ISK(b) || ISK(c))
			{
				printf("\t; ");
				if (ISK(b)) PrintConstant(f, INDEXK(b)); else printf("-");
				printf(" ");
				if (ISK(c)) PrintConstant(f, INDEXK(c)); else printf("-");
			}
			break;
		case OP_JMP:
		case OP_FORLOOP:
		case OP_FORPREP:
		case OP_TFORLOOP:
			printf("\t; to %d", sbx + pc + 2);
			break;
		case OP_CLOSURE:
			printf("\t; %p", (void*)(f->p[bx]));
			break;
		case OP_SETLIST:
			if (c == 0) printf("\t; %d", (int)code[++pc]); else printf("\t; %d", c);
			break;
		case OP_EXTRAARG:
			printf("\t; "); PrintConstant(f, ax);
			break;
		default:
			break;
		}
		printf("\n");
	}
}

#define SS(x)	((x==1)?"":"s")
#define S(x)	(int)(x),SS(x)

#define PrintHeader(f)	_PrintHeader(printf_usage (f))
static void _PrintHeader(printf_decls const Proto* f)
{
	const char* s = f->source ? getstr(f->source) : "=?";
	if (*s == '@' || *s == '=')
		s++;
	else if (*s == LUA_SIGNATURE[0])
		s = "(bstring)";
	else
		s = "(string)";
	printf("\n%s <%s:%d,%d> (%d instruction%s at %p)\n",
		(f->linedefined == 0) ? "main" : "function", s,
		f->linedefined, f->lastlinedefined,
		S(f->sizecode), (void*)(f));
	printf("%d%s param%s, %d slot%s, %d upvalue%s, ",
		(int)(f->numparams), f->is_vararg ? "+" : "", SS(f->numparams),
		S(f->maxstacksize), S(f->sizeupvalues));
	printf("%d local%s, %d constant%s, %d function%s\n",
		S(f->sizelocvars), S(f->sizek), S(f->sizep));
}

#define PrintDebug(f)	_PrintDebug(printf_usage (f))
static void _PrintDebug(printf_decls const Proto* f)
{
	int i, n;
	n = f->sizek;
	printf("constants (%d) for %p:\n", n, (void*)(f));
	for (i = 0; i < n; i++)
	{
		printf("\t%d\t", i + 1);
		PrintConstant(f, i);
		printf("\n");
	}
	n = f->sizelocvars;
	printf("locals (%d) for %p:\n", n, (void*)(f));
	for (i = 0; i < n; i++)
	{
		printf("\t%d\t%s\t%d\t%d\n",
			i, getstr(f->locvars[i].varname), f->locvars[i].startpc + 1, f->locvars[i].endpc + 1);
	}
	n = f->sizeupvalues;
	printf("upvalues (%d) for %p:\n", n, (void*)(f));
	for (i = 0; i < n; i++)
	{
		printf("\t%d\t%s\t%d\t%d\n",
			i, UPVALNAME(i), f->upvalues[i].instack, f->upvalues[i].idx);
	}
}

#define PrintFunction(f,full)	_PrintFunction(printf_usage (f),(full))
static void _PrintFunction(printf_decls const Proto* f, int full)
{
	//int i, n = f->sizep;
	PrintHeader(f);
	PrintCode(f);
	if (full) PrintDebug(f);
	//for (i = 0; i < n; i++) PrintFunction(f->p[i], full);
}

