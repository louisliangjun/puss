// puss_cobject.c

#define _PUSS_IMPLEMENT
#include "puss_cobject.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

#include "puss_bitset.h"

#define DEFAULT_DIRTY_NOTIFY_MAX_LOOP_TIMES		3

#define PUSS_CSCHEMA_MT	"PussCSchema"
#define PUSS_COBJECT_MT	"PussCObject"
#define PUSS_COBJREF_MT	"PussCObjRef"

typedef struct _PussCProperty {
	const char*			name;			// property name
	PussCObjectFormula	formular;		// formular
	const uint16_t*		actives;		// deps actives list, actives[0] used for num of array
	uint16_t			level;			// dependence level
	uint8_t				notify;			// change notify to lua
} PussCProperty;

struct _PussCSchema {
	// basic
	lua_Unsigned		id_mask;
	lua_Integer			field_count;
	const char*			types;
	const PussCValue*	defs;
	int					values_ref;
	int					formulars_ref;

	// cache
	size_t				total_count;
	size_t				cache_count;

	// property module
	PussCProperty*		properties;				// array of properties
	PussCObjectChanged*	change_handles;			// array of handles, endswith NULL
	lua_Integer			prop_dirty_notify_max_loop_times;	// may modify object in changed event, so need loop notify dirty, default loop 3 times
	int					change_notify_script;	// properties has notify & script notify handle registed
	int					change_notify_mode;		// 0-module first other-property first
	int					change_handle_ref;

	// property sync module
	uint16_t			sync_field_count;
	const uint8_t*		sync_field_mask;		// array[1+field_count]
	const uint16_t*		sync_field_idx;			// array[1+field_count]
	const uint16_t*		sync_idx_to_field;		// array[1+sync_field_count] of sync index to field
};

// ref object support

typedef struct _PussCObjRef {
	PussCObject*	obj;
} PussCObjRef;

// property module

struct _PussCPropModule {
	int			lock;			// lock changes notify
	uint16_t	dirty[1];
};

static inline void prop_dirtys_init(PussCObject* obj) {
	uint16_t num = (uint16_t)(obj->schema->field_count);
	uint16_t* arr = obj->prop_module->dirty;
	obj->prop_module->lock = 0;
	PUSS_BITSETLIST_INIT(num, arr);
}

static inline int prop_dirtys_test(PussCObject* obj, uint16_t pos) {
	uint16_t num = (uint16_t)(obj->schema->field_count);
	uint16_t* arr = obj->prop_module->dirty;
	return PUSS_BITSETLIST_TEST(num, arr, pos) ? 1 : 0;
}

static inline void prop_dirtys_set(PussCObject* obj, uint16_t pos) {
	const PussCProperty* props = obj->schema->properties;
	uint16_t num = (uint16_t)(obj->schema->field_count);
	uint16_t* arr = obj->prop_module->dirty;
	const uint16_t* actives = props[pos].actives;
	uint16_t i, id;
	if( !PUSS_BITSETLIST_TEST(num, arr, pos) ) {
		for( i=actives[0]; i>0; --i ) {
			id = actives[i];
			assert( id > 0 );
			assert( id <= num );
			// assert( props[id].formular );
			if( !PUSS_BITSETLIST_TEST(num, arr, id) ) {
				PUSS_BITSETLIST_SET(num, arr, id);
				PUSS_BITSETLIST_RESET(num, arr, id);
			}
		}
		PUSS_BITSETLIST_SET(num, arr, pos);
	}
}

static inline void prop_dirtys_reset(PussCObject* obj, uint16_t pos) {
	uint16_t num = (uint16_t)(obj->schema->field_count);
	uint16_t* arr = obj->prop_module->dirty;
	PUSS_BITSETLIST_RESET(num, arr, pos);
}

static inline void prop_dirtys_clear(PussCObject* obj) {
	uint16_t num = (uint16_t)(obj->schema->field_count);
	uint16_t* arr = obj->prop_module->dirty;
	PUSS_BITSETLIST_CLEAR(num, arr);
}

// sync module

struct _PussCSyncModule {
	uint16_t	dirty[1];
};

static inline void sync_dirtys_init(PussCObject* obj) {
	uint16_t num = (uint16_t)(obj->schema->sync_field_count);
	uint16_t* arr = obj->sync_module->dirty;
	PUSS_BITSETLIST_INIT(num, arr);
}

static void sync_on_changed(const PussCStackObject* stobj, lua_Integer field) {
	PussCObject* obj = (PussCObject*)(stobj->obj);
	const PussCSchema* schema = obj->schema;
	uint16_t sync_idx, num, *arr;
	assert( schema );
	assert( obj->sync_module );
	assert( (field > 0) && (field <= schema->field_count) );
	if( !(sync_idx = schema->sync_field_idx[field]) )
		return;
	num = schema->sync_field_count;
	arr = obj->sync_module->dirty;
	PUSS_BITSETLIST_SET(num, arr, sync_idx);
}

// values

static int cobj_geti(lua_State* L, const PussCObject* obj, lua_Integer field) {
	const PussCValue* v = &(obj->values[field]);
	int tp = LUA_TNIL;
	switch( obj->schema->types[field] ) {
	case PUSS_CVTYPE_BOOL: lua_pushboolean(L, v->b!=0); tp = LUA_TBOOLEAN; break;
	case PUSS_CVTYPE_INT:  lua_pushinteger(L, v->i); tp = LUA_TNUMBER; break;
	case PUSS_CVTYPE_NUM:  lua_pushnumber(L, v->n); tp = LUA_TNUMBER; break;
	case PUSS_CVTYPE_LUA:
		if( v->o > 0 ) {
			lua_rawgeti(L, LUA_REGISTRYINDEX, obj->schema->values_ref);
			lua_rawgeti(L, -1, v->o);
			tp = lua_rawgetp(L, -1, obj);
			lua_copy(L, -1, -3);
			lua_pop(L, 2);
		} else {
			lua_pushnil(L);
		}
		break;
	default:
		lua_pushnil(L);
		break;
	}
	return tp;
}

static int cobj_seti_vlua(const PussCStackObject* stobj, lua_Integer field, PussCValue* v) {
	PussCObject* obj = (PussCObject*)(stobj->obj);
	lua_State* L = stobj->L;
	int top = lua_gettop(L);
	int vtype = lua_type(L, top);
	int changed = FALSE;
	if( top <= stobj->arg )
		luaL_error(L, "need value");
	lua_rawgeti(L, LUA_REGISTRYINDEX, obj->schema->values_ref);
	lua_rawgeti(L, -1, field);	// values
	if( lua_rawgetp(L, -1, obj)!=vtype ) {		// fetch old value
		changed = TRUE;	// type changed
	} else if( vtype==LUA_TTABLE || vtype==LUA_TUSERDATA ) {
		changed = TRUE;	// table & userdata, always return changed, used for notify changed
	} else {
		changed = !lua_rawequal(L, top, -1);	// value changed
	}
	if( changed ) {
		lua_pushvalue(L, top);
		lua_rawsetp(L, -3, obj);
		v->o = (vtype!=LUA_TNIL) ? field : -field;
	}
	lua_settop(L, top-1);
	return changed;
}

static int cobj_seti(const PussCStackObject* stobj, lua_Integer field) {
	PussCObject* obj = (PussCObject*)(stobj->obj);
	lua_State* L = stobj->L;
	PussCObjectFormula formular = obj->schema->properties[field].formular;
	PussCValue* v = (PussCValue*)&(obj->values[field]);
	char tp = obj->schema->types[field];
	int st = 0;	// -1: failed 0: nochange 1:changed
	PussCValue nv;
	if( (field <= 0) || (field > obj->schema->field_count) )
		return -1;	// failed

	// fill nv
	switch( tp ) {
	case PUSS_CVTYPE_BOOL: nv.b = lua_toboolean(L, -1); break;
	case PUSS_CVTYPE_INT:  nv.i = luaL_checkinteger(L, -1); break;
	case PUSS_CVTYPE_NUM:  nv.n = luaL_checknumber(L, -1); break;
	case PUSS_CVTYPE_LUA:  break;
	}

	if( formular && (!(*formular)(stobj, field, &nv)) ) {
		st = -1;
	} else if( tp==PUSS_CVTYPE_LUA ) {
		st = cobj_seti_vlua(stobj, field, v);
	} else if( v->p != nv.p ) {
		*v = nv;
		st = 1;
	}
	return st;
}

static void cobj_do_clear(lua_State* L, const PussCSchema* schema, const PussCObject* obj) {
	PussCValue* values = (PussCValue*)(obj->values);
	const PussCValue* defs = schema->defs;
	lua_Integer i;
	lua_rawgeti(L, LUA_REGISTRYINDEX, schema->values_ref);
	for( i=1; i<=schema->field_count; ++i ) {
		char vt = schema->types[i];
		PussCValue* v = &(values[i]);
		if( (vt==PUSS_CVTYPE_LUA) && (v->o > 0) ) {
			v->o = -i;
			lua_rawgeti(L, -1, i);
			lua_rawsetp(L, -1, obj);
			lua_pop(L, 1);
		} else {
			*v = defs[i];
		}
	}
	lua_pop(L, 1);
}

static lua_Integer sync_fetch_and_reset(lua_State* L, PussCObject* obj, PussCSyncModule* m, int ret, uint8_t filter_mask) {
	const uint8_t* masks = obj->schema->sync_field_mask;
	const uint16_t* map = obj->schema->sync_idx_to_field;
	uint16_t num = obj->schema->sync_field_count;
	uint16_t* arr = m->dirty;
	uint16_t pos;
	lua_Integer i = 0;
	lua_Integer field;
	if( filter_mask==0xFF ) {
		PUSS_BITSETLIST_ITER(num, arr, pos) {
			assert( (pos > 0) && (pos <= num) );
			field = map[pos];
			assert( (field > 0) && (field <= obj->schema->field_count) );

			lua_pushinteger(L, field);			lua_rawseti(L, ret, ++i);
			lua_pushinteger(L, masks[field]);	lua_rawseti(L, ret, ++i);
			cobj_geti(L, obj, field);			lua_rawseti(L, ret, ++i);
		}
		PUSS_BITSETLIST_CLEAR(num, arr);
	} else {
		PUSS_BITSETLIST_ITER(num, arr, pos) {
			if( PUSS_BITSETLIST_TEST(num, arr, pos) ) {
				field = map[pos];
				if( (masks[field] & filter_mask)==filter_mask ) {
					PUSS_BITSETLIST_RESET(num, arr, pos);

					lua_pushinteger(L, field);			lua_rawseti(L, ret, ++i);
					lua_pushinteger(L, masks[field]);	lua_rawseti(L, ret, ++i);
					cobj_geti(L, obj, field);			lua_rawseti(L, ret, ++i);
				}
			}
		}
	}
	return i / 3;
}

static inline uint16_t _dirtys_count(uint16_t num, uint16_t* arr) {
	uint16_t n = 0;
	uint16_t pos;
	PUSS_BITSETLIST_ITER(num, arr, pos)
		++n;
	return n;
}

static int lua_formular_wrap(const PussCStackObject* stobj, lua_Integer field, PussCValue* nv) {
	const PussCObject* obj = stobj->obj;
	lua_State* L = stobj->L;
	int top = lua_gettop(L);
	int tp = obj->schema->types[field];
	int state = LUA_OK;
	assert( (field > 0) && (field <= obj->schema->field_count) );
	lua_rawgeti(L, LUA_REGISTRYINDEX, obj->schema->formulars_ref);
	lua_rawgeti(L, -1, field);	// formular
	lua_replace(L, -2);
	assert( lua_isfunction(L, -1) );
	assert( stobj->arg > 0 );
	assert( puss_cobject_testudata(L, stobj->arg, obj->schema->id_mask)==obj );
	assert( top >= stobj->arg );
	lua_pushvalue(L, stobj->arg );	// object

	switch( tp ) {
	case PUSS_CVTYPE_BOOL:
		lua_pushboolean(L, nv->b!=0);
		if( (state = lua_pcall(L,2,1,0))==LUA_OK )
			nv->b = lua_toboolean(L, -1);
		break;
	case PUSS_CVTYPE_INT:
		lua_pushinteger(L, nv->i);
		if( (state = lua_pcall(L,2,1,0))==LUA_OK )
			nv->i = luaL_checkinteger(L, -1);
		break;
	case PUSS_CVTYPE_NUM:
		lua_pushnumber(L, nv->n);
		if( (state = lua_pcall(L,2,1,0))==LUA_OK )
			nv->n = luaL_checknumber(L, -1);
		break;
	case PUSS_CVTYPE_LUA:
		assert( top > stobj->arg );
		assert( lua_gettop(L)==top+2 );
		lua_rotate(L, top, 2);
		if( (state = lua_pcall(L,2,1,0))==LUA_OK ) {
			assert( lua_gettop(L)==top );
		} else {
			assert( lua_gettop(L)==top );
			--top;
		}
	}
	lua_settop(L, top);
	return state==LUA_OK;
}

static void props_do_notify_module_first(const PussCStackObject* stobj, const PussCProperty* props, const PussCObjectChanged* changes, int notify_script, uint16_t* updates, uint16_t n, int top) {
	const PussCObject* obj = stobj->obj;
	lua_State* L = stobj->L;
	PussCObjectChanged h;
	uint16_t i;

	if( changes ) {
		assert( changes == obj->schema->change_handles );
		for( ; *changes; ++changes ) {
			h = *changes;
			for( i=0; i<n; ++i ) {
				assert( updates[i] );
				assert( lua_gettop(L) == top );
				// notify prop changed, changes 
				(*h)(stobj, updates[i]);
				assert( lua_gettop(L) >= top );
				lua_settop(L, top);
			}
		}
	}

	// notify lua changed handle
	if( notify_script ) {
		if( lua_rawgeti(L, LUA_REGISTRYINDEX, obj->schema->change_handle_ref)==LUA_TFUNCTION ) {
			for( i=0; i<n; ++i ) {
				if( props[updates[i]].notify ) {
					lua_pushvalue(L, -1);
					lua_pushvalue(L, 1);
					lua_pushinteger(L, updates[i]);
					if( lua_pcall(L, 2, 0, 0) )
						lua_pop(L, 1);
				}
			}
		}
		lua_settop(L, top);
	}
}

static void props_do_notify_property_first(const PussCStackObject* stobj, const PussCProperty* props, const PussCObjectChanged* changes, int notify_script, uint16_t* updates, uint16_t n, int top) {
	const PussCObject* obj = stobj->obj;
	lua_State* L = stobj->L;
	const PussCObjectChanged* p;
	PussCObjectChanged h;
	uint16_t pos, i;
	for( i=0; i<n; ++i ) {
		pos = updates[i];
		assert( changes == obj->schema->change_handles );
		assert( pos );
		// notify prop changes
		if( (p = changes) != NULL ) {
			for( ; *p; ++p ) {
				h = *p;
				assert( lua_gettop(L) == top );
				// notify prop changed, changes 
				(*h)(stobj, pos);
				assert( lua_gettop(L) >= top );
				lua_settop(L, top);
			}
		}
		if( notify_script && props[updates[i]].notify ) {
			lua_rawgeti(L, LUA_REGISTRYINDEX, obj->schema->change_handle_ref);
			lua_pushvalue(L, 1);
			lua_pushinteger(L, pos);
			if( lua_pcall(L, 2, 0, 0) )
				lua_pop(L, 1);
			assert( lua_gettop(L) == top );
		}
	}
}

static void props_apply_formular_and_notify_once(const PussCStackObject* stobj, uint16_t num, uint16_t* arr, uint16_t n, int top) {
	PussCObject* obj = (PussCObject*)(stobj->obj);
	lua_State* L = stobj->L;
	const char* types = obj->schema->types;
	const PussCProperty* props = obj->schema->properties;
	const PussCObjectChanged* changes = obj->schema->change_handles;
	PussCPropModule* m = (PussCPropModule*)(obj->prop_module);
	uint16_t* updates = (uint16_t*)_alloca(sizeof(uint16_t) * n);
	uint16_t pos, i, j;
	int notify_script = obj->schema->change_notify_script;
	int changed;
	PussCObjectFormula formular;
	PussCValue* v;
	PussCValue nv;

	// sort insert to updates
	updates[0] = pos = arr[0] & PUSS_BITSETLIST_MASK_POS;
	for( i=1; i<n; ++i ) {
		pos = arr[pos] & PUSS_BITSETLIST_MASK_POS;
		assert( pos );
		for( j=i; j>0; --j ) {
			if( props[updates[j-1]].level <= props[pos].level )
				break;
			updates[j] = updates[j-1];
		}
		updates[j] = pos;
	}

	// update by formular & set updates[i]=0 in updates list whitch not dirty
	for( i=0; i<n; ++i ) {
		pos = updates[i];
		// not dirty & in active list, need use formular update values
		if( (!PUSS_BITSETLIST_TEST(num, arr, pos)) ) {
			changed = FALSE;
			if( (formular = props[pos].formular)!=NULL ) {
				nv = *(v = (PussCValue*)(&obj->values[pos]));
				if( types[pos]==PUSS_CVTYPE_LUA ) {
					cobj_geti(L, obj, pos);
					if( (*formular)(stobj, pos, &nv) )
						changed = cobj_seti_vlua(stobj, pos, v);
				} else {
					if( (*formular)(stobj, pos, &nv) ) {
						changed = (nv.p != v->p);
						*v = nv;
					}
				}
			}
			if( changed ) {
				prop_dirtys_set((PussCObject*)obj, pos);
			} else {
				assert( !PUSS_BITSETLIST_TEST(num, arr, pos) );
				updates[i] = 0;
			}
			// assert formular modify self only!
			assert( n==_dirtys_count(num, arr) );
			lua_settop(L, top);
		}
	}

	// clear dirtys
	PUSS_BITSETLIST_CLEAR(num, arr);

	if( changes || notify_script ) {
		notify_script = 0;
		i = 0;
		for( j=i; j<n; ++j ) {
			if( (pos = updates[j])!=0 ) {
				if( i != j )
					updates[i] = pos;
				if( props[pos].notify )
					notify_script = 1;
				++i;
			}
		}
		n = i;
		if( n > 0 ) {
			if( obj->schema->change_notify_mode==0 ) {
				props_do_notify_module_first(stobj, props, changes, notify_script, updates, n, top);
			} else {
				props_do_notify_property_first(stobj, props, changes, notify_script, updates, n, top);
			}
		}
	}
}

static void props_changes_notify(const PussCStackObject* stobj) {
	PussCObject* obj = (PussCObject*)(stobj->obj);
	uint16_t num = (uint16_t)(obj->schema->field_count);
	uint16_t* arr;
	uint16_t dirty_num;
	lua_Integer loop_count = obj->schema->prop_dirty_notify_max_loop_times;
	int top = lua_gettop(stobj->L);
	assert( obj->prop_module && obj->prop_module->lock );
	arr = obj->prop_module->dirty;

	while( ((--loop_count) >= 0) && ((dirty_num = _dirtys_count(num, arr)) > 0) ) {
		props_apply_formular_and_notify_once(stobj, num, arr, dirty_num, top);
	}
	lua_settop(stobj->L, top);
}

static inline void props_mark_dirty_and_notify(const PussCStackObject* stobj, lua_Integer field) {
	PussCObject* obj = (PussCObject*)(stobj->obj);
	PussCPropModule* m = obj->prop_module;
	if( m ) {
		prop_dirtys_set(obj, (uint16_t)field);
		if( !(m->lock) ) {
			m->lock = TRUE;
			props_changes_notify(stobj);
			m->lock = FALSE;
		}
	}
}

static PussCObject* cobject_check(lua_State* L, int arg) {
	PussCObject* obj = (PussCObject*)lua_touserdata(L, arg);
	if( !obj)
		luaL_argerror(L, arg, "cobject check failed!");
	if( lua_getmetatable(L, arg)	// MT
		&& lua_getmetatable(L, -1)	// MT.MT
		&& luaL_getmetatable(L, PUSS_COBJECT_MT)==LUA_TTABLE
		&& (!lua_rawequal(L, -2, -1)) )
	{
		luaL_argerror(L, 1, "cobject check failed!");
	}
	lua_pop(L, 3);
	return obj;
}

static int cobject_gc(lua_State* L) {
	PussCObject* obj = cobject_check(L, 1);
	PussCSchema* schema = (PussCSchema*)(obj->schema);
	if( !schema )
		return 0;
	schema->total_count--;
	obj->schema = NULL;
	cobj_do_clear(L, schema, obj);
	return 0;
}

static int cobject_clear(lua_State* L) {
	PussCObject* obj = cobject_check(L, 1);
	cobj_do_clear(L, obj->schema, obj);
	lua_settop(L, 1);
	return 1;
}

static int cobject_get(lua_State* L) {
	PussCObject* obj = cobject_check(L, 1);
	if( lua_type(L, 2)==LUA_TSTRING ) {
		lua_getmetatable(L, 1);
		lua_pushvalue(L, 2);
		lua_gettable(L, -2);
	} else {
		lua_Integer field = luaL_checkinteger(L, 2);
		luaL_argcheck(L, (field>0 && field<=obj->schema->field_count), 2, "out of range");
		cobj_geti(L, obj, field);
	}
	return 1;
}

static int cobject_set(lua_State* L) {
	PussCStackObject stobj = { cobject_check(L, 1), L, 1 };
	lua_Integer field = luaL_checkinteger(L, 2);
	luaL_checkany(L, 3);
	int st = cobj_seti(&stobj, field);
	if( st > 0 ) {
		props_mark_dirty_and_notify(&stobj, field);
		lua_pushboolean(L, TRUE);
	} else {
		lua_pushboolean(L, st==0 );
	}
	return 1;
}

static int cobject_call(lua_State* L) {
	PussCStackObject stobj = { cobject_check(L, 1), L, 1 };
	PussCPropModule* m = stobj.obj->prop_module;
	luaL_checktype(L, 2, LUA_TFUNCTION);
	lua_pushvalue(L, 1);
	if( m==NULL || m->lock ) {
		lua_pushvalue(L, 2);
		lua_replace(L, 1);
		lua_replace(L, 2);
		lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);
		return lua_gettop(L);
	}
	lua_insert(L, 3);
	m->lock = TRUE;
	if( lua_pcall(L, lua_gettop(L) - 2, LUA_MULTRET, 0) )
		lua_pop(L, 1);
	props_changes_notify(&stobj);
	m->lock = FALSE;
	return lua_gettop(L) - 1;
}

static int cobject_sync(lua_State* L) {
	PussCObject* obj = cobject_check(L, 1);
	PussCSyncModule* m = obj->sync_module;
	lua_Unsigned filter_mask;
	luaL_checktype(L, 2, LUA_TTABLE);
	filter_mask = (lua_Unsigned)luaL_optinteger(L, 3, 0x0FF);
	luaL_argcheck(L, (filter_mask & 0x0FF)==filter_mask, 3, "bad mask!");
	lua_pushinteger(L, m ? sync_fetch_and_reset(L, obj, m, 2, (uint8_t)filter_mask) : 0);
	return 1;
}

static int cobject_create(lua_State* L) {
	PussCSchema* schema = (PussCSchema*)lua_touserdata(L, lua_upvalueindex(2));
	size_t _prop_module_sz = sizeof(PussCPropModule) + sizeof(uint16_t) * schema->field_count;
	size_t _sync_module_sz = sizeof(PussCSyncModule) + sizeof(uint16_t) * schema->sync_field_count;
	size_t prop_module_sz = (schema->id_mask & PUSS_COBJECT_IDMASK_SUPPORT_PROP) ? PUSS_COBJECT_MEMALIGN_SIZE(_prop_module_sz) : 0;
	size_t sync_module_sz = (schema->id_mask & PUSS_COBJECT_IDMASK_SUPPORT_SYNC) ? PUSS_COBJECT_MEMALIGN_SIZE(_sync_module_sz) : 0;
	size_t values_sz = sizeof(PussCValue) * schema->field_count;
	size_t cobject_sz = sizeof(PussCObject) + values_sz + prop_module_sz + sync_module_sz;
	PussCObject* ud = (PussCObject*)lua_newuserdata(L, cobject_sz);
	ud->schema = schema;
	ud->prop_module = NULL;
	ud->sync_module = NULL;
	memcpy(ud->values, schema->defs, sizeof(PussCValue) * (1 + schema->field_count));
	if( prop_module_sz ) {
		ud->prop_module = (PussCPropModule*)((char*)(ud + 1) + values_sz);
		assert( ud->prop_module == (PussCPropModule*)(((char*)ud) + sizeof(PussCObject) + values_sz) );
		prop_dirtys_init(ud);
	}
	if( sync_module_sz ) {
		ud->sync_module = (PussCSyncModule*)((char*)(ud + 1) + values_sz + prop_module_sz);
		assert( ud->sync_module == (PussCSyncModule*)(((char*)ud) + sizeof(PussCObject) + values_sz + prop_module_sz) );
		sync_dirtys_init(ud);
	}
	lua_pushvalue(L, lua_upvalueindex(1));	// MT
	lua_setmetatable(L, -2);
	schema->total_count++;
	return 1;
}

static inline PussCObjRef* cobjref_check(lua_State* L, int arg) {
	PussCObjRef* ref = (PussCObjRef*)lua_touserdata(L, arg);
	if( !ref )
		luaL_argerror(L, arg, "cobject ref check failed!");
	if( lua_getmetatable(L, arg)	// MT
		&& lua_getmetatable(L, -1)	// MT.MT
		&& luaL_getmetatable(L, PUSS_COBJREF_MT)==LUA_TTABLE
		&& (!lua_rawequal(L, -2, -1)) )
	{
		luaL_argerror(L, arg, "cobject ref check failed!");
	}
	lua_pop(L, 3);
	return ref;
}

static int cobjref_unref(lua_State* L) {
	PussCObjRef* ref = cobjref_check(L, 1);
	int not_cache = lua_toboolean(L, 2);
	if( ref->obj ) {
		if( not_cache ) {
			cobj_do_clear(L, ref->obj->schema, ref->obj);
		} else {
			// cache_table: mt.__schema.uv 
			PussCSchema* schema = (PussCSchema*)(ref->obj->schema);
			lua_Integer num = schema->cache_count + 1;
			assert( num > 0 );
			if( lua_getuservalue(L, lua_upvalueindex(2))!=LUA_TTABLE ) {	// caches: schema.uv
				lua_pop(L, 1);
				lua_createtable(L, 1, 0);
				lua_pushvalue(L, -1);
				lua_setuservalue(L, lua_upvalueindex(2));
			}
			lua_getuservalue(L, 1);		// raw obj
			lua_rawseti(L, -2, num);
			schema->cache_count = num;
		}
		ref->obj = NULL;
		lua_pushnil(L);
		lua_setuservalue(L, 1);
	}
	return 0;
}

static int cobjref_clear(lua_State* L) {
	PussCObjRef* ref = cobjref_check(L, 1);
	if( ref->obj ) {
		cobj_do_clear(L, ref->obj->schema, ref->obj);
	}
	lua_pushvalue(L, 1);
	return 1;
}

static int cobjref_get(lua_State* L) {
	PussCObjRef* ref = cobjref_check(L, 1);
	if( !ref->obj )
		luaL_argerror(L, 1, "already freed");

	if( lua_type(L, 2)==LUA_TSTRING ) {
		lua_getmetatable(L, 1);
		lua_pushvalue(L, 2);
		lua_gettable(L, -2);
	} else {
		lua_Integer field = luaL_checkinteger(L, 2);
		luaL_argcheck(L, (field>0 && field<=ref->obj->schema->field_count), 2, "out of range");
		cobj_geti(L, ref->obj, field);
	}
	return 1;
}

static int cobjref_set(lua_State* L) {
	PussCObjRef* ref = cobjref_check(L, 1);
	PussCStackObject stobj = { ref->obj, L, 1 };
	lua_Integer field = luaL_checkinteger(L, 2);
	luaL_checkany(L, 3);
	int st = stobj.obj ? cobj_seti(&stobj, field) : -1;
	if( st > 0 ) {
		props_mark_dirty_and_notify(&stobj, field);
		lua_pushboolean(L, TRUE);
	} else {
		lua_pushboolean(L, st==0 );
	}
	return 1;
}

static int cobjref_call(lua_State* L) {
	PussCObjRef* ref = cobjref_check(L, 1);
	PussCStackObject stobj = { ref->obj, L, 1 };
	PussCPropModule* m;
	if( !stobj.obj )
		luaL_argerror(L, 1, "already freed");
	luaL_checktype(L, 2, LUA_TFUNCTION);
	lua_pushvalue(L, 1);
	if( ((m = stobj.obj->prop_module)==NULL) || m->lock ) {
		lua_pushvalue(L, 2);
		lua_replace(L, 1);
		lua_replace(L, 2);
		lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);
		return lua_gettop(L);
	}
	lua_insert(L, 3);
	m->lock = TRUE;
	if( lua_pcall(L, lua_gettop(L) - 2, LUA_MULTRET, 0) )
		lua_pop(L, 1);
	props_changes_notify(&stobj);
	m->lock = FALSE;
	return lua_gettop(L) - 1;
}

static int cobjref_sync(lua_State* L) {
	PussCObjRef* ref = cobjref_check(L, 1);
	PussCSyncModule* m = ref->obj->sync_module;
	lua_Unsigned filter_mask;
	luaL_checktype(L, 2, LUA_TTABLE);
	filter_mask = (lua_Unsigned)luaL_optinteger(L, 3, 0x0FF);
	luaL_argcheck(L, (filter_mask & 0x0FF)==filter_mask, 3, "bad mask!");
	lua_pushinteger(L, m ? sync_fetch_and_reset(L, ref->obj, m, 2, (uint8_t)filter_mask) : 0);
	return 1;
}

static int cobjref_stat(lua_State* L) {
	PussCObjRef* ref = cobjref_check(L, 1);
	lua_pushboolean(L, ref->obj ? 1 : 0);
	return 1;
}

static int cobjref_create(lua_State* L) {
	int custom_init = lua_isfunction(L, 1);
	PussCSchema* schema = (PussCSchema*)lua_touserdata(L, lua_upvalueindex(2));
	PussCObjRef* ud = (PussCObjRef*)lua_newuserdata(L, sizeof(PussCObjRef));
	PussCObject* obj = NULL;
	if( schema->cache_count > 0 ) {
		if( lua_getuservalue(L, lua_upvalueindex(2))!=LUA_TTABLE ) {	// cache_table
			schema->cache_count = 0;
		} else if( lua_rawgeti(L, -1, schema->cache_count--)!=LUA_TUSERDATA ) {
			lua_pop(L, 2);
		} else {
			lua_remove(L, -2);
			obj = (PussCObject*)lua_touserdata(L, -1);
			if( custom_init ) {
				custom_init = 0;	// reset not call init
			} else {
				cobj_do_clear(L, schema, obj);	// values
			}
		}
	}
	if( !obj ) {
		lua_pushvalue(L, lua_upvalueindex(6));	// cobject_create()
		lua_call(L, 0, 1);
	}
	ud->obj = lua_touserdata(L, -1);
	lua_setuservalue(L, -2);
	lua_pushvalue(L, lua_upvalueindex(1));	// PussCObjRef MT
	lua_setmetatable(L, -2);
	if( custom_init ) {
		lua_pushvalue(L, -1);
		lua_insert(L, 1);
		lua_call(L, lua_gettop(L)-2, 0);
	}
	return 1;
}

const PussCObject* puss_cobject_checkudata(lua_State* L, int arg, lua_Unsigned id_mask) {
	PussCObject* obj;
	if( (id_mask & PUSS_COBJECT_IDMASK_SUPPORT_REF)==0 ) {
		obj = cobject_check(L, arg);
	} else if( !(obj = cobjref_check(L, arg)->obj) ) {
		luaL_argerror(L, arg, "already freed");
	}
	if( (obj->schema->id_mask & id_mask) != id_mask )
		luaL_argerror(L, arg, "struct check id mask failed!");
	return obj;
}

const PussCObject* puss_cobject_testudata(lua_State* L, int arg, lua_Unsigned id_mask) {
	PussCObject* obj;
	if( lua_isnoneornil(L, arg) )
		return NULL;
	if( id_mask & PUSS_COBJECT_IDMASK_SUPPORT_REF ) {
		if( !(obj = cobjref_check(L, arg)->obj) )
			return NULL;
	} else {
		obj = cobject_check(L, arg);
	}
	return (obj->schema->id_mask & id_mask)==id_mask ? obj : NULL;
}

const PussCObject* puss_cobject_check(const PussCStackObject* stobj, lua_Unsigned id_mask) {
	const PussCObject* obj = stobj->obj;
	assert( obj );
	if( (obj->schema->id_mask & id_mask) != id_mask )
		luaL_error(stobj->L, "cobject match id mask failed!");
	return obj;
}

const PussCObject* puss_cobject_test(const PussCStackObject* stobj, lua_Unsigned id_mask) {
	assert( stobj->obj );
	return (stobj->obj->schema->id_mask & id_mask)==id_mask ? stobj->obj : NULL;
}

int puss_cobject_stack_get(const PussCStackObject* stobj, lua_Integer field) {
	const PussCObject* obj = stobj->obj;
	if( !(field > 0) && (field <= obj->schema->field_count) ) {
		lua_pushnil(stobj->L);
		return LUA_TNIL;
	}
	return cobj_geti(stobj->L, obj, field);
}

int puss_cobject_stack_set(const PussCStackObject* stobj, lua_Integer field) {
	const PussCObject* obj = stobj->obj;
	lua_State* L = stobj->L;
	int top = lua_gettop(L);
	int st = -1;
	assert( stobj->arg > 0 );
	if( top > stobj->arg ) {
		if( obj && ((st = cobj_seti(stobj, field)) > 0) )
			props_mark_dirty_and_notify(stobj, field);
		lua_settop(L, top-1);
	}
	return (st >= 0);
}

int puss_cobject_set_bool(const PussCStackObject* stobj, lua_Integer field, PussCBool nv) {
	const PussCObject* obj = stobj->obj;
	int ret = FALSE;
	PussCValue* v;
	PussCObjectFormula formular;
	assert( stobj->arg > 0 );
	assert( lua_gettop(stobj->L) >= stobj->arg );
	if( (field > 0) && (field <= obj->schema->field_count) && obj->schema->types[field]==PUSS_CVTYPE_BOOL ) {
		if( (formular = obj->schema->properties[field].formular)==NULL ) {
			nv = (nv != 0);
			ret = TRUE;
		} else {
			assert( sizeof(PussCValue)==sizeof(nv) );
			ret = (*formular)(stobj, field, (PussCValue*)(&nv));
		}
		if( ret && ((v=(PussCValue*)&(obj->values[field]))->b != nv) ) {
			v->b = nv;
			props_mark_dirty_and_notify(stobj, field);
		}
	}
	return ret;
}

int puss_cobject_set_int(const PussCStackObject* stobj, lua_Integer field, PussCInt nv) {
	const PussCObject* obj = stobj->obj;
	int ret = FALSE;
	PussCValue* v;
	PussCObjectFormula formular;
	assert( stobj->arg > 0 );
	assert( lua_gettop(stobj->L) >= stobj->arg );
	if( (field > 0) && (field <= obj->schema->field_count) && obj->schema->types[field]==PUSS_CVTYPE_INT ) {
		if( (formular = obj->schema->properties[field].formular)==NULL ) {
			ret = TRUE;
		} else {
			assert( sizeof(PussCValue)==sizeof(nv) );
			ret = (*formular)(stobj, field, (PussCValue*)(&nv));
		}
		if( ret && ((v=(PussCValue*)&(obj->values[field]))->i != nv) ) {
			v->i = nv;
			props_mark_dirty_and_notify(stobj, field);
		}
	}
	return ret;
}

int puss_cobject_set_num(const PussCStackObject* stobj, lua_Integer field, PussCNum nv) {
	const PussCObject* obj = stobj->obj;
	int ret = FALSE;
	PussCValue* v;
	PussCObjectFormula formular;
	assert( stobj->arg > 0 );
	assert( lua_gettop(stobj->L) >= stobj->arg );
	if( (field > 0) && (field <= obj->schema->field_count) && obj->schema->types[field]==PUSS_CVTYPE_NUM ) {
		if( (formular = obj->schema->properties[field].formular)==NULL ) {
			ret = TRUE;
		} else {
			assert( sizeof(PussCValue)==sizeof(nv) );
			ret = (*formular)(stobj, field, (PussCValue*)(&nv));
		}
		if( ret && ((v=(PussCValue*)&(obj->values[field]))->n != nv) ) {
			v->n = nv;
			props_mark_dirty_and_notify(stobj, field);
		}
	}
	return ret;
}

void puss_cobject_batch_call(lua_State* L, int obj, int nargs, int nresults) {
	int fidx;
	if( lua_gettop(L) < (1+nargs) )
		luaL_error(L, "bad logic, no matched args!");
	fidx = lua_absindex(L, -(1+nargs));
	if( !lua_isfunction(L, fidx ) )
		luaL_error(L, "bad logic, need function!");
	lua_pushvalue(L, obj);
	if( luaL_getmetafield((L), -1, "__call")!=LUA_TFUNCTION )
		luaL_error(L, "puss_cobject_batch_modify MT.__call not function");
	lua_insert(L, -2);
	lua_rotate(L, fidx, 2);
	lua_call(L, 1+1+nargs, nresults);
}

static const uint16_t _empty_active_arr[1] = { 0 };

static int cschema_gc(lua_State* L) {
	PussCSchema* schema = (PussCSchema*)lua_touserdata(L, 1);
	int values_ref = schema->values_ref;
	int formulars_ref = schema->formulars_ref;
	int change_handle_ref = schema->change_handle_ref;
	lua_Integer n = schema->field_count;
	lua_Integer i;
	for( i=0; i<=n; ++i ) {
		PussCProperty* prop = (PussCProperty*)(&schema->properties[i]);
		if( prop->actives[0] ) {
			uint16_t* r = (uint16_t*)(prop->actives);
			prop->actives = _empty_active_arr;
			assert( r != _empty_active_arr );
			free(r);
		}
	}
	schema->values_ref = LUA_NOREF;
	schema->formulars_ref = LUA_NOREF;
	schema->change_handle_ref = LUA_NOREF;
	luaL_unref(L, LUA_REGISTRYINDEX, values_ref);
	luaL_unref(L, LUA_REGISTRYINDEX, formulars_ref);
	luaL_unref(L, LUA_REGISTRYINDEX, change_handle_ref);
	return 0;
}

static char vtype_cast(const char* vt) {
	if( strcmp(vt, "bool")==0 ) {
		return PUSS_CVTYPE_BOOL;
	} else if( strcmp(vt, "int")==0 ) {
		return PUSS_CVTYPE_INT;
	} else if( strcmp(vt, "num")==0 ) {
		return PUSS_CVTYPE_NUM;
	} else if( strcmp(vt, "lua")==0 ) {
		return PUSS_CVTYPE_LUA;
	}
	return PUSS_CVTYPE_NULL;
}

static const char* CSCHEMA_PROPS_BUILD_ACTIVES_SCRIPT =
	"local props = ...\n"
	"local loop = false\n"
	"local function deps_add(pos, dep)\n"
	"	if pos==dep then loop, props[pos]._dep_loop=true, true; return true; end\n"
	"	props[pos]._depends[dep], props[dep]._actives[pos] = true, true\n"
	"end\n"
	"local function deps_insert(pos, dep)\n"
	"	if deps_add(pos, dep) then return end\n"
	"	if dep < pos then\n"
	"		for src in pairs(props[dep]._depends) do deps_add(pos, src) end\n"
	"	end\n"
	"	for dst in pairs(props[pos]._actives) do\n"
	"		for src in pairs(props[pos]._depends) do deps_add(dst, src) end\n"
	"	end\n"
	"end\n"
	"local empty = {}\n"
	"local name_index = {}\n"
	"for i,prop in ipairs(props) do\n"
	"	name_index[prop.name], prop.dep_level, prop.dep_actives = i, 0, {}\n"
	"	prop._depends, prop._actives = {}, {}\n"
	"end\n"
	"for i,prop in ipairs(props) do\n"
	"	for _,v in ipairs(prop.deps or empty) do\n"
	"		if deps_insert(i, name_index[v]) then break end\n"
	"	end\n"
	"end\n"
	"if loop then error('properties loop!') end\n"
	"for i,prop in ipairs(props) do\n"
	"	for v in pairs(prop._actives) do\n"
	"		props[v].dep_level = math.max(props[v].dep_level, prop.dep_level + 1)\n"
	"		table.insert(prop.dep_actives, v)\n"
	"	end\n"
	"end\n"
	;
static int cschema_prop_deps_parse(lua_State* L) {
	if( luaL_loadstring(L, CSCHEMA_PROPS_BUILD_ACTIVES_SCRIPT) )
		luaL_error(L, "build actives script error: %s", lua_tostring(L, -1));
	lua_pushvalue(L, 1);
	lua_call(L, 1, 0);
	return 0;
}

static void cschema_prop_reset_props(lua_State* L, PussCSchema* raw, int descs) {
	const char* types = raw->types;
	PussCProperty* raw_props = (PussCProperty*)(raw->properties);
	PussCProperty* prop;
	lua_Integer i, j, m, t;
	lua_Integer n = raw->field_count;
	uint16_t* actives;
	size_t props_sz = sizeof(PussCProperty) * (1 + n);
	size_t defs_sz = sizeof(PussCValue) * (1 + n);
	size_t struct_sz = sizeof(PussCSchema) + defs_sz + props_sz;
	PussCSchema* schema = (PussCSchema*)lua_newuserdata(L, struct_sz);	// top
	PussCValue* defs = (PussCValue*)(schema + 1);
	PussCProperty* props = (PussCProperty*)(defs + 1 + n);
	int enable_change_notify = 0;
	int top = lua_gettop(L);
	memset(schema, 0, struct_sz);
	assert( (char*)props == ((char*)(schema + 1) + defs_sz) );
	schema->id_mask = raw->id_mask;
	schema->field_count = n;
	schema->prop_dirty_notify_max_loop_times = raw->prop_dirty_notify_max_loop_times;
	schema->properties = props;
	schema->types = types;
	schema->defs = defs;
	for( i=0; i<=n; ++i ) {
		prop = &props[i];
		*prop = raw_props[i];
		prop->actives = _empty_active_arr;
	}
	if( luaL_newmetatable(L, PUSS_CSCHEMA_MT) ) {
		lua_pushcfunction(L, cschema_gc);
		lua_setfield(L, -2, "__gc");
	}
	lua_setmetatable(L, -2);

	for( i=1; i<=n; ++i ) {
		const int desc = top + 1;
		prop = &(props[i]);
		lua_settop(L, top);
		lua_geti(L, descs, i);	// property desc

		lua_getfield(L, desc, "def");	// desc.def
		switch( schema->types[i] ) {
		case PUSS_CVTYPE_BOOL:	defs[i].b = lua_toboolean(L, -1);	break;
		case PUSS_CVTYPE_INT:	defs[i].i = lua_tointeger(L, -1);	break;
		case PUSS_CVTYPE_NUM:	defs[i].n = lua_tonumber(L, -1);	break;
		case PUSS_CVTYPE_LUA:	defs[i].o = -i;						break;
		}
		lua_settop(L, desc);

		lua_getfield(L, desc, "dep_level");	// desc.dep_level
		prop->level = (uint16_t)lua_tointeger(L, -1);
		lua_settop(L, desc);

		if( (lua_getfield(L, desc, "dep_actives")==LUA_TTABLE) && ((m = luaL_len(L, -1)) > 0) ) {	// desc.dep_actives
			if( m > n )
				luaL_error(L, "bad actives list!");
			if( !(actives = (uint16_t*)malloc(sizeof(uint16_t) * (1 + m))) )
				luaL_error(L, "prop actives memory error!");
			prop->actives = actives;
			actives[0] = (uint16_t)m;
			for( j=1; j<=m; ++j ) {
				if( lua_geti(L, -1, j)!=LUA_TNUMBER )
					luaL_error(L, "prop(%s) actives[%d] not number!", prop->name, j);
				t = lua_tointeger(L, -1);
				lua_pop(L, 1);
				if( t<=0 || t>n )
					luaL_error(L, "prop(%s) actives[%d] bad range!", prop->name, j);
				actives[j] = (uint16_t)t;
			}
		}
		lua_settop(L, desc);

		if( lua_getfield(L, desc, "change_notify")!=LUA_TNIL ) {	// desc.change_notify
			if( lua_toboolean(L, -1) ) {
				prop->notify = 1;
				enable_change_notify = 1;
			}
		}
		lua_settop(L, desc);
	}

	memcpy((void*)(raw->defs), defs, defs_sz);
	for( i=1; i<=n; ++i ) {
		PussCProperty tmp = raw_props[i];
		raw_props[i] = props[i];
		props[i] = tmp;
	}

	raw->change_notify_script = enable_change_notify && (schema->change_handle_ref != LUA_NOREF);
}

static int cschema_create(lua_State* L) {
	lua_Unsigned id_mask = (lua_Unsigned)luaL_checkinteger(L, 1);
	lua_Integer i, n;
	size_t types_sz, defs_sz, props_sz, sync_masks_sz, sync_idx_sz, struct_sz;
	PussCSchema* schema = NULL;
	char* types = NULL;
	PussCValue* defs = NULL;
	PussCProperty* props = NULL;
	uint8_t* sync_field_mask = NULL;
	uint16_t* sync_field_idx = NULL;
	PussCProperty* prop;
	const char* vt;
	lua_Unsigned sync_mask;
	uint16_t sync_num = 0;

	luaL_checkstack(L, 32, NULL);
	luaL_checktype(L, 2, LUA_TTABLE);
	lua_settop(L, 2);

	lua_pushcfunction(L, cschema_prop_deps_parse);
	lua_pushvalue(L, 2);
	lua_call(L, 1, 0);

	lua_newtable(L);	// RefMT : 3
	lua_newtable(L);	// ObjMT : 4

	if( (n = luaL_len(L, 2)) > 0x7FFE )
		luaL_error(L, "too many fields(%d)!", n);

	types_sz = PUSS_COBJECT_MEMALIGN_SIZE( sizeof(char) * (1 + n) );
	defs_sz = PUSS_COBJECT_MEMALIGN_SIZE( sizeof(PussCValue) * (1 + n) );
	props_sz = PUSS_COBJECT_MEMALIGN_SIZE( sizeof(PussCProperty) * (1 + n) );
	sync_masks_sz = PUSS_COBJECT_MEMALIGN_SIZE( sizeof(uint8_t) * (1 + n) );
	sync_idx_sz = PUSS_COBJECT_MEMALIGN_SIZE( sizeof(uint16_t) * (1 + n) );
	struct_sz = sizeof(PussCSchema) + types_sz + defs_sz + props_sz + sync_masks_sz + sync_idx_sz;
	schema = (PussCSchema*)lua_newuserdata(L, struct_sz);	// schema : 5
	memset(schema, 0, struct_sz);
	types = (char*)(schema + 1);
	defs = (PussCValue*)(((char*)types) + types_sz);
	props =  (PussCProperty*)(((char*)defs) + defs_sz);
	sync_field_mask = (uint8_t*)(((char*)props) + props_sz);
	sync_field_idx = (uint16_t*)(((char*)sync_field_mask) + sync_masks_sz);
	assert( (((char*)sync_field_idx) + sync_idx_sz) ==  ((char*)(schema) + struct_sz) );
	schema->id_mask = id_mask;
	schema->field_count = n;
	schema->types = types;
	schema->defs = defs;
	schema->values_ref = LUA_NOREF;
	schema->formulars_ref = LUA_NOREF;
	schema->change_handle_ref = LUA_NOREF;
	schema->prop_dirty_notify_max_loop_times = DEFAULT_DIRTY_NOTIFY_MAX_LOOP_TIMES;
	schema->properties = props;
	schema->sync_field_mask = sync_field_mask;
	schema->sync_field_idx = sync_field_idx;
	for( i=0; i<=n; ++i ) {
		prop = &props[i];
		prop->actives = _empty_active_arr;
	}
	if( luaL_newmetatable(L, PUSS_CSCHEMA_MT) ) {
		lua_pushcfunction(L, cschema_gc);
		lua_setfield(L, -2, "__gc");
	}
	lua_setmetatable(L, -2);

	lua_createtable(L, (int)n, 0);	// values table: 6
	lua_createtable(L, (int)n, 0);	// formulars table: 7
	lua_createtable(L, 0, 0);		// changes table: 8
	lua_createtable(L, (int)n, 0);	// names table: 9

#define idxof_descs		2
#define idxof_ref_mt	3
#define idxof_obj_mt	4
#define idxof_schema	5
#define idxof_values	6
#define idxof_formulars	7
#define idxof_changes	8
#define idxof_names		9
	lua_pushvalue(L, idxof_values);		schema->values_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	lua_pushvalue(L, idxof_formulars);	schema->formulars_ref = luaL_ref(L, LUA_REGISTRYINDEX);

#define idxof_elem		10
	{
		// init prop[0]
		props[0].name = lua_pushstring(L, "");
		lua_rawseti(L, idxof_names, i);
	}
	for( i=1; i<=n; ++i ) {
		prop = &(schema->properties[i]);
		lua_settop(L, idxof_names);
		if( lua_geti(L, idxof_descs, i) != LUA_TTABLE )	// property desc element: 10
			luaL_error(L, "arg[2] index:%d not desc table!", i);

		// desc.name
		if( lua_getfield(L, idxof_elem, "name")!=LUA_TSTRING)
			luaL_error(L, "arg[2] index:%d name not string!", i);
		prop->name = lua_tostring(L, -1);
		lua_rawseti(L, idxof_names, i);
		if( lua_getfield(L, idxof_names, prop->name)!=LUA_TNIL )
			luaL_error(L, "arg[2] index:%d name(%s) dup!", i, prop->name);
		lua_pushinteger(L, i);
		lua_setfield(L, idxof_names, prop->name);
		lua_settop(L, idxof_elem);

		// desc.type
		if( lua_getfield(L, idxof_elem, "type")!=LUA_TSTRING )
			luaL_error(L, "arg[2] index:%d type not string!", i);
		vt = lua_tostring(L, -1);
		types[i] = vtype_cast(vt);
		if( types[i]==PUSS_CVTYPE_NULL )
			luaL_error(L, "arg[2] index:%d type(%s) not support!", i, vt);
		if( types[i]==PUSS_CVTYPE_LUA )
			lua_newtable(L);
		else
			lua_pushinteger(L, i);
		lua_rawseti(L, idxof_values, i);
		lua_settop(L, idxof_elem);

		// init formulars table
		lua_pushboolean(L, 0);
		lua_rawseti(L, idxof_formulars, i);

		if( id_mask & PUSS_COBJECT_IDMASK_SUPPORT_SYNC ) {
			// desc.sync
			if( lua_getfield(L, idxof_elem, "sync")!=LUA_TNIL ) {
				if( !lua_isinteger(L, -1) )
					luaL_error(L, "arg[2] index:%d sync not integer!", i);
				sync_mask = (lua_Unsigned)lua_tointeger(L, -1);
				if( (sync_mask & 0x00FF) != sync_mask )
					luaL_error(L, "arg[2] index:%d sync mask MUST less then 0xFF!", i);
				if( sync_mask ) {
					sync_field_mask[i] = (uint8_t)sync_mask;
					sync_field_idx[i] = ++sync_num;
				}
			}
			lua_settop(L, idxof_elem);
		}
	}
#undef idxof_elem

	cschema_prop_reset_props(L, schema, idxof_descs);
	lua_settop(L, idxof_names);

	if( sync_num > 0 ) {
		uint16_t* map = (uint16_t*)lua_newuserdata(L, sizeof(uint16_t) * (1 + sync_num));
		lua_setfield(L, idxof_formulars, "@mem_of_sync_file_idx_map");
		schema->sync_field_count = sync_num;
		map[0] = 0;
		for( i=1; i<=n; ++i ) {
			if( sync_field_idx[i] ) {
				map[sync_field_idx[i]] = (uint16_t)i;
				--sync_num;
			}
		}
		assert( sync_num==0 );
		schema->sync_idx_to_field = map;
	}

	luaL_newmetatable(L, PUSS_COBJECT_MT);	lua_setmetatable(L, idxof_obj_mt);
	lua_pushfstring(L, "PussC_%I", (lua_Integer)id_mask);	lua_setfield(L, idxof_obj_mt, "__name");
	lua_pushvalue(L, idxof_names); lua_pushcclosure(L, cobject_gc, 1); lua_setfield(L, idxof_obj_mt, "__gc");
	lua_pushcfunction(L, cobject_get); lua_setfield(L, idxof_obj_mt, "__index");
	lua_pushcfunction(L, cobject_set); lua_setfield(L, idxof_obj_mt, "__newindex");
	lua_pushcfunction(L, cobject_call); lua_setfield(L, idxof_obj_mt, "__call");
	lua_pushcfunction(L, cobject_sync); lua_setfield(L, idxof_obj_mt, "__sync");
	lua_pushcfunction(L, cobject_clear); lua_setfield(L, idxof_obj_mt, "__clear");

	luaL_newmetatable(L, PUSS_COBJREF_MT);	lua_setmetatable(L, idxof_ref_mt);
	lua_pushfstring(L, "PussCObjRef_%I", (lua_Integer)id_mask); lua_setfield(L, idxof_ref_mt, "__name");
	lua_pushcfunction(L, cobjref_get); lua_setfield(L, idxof_ref_mt, "__index");
	lua_pushcfunction(L, cobjref_set); lua_setfield(L, idxof_ref_mt, "__newindex");
	lua_pushcfunction(L, cobjref_call); lua_setfield(L, idxof_ref_mt, "__call");
	lua_pushcfunction(L, cobjref_sync); lua_setfield(L, idxof_ref_mt, "__sync");
	lua_pushcfunction(L, cobjref_clear); lua_setfield(L, idxof_ref_mt, "__clear");
	lua_pushcfunction(L, cobjref_unref); lua_setfield(L, idxof_ref_mt, "__unref");
	lua_pushcfunction(L, cobjref_stat);	lua_setfield(L, idxof_ref_mt, "__stat");

	lua_settop(L, idxof_changes);
	lua_pushcclosure(L, cobject_create, 5);	// 4(ObjMT) 5(Schema) 6(Values) 7(Formulars) 8(Changes)
	if( id_mask & PUSS_COBJECT_IDMASK_SUPPORT_REF ) {
		lua_getupvalue(L, -1, 2);	// Schema
		lua_getupvalue(L, -2, 3);	// Values
		lua_getupvalue(L, -3, 4);	// Formulars
		lua_getupvalue(L, -4, 5);	// Changes
		lua_rotate(L, 4, 4);		// insert after RefMT
		lua_pushcclosure(L, cobjref_create, 6);	// 3(RefMT) (Schema) (Values) (Formulars) (Changes) (cobject_create())
	}

#undef idxof_names
#undef idxof_changes
#undef idxof_formulars
#undef idxof_values
#undef idxof_schema
#undef idxof_obj_mt
#undef idxof_ref_mt
#undef idxof_descs

	if( lua_geti(L, lua_upvalueindex(1), id_mask) != LUA_TNIL )
		luaL_error(L, "already exist!");
	lua_pop(L, 1);

	if( schema->sync_field_count ) {
		int top = lua_gettop(L);
		lua_pushboolean(L, 1);
		puss_cschema_changed_reset(L, -2, "@builtin_sync_module", sync_on_changed);
		lua_settop(L, top);
		if( !lua_isfunction(L, -1) )
			luaL_error(L, "sync module reg bad logic!");
	}
	lua_pushvalue(L, -1);
	lua_rawseti(L, lua_upvalueindex(1), id_mask);
	return 1;
}

static PussCSchema* cschema_check_fetch(lua_State* L, int creator_idx) {
	// upvalues: 1:MT 2:Schema 3:Values 4:Formulars 5:Changes
	lua_CFunction creator = lua_tocfunction(L, creator_idx);
	PussCSchema* schema;
	if( creator!=cobject_create && creator!=cobjref_create )
		luaL_error(L, "cobject creator need!");
	if( !(lua_getupvalue(L, creator_idx, 2) && lua_type(L, -1)==LUA_TUSERDATA) )
		luaL_error(L, "cschema fetch failed!");
	if( lua_getmetatable(L, -1)==0 || luaL_getmetatable(L, PUSS_CSCHEMA_MT)==0 || (!lua_rawequal(L, -1, -2)) )
		luaL_error(L, "cschema match failed!");
	if( !(schema = (PussCSchema*)lua_touserdata(L, -3)) )
		luaL_error(L, "cschema fetch failed!");
	lua_pop(L, 2);
	return schema;
}

static int cschema_refresh(lua_State* L) {
	PussCSchema* schema;
	lua_Integer i, n;
	luaL_checktype(L, 2, LUA_TTABLE);
	schema = cschema_check_fetch(L, 1);
	if( (n = schema->field_count) != luaL_len(L, 2) )
		luaL_error(L, "cschema property num not matched!");
	lua_settop(L, 2);

	lua_pushcfunction(L, cschema_prop_deps_parse);
	lua_pushvalue(L, 2);
	lua_call(L, 1, 0);

	// now stack 1:creator 2:descs
	for( i=1; i<=n; ++i ) {
		PussCProperty* prop = &(schema->properties[i]);
		const int desc = 3;
		const char* name;
		lua_settop(L, 3);
		if( lua_geti(L, 2, i) != LUA_TTABLE )	// property desc
			luaL_error(L, "arg[2] index:%d not desc table!", i);

		if( lua_getfield(L, desc, "name")!=LUA_TSTRING )
			luaL_error(L, "arg[2] index:%d name not found!", i);
		if( (name=lua_tostring(L, -1))!=prop->name && strcmp(name, prop->name)!=0 )
			luaL_error(L, "arg[2] index:%d name(%s) not match(%s)!", name, prop->name);
		lua_settop(L, desc);

		if( lua_getfield(L, desc, "type")!=LUA_TSTRING )	// desc.type
			luaL_error(L, "arg[2] index:%d type not string!", i);
		if( vtype_cast(lua_tostring(L, -1)) != schema->types[i] )
			luaL_error(L, "arg[2] index:%d type not match!", i);
		lua_settop(L, desc);
	}

	cschema_prop_reset_props(L, schema, 2);
	return 0;
}

static int cschema_dirty_loop_reset(lua_State* L) {
	PussCSchema* schema;
	lua_Integer times = luaL_optinteger(L, 2, DEFAULT_DIRTY_NOTIFY_MAX_LOOP_TIMES);
	luaL_argcheck(L, times > 0, 2, "dirty notify max loop times MUST > 0");
	schema = cschema_check_fetch(L, 1);
	schema->prop_dirty_notify_max_loop_times = times;
	lua_pushinteger(L, times);
	return 1;
}

static int cobject_metatable(lua_State* L) {
	lua_CFunction creator = lua_tocfunction(L, 1);
	if( creator==cobject_create || creator==cobjref_create ) {
		if( lua_getupvalue(L, 1, 1) )
			return 1;
	}
	lua_pushnil(L);
	return 1;
}

static int cobject_cache(lua_State* L) {
	PussCSchema* schema;
	int clear = lua_toboolean(L, 2);
	schema = cschema_check_fetch(L, 1);
	if( clear && schema->cache_count && lua_getuservalue(L,-1)==LUA_TTABLE ) {
		schema->cache_count = 0;
		lua_pushnil(L);
		lua_setuservalue(L, -2);
	}
	lua_pushinteger(L, schema->total_count);
	lua_pushinteger(L, schema->cache_count);
	return 2;
}

static int cschema_formular_reset(lua_State* L) {
	PussCSchema* schema;
	lua_Integer field = luaL_checkinteger(L, 2);
	int clear = lua_isnoneornil(L, 3);
	if( !clear )
		luaL_checktype(L, 3, LUA_TFUNCTION);
	schema = cschema_check_fetch(L, 1);
	luaL_argcheck(L, (field>0 && field<=schema->field_count), 2, "out of range");
	if( !(lua_getupvalue(L, 1, 4) && lua_type(L, -1)==LUA_TTABLE) )
		luaL_error(L, "fetch formular table failed!");
	if( clear ) {
		lua_pushboolean(L, 0);
		lua_rawseti(L, -2, field);
		schema->properties[field].formular = NULL;
	} else {
		lua_pushvalue(L, 3);
		lua_rawseti(L, -2, field);
		schema->properties[field].formular = lua_formular_wrap;
	}
	return 0;
}

static int cschema_changed_notify_handle_reset(lua_State* L) {
	PussCSchema* schema;
	int clear = lua_isnoneornil(L, 2);
	int change_notify = 0;
	lua_Integer i;
	if( !clear )
		luaL_checktype(L, 2, LUA_TFUNCTION);
	schema = cschema_check_fetch(L, 1);
	if( clear ) {
		luaL_unref(L, LUA_REGISTRYINDEX, schema->change_handle_ref);
		schema->change_handle_ref = LUA_NOREF;
	} else {
		lua_pushvalue(L, 2);
		if( schema->change_handle_ref!=LUA_NOREF ) {
			lua_rawseti(L, LUA_REGISTRYINDEX, schema->change_handle_ref);
		} else {
			schema->change_handle_ref = luaL_ref(L, LUA_REGISTRYINDEX);
		}
		for( i=1; i<=schema->field_count; ++i ) {
			if( schema->properties[i].notify ) {
				change_notify = 1;
				break;
			}
		}
	}
	schema->change_notify_script = change_notify;
	return 0;
}

static int cschema_changed_notify_mode_reset(lua_State* L) {
	PussCSchema* schema;
	int notify_mode = (int)luaL_optinteger(L, 2, -1);
	schema = cschema_check_fetch(L, 1);
	if( notify_mode >= 0 )
		schema->change_notify_mode = notify_mode;
	lua_pushinteger(L, schema->change_notify_mode);
	return 1;
}

void puss_cschema_formular_reset(lua_State* L, int creator, lua_Integer field, PussCObjectFormula formular) {
	PussCSchema* schema = cschema_check_fetch(L, creator);
	lua_pop(L, 1);
	if( !(field>0 && field<=schema->field_count) )
		luaL_error(L, "field out of range");
	if( !formular )
		luaL_error(L, "cschema formular MUST exist!");
	if( lua_isnoneornil(L, -1) )
		luaL_error(L, "cschema formular ref MUST module ref, can NOT none or nil!");
	if( !(lua_getupvalue(L, creator, 4) && lua_type(L, -1)==LUA_TTABLE) )
		luaL_error(L, "fetch formular table failed!");
	lua_pushvalue(L, -2);
	lua_rawseti(L, -2, field);
	lua_pop(L, 1);
	schema->properties[field].formular = formular;
}

void puss_cobject_sync_fetch_infos(lua_State* L, int creator, PussCSyncInfo* info) {
	PussCSchema* schema = cschema_check_fetch(L, creator);
	lua_pop(L, 1);
	if( info ) {
		info->id_mask = schema->id_mask;
		info->field_count = schema->field_count;
		info->sync_count = schema->sync_field_count;
		info->field_masks = schema->sync_field_mask;
	}
}

size_t puss_cobject_sync_fetch_and_clear(lua_State* L, const PussCObject* obj, uint16_t* res, size_t len) {
	size_t ret = 0;
	if( obj && obj->sync_module && res && (len >= obj->schema->sync_field_count) ) {
		const PussCSchema* schema = obj->schema;
		const uint16_t* map = schema->sync_idx_to_field;
		uint16_t* arr = obj->sync_module->dirty;
		uint16_t i, n;
		PUSS_BITSETLIST_MOVE(res, n, arr);
		assert( n <= len );
		for( i=0; i<n; ++i ) {
			res[i] = map[res[i]];
		}
		ret = n;
	}
	return ret;
}

size_t puss_cobject_sync_fetch_and_reset(lua_State* L, const PussCObject* obj, uint16_t* res, size_t len, uint8_t filter_mask) {
	size_t ret = 0;
	if( obj && obj->sync_module && res && (len >= obj->schema->sync_field_count) ) {
		const PussCSchema* schema = obj->schema;
		const uint8_t* masks = schema->sync_field_mask;
		const uint16_t* map = schema->sync_idx_to_field;
		uint16_t num = schema->sync_field_count;
		uint16_t* arr = obj->sync_module->dirty;
		uint16_t pos, field;
		PUSS_BITSETLIST_ITER(num, arr, pos) {
			if( PUSS_BITSETLIST_TEST(num, arr, pos) ) {
				field = map[pos];
				if( (masks[field] & filter_mask)==filter_mask ) {
					PUSS_BITSETLIST_RESET(num, arr, pos);
					res[ret++] = field;
				}
			}
		}
		assert( ret <= len );
	}
	return ret;
}

static int change_handle_add(lua_State* L) {
	PussCObjectChanged* arr = lua_touserdata(L, 1);
	PussCObjectChanged h = (PussCObjectChanged)lua_touserdata(L, 2);
	while( *arr)
		++arr;
	arr[0] = h;
	arr[1] = NULL;
	return 0;
}

static const char* CSCHEMA_CHANGES_RESET_SCRIPT =
	// "print(...)\n"
	"local tbl, change_handle_add, mem, name, handle, ref = ...\n"
	"local idx = #tbl + 1\n"
	"for i,v in ipairs(tbl) do\n"
	"	if v.name==name then idx=i; break; end\n"
	"end\n"
	"if tbl[idx] then\n"
	"	tbl[idx][2], tbl[idx][3] = handle, ref\n"
	"else\n"
	"	tbl[idx] = {name, handle, ref}\n"
	"end\n"
	"print(idx)\n"
	"for i,v in ipairs(tbl) do\n"
	"	change_handle_add(mem, v[2])\n"
	"end\n"
	"tbl.changed_handles_mem = mem	-- mem use GC\n"
	;

void puss_cschema_changed_reset(lua_State* L, int creator, const char* name, PussCObjectChanged handle) {
	int absidx = lua_absindex(L, creator);
	PussCSchema* schema = cschema_check_fetch(L, absidx);
	PussCObjectChanged* arr = schema->change_handles;
	lua_pop(L, 1);
	if( !(name && *name) )
		luaL_error(L, "event handle name MUST exist & not empty string");
	if( handle && lua_isnoneornil(L, -1) )	// module ref
		luaL_error(L, "cschema changed ref MUST module ref, can NOT none or nil!");

	if( luaL_loadstring(L, CSCHEMA_CHANGES_RESET_SCRIPT) )	// +1
		luaL_error(L, "build changes reset script error: %s", lua_tostring(L, -1));
	if( !(lua_getupvalue(L, absidx, 5) && lua_type(L, -1)==LUA_TTABLE) )	// +2
		luaL_error(L, "cschema fetch changed events hanele table failed!");
	lua_pushcfunction(L, change_handle_add);	// +3
	arr = (PussCObjectChanged*)lua_newuserdata(L, sizeof(PussCObjectChanged) * (luaL_len(L, -2) + 2));	// +4
	arr[0] = NULL;
	lua_pushstring(L, name);	// +5
	lua_pushlightuserdata(L, handle);	// +6
	if( handle ) {
		lua_pushvalue(L, -7);	// +7
	} else {
		lua_pushnil(L);
	}
	lua_call(L, 6, 0);
	schema->change_handles = arr[0] ? arr : NULL;
}

static luaL_Reg cobject_lib_methods[] =
	{ {"cschema_create", NULL}
	, {"cschema_refresh", cschema_refresh}
	, {"cschema_formular_reset", cschema_formular_reset}
	, {"cschema_changed_notify_handle_reset", cschema_changed_notify_handle_reset}
	, {"cschema_changed_notify_mode_reset", cschema_changed_notify_mode_reset}
	, {"cschema_dirty_loop_reset", cschema_dirty_loop_reset}
	, {"cobject_metatable", cobject_metatable}
	, {"cobject_cache", cobject_cache}
	, {NULL, NULL}
	};

int puss_reg_cobject(lua_State* L) {
	luaL_setfuncs(L, cobject_lib_methods, 0);

	lua_newtable(L);
	lua_pushcclosure(L, cschema_create, 1);
	lua_setfield(L, -2, "cschema_create");
	return 0;
}
