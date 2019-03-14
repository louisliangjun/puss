// puss_async_service.c

#include "puss_toolkit.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

#include "rbx_tree.h"

#define TIMEOUT_FREE	0xFFFFFFFFFFFFFFFFull
#define TIMEOUT_WORK	(TIMEOUT_FREE - 1ull)
#define TIMEOUT_MAX		(TIMEOUT_FREE - 2ull)

typedef struct _GroupNode	GroupNode;

struct _GroupNode {
	GroupNode*		prev;
	GroupNode*		next;
};

typedef struct _AsyncTaskName {
	lua_Integer		id;
	lua_State*		co;
} AsyncTaskName;

typedef struct _AsyncTask {
	RBXNode			rbx_node;	// MUST first
	GroupNode		grp_node;
	uint64_t		timeout;
	lua_Integer		id;
	lua_Integer		skey;
	lua_State*		co;
	GroupNode*		gkey;
	// uservalue : co
} AsyncTask;

typedef struct _AsyncTaskService {
	uint64_t		now;				// async task now
	lua_Integer		id_last;
	lua_Integer		skey_last;
	lua_Integer		count;

	RBXTree			timers;
	GroupNode		sleep_group;

	AsyncTask		dummy_work_task;	// for optimization
	AsyncTask		dummy_free_task;	// for optimization
} AsyncTaskService;

#define	grp_cast(grp)	((AsyncTask*)((char*)(grp)-(intptr_t)(&((AsyncTask *)0)->grp_node)))

static inline void grp_list_init(GroupNode* node) {
	node->prev = node;
	node->next = node;
}

static inline void grp_list_reset(AsyncTask* qnode, GroupNode* grp) {
	GroupNode* node = &(qnode->grp_node);
	if( node != node->next ) {
		GroupNode* prev = node->prev;
		GroupNode* next = node->next;
		next->prev = prev;
		prev->next = next;
	}

	qnode->gkey = grp;
	if( grp ) {
		GroupNode* last = grp->prev;
		node->prev = last;
		node->next = grp;
		last->next = node;
		grp->prev = node;
	} else {
		node->prev = node;
		node->next = node;
	}
}

static inline int timer_qnode_is_attached(AsyncTask* qnode) {
	assert( qnode );
	return qnode->rbx_node.color!=RBX_FREE;
}

static inline void timer_queue_insert(AsyncTaskService* svs, AsyncTask* qnode) {
	RBXNode* p = 0;
	uint64_t timeout = qnode->timeout;
	RBXNode** link = &(svs->timers.root);
	assert( !timer_qnode_is_attached(qnode) );

	// find insert pos
	while( *link ) {
		p = *link;
		timeout = ((AsyncTask*)p)->timeout;
		if( qnode->timeout < timeout )
			link = &(p->left);
		else if( qnode->timeout > timeout )
			link = &(p->right);
		else
			break;
	}

	rbx_insert(&(qnode->rbx_node), p, link, &(svs->timers));
}

static inline void timer_queue_remove(AsyncTaskService* svs, AsyncTask* qnode) {
	assert( timer_qnode_is_attached(qnode) );
	rbx_erase((RBXNode*)qnode, &(svs->timers));
	qnode->rbx_node.color = RBX_FREE;
}

#define SERVICE_INDEX		lua_upvalueindex(1)
#define TASK_MAP_INDEX		lua_upvalueindex(2)
#define GROUP_MAP_INDEX		lua_upvalueindex(3)

static inline void async_task_reset_timeout(AsyncTaskService* svs, AsyncTask* task, uint64_t timeout) {
	assert( svs && task );

	if( task->timeout != timeout ) {
		timer_queue_remove(svs, task);
		task->timeout = timeout;
		timer_queue_insert(svs, task);
	}
}

static inline void async_task_append_to(AsyncTaskService* svs, AsyncTask* task, AsyncTask* head) {
	RBXNode* pos = &(head->rbx_node);
	task->timeout = head->timeout;
	rbx_insert(&(task->rbx_node), pos, &pos, &(svs->timers));
	assert( pos==&(head->rbx_node) );
}

static void async_task_delay(AsyncTaskService* svs, AsyncTask* task, uint64_t delay) {
	uint64_t timeout = svs->now + delay;
	if( timeout <= svs->now )
		timeout = svs->now + 1;
	if( (timeout < svs->now) || (timeout > TIMEOUT_MAX) )
		timeout = TIMEOUT_MAX;
	async_task_reset_timeout(svs, task, timeout);
}

static int async_task_continue(lua_State *L, int status, lua_KContext ctx) {
	AsyncTask* task;
	assert( lua_gettop(L) >= 1 );

	// main loop
	if( status==LUA_YIELD && lua_isfunction(L, 2) ) {
		lua_pcallk(L, lua_gettop(L)-2, 0, 1, ctx, async_task_continue);
	}

	lua_settop(L, 1);

	// check exist & set free
	task = lua_touserdata(L, lua_upvalueindex(2));
	if( task && task->co && timer_qnode_is_attached(task) ) {
		AsyncTaskService* svs = lua_touserdata(L, SERVICE_INDEX);
		timer_queue_remove(svs, task);
		async_task_append_to(svs, task, &(svs->dummy_free_task));
		return lua_yieldk(L, LUA_MULTRET, ctx, async_task_continue);
	}

	return 0;
}

static int async_task_main(lua_State* L) {
	assert( lua_gettop(L) == 0 );
	puss_lua_get(L, PUSS_KEY_ERROR_HANDLE);
	assert( lua_isfunction(L, -1) );
	return lua_yieldk(L, LUA_MULTRET, 0, async_task_continue);
}

static AsyncTask* async_service_create_co(lua_State* L, AsyncTaskService* svs) {
	AsyncTask* task = lua_newuserdata(L, sizeof(AsyncTask));
	memset(task, 0, sizeof(AsyncTask));
	grp_list_init(&(task->grp_node));
	task->co = lua_newthread(L);

	lua_pushvalue(L, SERVICE_INDEX);
	lua_pushvalue(L, -3);	// task
	lua_xmove(L, task->co, 2);
	lua_pushcclosure(task->co, async_task_main, 2);

	lua_setuservalue(L, -2);
	lua_rawsetp(L, TASK_MAP_INDEX, task->co);

	task->skey = ++(svs->skey_last);
	async_task_append_to(svs, task, &(svs->dummy_free_task));
	svs->count++;
	lua_resume(task->co, L, 0);
	assert( lua_status(task->co)==LUA_YIELD );
	return task;
}

static void async_service_destroy_co(lua_State* L, AsyncTaskService* svs, AsyncTask* task) {
	if( timer_qnode_is_attached(task) ) {
		timer_queue_remove(svs, task);
	}
	grp_list_reset(task, NULL);

	assert( lua_istable(L, TASK_MAP_INDEX) );
	if( task->co ) {
		lua_pushnil(L);
		lua_rawsetp(L, TASK_MAP_INDEX, task->co);
		task->co = NULL;
		svs->count--;
	}
}

static void async_task_reset_work(AsyncTaskService* svs, AsyncTask* task) {
	assert( svs && task );
	task->skey++;
	grp_list_reset(task, NULL);

	if( task->timeout != TIMEOUT_WORK ) {
		timer_queue_remove(svs, task);
		async_task_append_to(svs, task, &(svs->dummy_work_task));
	}
}

static int async_task_resume(lua_State* L, AsyncTaskService* svs, AsyncTask* task, int narg) {
	async_task_reset_work(svs, task);

	if( lua_resume(task->co, L, narg)==LUA_YIELD ) {
		if( task->timeout==TIMEOUT_WORK ) {
			async_task_delay(svs, task, 1);	// direct yield(), active next update
		}
		return 1;
	}

	async_service_destroy_co(L, svs, task);
	return 0;
}

static int lua_async_service_get_task_count(lua_State* L) {
	AsyncTaskService* svs = lua_touserdata(L, SERVICE_INDEX);
	lua_pushinteger(L, svs->count);
	return 1;
}

static AsyncTask* async_service_fetch_free_task(AsyncTaskService* svs) {
	AsyncTask* task = (AsyncTask*)(svs->dummy_free_task.rbx_node.next);
	return (task && task->timeout==TIMEOUT_FREE) ? task : NULL;
}

static int lua_async_service_collectgarbage(lua_State* L) {
	AsyncTaskService* svs = lua_touserdata(L, SERVICE_INDEX);
	lua_Integer num = 0;
	AsyncTask* task;
	while( (task = async_service_fetch_free_task(svs))!=NULL ) {
		async_service_destroy_co(L, svs, task);
		++num;
	}
	lua_pushinteger(L, num);
	return 1;
}

static int lua_async_service_run(lua_State* L) {
	AsyncTaskService* svs = lua_touserdata(L, SERVICE_INDEX);
	AsyncTask* task = async_service_fetch_free_task(svs);
	int n = lua_gettop(L);
	assert( lua_istable(L, TASK_MAP_INDEX) );
	luaL_checktype(L, 1, LUA_TFUNCTION);
	task = task ? task : async_service_create_co(L, svs);
	assert( task && task->co );
	assert( lua_status(task->co)==LUA_YIELD );
	task->id = ++(svs->id_last);
	luaL_checkstack(task->co, n+2, "too many arguments to run");
	lua_xmove(L, task->co, n);	// move func + args
	async_task_resume(L, svs, task, n-1);
	return 0;
}

static int lua_async_service_kill(lua_State* L) {
	AsyncTaskService* svs = lua_touserdata(L, SERVICE_INDEX);
	size_t n = 0;
	const char* s = luaL_optlstring(L, 1, NULL, &n);
	lua_Integer try_kill_times = luaL_optinteger(L, 2, 0);
	AsyncTask* task;
	AsyncTaskName name;
	assert( lua_istable(L, TASK_MAP_INDEX) );
	if( !s )
		return 0;
	luaL_argcheck(L, n==sizeof(AsyncTaskName), 1, "need task name");
	memcpy(&name, s, n);
	if( lua_rawgetp(L, TASK_MAP_INDEX, name.co)!=LUA_TUSERDATA )
		return 0;
	task = lua_touserdata(L, -1);
	lua_pop(L, 1);
	assert( task->co==name.co );
	if( task->id!=name.id )
		return 0;

	while( (task->timeout != TIMEOUT_FREE) && (try_kill_times > 0) ) {
		--try_kill_times;
		async_task_resume(L, svs, task, 0);
	}

	if( task->timeout == TIMEOUT_FREE )
		return 0;

	async_service_destroy_co(L, svs, task);
	return 0;
}

static int lua_async_service_wakeup(lua_State* L) {
	AsyncTaskService* svs = lua_touserdata(L, SERVICE_INDEX);
	size_t n = 0;
	const char* s = luaL_optlstring(L, 1, NULL, &n);
	lua_Integer delay = luaL_optinteger(L, 2, -1);
	AsyncTask* task;
	AsyncTaskName name;
	assert( lua_istable(L, TASK_MAP_INDEX) );
	if( !s )
		return 0;
	luaL_argcheck(L, n==sizeof(AsyncTaskName), 1, "need task name");
	memcpy(&name, s, n);
	if( lua_rawgetp(L, TASK_MAP_INDEX, name.co)!=LUA_TUSERDATA )
		return 0;
	task = lua_touserdata(L, -1);
	lua_pop(L, 1);
	assert( task->co==name.co );

	if( !(task->id==name.id && task->gkey==&(svs->sleep_group) && lua_status(task->co)==LUA_YIELD) )
        return 0;

    if( delay >= 0 ) {
        async_task_delay(svs, task, (uint64_t)delay);
    } else {
        async_task_resume(L, svs, task, 0);
    }
    return 0;
}

static int lua_async_service_resume(lua_State* L) {
	AsyncTaskService* svs = lua_touserdata(L, SERVICE_INDEX);
	void* h = (void*)luaL_checkinteger(L, 1);
	lua_Integer skey = luaL_checkinteger(L, 2);
	int n = lua_gettop(L) - 2;
	AsyncTask* task = NULL;
	assert( lua_istable(L, TASK_MAP_INDEX) );
	luaL_checkstack(L, n+2, "too many arguments to resume");

	if( lua_rawgetp(L, TASK_MAP_INDEX, h)!=LUA_TUSERDATA ) {
		lua_pushnil(L);	// bad task handle, not found!
		return 1;
	}

	task = lua_touserdata(L, -1);
	lua_pop(L, 1);
	if( !(task && task->skey==skey) ) {
		lua_pushboolean(L, 0);	// bad task handle, skey not matched!
		return 1;
	}
	assert( (void*)(task->co)==h );
	if( lua_status(task->co)!=LUA_YIELD ) {
		lua_pushboolean(L, 0);	// bad co status
		return 1;
	}

	if( n > 0 ) {
		lua_xmove(L, task->co, n);
	}
	lua_pushboolean(L, async_task_resume(L, svs, task, n));
	return 1;
}

static int lua_async_service_update(lua_State* L) {
	AsyncTaskService* svs = lua_touserdata(L, SERVICE_INDEX);
	uint64_t now = svs->now + (uint64_t)luaL_checkinteger(L, 1);
	int step = (int)luaL_optinteger(L, 2, 1);
	AsyncTask* task = NULL;
	RBXNode* list = &(svs->timers.list);
	RBXNode* iter;

	svs->now = now;

	for( ; step > 0; --step ) {
		if( (iter = list->next) == list )
			break;
		task = (AsyncTask*)iter;
		if( task->timeout > now ) {
			task = NULL;
			break;
		}
		async_task_resume(L, svs, task, 0);	// on timer, not need
	}

	lua_pushboolean(L, task!=NULL);	// need more

	task = (AsyncTask*)(list->next);
	if( task && (task->timeout <= TIMEOUT_MAX) ) {
		lua_pushinteger(L, task->timeout - now);	// next interval
	} else {
		lua_pushinteger(L, LUA_MAXINTEGER);
	}
	return 2;
}

static int lua_async_service_group_reg(lua_State* L) {
	GroupNode* grp;
	luaL_checkany(L, 1);
	lua_pushvalue(L, 1);
	if( lua_rawget(L, GROUP_MAP_INDEX)!=LUA_TUSERDATA ) {
		lua_pop(L, 1);
		lua_pushvalue(L, 1);
		grp = (GroupNode*)lua_newuserdata(L, sizeof(GroupNode));
		grp_list_init(grp);
		lua_rawset(L, GROUP_MAP_INDEX);
	}
	return 0;
}

static int lua_async_service_group_unreg(lua_State* L) {
	GroupNode* grp;
	GroupNode* iter;
	AsyncTask* task;
	luaL_checkany(L, 1);
	lua_pushvalue(L, 1);
	if( lua_rawget(L, GROUP_MAP_INDEX)!=LUA_TUSERDATA )
		return 0;
	grp = (GroupNode*)lua_touserdata(L, -1);
	while( (iter = grp->next) != grp ) {
		task = grp_cast(iter);
		assert( task->gkey==grp );
		grp_list_reset(task, NULL);
	}
	lua_pushvalue(L, 1);
	lua_pushnil(L);
	lua_rawset(L, GROUP_MAP_INDEX);
	return 0;
}

static int lua_async_service_group_cancel(lua_State* L) {
	AsyncTaskService* svs = lua_touserdata(L, SERVICE_INDEX);
	GroupNode* grp;
	GroupNode* iter;
	AsyncTask* task;
	luaL_checkany(L, 1);
	lua_pushvalue(L, 1);
	if( lua_rawget(L, GROUP_MAP_INDEX)!=LUA_TUSERDATA )
		return 0;
	grp = (GroupNode*)lua_touserdata(L, -1);
	while( (iter = grp->next) != grp ) {
		task = grp_cast(iter);
		assert( task->gkey==grp );
		grp_list_reset(task, NULL);
		async_task_reset_timeout(svs, task, svs->now);
	}
	return 0;
}

static int lua_async_task_self(lua_State* L) {
	// AsyncTaskService* svs = lua_touserdata(L, SERVICE_INDEX);
	AsyncTask* task;
	AsyncTaskName name;
	assert( lua_istable(L, TASK_MAP_INDEX) );
	task = (lua_rawgetp(L, TASK_MAP_INDEX, L)==LUA_TUSERDATA) ? lua_touserdata(L, -1) : NULL;
	if( !task )
		return luaL_error(L, "MUST in async task thread!");
	name.id = task->id;
	name.co = task->co;
	lua_pushlstring(L, (const char*)&name, sizeof(name));
	return 1;
}

static int lua_async_task_sleep(lua_State* L) {
	AsyncTaskService* svs = lua_touserdata(L, SERVICE_INDEX);
	uint64_t tms = (uint64_t)luaL_checkinteger(L, 1);
	AsyncTask* task;
	assert( lua_istable(L, TASK_MAP_INDEX) );
	task = (lua_rawgetp(L, TASK_MAP_INDEX, L)==LUA_TUSERDATA) ? lua_touserdata(L, -1) : NULL;
	if( !task )
		return luaL_error(L, "MUST in async task thread!");
	async_task_delay(svs, task, tms);
	grp_list_reset(task, &(svs->sleep_group));
	return lua_yield(L, 0);
}

static int lua_async_task_alarm(lua_State* L) {
	AsyncTaskService* svs = lua_touserdata(L, SERVICE_INDEX);
	uint64_t tms = (uint64_t)luaL_optinteger(L, 1, 0);
	GroupNode* grp = NULL;
	AsyncTask* task;
	assert( lua_istable(L, TASK_MAP_INDEX) );
	if( !lua_isnoneornil(L, 2) ) {
		lua_pushvalue(L, 2);
		if( lua_rawget(L, GROUP_MAP_INDEX)==LUA_TUSERDATA ) {
			grp = (GroupNode*)lua_touserdata(L, -1);
		}
		lua_pop(L, 1);
	}
	task = (lua_rawgetp(L, TASK_MAP_INDEX, L)==LUA_TUSERDATA) ? lua_touserdata(L, -1) : NULL;
	if( !task )
		return luaL_error(L, "MUST in async task thread!");
	if( tms ) {
		async_task_delay(svs, task, tms);
		grp_list_reset(task, grp);
		lua_pushinteger(L, (lua_Integer)L);
		lua_pushinteger(L, task->skey);
		return 2;
	}

	async_task_reset_work(svs, task);
	return 0;
}

static int lua_async_task_yield(lua_State* L) {
	AsyncTask* task;
	assert( lua_istable(L, TASK_MAP_INDEX) );
	task = (lua_rawgetp(L, TASK_MAP_INDEX, L)==LUA_TUSERDATA) ? lua_touserdata(L, -1) : NULL;
	if( !task )
		return luaL_error(L, "MUST in async task thread!");
	lua_pop(L, 1);
	return lua_yield(L, lua_gettop(L));
}

static int lua_async_task_trace_pcall_noyield(lua_State* L) {
	AsyncTask* task;
	assert( lua_istable(L, TASK_MAP_INDEX) );
	task = (lua_rawgetp(L, TASK_MAP_INDEX, L)==LUA_TUSERDATA) ? lua_touserdata(L, -1) : NULL;
	if( !task )
		return luaL_error(L, "MUST in async task thread!");
	lua_pop(L, 1);
	puss_lua_get(L, PUSS_KEY_ERROR_HANDLE);
	lua_insert(L, 1);
	lua_pushboolean(L, lua_pcall(L, lua_gettop(L)-2, LUA_MULTRET, 1)==LUA_OK);
	lua_replace(L, 1);
	return lua_gettop(L);
}

static luaL_Reg async_task_service_methods[] =
	{ {"async_service_get_task_count", lua_async_service_get_task_count}
	, {"async_service_collectgarbage", lua_async_service_collectgarbage}
	, {"async_service_run", lua_async_service_run}
	, {"async_service_kill", lua_async_service_kill}
	, {"async_service_wakeup", lua_async_service_wakeup}
	, {"async_service_resume", lua_async_service_resume}
	, {"async_service_update", lua_async_service_update}
	, {"async_service_group_reg", lua_async_service_group_reg}
	, {"async_service_group_unreg", lua_async_service_group_unreg}
	, {"async_service_group_cancel", lua_async_service_group_cancel}
	, {"async_task_self", lua_async_task_self}
	, {"async_task_sleep", lua_async_task_sleep}
	, {"async_task_alarm", lua_async_task_alarm}
	, {"async_task_yield", lua_async_task_yield}
	, {"async_task_trace_pcall_noyield", lua_async_task_trace_pcall_noyield}
	, {NULL, NULL}
	};

int puss_reg_async_service(lua_State* L) {
	AsyncTaskService* svs = lua_newuserdata(L, sizeof(AsyncTaskService));
	memset(svs, 0, sizeof(AsyncTaskService));
	rbx_init(&(svs->timers));
	grp_list_init(&(svs->sleep_group));

	svs->dummy_work_task.timeout = TIMEOUT_WORK;
	timer_queue_insert(svs, &(svs->dummy_work_task));

	svs->dummy_free_task.timeout = TIMEOUT_FREE;
	timer_queue_insert(svs, &(svs->dummy_free_task));

	lua_newtable(L);	// tasks map
	lua_newtable(L);	// groups map
	luaL_setfuncs(L, async_task_service_methods, 3);
	return 0;
}
