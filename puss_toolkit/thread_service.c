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
	PussMutex		mutex;			// NOTICE: alway lock self mutex before lock owner mutex !!!

	QEnv*			qenv_owner;		// owner lua state qenv
	QEnv*			qenv_children;	// list head
	QEnv*			qenv_sibling;	// list next

	lua_State*		L;
	int				detached;

	// task queue
#ifdef _WIN32
	HANDLE			ev;
#else
	pthread_cond_t	cond;
#endif
	size_t			size;
	TMsg*			head;
	TMsg*			tail;
	TMsg			detach;
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
	puss_mutex_init(&q->mutex);
}

static void qenv_uninit(QEnv* q) {
#ifdef _WIN32
	CloseHandle(q->ev);
#else
	pthread_cond_destroy(&q->cond);
#endif
	puss_mutex_uninit(&q->mutex);
}

static void qenv_push(QEnv* q, TMsg* task) {
	int need_event;
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
	int need_event;
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

static int thread_detach(lua_State* L) {
	PussLuaThread* self = (PussLuaThread*)luaL_checkudata(L, 1, PUSS_NAME_THREAD_MT);
	QEnv* q = self->env;
	if( q ) {
		QEnv* owner;
		int need_event = 0;
		puss_mutex_lock(&q->mutex);
		owner = q->qenv_owner;
		if( owner ) {
			QEnv* p = owner->qenv_children;
			if( p==q ) {
				owner->qenv_children = p->qenv_sibling;
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
			task_queue_push(q, &(q->detach));
			need_event = 1;
		}
		puss_mutex_unlock(&q->mutex);
		if( need_event ) {
#ifdef _WIN32
			SetEvent(q->ev);
#else
			pthread_cond_signal(&q->cond);
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
		return luaL_error(L, "thread already detached!");
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
	q->size++;
	task_queue_push(q, msg);
	need_event = (q->head==msg);
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
	QEnv* q = (QEnv*)arg;
	lua_State* L = q->L;

	// thread main
	assert( lua_isfunction(L, 1) );
	assert( lua_isfunction(L, 2) );
	lua_pcall(L, lua_gettop(L)-2, 0, 1);

	// detach q from L
	if( lua_rawgetp(L, LUA_REGISTRYINDEX, q)==LUA_TUSERDATA ) {
		QEnv** ud = (QEnv**)lua_touserdata(L, -1);
		*ud = NULL;
	}

	// close state
	lua_close(L);
	assert( q->qenv_children==NULL );	// assert children threads detached

	// wait detach signal if self not detached
	if( !(q->detached) ) {
		TMsg* msg = NULL;
		for( ; msg!=&(q->detach); msg=qenv_bpop(q, 0xFFFFFFFF) )
			free(msg);
		assert( msg==&(q->detach) );
		assert( !(msg->next) );
	}
	assert( q->qenv_owner==NULL );
	assert( q->qenv_sibling==NULL );
	qenv_uninit(q);
	free(q);
	return 0;
}

static int puss_lua_thread_create(lua_State* L) {
	PussLuaThread* ud;
	QEnv* self_env = NULL;
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
		self_env = (QEnv*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	if( self_env ) {
		puss_mutex_lock(&new_env->mutex);
		new_env->qenv_owner = self_env;
		new_env->qenv_sibling = self_env->qenv_children;
		self_env->qenv_children = new_env;
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
	QEnv* self_env = NULL;
	if( puss_lua_get(L, PUSS_KEY_THREAD_ENV)==LUA_TLIGHTUSERDATA )
		self_env = (QEnv*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	lua_pushboolean(L, self_env==NULL || self_env->detached);
	return 1;
}

static int puss_lua_thread_dispatch(lua_State* L) {
	QEnv* self_env = NULL;
	TMsg* msg = NULL;
	int res = LUA_OK;
	luaL_checktype(L, 1, LUA_TFUNCTION);	// error handle
	if( puss_lua_get(L, PUSS_KEY_THREAD_ENV)==LUA_TLIGHTUSERDATA )
		self_env = (QEnv*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	if( !self_env )
		return 0;
	if( self_env->detached )
		return 0;
	msg = qenv_pop(self_env);
	if( !msg )
		return 0;
	if( msg==&(self_env->detach) ) {
		assert( self_env->qenv_owner==NULL );
		self_env->detached = 1;
		return 0;
	}

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
	QEnv* self_env = NULL;
	TMsg* msg = NULL;
	size_t len = 0;
	void* pkt;
	QEnv* owner;
	if( puss_lua_get(L, PUSS_KEY_THREAD_ENV)==LUA_TLIGHTUSERDATA )
		self_env = (QEnv*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	if( !self_env )
		return 0;
	if( !(self_env->qenv_owner) ) {
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

	puss_mutex_lock(&self_env->mutex);
	if( (owner = self_env->qenv_owner) != NULL ) {
		qenv_push(owner, msg);
		msg = NULL;
	}
	puss_mutex_unlock(&self_env->mutex);

	lua_pushboolean(L, msg==NULL);	// owner not exist, maybe detached
	if(msg)	free(msg);
	return 1;
}

static int puss_lua_thread_post_self(lua_State* L) {
	QEnv* self_env = NULL;
	TMsg* msg = NULL;
	size_t len = 0;
	void* pkt;
	if( puss_lua_get(L, PUSS_KEY_THREAD_ENV)==LUA_TLIGHTUSERDATA )
		self_env = (QEnv*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	if( !self_env )
		return 0;
	pkt = puss_simple_pack(&len, L, 1, -1);
	msg = (TMsg*)malloc(sizeof(TMsg) + len);
	if( !msg )
		return luaL_error(L, "no memory!");
	msg->next = NULL;
	msg->len = len;
	memcpy(msg->buf, pkt, len);
	qenv_push(self_env, msg);
	lua_pushboolean(L, 1);
	return 1;
}

static luaL_Reg thread_service_methods[] =
	{ {"thread_create", puss_lua_thread_create}
	, {"thread_detached", puss_lua_thread_detached}
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
	assert( env->qenv_owner==NULL );

	if( env->qenv_children ) {
		QEnv* p = env->qenv_children;
		env->qenv_children = NULL;
		while( p ) {
			QEnv* t = p;
			puss_mutex_lock(&t->mutex);
			p = t->qenv_sibling;
			t->qenv_owner = NULL;
			t->qenv_sibling = NULL;
			puss_mutex_unlock(&t->mutex);
		}
	}

	qenv_uninit(env);
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
	qenv_init(env);
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
