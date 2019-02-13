// thread_service.c

#include "puss_toolkit.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

#include "thread_utils.h"

#define PUSS_NAME_THREAD_MT		"_@PussThread@_"

#define PUSS_DETACH_MSG_LEN		(~((size_t)0))

typedef struct _TMsg	TMsg;
typedef struct _TQueue	TQueue;

struct _TMsg {
	TMsg*			next;
	size_t			len;
	char			buf[1];
};

struct _TQueue {
	PussMutex		mutex;
	int				_ref;

	// task queue
#ifdef _WIN32
	HANDLE			ev;
#else
	pthread_cond_t	cond;
#endif
	TMsg*			head;
	TMsg*			tail;
};

typedef struct _QEnv {
	TQueue*			q;
	int				detached;
} QEnv;

typedef struct _ThreadHandle {
	TQueue*			q;
	PussThreadID	tid;
} ThreadHandle;

static TQueue* queue_ref(TQueue* q) {
	if( q ) {
		puss_mutex_lock(&q->mutex);
		++(q->_ref);
		puss_mutex_unlock(&q->mutex);
	}
	return q;
}

static void queue_unref(TQueue* q) {
	int ref;
	if( !q )
		return;
	puss_mutex_lock(&q->mutex);
	ref = (--(q->_ref));
	puss_mutex_unlock(&q->mutex);
	if( ref > 0 )
		return;
#ifdef _WIN32
	CloseHandle(q->ev);
#else
	pthread_cond_destroy(&q->cond);
#endif
	puss_mutex_uninit(&q->mutex);

	while( q->head ) {
		TMsg* msg = q->head;
		q->head = msg->next;
		free(msg);
	}

	free(q);
}

#define task_queue_push(queue, msg)	{ \
			assert( (msg) && ((msg)->next==0) ); \
			if( (queue)->head ) \
				(queue)->tail->next = msg; \
			else \
				(queue)->head = msg; \
			(queue)->tail = msg; \
		}

#define task_queue_pop(queue, msg)	{ \
			if( (msg = queue->head)!=NULL ) { \
				queue->head = msg->next; \
				msg->next = NULL; \
			} \
		}

static void queue_push(TQueue* q, TMsg* task) {
#ifdef _WIN32
	puss_mutex_lock(&q->mutex);
	task_queue_push(q, task);
	if( q->head==task )
		SetEvent(q->ev);
	puss_mutex_unlock(&q->mutex);
#else
	int need_event;
	puss_mutex_lock(&q->mutex);
	task_queue_push(q, task);
	need_event = (q->head==task);
	puss_mutex_unlock(&q->mutex);
	if( need_event )
		pthread_cond_signal(&q->cond);
#endif
}

static TMsg* queue_pop(TQueue* q) {
	TMsg* res = NULL;
	puss_mutex_lock(&q->mutex);
	if( q->head ) {
		res = q->head;
		q->head = res->next;
		res->next = NULL;
#ifdef _WIN32
		if( !(q->head) )
			ResetEvent(q->ev);
#endif
	}
	puss_mutex_unlock(&q->mutex);
	return res;
}

static TMsg* queue_wait(TQueue* q, uint32_t wait_time) {
	TMsg* res = queue_pop(q);
	if( (!res) && (wait_time>0) ) {
#ifdef _WIN32
		if( WaitForSingleObject(q->ev, wait_time)==WAIT_OBJECT_0 )
			res = queue_pop(q);
#else
		puss_mutex_lock(&q->mutex);
		task_queue_pop(q, res);
		if( !res ) {
			puss_pthread_cond_timedwait(&q->cond, &q->mutex, wait_time);
			task_queue_pop(q, res);
		}
		puss_mutex_unlock(&q->mutex);
#endif
	}
	return res;
}

static int thread_env_gc(lua_State* L) {
	QEnv* ud = (QEnv*)lua_touserdata(L, 1);
	TQueue* q = ud->q;
	ud->q = NULL;
	queue_unref(q);
	return 0;
}

static int thread_env_create(lua_State* L) {
	TQueue* q = (TQueue*)malloc(sizeof(TQueue));
	QEnv* ud = (QEnv*)lua_newuserdata(L, sizeof(QEnv));
	memset(ud, 0, sizeof(QEnv));
	if( luaL_newmetatable(L, "_@PussThreadEnv@_") ) {
		lua_pushcfunction(L, thread_env_gc);
		lua_setfield(L, -2, "__gc");
	}
	lua_setmetatable(L, -2);

	memset(q, 0, sizeof(TQueue));
#ifdef _WIN32
	q->ev = CreateEventA(NULL, TRUE, FALSE, NULL);
#else
	pthread_cond_init(&q->cond, NULL);
#endif
	puss_mutex_init(&q->mutex);

	ud->q = queue_ref(q);
	ud->detached = 0;
	lua_pushvalue(L, -1);
	__puss_config__.state_set_key(L, PUSS_KEY_THREAD_ENV);
	return 1;
}

static QEnv* puss_thread_env_fetch(lua_State* L) {
	QEnv* env = puss_lua_get(L, PUSS_KEY_THREAD_ENV)==LUA_TUSERDATA ? lua_touserdata(L, -1) : NULL;
	lua_pop(L, 1);
	return env;
}

static QEnv* puss_thread_env_ensure(lua_State* L) {
	int top = lua_gettop(L);
	QEnv* env = NULL;
	if( puss_lua_get(L, PUSS_KEY_THREAD_ENV)!=LUA_TUSERDATA ) {
		lua_pop(L, 1);
		puss_lua_get(L, PUSS_KEY_ERROR_HANDLE);
		lua_pushcfunction(L, thread_env_create);
		lua_pcall(L, 0, 1, top);
	}
	env = lua_touserdata(L, -1);
	lua_settop(L, top);
	return env;
}

static inline TQueue* puss_thread_queue_ensure(lua_State* L) {
	QEnv* env = puss_thread_env_ensure(L);
	return env ? env->q : NULL;
}

static PUSS_THREAD_DECLARE(thread_main_wrapper, arg) {
	lua_State* L = (lua_State*)arg;
	TQueue* q = queue_ref(puss_thread_queue_ensure(L));

	// thread main
	assert( lua_isfunction(L, 1) );
	assert( lua_isfunction(L, 2) );
	lua_pcall(L, lua_gettop(L)-2, 0, 1);

	// close state
	lua_close(L);
	queue_unref(q);
	return 0;
}

static int puss_lua_thread_detach(lua_State* L) {
	ThreadHandle* thread = (ThreadHandle*)luaL_checkudata(L, 1, PUSS_NAME_THREAD_MT);
	TQueue* q = thread->q;
	if( thread->tid ) {
		if( q ) {
			TMsg* detach_msg = (TMsg*)malloc(sizeof(TMsg));
			if( !detach_msg )
				return luaL_error(L, "detach no memory!");
			detach_msg->next = NULL;
			detach_msg->len = PUSS_DETACH_MSG_LEN;
			queue_push(q, detach_msg);
		}
		puss_thread_detach(thread->tid);
		thread->tid = 0;
	}
	thread->q = NULL;
	queue_unref(q);
	return 0;
}

static ThreadHandle* puss_lua_thread_new(lua_State* L) {
	ThreadHandle* ud = (ThreadHandle*)lua_newuserdata(L, sizeof(ThreadHandle));
	memset(ud, 0, sizeof(ThreadHandle));
	if( luaL_newmetatable(L, PUSS_NAME_THREAD_MT) ) {
		lua_pushcfunction(L, puss_lua_thread_detach);
		lua_setfield(L, -2, "__gc");
	}
	lua_setmetatable(L, -2);
	return ud;
}

typedef struct _ThreadCreateArg {
	TQueue*			owner_queue;
	size_t			args_len;
	void*			args_buf;
} ThreadCreateArg;

static int do_thread_state_prepare(lua_State* L) {
	ThreadCreateArg* arg = (ThreadCreateArg*)lua_touserdata(L, 1);
	puss_lua_get(L, PUSS_KEY_PUSS);
	{
		ThreadHandle* ud = puss_lua_thread_new(L);
		ud->tid = 0;	// owner not use tid
		ud->q = queue_ref(arg->owner_queue);
	}
	lua_setfield(L, -2, "thread_owner");

	lua_settop(L, 0);
	puss_lua_get(L, PUSS_KEY_ERROR_HANDLE);
	puss_lua_get(L, PUSS_KEY_PUSS);
	lua_getfield(L, -1, "dostring");
	lua_replace(L, -2);
	puss_simple_unpack(L, arg->args_buf, arg->args_len);
	return lua_gettop(L);
}

static int puss_lua_thread_create(lua_State* L) {
	ThreadCreateArg create_arg = { NULL, 0, 0 };
	QEnv* new_env = NULL;
	ThreadHandle* shared = NULL;
	lua_State* new_state;
	ThreadHandle* ud;
	int args_pos = 1;
	if( lua_type(L, args_pos)==LUA_TUSERDATA )
		shared = (ThreadHandle*)luaL_checkudata(L, args_pos++, PUSS_NAME_THREAD_MT);
	create_arg.args_buf = puss_simple_pack(&(create_arg.args_len), L, args_pos, -1);
	create_arg.owner_queue = puss_thread_queue_ensure(L);
	if( !create_arg.owner_queue )
		return luaL_error(L, "bad logic");

	ud = puss_lua_thread_new(L);

	new_state = puss_lua_newstate();
	if( !new_state )
		return luaL_error(L, "puss_lua_newstate failed!");
	new_env = puss_thread_env_ensure(new_state);
	if( !new_env )
		return luaL_error(L, "bad logic, puss_lua_newstate no env!");
	if( shared && shared->q ) {
		queue_unref(new_env->q);
		new_env->q = queue_ref(shared->q);
	}
	if( !new_env->q ) {
		lua_close(new_state);
		return luaL_error(L, "bad logic, puss_lua_newstate no env!");
	}
	ud->q = queue_ref(new_env->q);

	lua_settop(new_state, 0);
	lua_pushcfunction(new_state, do_thread_state_prepare);
	lua_pushlightuserdata(new_state, &create_arg);
	if( lua_pcall(new_state, 1, LUA_MULTRET, 0) )
		return luaL_error(L, "new state init: %s", lua_isstring(new_state, -1) ? lua_tostring(new_state, -1) : "do_thread_state_prepare failed!");

	if( !puss_thread_create(&(ud->tid), thread_main_wrapper, new_state) ) {
		queue_unref(ud->q);
		ud->q = NULL;
		assert( ud->q->_ref > 0 );
		lua_close(new_state);
		return luaL_error(L, "create thread failed!");
	}

	return 1;
}

static int puss_lua_thread_post(lua_State* L) {
	int top = lua_gettop(L);
	size_t len = 0;
	void* pkt;
	TMsg* msg;
	ThreadHandle* thread = (ThreadHandle*)luaL_testudata(L, 1, PUSS_NAME_THREAD_MT);
	TQueue* q = thread ? thread->q : puss_thread_queue_ensure(L);
	if( top < 2 )
		return luaL_error(L, "thread post bad args!");
	if( !q )
		return 0;
	pkt = puss_simple_pack(&len, L, 2, -1);
	msg = (TMsg*)malloc(sizeof(TMsg) + len);
	if( !msg )
		return luaL_error(L, "no memory!");
	msg->next = NULL;
	msg->len = len;
	memcpy(msg->buf, pkt, len);
	queue_push(q, msg);
	return 0;
}

static int simple_unpack_msg(lua_State* L) {
	TMsg* msg = (TMsg*)lua_touserdata(L, 1);
	puss_simple_unpack(L, msg->buf, msg->len);
	return lua_gettop(L) - 1;
}

static int puss_lua_thread_wait(lua_State* L) {
	uint32_t wait_time = (uint32_t)luaL_optinteger(L, 1, 0);
	QEnv* env;
	TQueue* q;
	TMsg* msg;
	int res;
	if( (env = puss_thread_env_fetch(L))==NULL )
		return 0;
	if( (q = env->q)==NULL )
		return 0;
	if( (msg = queue_wait(q, wait_time))==NULL )
		return 0;
	if( msg->len==PUSS_DETACH_MSG_LEN ) {
		env->detached = 1;
		free(msg);
		return 0;
	}
	lua_settop(L, 0);
	lua_pushcfunction(L, simple_unpack_msg);
	lua_pushlightuserdata(L, msg);
	res = lua_pcall(L, 1, LUA_MULTRET, 0);
	free(msg);
	return res ? lua_error(L) : lua_gettop(L);
}

static int puss_lua_thread_detached(lua_State* L) {
	ThreadHandle* thread = (ThreadHandle*)luaL_testudata(L, 1, PUSS_NAME_THREAD_MT);
	if( thread ) {
		lua_pushboolean(L, thread->tid==0);
	} else {
		QEnv* env = puss_thread_env_fetch(L);
		lua_pushboolean(L, env ? env->detached : 1);
	}
	return 1;
}

static luaL_Reg thread_service_methods[] =
	{ {"thread_owner", NULL}
	, {"thread_detach", puss_lua_thread_detach}
	, {"thread_create", puss_lua_thread_create}
	, {"thread_post", puss_lua_thread_post}
	, {"thread_wait", puss_lua_thread_wait}
	, {"thread_detached", puss_lua_thread_detached}
	, {NULL, NULL}
	};

void puss_reg_thread_service(lua_State* L) {
	luaL_setfuncs(L, thread_service_methods, 0);
}
