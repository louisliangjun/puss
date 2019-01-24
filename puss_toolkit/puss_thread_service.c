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

	PussMutex		qenv_mutex;		// NOTICE: alway lock owner qenv_mutex before lock child qenv_mutex !!!
	QEnv*			qenv_owner;		// owner lua state qenv
	QEnv*			qenv_children;	// list head
	QEnv*			qenv_sibling;	// list next
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

static TMsg* qenv_pop_all(QEnv* q) {
	TMsg* res = NULL;
	puss_mutex_lock(&q->mutex);
	q->size = 0;
	if( q->head ) {
		q->tail->next = NULL;
		res = q->head;
		q->head = NULL;

#ifdef _WIN32
		ResetEvent(q->ev);
#endif
	}
	puss_mutex_unlock(&q->mutex);

	return res;
}

static TMsg* __qenv_pop(QEnv* q) {
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

static TMsg* qenv_pop(QEnv* q, uint32_t wait_time) {
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
		if( wait_time==0xFFFFFFFF) {
			if( ks_pthread_wait_cond(&q->mutex, &(q->cond)) )
				goto task_pop_label;
		} else {
			if( ks_pthread_wait_cond_timed(&q->mutex, &(q->cond), wait_time) )
				goto task_pop_label;
		}
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

static int _qstate_reply(lua_State* L) {
	lua_State* from = (lua_State*)lua_touserdata(L, 1);
	int top = (int)lua_tointeger(L, 2);
	size_t len = 0;
	void* pkt = puss_simple_pack(&len, from, top, -1);
	lua_settop(L, 0);
	puss_simple_unpack(L, pkt, len);
	return lua_gettop(L);
}

static int qenv_query(lua_State* L) {
	TMsg* task = (TMsg*)lua_touserdata(L, 1);
	lua_settop(L, 0);
	puss_simple_unpack(L, task->buf, task->len);
	lua_pushglobaltable(L);
	lua_pushvalue(L, 1);
	lua_gettable(L, -2);
	lua_replace(L, 1);
	lua_pop(L, 1);
	lua_call(L, lua_gettop(L)-1, 0);
	return 0;
}

static PUSS_THREAD_DECLARE(thread_main, arg) {
	QEnv* q = (QEnv*)arg;
	lua_State* L = q->L;
	TMsg* msg = NULL;
	uint32_t wait_time = 1000;	// TODO : idle

	while( (msg = qenv_pop(q, wait_time)) != &(q->kill) ) {
		if( msg ) {
			lua_settop(L, 0);
			lua_pushcfunction(L, qenv_query);
			lua_pushlightuserdata(L, msg);
			if( lua_pcall(L, 1, 0, 0) ) {
				lua_pop(L, 1);
			}
			free(msg);
		} else {
			// idle
		}
	}


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

static int puss_lua_thread_create(lua_State* L) {
	PussLuaThread* ud = (PussLuaThread*)lua_newuserdata(L, sizeof(PussLuaThread));
	QEnv* self_env = NULL;
	lua_State* new_state = NULL;
	QEnv* new_env = NULL;
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
		new_env->qenv_owner = (QEnv*)lua_touserdata(L, -1);
		new_env->qenv_sibling = self_env->qenv_children;
		self_env->qenv_children = new_env;
		puss_mutex_unlock(&new_env->qenv_mutex);
		puss_mutex_unlock(&self_env->qenv_mutex);
	}

	return 1;
}

static int puss_lua_thread_reply(lua_State* L) {
	QEnv* self_env = NULL;
	size_t len = 0;
	void* pkt;
	TMsg* msg;
	if( PUSS_LUA_GET(L, PUSS_KEY_THREAD_ENV)==LUA_TUSERDATA )
		self_env = (QEnv*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	if( !(self_env && self_env->qenv_owner) )
		return 0;
	pkt = puss_simple_pack(&len, L, 1, -1);
	msg = (TMsg*)malloc(sizeof(TMsg) + len);
	if( !msg )
		return luaL_error(L, "no memory!");
	msg->next = NULL;
	msg->len = len;
	memcpy(msg->buf, pkt, len);

	puss_mutex_lock(&self_env->qenv_mutex);
	if( self_env->qenv_owner ) {
		qenv_push(self_env->qenv_owner, msg);
		msg = NULL;
	}
	puss_mutex_unlock(&self_env->qenv_mutex);

	if( msg )
		free(msg);
	return 0;
}

static luaL_Reg thread_service_methods[] =
	{ {"thread_create", puss_lua_thread_create}
	, {"thread_reply", puss_lua_thread_reply}
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

void puss_thread_service_reg(lua_State* L) {
	int ret = PUSS_LUA_GET(L, PUSS_KEY_THREAD_ENV);
	lua_pop(L, 1);
	if( ret==LUA_TUSERDATA )
		return;

	lua_pushcfunction(L, thread_env_ensure);
	lua_pcall(L, 0, 0, 0);

	ret = PUSS_LUA_GET(L, PUSS_KEY_THREAD_ENV);
	lua_pop(L, 1);

	if( ret==LUA_TUSERDATA )
		luaL_setfuncs(L, thread_service_methods, 0);
}
