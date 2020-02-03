// puss_cobject.h

#ifndef _PUSS_LUA_INC_COBJECT_H_
#define _PUSS_LUA_INC_COBJECT_H_

#include <stdint.h>
#include "puss_lua.h"

#define	PUSS_COBJECT_MEMALIGN_SIZE(size)	( ((size) + (sizeof(void*)-1)) & (~(sizeof(void*)-1)) )

#define PUSS_COBJECT_IDMASK_SUPPORT_REF		0x80000000	// object ref & cache support
#define PUSS_COBJECT_IDMASK_SUPPORT_PROP	0x40000000	// property support: formula, change notify, dirty notify
#define PUSS_COBJECT_IDMASK_SUPPORT_SYNC	0x20000000	// sync support

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

typedef struct _PussCStr	PussCStr;
typedef lua_Integer	PussCBool;
typedef lua_Integer	PussCInt;
typedef lua_Number	PussCNum;
typedef lua_Integer	PussCLua;

typedef struct _PussCPropModule	PussCPropModule;
typedef struct _PussCSyncModule	PussCSyncModule;

// lua stack: -1:new-value
typedef int  (*PussCObjectFormula)(lua_State* L, const PussCObject* obj, lua_Integer field);	// [-0, +1, e] push new value, return succeed

typedef void (*PussCObjectChanged)(lua_State* L, const PussCObject* obj, lua_Integer field);

struct _PussCStr {
	size_t		len;
	char		buf[1];
};

union _PussCValue {
	void*		p;
	PussCBool	b;
	PussCInt	i;
	PussCNum	n;
	PussCStr*	s;
	PussCLua	o;
};

struct _PussCObject {
	const PussCSchema*	schema;
	PussCPropModule*	prop_module;
	PussCSyncModule*	sync_module;
	PussCValue			values[1];	// values[0] no use!
};

// puss-toolkit & plugin both used intrfaces
// 
#ifdef _PUSS_IMPLEMENT
	const PussCObject*	puss_cobject_check(lua_State* L, int arg, lua_Unsigned id_mask);
	const PussCObject*	puss_cobject_test(lua_State* L, int arg, lua_Unsigned id_mask);

	int   puss_cobject_stack_get(lua_State* L, const PussCObject* obj, lua_Integer field);	// [-0, +1, e] return PUSS_CVTYPE_ type
	int   puss_cobject_stack_set(lua_State* L, const PussCObject* obj, lua_Integer field);	// [-1, +0, e] pop new-value from stack, return set succeed

	int   puss_cobject_set_bool(lua_State* L, const PussCObject* obj, lua_Integer field, PussCBool nv);	// return set succeed
	int   puss_cobject_set_int(lua_State* L, const PussCObject* obj, lua_Integer field, PussCInt nv);	// return set succeed
	int   puss_cobject_set_num(lua_State* L, const PussCObject* obj, lua_Integer field, PussCNum nv);	// return set succeed
	int   puss_cobject_set_str(lua_State* L, const PussCObject* obj, lua_Integer field, const char* str, size_t len);	// return set succeed

	void  puss_cschema_formular_reset(lua_State* L, int creator, lua_Integer field, PussCObjectFormula formular);	// top of stack used as module ref
	void  puss_cschema_changed_reset(lua_State* L, int creator, const char* name, PussCObjectChanged handle);		// top of stack used as module ref
#else
	#define puss_cobject_check(L, arg, id_mask)		(*(__puss_iface__->cobject_check))((L),(arg),(id_mask))
	#define puss_cobject_test(L, arg, id_mask)		(*(__puss_iface__->cobject_test))((L),(arg),(id_mask))

	#define puss_cobject_stack_get(L, obj, field)	(*(__puss_iface__->cobject_stack_get))((L),(obj),(field))
	#define puss_cobject_stack_set(L, obj, field)	(*(__puss_iface__->cobject_stack_set))((L),(obj),(field))
	#define puss_cobject_set_bool(L,o,f,v)			(*(__puss_iface__->cobject_set_bool))((L),(o),(f),(v))
	#define puss_cobject_set_int(L,o,f,v)			(*(__puss_iface__->cobject_set_int))((L),(o),(f),(v))
	#define puss_cobject_set_num(L,o,f,v)			(*(__puss_iface__->cobject_set_num))((L),(o),(f),(v))
	#define puss_cobject_set_str(L,o,f,s,n)			(*(__puss_iface__->cobject_set_str))((L),(o),(f),(s),(n))

	#define puss_cschema_formular_reset(L,c,i,f)	(*(__puss_iface__->cschema_formular_reset))((L),(c),(i),(f))
	#define puss_cschema_changed_reset(L,c,n,h)		(*(__puss_iface__->cschema_changed_reset))((L),(c),(n),(h))
#endif

// puss-toolkit interfaces, can not used in plugin
// 
#ifdef _PUSS_IMPLEMENT
	void  puss_cobject_batch_call(lua_State* L, int obj, int nargs, int nresults);	// [-(1+nargs), +(nresults), e]

	typedef struct _PussCSyncInfo {
		lua_Unsigned	id_mask;
		size_t			field_count;
		size_t			sync_count;
		const uint8_t*	field_masks;	// array[1+field_count]
	} PussCSyncInfo;

	void   puss_cobject_sync_fetch_infos(lua_State* L, int creator, PussCSyncInfo* info);
	size_t puss_cobject_sync_fetch_and_clear(lua_State* L, const PussCObject* obj, uint16_t* res, size_t len);	// len MUST >= PussCSyncInfo.sync_count
	size_t puss_cobject_sync_fetch_and_reset(lua_State* L, const PussCObject* obj, uint16_t* res, size_t len, uint8_t filter_mask);	// len MUST >= PussCSyncInfo.sync_count
#endif

#endif//_PUSS_LUA_INC_COBJECT_H_
