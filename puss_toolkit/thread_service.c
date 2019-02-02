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
	PussMutex		mutex;		// NOTICE: alway lock self mutex before lock owner mutex !!!

	QEnv*			owner;
	QEnv*			sibling;
	QEnv*			children;

	lua_State*		L;
	int				detached;

	// task queue
#ifdef _WIN32
	HANDLE			ev;
#else
	pthread_cond_t	cond;
#endif
	TMsg*			head;
	TMsg*			tail;
	TMsg			detach;
};

typedef struct _PussLuaThread {
	PussThreadID	tid;
	QEnv*			env;
} PussLuaThread;

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
		if( res==&q->detach ) {
			assert( q->owner==NULL );
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
	if( env ) {
		QEnv* owner;
		int need_event = 0;
		puss_mutex_lock(&env->mutex);
		owner = env->owner;
		if( owner ) {
			QEnv* p = owner->children;
			if( p==env ) {
				owner->children = p->sibling;
			} else {
				for( ; p; p=p->sibling ) {
					if( p->sibling==env ) {
						p->sibling = env->sibling;
						break;
					}
				}
			}
			env->owner = NULL;
			env->sibling = NULL;
			task_queue_push(env, &env->detach);
			need_event = 1;
		}
		puss_mutex_unlock(&env->mutex);
		if( need_event ) {
#ifdef _WIN32
			SetEvent(env->ev);
#else
			pthread_cond_signal(&env->cond);
#endif
		}
	}

	if( self->tid ) {
		puss_thread_detach(self->tid);
		self->tid = 0;
	}
	return 0;
}

static int thread_post(lua_State* L) {
	PussLuaThread* self = (PussLuaThread*)luaL_checkudata(L, 1, PUSS_NAME_THREAD_MT);
	QEnv* q = self->env;
	int top = lua_gettop(L);
	size_t len = 0;
	void* pkt;
	TMsg* msg;
	int need_event;
	if( !q )
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

	puss_mutex_lock(&q->mutex);
	if( q->detached ) {
		self->env = NULL;
		free(msg);
	} else {
		task_queue_push(q, msg);
		need_event = (q->head==msg);
	}
	puss_mutex_unlock(&q->mutex);

	if( need_event ) {
#ifdef _WIN32
		SetEvent(q->ev);
#else
		pthread_cond_signal(&q->cond);
#endif
	}
	return 0;
}

static luaL_Reg thread_methods[] =
	{ {"__gc", thread_detach}
	, {"detach", thread_detach}
	, {"post", thread_post}
	, {NULL, NULL}
	};

static PUSS_THREAD_DECLARE(thread_main_wrapper, arg) {
	QEnv* env = (QEnv*)arg;
	lua_State* L = env->L;

	// thread main
	assert( lua_isfunction(L, 1) );
	assert( lua_isfunction(L, 2) );
	lua_pcall(L, lua_gettop(L)-2, 0, 1);

	// detach q from L
	if( lua_rawgetp(L, LUA_REGISTRYINDEX, env)==LUA_TUSERDATA ) {
		QEnv** ud = (QEnv**)lua_touserdata(L, -1);
		*ud = NULL;
	}

	// close state
	lua_close(L);
	assert( env->children==NULL );	// assert children threads detached

	// wait detach signal if self not detached
	while( !env->detached ) {
		TMsg* msg = qenv_pop(env);
		if( msg )	free(msg);
	}
	assert( env->owner==NULL );
	assert( env->sibling==NULL );
#ifdef _WIN32
	CloseHandle(env->ev);
#else
	pthread_cond_destroy(&env->cond);
#endif
	puss_mutex_uninit(&env->mutex);
	free(env);
	return 0;
}

static int puss_lua_thread_create(lua_State* L) {
	PussLuaThread* ud;
	QEnv* env = NULL;
	lua_State* new_state = NULL;
	QEnv* new_env = NULL;
	lua_CFunction thread_main = lua_iscfunction(L, 1) ? lua_tocfunction(L, 1) : NULL;
	size_t size = 0;
	void* args = puss_simple_pack(&size, L, thread_main ? 2 : 1, -1);
	ud = (PussLuaThread*)lua_newuserdata(L, sizeof(PussLuaThread));
	memset(ud, 0, sizeof(PussLuaThread));

	if( luaL_newmetatable(L, PUSS_NAME_THREAD_MT) ) {
		luaL_setfuncs(L, thread_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);

	new_state = puss_lua_newstate();
	lua_settop(new_state, 0);
	puss_lua_get(new_state, PUSS_KEY_ERROR_HANDLE);
	if( thread_main ) {
		lua_pushcfunction(new_state, thread_main);
	} else {
		puss_lua_get(new_state, PUSS_KEY_PUSS);
		lua_getfield(new_state, -1, "dofile");
		lua_replace(new_state, -2);
	}
	puss_simple_unpack(new_state, args, size);

	if( puss_lua_get(new_state, PUSS_KEY_THREAD_ENV)==LUA_TLIGHTUSERDATA )
		new_env = (QEnv*)lua_touserdata(new_state, -1);
	lua_pop(new_state, 1);
	if( !new_env ) {
		if( new_state ) lua_close(new_state);
		return luaL_error(L, "no memory!");
	}

	if( !puss_thread_create(&(ud->tid), thread_main_wrapper, new_env) ) {
		lua_close(new_state);
		return luaL_error(L, "create thread failed!");
	}
	ud->env = new_env;

	if( puss_lua_get(L, PUSS_KEY_THREAD_ENV)==LUA_TLIGHTUSERDATA )
		env = (QEnv*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	if( env ) {
		puss_mutex_lock(&new_env->mutex);
		new_env->owner = env;
		new_env->sibling = env->children;
		env->children = new_env;
		puss_mutex_unlock(&new_env->mutex);
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
	if( puss_lua_get(L, PUSS_KEY_THREAD_ENV)==LUA_TLIGHTUSERDATA )
		env = (QEnv*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	lua_pushboolean(L, env==NULL || env->detached);
	return 1;
}

static int puss_lua_thread_wait(lua_State* L) {
	QEnv* env = NULL;
	uint32_t wait_time = (uint32_t)luaL_checkinteger(L, 1);
	if( puss_lua_get(L, PUSS_KEY_THREAD_ENV)==LUA_TLIGHTUSERDATA )
		env = (QEnv*)lua_touserdata(L, -1);
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
	if( puss_lua_get(L, PUSS_KEY_THREAD_ENV)==LUA_TLIGHTUSERDATA )
		env = (QEnv*)lua_touserdata(L, -1);
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

static int puss_lua_thread_post_owner(lua_State* L) {
	QEnv* env = NULL;
	TMsg* msg = NULL;
	size_t len = 0;
	void* pkt;
	QEnv* owner;
	if( puss_lua_get(L, PUSS_KEY_THREAD_ENV)==LUA_TLIGHTUSERDATA )
		env = (QEnv*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	if( !env )
		return 0;
	if( !(env->owner) ) {
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

	puss_mutex_lock(&env->mutex);
	if( (owner = env->owner) != NULL ) {
		qenv_push(owner, msg);
		msg = NULL;
	}
	puss_mutex_unlock(&env->mutex);

	lua_pushboolean(L, msg==NULL);	// owner not exist, maybe detached
	if(msg)	free(msg);
	return 1;
}

static int puss_lua_thread_post_self(lua_State* L) {
	QEnv* env = NULL;
	TMsg* msg = NULL;
	size_t len = 0;
	void* pkt;
	if( puss_lua_get(L, PUSS_KEY_THREAD_ENV)==LUA_TLIGHTUSERDATA )
		env = (QEnv*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	if( !env )
		return 0;
	pkt = puss_simple_pack(&len, L, 1, -1);
	msg = (TMsg*)malloc(sizeof(TMsg) + len);
	if( !msg )
		return luaL_error(L, "no memory!");
	msg->next = NULL;
	msg->len = len;
	memcpy(msg->buf, pkt, len);
	qenv_push(env, msg);
	lua_pushboolean(L, 1);
	return 1;
}

static luaL_Reg thread_service_methods[] =
	{ {"thread_create", puss_lua_thread_create}
	, {"thread_detached", puss_lua_thread_detached}
	, {"thread_wait", puss_lua_thread_wait}
	, {"thread_dispatch", puss_lua_thread_dispatch}
	, {"thread_post_owner", puss_lua_thread_post_owner}
	, {"thread_post_self", puss_lua_thread_post_self}
	, {NULL, NULL}
	};

static int thread_env_gc(lua_State* L) {
	QEnv** ud = (QEnv**)lua_touserdata(L, 1);
	QEnv* env = *ud;
	*ud = NULL;
	if( !env )
		return 0;
	lua_pushlightuserdata(L, NULL);
	__puss_config__.state_set_key(L, PUSS_KEY_THREAD_ENV);

	// destroy thread MUST after detached
	assert( env->owner==NULL );

	if( env->children ) {
		QEnv* p = env->children;
		env->children = NULL;
		while( p ) {
			QEnv* t = p;
			puss_mutex_lock(&t->mutex);
			p = t->sibling;
			t->owner = NULL;
			t->sibling = NULL;
			puss_mutex_unlock(&t->mutex);
		}
	}

#ifdef _WIN32
	CloseHandle(env->ev);
#else
	pthread_cond_destroy(&env->cond);
#endif
	puss_mutex_uninit(&env->mutex);
	free(env);
	return 0;
}

static int thread_env_create(lua_State* L) {
	QEnv* env = (QEnv*)malloc(sizeof(QEnv));
	QEnv** ud = (QEnv**)lua_newuserdata(L, sizeof(QEnv*));
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
	*ud = env;
	lua_rawsetp(L, LUA_REGISTRYINDEX, env);	// REGISTER[env] = ud;

	lua_pushlightuserdata(L, env);
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
