// gffi.c
//

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

#define	align_size(size)	( ((size) + (sizeof(void*)-1)) & (~(sizeof(void*)-1)) )

#define LIBFFI_HIDE_BASIC_TYPES

#if defined(_MSC_VER) && defined(_WIN64)
	#ifndef WIN64
		#define WIN64
	#endif
#endif

#include <ffi.h>

#include "gffi.h"

typedef struct _GFFIType		GFFIType;
typedef struct _GFFIFunction	GFFIFunction;

typedef void (*GFFILuaCheck)(lua_State* L, int idx, gpointer pval, gboolean opt, GFFIType* tp);	// FFI C function argument
typedef int  (*GFFILuaPush)(lua_State* L, gconstpointer pval, GFFIType* tp, gboolean isnew);	// FFI C Function return

struct _GFFIType {
	ffi_type		ffitp;
	GType			gtype;
	GFFILuaCheck	check;
	GFFILuaPush		push;
};

typedef enum _GFFIArgMode
	{ GFFI_ARG_MODE_IN	= (1 << 0)
	, GFFI_ARG_MODE_OUT	= (1 << 1)
	, GFFI_ARG_MODE_OPT	= (1 << 2)	// use lua_test not need check, can be nil
	, GFFI_ARG_MODE_NEW	= (1 << 3)	// pointer not need ref/copy/...
	, GFFI_ARG_MODE_INOUT	= (GFFI_ARG_MODE_IN | GFFI_ARG_MODE_OUT)
	} GFFIArgMode;

struct _GFFIFunction {
	ffi_cif			cif;
	const gchar*	name;
	gconstpointer	func;
	size_t			narg;
	size_t			args_size;
	gboolean		rnew;
	GFFIType*		rtype;
	GFFIType**		atypes;
	guchar*			amodes;
};

#define	G_TYPE_MAX	4096

static GQuark		__gtype_quark__ = 0;
static size_t		__gtype_num__ = 0;
static GFFIType		__gtype_arr__[G_TYPE_MAX];

static GFFIType* _gtype_reg(GType tp, ffi_type ffitp, GFFILuaCheck check, GFFILuaPush push) {
	assert( __gtype_quark__ );
	GFFIType* t = g_type_get_qdata(tp, __gtype_quark__);
	if( t ) {
		g_error("gtype ffi type already register!");
		return NULL;
	}

	t = &(__gtype_arr__[ __gtype_num__++ ]);
	if( __gtype_num__ >= G_TYPE_MAX ) {
		g_error("gtype ffi register out of range!");
		abort();
	}

	t->ffitp = ffitp;
	t->gtype = tp;
	t->check = check;
	t->push = push;

	g_type_set_qdata(tp, __gtype_quark__, t);
	return t;
}

static void ffi_lua_check_none(lua_State* L, int idx, gpointer pval, gboolean opt, GFFIType* tp) {
}

static int ffi_lua_push_none(lua_State* L, gconstpointer pval, GFFIType* tp, gboolean isnew) {
	return 0;
}

static void ffi_lua_check_char(lua_State* L, int idx, gpointer pval, gboolean opt, GFFIType* tp) {
	size_t len = 0;
	const gchar* str = opt ? luaL_optlstring(L, idx, "", &len) : luaL_checklstring(L, idx, &len);
	if( len != 1 ) {
		if( opt )
			str = "";
		else
			luaL_error(L, "check G_TYPE_CHAR not string or length not 1");
	}

	*((gchar*)pval) = *str;
}

static int ffi_lua_push_char(lua_State* L, gconstpointer pval, GFFIType* tp, gboolean isnew) {
	lua_pushlstring(L, (gchar*)pval, 1);
	return 1;
}

static void ffi_lua_check_uchar(lua_State* L, int idx, gpointer pval, gboolean opt, GFFIType* tp) {
	*((guchar*)pval) = (guchar)( opt ? luaL_optinteger(L, idx, 0) : luaL_checkinteger(L, idx) );
}

static int ffi_lua_push_uchar(lua_State* L, gconstpointer pval, GFFIType* tp, gboolean isnew) {
	lua_pushinteger(L, *((guchar*)pval));
	return 1;
}

static void ffi_lua_check_boolean(lua_State* L, int idx, gpointer pval, gboolean opt, GFFIType* tp) {
	*((gboolean*)pval) = (gboolean)lua_toboolean(L, idx);
}

static int ffi_lua_push_boolean(lua_State* L, gconstpointer pval, GFFIType* tp, gboolean isnew) {
	lua_pushboolean(L, *((gboolean*)pval));
	return 1;
}

static void ffi_lua_check_int(lua_State* L, int idx, gpointer pval, gboolean opt, GFFIType* tp) {
	*((gint*)pval) = (gint)( opt ? luaL_optinteger(L, idx, 0) : luaL_checkinteger(L, idx) );
}

static int ffi_lua_push_int(lua_State* L, gconstpointer pval, GFFIType* tp, gboolean isnew) {
	lua_pushinteger(L, *((gint*)pval));
	return 1;
}

static void ffi_lua_check_uint(lua_State* L, int idx, gpointer pval, gboolean opt, GFFIType* tp) {
	*((guint*)pval) = (guint)( opt ? luaL_optinteger(L, idx, 0) : luaL_checkinteger(L, idx) );
}

static int ffi_lua_push_uint(lua_State* L, gconstpointer pval, GFFIType* tp, gboolean isnew) {
	lua_pushinteger(L, *((guint*)pval));
	return 1;
}

static void ffi_lua_check_int64(lua_State* L, int idx, gpointer pval, gboolean opt, GFFIType* tp) {
	*((gint64*)pval) = (gint64)( opt ? luaL_optinteger(L, idx, 0) : luaL_checkinteger(L, idx) );
}

static int ffi_lua_push_int64(lua_State* L, gconstpointer pval, GFFIType* tp, gboolean isnew) {
	lua_pushinteger(L, *((gint64*)pval));
	return 1;
}

static void ffi_lua_check_uint64(lua_State* L, int idx, gpointer pval, gboolean opt, GFFIType* tp) {
	*((guint64*)pval) = (guint64)( opt ? luaL_optinteger(L, idx, 0) : luaL_checkinteger(L, idx) );
}

static int ffi_lua_push_uint64(lua_State* L, gconstpointer pval, GFFIType* tp, gboolean isnew) {
	lua_pushinteger(L, *((guint64*)pval));
	return 1;
}

static void ffi_lua_check_float(lua_State* L, int idx, gpointer pval, gboolean opt, GFFIType* tp) {
	*((gfloat*)pval) = (gfloat)( opt ? luaL_optnumber(L, idx, 0) : luaL_checknumber(L, idx) );
}

static int ffi_lua_push_float(lua_State* L, gconstpointer pval, GFFIType* tp, gboolean isnew) {
	lua_pushnumber(L, *((gfloat*)pval));
	return 1;
}

static void ffi_lua_check_double(lua_State* L, int idx, gpointer pval, gboolean opt, GFFIType* tp) {
	*((gdouble*)pval) = (gdouble)( opt ? luaL_optnumber(L, idx, 0) : luaL_checknumber(L, idx));
}

static int ffi_lua_push_double(lua_State* L, gconstpointer pval, GFFIType* tp, gboolean isnew) {
	lua_pushnumber(L, *((gdouble*)pval));
	return 1;
}

static void ffi_lua_check_string(lua_State* L, int idx, gpointer pval, gboolean opt, GFFIType* tp) {
	*((const gchar**)pval) = (const gchar*)( opt ? luaL_optstring(L, idx, NULL) : luaL_checkstring(L, idx) );
}

static int ffi_lua_push_string(lua_State* L, gconstpointer pval, GFFIType* tp, gboolean isnew) {
	const gchar* val = *((const gchar**)pval);
	lua_pushstring(L, val);
	if( isnew )
		g_free((gchar*)val);
	return 1;
}

static void ffi_lua_check_pointer(lua_State* L, int idx, gpointer pval, gboolean opt, GFFIType* tp) {
	*((gpointer*)pval) = (gpointer)lua_topointer(L, idx);
}

static int ffi_lua_push_pointer(lua_State* L, gconstpointer pval, GFFIType* tp, gboolean isnew) {
	lua_pushlightuserdata(L, *((gpointer*)pval));
	return 1;
}

static void ffi_lua_check_gvalue(lua_State* L, int idx, gpointer pval, gboolean opt, GFFIType* tp) {
	GValue* v = opt ? glua_value_test(L, idx) : glua_value_check(L, idx);
	*((GValue**)pval) = v ? v : glua_value_new(L, G_TYPE_INVALID);
}

static int ffi_lua_push_gvalue(lua_State* L, gconstpointer pval, GFFIType* tp, gboolean isnew) {
	const GValue* v = *((const GValue**)pval);
	glua_value_push(L, v);
	if( isnew )
		g_value_unset((GValue*)v);
	return 1;
}

static void ffi_lua_check_boxed(lua_State* L, int idx, gpointer pval, gboolean opt, GFFIType* tp) {
	GValue* v = opt ? glua_value_test(L, idx) : glua_value_check_type(L, idx, tp->gtype);
	(*((gpointer*)pval)) = NULL;

	if( !opt ) {
		GType vtp = G_VALUE_TYPE(v);
		if( g_type_is_a(vtp, tp->gtype) ) {
			(*((gpointer*)pval)) = g_value_get_boxed(v);
		} else {
			char err[512];
			sprintf(err, "need(%s) but got(%s)", g_type_name(tp->gtype), g_type_name(vtp));
			luaL_argerror(L, idx, err);
		}
	} else if( v ) {
		GType vtp = G_VALUE_TYPE(v);
		if( g_type_is_a(vtp, tp->gtype) ) {
			(*((gpointer*)pval)) = g_value_get_boxed(v);
		}
	}
}

static int ffi_lua_push_boxed(lua_State* L, gconstpointer pval, GFFIType* tp, gboolean isnew) {
	glua_value_push_boxed(L, tp->gtype, (*((gconstpointer*)pval)), isnew);
	return 1;
}

static void ffi_lua_check_object(lua_State* L, int idx, gpointer pval, gboolean opt, GFFIType* tp) {
	GObject* v = opt ? glua_object_test(L, idx) : glua_object_check(L, idx);
	if( !opt ) {
		GType vtp = G_OBJECT_TYPE(v);
		if( !g_type_is_a(vtp, tp->gtype) ) {
			char err[512];
			sprintf(err, "check_object need(%s) but got(%s)", g_type_name(tp->gtype), g_type_name(vtp));
			luaL_argerror(L, idx, err);
		}
	} else if( v ) {
		GType vtp = G_OBJECT_TYPE(v);
		if( !g_type_is_a(vtp, tp->gtype) )
			v = NULL;
	}
	*((gpointer*)pval) = v;
}

static int ffi_lua_push_object(lua_State* L, gconstpointer pval, GFFIType* tp, gboolean isnew) {
	GObject* v = G_OBJECT(*(gpointer*)pval);
	if( v ) {
		GType vtp = v ? G_OBJECT_TYPE(v) : G_TYPE_INVALID;
		if( g_type_is_a(vtp, tp->gtype) ) {
			glua_object_push(L, v);
			if( isnew )
				g_object_unref(v);
		} else {
			char err[512];
			sprintf(err, "push_object need(%s) but got(%s)", g_type_name(tp->gtype), g_type_name(vtp));
			luaL_error(L, err);
		}
	} else {
		lua_pushnil(L);
	}
	return 1;
}

#define FFI_TYPEDEF(name, type, id, maybe_const)\
struct struct_align_##name {			\
  char c;					\
  type x;					\
};						\
static ffi_type ffi_type_##name = {	\
  sizeof(type),					\
  offsetof(struct struct_align_##name, x),	\
  id, NULL					\
}

/* Size and alignment are fake here. They must not be 0. */
static ffi_type ffi_type_void = { 1, 1, FFI_TYPE_VOID, NULL };

FFI_TYPEDEF(uint8, guchar, FFI_TYPE_UINT8, const);
FFI_TYPEDEF(sint8, gchar, FFI_TYPE_SINT8, const);
FFI_TYPEDEF(uint32, guint32, FFI_TYPE_UINT32, const);
FFI_TYPEDEF(sint32, gint32, FFI_TYPE_SINT32, const);
FFI_TYPEDEF(uint64, guint64, FFI_TYPE_UINT64, const);
FFI_TYPEDEF(sint64, gint64, FFI_TYPE_SINT64, const);

FFI_TYPEDEF(pointer, void*, FFI_TYPE_POINTER, const);

FFI_TYPEDEF(float, float, FFI_TYPE_FLOAT, const);
FFI_TYPEDEF(double, double, FFI_TYPE_DOUBLE, const);

static GFFIType* _gtype_get(GType gtype) {
	GFFIType* tp = g_type_get_qdata( (gtype==G_TYPE_INVALID) ? G_TYPE_NONE : gtype, __gtype_quark__ );
	if( !tp ) {
		GType fundamental_type = G_TYPE_IS_FUNDAMENTAL(gtype) ? gtype : g_type_fundamental(gtype);
		switch( fundamental_type ) {
		case G_TYPE_ENUM:
		case G_TYPE_FLAGS:
			tp = _gtype_reg(gtype, ffi_type_pointer, ffi_lua_check_uint, ffi_lua_push_uint);
			break;
		case G_TYPE_INTERFACE:
		case G_TYPE_OBJECT:
			tp = _gtype_reg(gtype, ffi_type_pointer, ffi_lua_check_object, ffi_lua_push_object);
			break;
		case G_TYPE_BOXED:
			if( gtype==G_TYPE_VALUE ) {
				tp = _gtype_reg(gtype, ffi_type_pointer, ffi_lua_check_gvalue, ffi_lua_push_gvalue);
			} else {
				tp = _gtype_reg(gtype, ffi_type_pointer, ffi_lua_check_boxed, ffi_lua_push_boxed);
			}
			break;
		default:
			break;
		}

		if( !tp )
			g_error("gtype(%s) not register as ffi type!", g_type_name(gtype));
	}
	return tp;
}

void gffi_init(void) {
	if( __gtype_quark__ )
		return;
	__gtype_quark__ = g_quark_from_static_string("__ks_gtype_ffi__");
	assert( __gtype_quark__ );

	// GType 
	_gtype_reg(G_TYPE_NONE,			ffi_type_void,		ffi_lua_check_none,		ffi_lua_push_none);
	_gtype_reg(G_TYPE_INTERFACE,	ffi_type_pointer,	ffi_lua_check_object,	ffi_lua_push_object);	// use object, need test
	_gtype_reg(G_TYPE_CHAR,			ffi_type_sint8,		ffi_lua_check_char,		ffi_lua_push_char);
	_gtype_reg(G_TYPE_UCHAR,		ffi_type_uint8,		ffi_lua_check_uchar,	ffi_lua_push_uchar);
	_gtype_reg(G_TYPE_BOOLEAN,		ffi_type_sint32,	ffi_lua_check_boolean,	ffi_lua_push_boolean);

	_gtype_reg(G_TYPE_INT,			((sizeof(gint)==4)   ? ffi_type_sint32 : ffi_type_sint64),	ffi_lua_check_int,		ffi_lua_push_int);
	_gtype_reg(G_TYPE_UINT,			((sizeof(gint)==4)   ? ffi_type_uint32 : ffi_type_uint64),	ffi_lua_check_uint,		ffi_lua_push_uint);

	_gtype_reg(G_TYPE_LONG,			((sizeof(glong)==4)  ? ffi_type_sint32 : ffi_type_sint64),	ffi_lua_check_int64,	ffi_lua_push_int64);
	_gtype_reg(G_TYPE_ULONG,		((sizeof(gulong)==4) ? ffi_type_uint32 : ffi_type_uint64),	ffi_lua_check_uint64,	ffi_lua_push_uint64);

	_gtype_reg(G_TYPE_INT64,		ffi_type_sint64,	ffi_lua_check_int64,	ffi_lua_push_int64);
	_gtype_reg(G_TYPE_UINT64,		ffi_type_uint64,	ffi_lua_check_uint64,	ffi_lua_push_uint64);
	_gtype_reg(G_TYPE_ENUM,			ffi_type_uint32,	ffi_lua_check_uint,		ffi_lua_push_uint);
	_gtype_reg(G_TYPE_FLAGS,		ffi_type_uint32,	ffi_lua_check_uint,		ffi_lua_push_uint);

	_gtype_reg(G_TYPE_FLOAT,		ffi_type_float,		ffi_lua_check_float,	ffi_lua_push_float);
	_gtype_reg(G_TYPE_DOUBLE,		ffi_type_double,	ffi_lua_check_double,	ffi_lua_push_double);
	_gtype_reg(G_TYPE_STRING,		ffi_type_pointer,	ffi_lua_check_string,	ffi_lua_push_string);
	_gtype_reg(G_TYPE_POINTER,		ffi_type_pointer,	ffi_lua_check_pointer,	ffi_lua_push_pointer);
	_gtype_reg(G_TYPE_BOXED,		ffi_type_pointer,	ffi_lua_check_boxed,	ffi_lua_push_boxed);
	// _gtype_reg(G_TYPE_PARAM,		ffi_type_pointer,	ffi_lua_check_param,	ffi_lua_push_param);
	_gtype_reg(G_TYPE_OBJECT,		ffi_type_pointer,	ffi_lua_check_object,	ffi_lua_push_object);
	// _gtype_reg(G_TYPE_VARIANT,	ffi_type_pointer,	ffi_lua_check_variant,	ffi_lua_push_variant);
}

static ffi_type _ffi_type_pointer = {sizeof(void*), sizeof(void*), FFI_TYPE_POINTER, NULL};

static int _lua_ffi_wrapper(lua_State* L) {
	GFFIFunction* up = (GFFIFunction*)lua_touserdata(L, lua_upvalueindex(1));
	int narg = (int)(up->narg);
	size_t args_ptr_size = sizeof(void*) * narg;
	size_t rtype_align = align_size(up->rtype->ffitp.alignment);
	size_t stack_size = rtype_align		// ret
				+ args_ptr_size			// values
				+ args_ptr_size			// pvalues
				+ up->args_size			// vals
				;
	guchar* stack = (guchar*)alloca(stack_size);
	void* ret = (void*)stack;
	void** values = (void**)(((guchar*)ret) + rtype_align);
	void** pvalues = (void**)(((guchar*)values) + args_ptr_size);	// for out or in-out argument
	guchar* pv = ((guchar*)pvalues) + args_ptr_size;
	size_t naret = 0;
	int argidx = 0;
	GFFIType* tp;
	int nret;
	int i;
	if( up->args_size )
		memset(pv, 0, up->args_size);

	// fetch args:
	for( i=0; i<narg; ++i ) {
		tp = up->atypes[i];
		pvalues[i] = (void*)pv;
		pv += tp->ffitp.alignment;

		if( up->amodes[i] & GFFI_ARG_MODE_IN ) {
			values[i] = pvalues[i];
			(*(tp->check))(L, ++argidx, pvalues[i], (up->amodes[i] & GFFI_ARG_MODE_OPT), tp);
		} else {
			// basic type, use pointer as memory
			values[i] = (void*)&(pvalues[i]);
		}

		if( up->amodes[i] & GFFI_ARG_MODE_OUT )
			++naret;
	}

	// ffi call
	ffi_call(&(up->cif), FFI_FN(up->func), ret, values);

	// ffi return
	nret = (*(up->rtype->push))(L, ret, up->rtype, up->rnew);

	// ffi argument return if need
	for( i=0; naret>0 && i<narg; ++i ) {
		guchar amode = up->amodes[i];
		if( amode & GFFI_ARG_MODE_OUT ) {
			--naret;
			nret += (*(up->atypes[i]->push))(L, pvalues[i], up->atypes[i], (amode & GFFI_ARG_MODE_NEW)?TRUE:FALSE);
		}
	}

	return nret;
}

static size_t _lua_ffi_types_calc_data_size(size_t narg, GFFIType** atypes) {
	size_t values_size = 0;
	size_t i;

	for( i=0; i<narg; ++i ) {
		values_size += atypes[i]->ffitp.alignment;
	}

	return values_size;
}

#define GFFI_FUNCTION_ARG_MAX	64

static size_t _fetch_narg(GType* at, GFFIType* atypes[GFFI_FUNCTION_ARG_MAX], guchar amodes[GFFI_FUNCTION_ARG_MAX]) {
	size_t n = 0;
	if( at ) {
		amodes[n] = 0;
		for( ; *at != G_TYPE_INVALID; ++at ) {
			if( n >= GFFI_FUNCTION_ARG_MAX ) {
				g_error("function arg num large then GFFI_FUNCTION_ARG_MAX!");
				abort();
			}

			if( *at==GFFI_PESUDO_TYPE_OUT_ARG ) {
				amodes[n] |= GFFI_ARG_MODE_OUT;
				continue;
			} else if( *at==GFFI_PESUDO_TYPE_REF_OUT_ARG ) {
				amodes[n] |= GFFI_ARG_MODE_OUT | GFFI_ARG_MODE_NEW;
				continue;
			} else if( *at==GFFI_PESUDO_TYPE_INOUT_ARG ) {
				amodes[n] |= GFFI_ARG_MODE_INOUT;
				continue;
			} else if( *at==GFFI_PESUDO_TYPE_OPT_ARG ) {
				amodes[n] |= GFFI_ARG_MODE_OPT;
				continue;
			} else if( (amodes[n] & GFFI_ARG_MODE_OUT)==0 ) {
				amodes[n] |= GFFI_ARG_MODE_IN;
			}

			atypes[n] = _gtype_get(*at);
			amodes[++n] = 0;
		}
	}
	return n;
}

gboolean gffi_function_create(lua_State* L, const char* name, GType _g_rtype, const void* addr, GType* _g_atypes) {
	GFFIType* atypes[GFFI_FUNCTION_ARG_MAX];
	guchar amodes[GFFI_FUNCTION_ARG_MAX];
	gboolean rnew = name[0]=='*';
	GFFIType* rtype = _gtype_get(_g_rtype);
	size_t narg = _fetch_narg(_g_atypes, atypes, amodes);
	size_t name_size = align_size( strlen(name) + 1 );
	size_t mode_size = align_size( sizeof(guchar)*narg );
	size_t args_size = _lua_ffi_types_calc_data_size(narg, atypes);
	size_t struct_size = sizeof(GFFIFunction)
		+ name_size						// name
		+ mode_size						// amodes
		+ sizeof(GFFIType*)*narg		// atypes
		+ sizeof(ffi_type*)*narg		// ffi_atypes
		//+ sizeof(void*)*narg			// defs
		//+ sizeof(void*)*narg			// _defs
		//+ args_size					// default values
		;
	GFFIFunction* f = (GFFIFunction*)lua_newuserdata(L, struct_size);
	guchar* p = ((guchar*)f) + sizeof(GFFIFunction);
	ffi_type** ffi_atypes;
	size_t i;
	memset(f, 0, struct_size);
	name = rnew ? (name+1) : name;
	f->name = (const char*)p;	memcpy(p, name, strlen(name));	p += name_size;
	f->func = addr;
	f->narg = narg;
	f->args_size = args_size;
	f->rnew = rnew;
	f->rtype = rtype;
	f->amodes = (guchar*)p;			p += mode_size;
	f->atypes = (GFFIType**)p;		p += (sizeof(GFFIType*)*narg);
	ffi_atypes = (ffi_type**)p;		p += (sizeof(ffi_type*)*narg);
	//f->defs = (void**)p;			p += (sizeof(void*)*narg);
	//f->_defs = (void**)p;			p += (sizeof(void*)*narg);

	// args 
	memcpy(f->atypes, atypes, sizeof(GFFIType*)*narg);
	memcpy(f->amodes, amodes, sizeof(guchar)*narg);
	memcpy(ffi_atypes, atypes, sizeof(GFFIType*)*narg);
	for( i=0; i<narg; ++i ) {
		if( amodes[i] & GFFI_ARG_MODE_OUT )
			ffi_atypes[i] = &_ffi_type_pointer;
	}

	// cif 
	if( FFI_OK!=ffi_prep_cif(&f->cif, FFI_DEFAULT_ABI, (unsigned int)narg, (ffi_type*)(f->rtype), ffi_atypes) ) {
		lua_pop(L, 1);
		lua_pushnil(L);
		return FALSE;
	}

	// defs 
	//for( i=0; i<narg; ++i ) {
	//	f->defs[i] = NULL;
	//	f->_defs[i] = p;
	//	p += atypes[i]->tp.alignment;
	//}

	assert( (((size_t)p) - ((size_t)f))==struct_size );
	lua_pushcclosure(L, _lua_ffi_wrapper, 1);
	return TRUE;
}

gboolean gffi_function_va_create(lua_State* L, const char* name, GType rtype, const void* addr, ...) {
	GType atypes[GFFI_FUNCTION_ARG_MAX*2];
	GType* ps = atypes;
	GType* pe = ps + (sizeof(atypes) / sizeof(GType));
	va_list args;
	va_start(args, addr);
	for(;;) {
		if( ps >= pe ) {
			g_error("function arg num large then GFFI_FUNCTION_ARG_MAX!");
			abort();
		}

		*ps = va_arg(args, GType);
		if( *ps==G_TYPE_INVALID )
			break;
		++ps;
	}
	va_end(args);

	return gffi_function_create(L, name, rtype, addr, atypes);
}

