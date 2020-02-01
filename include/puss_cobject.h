// puss_cobject.h

#ifndef _PUSS_LUA_INC_COBJECT_H_
#define _PUSS_LUA_INC_COBJECT_H_

#include <stdint.h>
#include "puss_lua.h"

#define	PUSS_COBJECT_MEMALIGN_SIZE(size)	( ((size) + (sizeof(void*)-1)) & (~(sizeof(void*)-1)) )

#define PUSS_COBJECT_IDMASK_SUPPORT_REF		0x80000000	// object ref & cache support
#define PUSS_COBJECT_IDMASK_SUPPORT_PROPS	0x40000000	// property support: formula, change notify, dirty notify

#define	PUSS_CVTYPE_CUSTOM	0
#define	PUSS_CVTYPE_BOOL	1
#define	PUSS_CVTYPE_INT		2
#define	PUSS_CVTYPE_NUM		3
#define	PUSS_CVTYPE_STR		4
#define	PUSS_CVTYPE_LUA		5
#define	PUSS_CVTYPE_COUNT	6

#ifndef TRUE
	#define TRUE 1
#endif

#ifndef FALSE
	#define FALSE 0
#endif

typedef struct _PussCModule	PussCModule;
typedef struct _PussCSchema	PussCSchema;
typedef struct _PussCObject	PussCObject;
typedef union  _PussCValue	PussCValue;

typedef struct _PussCString	PussCString;
typedef lua_Integer	PussCBool;
typedef lua_Integer	PussCInt;
typedef lua_Number	PussCNum;
typedef lua_Integer	PussCLua;

typedef struct _PussCPropsModule	PussCPropsModule;

// lua stack: 1:object -1:new-value
typedef int  (*PussCObjectFormula)(lua_State* L, const PussCObject* obj, lua_Integer field);	// [-0, +1, e] push new value, return succeed

// lua stack: 1:object
typedef void (*PussCObjectChanged)(lua_State* L, const PussCObject* obj, lua_Integer field);

struct _PussCString {
	size_t				len;
	char				buf[1];
};

union _PussCValue {
	void*				p;
	PussCBool			b;
	PussCInt			i;
	PussCNum			n;
	PussCString*		s;
	PussCLua			o;
};

struct _PussCObject {
	const PussCSchema*	schema;
	PussCPropsModule*	props_module;
	PussCValue			values[1];	// values[0] no use!
};

#ifdef _PUSS_IMPLEMENT
	const PussCObject*	puss_cobject_check(lua_State* L, int arg, lua_Unsigned struct_id_mask);
	const PussCObject*	puss_cobject_test(lua_State* L, int arg, lua_Unsigned struct_id_mask);

	int   puss_cobject_get(lua_State* L, int obj, lua_Integer field);	// [-0, +1, e] return typeof value
	int   puss_cobject_set(lua_State* L, int obj, lua_Integer field);	// [-1, +0, e] pop new-value from stack, return set succeed

	void  puss_cobject_batch_call(lua_State* L, int obj, int nargs, int nresults);	// [-(1+nargs), +(nresults), e]

	void  puss_cschema_formular_reset(lua_State* L, int creator, lua_Integer field, PussCObjectFormula formular);	// [-1, +0, e] pop module ref from stack
	void  puss_cschema_changed_reset(lua_State* L, int creator, const char* name, PussCObjectChanged handle);		// [-0|-1, +0, e] pop module ref from stack if handle!=NULL
#else
	#define puss_cobject_check(L, arg, struct_id_mask)		(*(__puss_iface__->cobject_check))((L),(arg),(struct_id_mask))
	#define puss_cobject_test(L, arg, struct_id_mask)		(*(__puss_iface__->cobject_test))((L),(arg),(struct_id_mask))

	#define puss_cobject_get(L, obj, field)					(*(__puss_iface__->cobject_get))((L),(obj),(field))
	#define puss_cobject_set(L, obj, field)					(*(__puss_iface__->cobject_set))((L),(obj),(field))

	#define puss_cobject_batch_call(L, obj, nargs, nret)	(*(__puss_iface__->cobject_batch_call))((L),(obj),(nargs),(nret))

	#define puss_cschema_formular_reset(L,c,i,f)			(*(__puss_iface__->cschema_formular_reset))((L),(c),(i),(f))
	#define puss_cschema_changed_reset(L,c,n,h)				(*(__puss_iface__->cschema_changed_reset))((L),(c),(n),(h))
#endif

#endif//_PUSS_LUA_INC_COBJECT_H_
