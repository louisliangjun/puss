// puss_thread_service.c

#include "puss_toolkit.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

#include "thread_utils.h"

#define PUSS_NAME_THREAD_MT		"_@PussThreadMT@_"
#define PUSS_NAME_THREADENV_MT	"_@PussThreadEnvMT@_"

typedef struct _TMsg	TMsg;
typedef struct _QEnv	QEnv;

struct _TMsg {
	TMsg*			next;
	size_t			len;
	char			buf[1];
};

struct _QEnv {
	PussMutex		qenv_mutex;		// NOTICE: alway lock owner qenv_mutex before lock child qenv_mutex !!!
	QEnv*			qenv_owner;		// owner lua state qenv
	QEnv*			qenv_children;	// list head
	QEnv*			qenv_sibling;	// list next

	PussMutex		mutex;
#ifdef _WIN32
	HANDLE			ev;
#else
	pthread_cond_t	cond;
#endif
	size_t			size;
	TMsg*			head;
	TMsg*			tail;
	TMsg			kill;
	lua_State*		L;
	lua_CFunction	event_handle;	// function event_handle(TMsg** ptask);	-- now simple use ptask==nil as idle

};

typedef struct _PussLuaThread {
	PussThreadID	tid;
	QEnv*			env;
} PussLuaThread;

#define task_queue_push_all(queue, other_head, other_tail)	{ \
			assert( (other_tail) && ((other_tail)->next==0) ); \
			if( (queue)->head ) \
				(queue)->tail->next = other_head; \
			else \
				(queue)->head = other_head; \
			(queue)->tail = other_tail; \
		}

#define task_queue_push(queue, msg)	{ \
			assert( (msg) && ((msg)->next==0) ); \
			if( (queue)->head ) \
				(queue)->tail->next = msg; \
			else \
				(queue)->head = msg; \
			(queue)->tail = msg; \
		}

static void qenv_init(QEnv* q) {
	memset(q, 0, sizeof(QEnv));
#ifdef _WIN32
	q->ev = CreateEventA(NULL, TRUE, FALSE, NULL);
#else
	pthread_cond_init(&q->cond, NULL);
#endif
	puss_mutex_init(&q->qenv_mutex);
	puss_mutex_init(&q->mutex);
}

static void qenv_uninit(QEnv* q) {
#ifdef _WIN32
	CloseHandle(q->ev);
#else
	pthread_cond_destroy(&q->cond);
#endif
	puss_mutex_uninit(&q->mutex);
	puss_mutex_uninit(&q->qenv_mutex);
}

static void qenv_push(QEnv* q, TMsg* task) {
	BOOL need_event;
	task->next = NULL;

	puss_mutex_lock(&q->mutex);
	q->size++;
	task_queue_push(q, task);
	need_event = (q->head==task);
	puss_mutex_unlock(&q->mutex);

	if( need_event ) {
#ifdef _WIN32
		SetEvent(q->ev);
#else
		pthread_cond_signal(&q->cond);
#endif
	}
}

static void qenv_push_all(QEnv* q, TMsg* head, TMsg* tail) {
	size_t n = 0;
	TMsg* p;
	BOOL need_event;
	tail->next = NULL;

	puss_mutex_lock(&q->mutex);
	for( p=head; p; p=p->next )
		++n;
	q->size += n;
	task_queue_push_all(q, head, tail);
	need_event = (q->head==head);
	puss_mutex_unlock(&q->mutex);

	if( need_event ) {
#ifdef _WIN32
		SetEvent(q->ev);
#else
		pthread_cond_signal(&q->cond);
#endif
	}
}

static inline TMsg* __qenv_pop(QEnv* q) {
	TMsg* res = NULL;
	if( q->head ) {
		res = q->head;
		q->head = res->next;
		res->next = NULL;
		q->size--;

#ifdef _WIN32
		if( !(q->head) )
			ResetEvent(q->ev);
#endif
	}
	return res;
}

static TMsg* qenv_pop(QEnv* q) {
	TMsg* res;
	puss_mutex_lock(&q->mutex);
	res = __qenv_pop(q);
	puss_mutex_unlock(&q->mutex);
	return res;
}

static TMsg* qenv_bpop(QEnv* q, uint32_t wait_time) {
	TMsg* res = NULL;

#ifdef _WIN32
	if( WaitForSingleObject(q->ev, wait_time)==WAIT_OBJECT_0 ) {
		puss_mutex_lock(&q->mutex);
		res = __qenv_pop(q);
		puss_mutex_unlock(&q->mutex);
	}
#else
	puss_mutex_lock(&q->mutex);
task_pop_label:
	if( (res = __qenv_pop(q)) == NULL ) {
		if( ks_pthread_wait_cond_timed(&q->mutex, &(q->cond), wait_time) )
			goto task_pop_label;
	}
	puss_mutex_unlock(&q->mutex);
#endif

	return res;
}

static inline TMsg* __qenv_pop_all(QEnv* q) {
	TMsg* res = NULL;
	q->size = 0;
	if( q->head ) {
		q->tail->next = NULL;
		res = q->head;
		q->head = NULL;
#ifdef _WIN32
		ResetEvent(q->ev);
#endif
	}
	return res;
}

static TMsg* qenv_pop_all(QEnv* q) {
	TMsg* res;
	puss_mutex_lock(&q->mutex);
	res = __qenv_pop_all(q);
	puss_mutex_unlock(&q->mutex);
	return res;
}

static TMsg* qenv_bpop_all(QEnv* q, uint32_t wait_time) {
	TMsg* res = NULL;
#ifdef _WIN32
	if( WaitForSingleObject(q->ev, wait_time)==WAIT_OBJECT_0 ) {
		puss_mutex_lock(&q->mutex);
		res = __qenv_pop_all(q);
		puss_mutex_unlock(&q->mutex);
	}
#else
	puss_mutex_lock(&q->mutex);
task_pop_all_label:
	if( (res = __qenv_pop_all(q)) == NULL ) {
		if( ks_pthread_wait_cond_timed(&q->mutex, &(q->cond), wait_time) )
			goto task_pop_all_label;
	}
	puss_mutex_unlock(&q->mutex);
#endif
	return res;
}

static size_t qenv_fetch_size(QEnv* q) {
	size_t n;
	puss_mutex_lock(&q->mutex);
	n = q->size;
	puss_mutex_unlock(&q->mutex);
	return n;
}

static int thread_destroy(lua_State* L) {
	PussLuaThread* self = (PussLuaThread*)luaL_checkudata(L, 1, PUSS_NAME_THREAD_MT);
	QEnv* q = self->env;
	if( q ) {
		QEnv* owner;

		self->env = NULL;
		puss_mutex_lock(&q->qenv_mutex);
		owner = q->qenv_owner;
		if( owner ) {
			QEnv* p;
			puss_mutex_lock(&owner->qenv_mutex);
			p = owner->qenv_children;
			if( p==q ) {
				q->qenv_owner->qenv_children = p->qenv_sibling;
			} else {
				for( ; p; p=p->qenv_sibling ) {
					if( p->qenv_sibling==q ) {
						p->qenv_sibling = q->qenv_sibling;
						break;
					}
				}
			}
			q->qenv_owner = NULL;
			q->qenv_sibling = NULL;
			puss_mutex_unlock(&owner->qenv_mutex);
		}
		puss_mutex_unlock(&q->qenv_mutex);

		qenv_push(q, &(q->kill));
	}

	if( self->tid ) {
		puss_thread_detach(self->tid);
		self->tid = 0;
	}
	return 0;
}

static int thread_query(lua_State* L) {
	PussLuaThread* self = (PussLuaThread*)luaL_checkudata(L, 1, PUSS_NAME_THREAD_MT);
	int top = lua_gettop(L);
	size_t len = 0;
	void* pkt;
	TMsg* msg;
	if( !self->env )
		return luaL_error(L, "thread already detached!");
	if( top < 2 )
		return luaL_error(L, "thread query bad args!");
	pkt = puss_simple_pack(&len, L, 2, -1);
	msg = (TMsg*)malloc(sizeof(TMsg) + len);
	if( !msg )
		return luaL_error(L, "no memory!");
	msg->next = NULL;
	msg->len = len;
	memcpy(msg->buf, pkt, len);
	qenv_push(self->env, msg);
	return 0;
}

static luaL_Reg thread_methods[] =
	{ {"__gc", thread_destroy}
	, {"destroy", thread_destroy}
	, {"query", thread_query}
	, {NULL, NULL}
	};

static PUSS_THREAD_DECLARE(thread_main, arg) {
	QEnv* q = (QEnv*)arg;
	lua_State* L = q->L;
	TMsg* msg = NULL;
	TMsg* next = NULL;
	uint32_t wait_time = 1000;	// TODO : idle interval

	for( ; msg!=&(q->kill); msg=next ) {
		next = msg ? msg->next : qenv_bpop_all(q, wait_time);
		if( msg ) {
			msg->next = NULL;
			lua_settop(L, 0);
			PUSS_LUA_GET(L, PUSS_KEY_ERROR_HANDLE);
			lua_pushcfunction(L, q->event_handle);
			lua_pushlightuserdata(L, &(msg));
			lua_pcall(L, 1, 0, 1);
			lua_settop(L, 0);
			if( msg )
				free(msg);
		} else {
			lua_settop(L, 0);
			PUSS_LUA_GET(L, PUSS_KEY_ERROR_HANDLE);
			lua_pushcfunction(L, q->event_handle);
			lua_pcall(L, 0, 0, 1);
			lua_settop(L, 0);
		}
	}

	assert( msg==&(q->kill) );
	assert( !(msg->next) );
	return 0;
}

static int thread_env_gc(lua_State* L) {
	QEnv* q = (QEnv*)lua_touserdata(L, 1);

	// only owner can destroy thread
	lua_assert( q->qenv_owner==NULL );

	if( q->L ) {
		QEnv* p;
		puss_mutex_lock(&q->qenv_mutex);
		while( (p = q->qenv_children) != NULL ) {
			puss_mutex_lock(&p->qenv_mutex);
			q->qenv_children = p->qenv_sibling;
			p->qenv_owner = NULL;
			p->qenv_sibling = NULL;
			puss_mutex_unlock(&p->qenv_mutex);
		}
		qenv_uninit(q);
		q->L = NULL;
		puss_mutex_unlock(&q->qenv_mutex);
	}
	return 0;
}

static int _default_thread_event_handle(lua_State* L) {
	void* ev = lua_touserdata(L, 1);
	if( ev ) {
		TMsg* task = *((TMsg**)ev);
		puss_simple_unpack(L, task->buf, task->len);
		if( lua_type(L, 2)==LUA_TSTRING ) {
			puss_get_value(L, lua_tostring(L, 2));
		} else {
			lua_pushglobaltable(L);
			lua_replace(L, 1);
			lua_pushvalue(L, 2);
			lua_gettable(L, 1);
		}
		lua_replace(L, 2);
		lua_call(L, lua_gettop(L)-2, 0);
	}
	return 0;
}

static int puss_lua_thread_create(lua_State* L) {
	PussLuaThread* ud = (PussLuaThread*)lua_newuserdata(L, sizeof(PussLuaThread));
	QEnv* self_env = NULL;
	lua_State* new_state = NULL;
	QEnv* new_env = NULL;
	lua_CFunction event_handle = lua_tocfunction(L, 1);
	if( !event_handle )	event_handle = _default_thread_event_handle;
	memset(ud, 0, sizeof(PussLuaThread));

	if( luaL_newmetatable(L, PUSS_NAME_THREAD_MT) ) {
		luaL_setfuncs(L, thread_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);

	new_state = __puss_toolkit_sink__.state_new();

	if( PUSS_LUA_GET(new_state, PUSS_KEY_THREAD_ENV)==LUA_TUSERDATA )
		new_env = (QEnv*)lua_touserdata(new_state, -1);
	lua_pop(new_state, 1);
	if( !new_env ) {
		if( new_state ) lua_close(new_state);
		return luaL_error(L, "no memory!");
	}

	if( !puss_thread_create(&(ud->tid), thread_main, new_env) ) {
		lua_close(new_state);
		return luaL_error(L, "create thread failed!");
	}
	ud->env = new_env;

	if( PUSS_LUA_GET(L, PUSS_KEY_THREAD_ENV)==LUA_TUSERDATA )
		self_env = (QEnv*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	if( self_env ) {
		puss_mutex_lock(&self_env->qenv_mutex);
		puss_mutex_lock(&new_env->qenv_mutex);
		new_env->event_handle = event_handle;
		new_env->qenv_owner = self_env;
		new_env->qenv_sibling = self_env->qenv_children;
		self_env->qenv_children = new_env;
		puss_mutex_unlock(&new_env->qenv_mutex);
		puss_mutex_unlock(&self_env->qenv_mutex);
	}

	return 1;
}

static int puss_lua_thread_notify(lua_State* L) {
	QEnv* self_env = NULL;
	TMsg* msg = NULL;
	size_t len = 0;
	void* pkt;
	QEnv* owner;
	if( PUSS_LUA_GET(L, PUSS_KEY_THREAD_ENV)==LUA_TUSERDATA )
		self_env = (QEnv*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	if( !(self_env && self_env->qenv_owner) ) {
		lua_pushboolean(L, 0);	// owner not exist, maybe detached
		return 1;
	}
	pkt = puss_simple_pack(&len, L, 1, -1);
	msg = (TMsg*)malloc(sizeof(TMsg) + len);
	if( !msg )
		return luaL_error(L, "no memory!");
	msg->next = NULL;
	msg->len = len;
	memcpy(msg->buf, pkt, len);

	puss_mutex_lock(&self_env->qenv_mutex);
	if( (owner = self_env->qenv_owner) != NULL ) {
		qenv_push(owner, msg);
		msg = NULL;
	}
	puss_mutex_unlock(&self_env->qenv_mutex);

	lua_pushboolean(L, msg==NULL);	// owner not exist, maybe detached
	if(msg)	free(msg);
	return 1;
}

static int simple_unpack_msg(lua_State* L) {
	TMsg* msg = (TMsg*)lua_touserdata(L, 1);
	puss_simple_unpack(L, msg->buf, msg->len);
	return lua_gettop(L) - 1;
}

static int puss_lua_thread_dispatch(lua_State* L) {
	QEnv* self_env = NULL;
	TMsg* msg = NULL;
	int res = LUA_OK;
	luaL_checktype(L, 1, LUA_TFUNCTION);	// error handle
	if( PUSS_LUA_GET(L, PUSS_KEY_THREAD_ENV)==LUA_TUSERDATA )
		self_env = (QEnv*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	if( !self_env )
		return 0;

	msg = qenv_pop(self_env);
	if( !msg )
		return 0;

	lua_pushcfunction(L, simple_unpack_msg);
	lua_pushlightuserdata(L, msg);
	res = lua_pcall(L, 1, LUA_MULTRET, 0);
	free(msg);
	if( res )
		return lua_error(L);

	lua_call(L, lua_gettop(L)-1, LUA_MULTRET);
	return lua_gettop(L);
}

static luaL_Reg thread_service_methods[] =
	{ {"thread_create", puss_lua_thread_create}
	, {"thread_event_notify", puss_lua_thread_notify}
	, {"thread_event_dispatch", puss_lua_thread_dispatch}
	, {NULL, NULL}
	};

static int thread_env_ensure(lua_State* L) {
	QEnv* env = (QEnv*)lua_newuserdata(L, sizeof(QEnv));
	if( luaL_newmetatable(L, PUSS_NAME_THREADENV_MT) ) {
		lua_pushcfunction(L, thread_env_gc);
		lua_setfield(L, -2, "__gc");
	}
	lua_setmetatable(L, -2);
	qenv_init(env);
	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	env->L = lua_tothread(L, -1);
	lua_pop(L, 1);
	__puss_toolkit_sink__.state_set_key(L, PUSS_KEY_THREAD_ENV);
	return 0;
}

void puss_reg_thread_service(lua_State* L) {
	int top;
	int ret = PUSS_LUA_GET(L, PUSS_KEY_THREAD_ENV);
	lua_pop(L, 1);
	if( ret==LUA_TUSERDATA )
		return;

	top = lua_gettop(L);
	PUSS_LUA_GET(L, PUSS_KEY_ERROR_HANDLE);
	lua_pushcfunction(L, thread_env_ensure);
	lua_pcall(L, 0, 0, top+1);
	lua_settop(L, top);

	ret = PUSS_LUA_GET(L, PUSS_KEY_THREAD_ENV);
	lua_pop(L, 1);

	if( ret==LUA_TUSERDATA )
		luaL_setfuncs(L, thread_service_methods, 0);
}
