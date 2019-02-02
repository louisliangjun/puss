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
	lua_State*		L;
	int				detached;
	int				_ref;
	TMsg			_detach;

	// task queue
#ifdef _WIN32
	HANDLE			ev;
#else
	pthread_cond_t	cond;
#endif
	TMsg*			head;
	TMsg*			tail;
};

typedef struct _PussLuaThread {
	PussThreadID	tid;
	QEnv*			env;
} PussLuaThread;

static QEnv* qenv_ref(QEnv* env) {
	if( env ) {
		puss_mutex_lock(&env->mutex);
		++(env->_ref);
		puss_mutex_unlock(&env->mutex);
	}
	return env;
}

static void qenv_unref(QEnv* env) {
	int ref;
	if( !env )
		return;
	puss_mutex_lock(&env->mutex);
	ref = (--(env->_ref));
	puss_mutex_unlock(&env->mutex);
	if( ref > 0 )
		return;
#ifdef _WIN32
	CloseHandle(env->ev);
#else
	pthread_cond_destroy(&env->cond);
#endif
	puss_mutex_uninit(&env->mutex);
	free(env);
}

#define task_queue_push(queue, msg)	{ \
			assert( (msg) && ((msg)->next==0) ); \
			if( (queue)->head ) \
				(queue)->tail->next = msg; \
			else \
				(queue)->head = msg; \
			(queue)->tail = msg; \
		}

static void qenv_push(QEnv* q, TMsg* task) {
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

static TMsg* qenv_pop(QEnv* q) {
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
		if( res==&q->_detach ) {
			q->detached = 1;
			res = NULL;
		}
	}
	puss_mutex_unlock(&q->mutex);
	return res;
}

static int thread_detach(lua_State* L) {
	PussLuaThread* self = (PussLuaThread*)luaL_checkudata(L, 1, PUSS_NAME_THREAD_MT);
	QEnv* env = self->env;
	self->env = NULL;
	if( self->tid ) {
		if( env )
			qenv_push(env, &env->_detach);
		puss_thread_detach(self->tid);
		self->tid = 0;
	}
	qenv_unref(env);
	return 0;
}

static int thread_post(lua_State* L) {
	PussLuaThread* self = (PussLuaThread*)luaL_checkudata(L, 1, PUSS_NAME_THREAD_MT);
	QEnv* env = self->env;
	int top = lua_gettop(L);
	size_t len = 0;
	void* pkt;
	TMsg* msg;
	if( !env )
		return 0;
	if( !(env->L) )
		return 0;
	if( top < 2 )
		return luaL_error(L, "thread post bad args!");
	pkt = puss_simple_pack(&len, L, 2, -1);
	msg = (TMsg*)malloc(sizeof(TMsg) + len);
	if( !msg )
		return luaL_error(L, "no memory!");
	msg->next = NULL;
	msg->len = len;
	memcpy(msg->buf, pkt, len);
	qenv_push(env, msg);
	return 0;
}

static luaL_Reg thread_methods[] =
	{ {"__gc", thread_detach}
	, {"detach", thread_detach}
	, {"post", thread_post}
	, {NULL, NULL}
	};

static PUSS_THREAD_DECLARE(thread_main_wrapper, arg) {
	QEnv* env = qenv_ref((QEnv*)arg);
	lua_State* L = env->L;

	// thread main
	assert( lua_isfunction(L, 1) );
	assert( lua_isfunction(L, 2) );
	lua_pcall(L, lua_gettop(L)-2, 0, 1);

	// close state
	env->L = NULL;
	lua_close(L);

	// wait detach signal if self not detached
	while( !env->detached ) {
		TMsg* msg = qenv_pop(env);
		if( msg )	free(msg);
	}

	qenv_unref(env);
	return 0;
}

static PussLuaThread* puss_lua_thread_new(lua_State* L) {
	PussLuaThread* ud = (PussLuaThread*)lua_newuserdata(L, sizeof(PussLuaThread));
	memset(ud, 0, sizeof(PussLuaThread));
	if( luaL_newmetatable(L, PUSS_NAME_THREAD_MT) ) {
		luaL_setfuncs(L, thread_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	return ud;
}

typedef struct _ThreadCreateArg {
	QEnv*			owner_env;
	lua_CFunction	thread_main;
	size_t			args_len;
	void*			args_buf;
} ThreadCreateArg;

static int do_thread_state_prepare(lua_State* L) {
	ThreadCreateArg* arg = (ThreadCreateArg*)lua_touserdata(L, 1);
	puss_lua_get(L, PUSS_KEY_PUSS);
	{
		PussLuaThread* ud = puss_lua_thread_new(L);
		ud->tid = 0;	// owner not use tid
		ud->env = qenv_ref(arg->owner_env);
	}
	lua_setfield(L, -2, "thread_owner");

	lua_settop(L, 0);
	puss_lua_get(L, PUSS_KEY_ERROR_HANDLE);
	if( arg->thread_main ) {
		lua_pushcfunction(L, arg->thread_main);
	} else {
		puss_lua_get(L, PUSS_KEY_PUSS);
		lua_getfield(L, -1, "dofile");
		lua_replace(L, -2);
	}
	puss_simple_unpack(L, arg->args_buf, arg->args_len);
	return lua_gettop(L);
}

static int puss_lua_thread_create(lua_State* L) {
	ThreadCreateArg create_arg;
	QEnv* new_env = NULL;
	lua_State* new_state;
	PussLuaThread* ud;
	create_arg.thread_main = lua_iscfunction(L, 1) ? lua_tocfunction(L, 1) : NULL;
	create_arg.args_buf = puss_simple_pack(&(create_arg.args_len), L, create_arg.thread_main ? 2 : 1, -1);

	create_arg.owner_env = puss_lua_get(L, PUSS_KEY_THREAD_ENV)==LUA_TUSERDATA ? *(QEnv**)lua_touserdata(L, -1) : NULL;
	lua_pop(L, 1);
	if( !create_arg.owner_env )
		return luaL_error(L, "bad logic");

	ud = puss_lua_thread_new(L);

	new_state = puss_lua_newstate();
	if( !new_state )
		return luaL_error(L, "puss_lua_newstate failed!");
	if( puss_lua_get(new_state, PUSS_KEY_THREAD_ENV)==LUA_TUSERDATA )
		new_env = *(QEnv**)lua_touserdata(new_state, -1);
	lua_pop(new_state, 1);
	if( !new_env ) {
		lua_close(new_state);
		return luaL_error(L, "bad logic, puss_lua_newstate no env!");
	}
	ud->env = qenv_ref(new_env);

	lua_settop(new_state, 0);
	lua_pushcfunction(new_state, do_thread_state_prepare);
	lua_pushlightuserdata(new_state, &create_arg);
	if( lua_pcall(new_state, 1, LUA_MULTRET, 0) )
		return luaL_error(L, "new state init: %s", lua_isstring(new_state, -1) ? lua_tostring(new_state, -1) : "do_thread_state_prepare failed!");

	if( !puss_thread_create(&(ud->tid), thread_main_wrapper, new_env) ) {
		ud->env = NULL;
		qenv_unref(new_env);
		assert( new_env->_ref > 0 );
		lua_close(new_state);
		return luaL_error(L, "create thread failed!");
	}

	return 1;
}

static int simple_unpack_msg(lua_State* L) {
	TMsg* msg = (TMsg*)lua_touserdata(L, 1);
	puss_simple_unpack(L, msg->buf, msg->len);
	return lua_gettop(L) - 1;
}

static int puss_lua_thread_detached(lua_State* L) {
	QEnv* env = NULL;
	if( puss_lua_get(L, PUSS_KEY_THREAD_ENV)==LUA_TUSERDATA )
		env = *(QEnv**)lua_touserdata(L, -1);
	lua_pop(L, 1);
	lua_pushboolean(L, env==NULL || env->detached);
	return 1;
}

static int puss_lua_thread_wait(lua_State* L) {
	QEnv* env = NULL;
	uint32_t wait_time = (uint32_t)luaL_checkinteger(L, 1);
	if( puss_lua_get(L, PUSS_KEY_THREAD_ENV)==LUA_TUSERDATA )
		env = *(QEnv**)lua_touserdata(L, -1);
	lua_pop(L, 1);
	if( !env )
		return 0;
	if( env->detached )
		return 0;
	if( env->head ) {
		lua_pushboolean(L, 1);
		return 1;
	}

#ifdef _WIN32
	WaitForSingleObject(env->ev, wait_time);
#else
	puss_mutex_lock(&env->mutex);
	if( env->head==NULL ) {
		puss_pthread_cond_timedwait(&env->mutex, &env->cond, wait_time);
	}
	puss_mutex_unlock(&env->mutex);
#endif

	lua_pushboolean(L, env->head!=NULL);
	return 1;
}

static int puss_lua_thread_dispatch(lua_State* L) {
	QEnv* env = NULL;
	TMsg* msg = NULL;
	int res = LUA_OK;
	luaL_checktype(L, 1, LUA_TFUNCTION);	// error handle
	if( puss_lua_get(L, PUSS_KEY_THREAD_ENV)==LUA_TUSERDATA )
		env = *(QEnv**)lua_touserdata(L, -1);
	lua_pop(L, 1);
	if( !env )
		return 0;
	msg = qenv_pop(env);
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
	{ {"thread_owner", NULL}
	, {"thread_create", puss_lua_thread_create}
	, {"thread_detached", puss_lua_thread_detached}
	, {"thread_wait", puss_lua_thread_wait}
	, {"thread_dispatch", puss_lua_thread_dispatch}
	, {NULL, NULL}
	};

static int thread_env_gc(lua_State* L) {
	QEnv** ud = (QEnv**)lua_touserdata(L, 1);
	QEnv* env = *ud;
	*ud = NULL;
	qenv_unref(env);
	return 0;
}

static int thread_env_create(lua_State* L) {
	QEnv* env = (QEnv*)malloc(sizeof(QEnv));
	QEnv** ud;
	ud = (QEnv**)lua_newuserdata(L, sizeof(QEnv*));
	if( luaL_newmetatable(L, PUSS_NAME_THREADENV_MT) ) {
		lua_pushcfunction(L, thread_env_gc);
		lua_setfield(L, -2, "__gc");
	}
	lua_setmetatable(L, -2);

	memset(env, 0, sizeof(QEnv));
#ifdef _WIN32
	env->ev = CreateEventA(NULL, TRUE, FALSE, NULL);
#else
	pthread_cond_init(&env->cond, NULL);
#endif
	puss_mutex_init(&env->mutex);

	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	env->L = lua_tothread(L, -1);
	lua_pop(L, 1);
	*ud = qenv_ref(env);
	__puss_config__.state_set_key(L, PUSS_KEY_THREAD_ENV);
	return 0;
}

void puss_reg_thread_service(lua_State* L) {
	int top = lua_gettop(L);
	int ret = puss_lua_get(L, PUSS_KEY_THREAD_ENV);
	if( ret!=LUA_TUSERDATA ) {
		lua_pop(L, 1);
		puss_lua_get(L, PUSS_KEY_ERROR_HANDLE);
		lua_pushcfunction(L, thread_env_create);
		lua_pcall(L, 0, 0, top);
	}
	lua_settop(L, top);
	luaL_setfuncs(L, thread_service_methods, 0);
}
