// thread_lib.c

#include "puss_toolkit.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

#include "thread_utils.h"

#define PUSS_NAME_QUEUE_MT	"_@PussQueue@_"
#define PUSS_NAME_THREAD_MT	"_@PussThread@_"

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
	int				_detached;

	// task queue
#ifdef _WIN32
	HANDLE			ev;
#else
	pthread_cond_t	cond;
#endif
	TMsg*			head;
	TMsg*			tail;
};

typedef struct _THandle {
	PussThreadID	tid;
	TQueue*			q;
} THandle;

static TQueue* queue_new(void) {
	TQueue* q = (TQueue*)malloc(sizeof(TQueue));
	if( q ) {
		memset(q, 0, sizeof(TQueue));
#ifdef _WIN32
		q->ev = CreateEventA(NULL, TRUE, FALSE, NULL);
#else
		pthread_cond_init(&q->cond, NULL);
#endif
		puss_mutex_init(&q->mutex);
	}
	return q;
}

static TQueue* queue_ref(TQueue* q) {
	if( q ) {
		puss_mutex_lock(&q->mutex);
		++(q->_ref);
		puss_mutex_unlock(&q->mutex);
	}
	return q;
}

static TQueue* queue_unref(TQueue* q) {
	int ref;
	if( !q )
		return NULL;
	puss_mutex_lock(&q->mutex);
	ref = (--(q->_ref));
	puss_mutex_unlock(&q->mutex);
	if( ref > 0 )
		return NULL;
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
	return NULL;
}

#define task_queue_push(queue, msg)	{ \
			assert( (msg) && ((msg)->next==0) ); \
			if( (queue)->head ) \
				(queue)->tail->next = msg; \
			else \
				(queue)->head = msg; \
			(queue)->tail = msg; \
		}

static void queue_push(TQueue* q, TMsg* task) {
#ifdef _WIN32
	puss_mutex_lock(&q->mutex);
	task_queue_push(q, task);
	if( q->head==task )
		SetEvent(q->ev);
	puss_mutex_unlock(&q->mutex);
#else
	puss_mutex_lock(&q->mutex);
	task_queue_push(q, task);
	pthread_cond_signal(&q->cond);
	puss_mutex_unlock(&q->mutex);
#endif
}

static TMsg* queue_pop(TQueue* q) {
	TMsg* res = NULL;
	puss_mutex_lock(&q->mutex);
	if( (res = q->head) != NULL ) {
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

static int queue_pack_push(lua_State* L, TQueue* q, int start) {
	size_t len = 0;
	void* pkt;
	TMsg* msg;
	if( !q )
		return 0;
	pkt = puss_simple_pack(&len, L, start, -1);
	msg = (TMsg*)malloc(sizeof(TMsg) + len);
	if( !msg )
		luaL_error(L, "no memory!");
	msg->next = NULL;
	msg->len = len;
	memcpy(msg->buf, pkt, len);
	queue_push(q, msg);
	return 1;
}

#ifndef _WIN32
	#define PUSS_THREAD_SIGNAL	SIGRTMAX
	static int thread_signal_installed = 0;

	static __thread TQueue* thread_wait_queue;

	static void thread_queue_signal(int sig) {
		// fprintf(stderr, "recv signal: %d\n", sig);
		if( thread_wait_queue ) {
			pthread_cond_broadcast(&thread_wait_queue->cond);	// thread_wait_queue maybe wait by more threads, need broadcast notify self wakeup
		}
	}

	static inline void fill_timeout(struct timespec* timeout, uint32_t wait_time_ms) {
		uint64_t ns = wait_time_ms;
		clock_gettime(CLOCK_REALTIME, timeout);
		ns = ns * 1000000 + timeout->tv_nsec;
		timeout->tv_sec += ns / 1000000000;
		timeout->tv_nsec = ns % 1000000000;
	}
#endif

static int tqueue_close(lua_State* L) {
	TQueue** ud = (TQueue**)luaL_checkudata(L, 1, PUSS_NAME_QUEUE_MT);
	*ud = queue_unref(*ud);
	return 0;
}

static int tqueue_push(lua_State* L) {
	TQueue* q = *((TQueue**)luaL_checkudata(L, 1, PUSS_NAME_QUEUE_MT));
	lua_pushboolean(L, queue_pack_push(L, q, 2));
	return 1;
}

static int simple_unpack_msg(lua_State* L) {
	TMsg* msg = (TMsg*)lua_touserdata(L, 1);
	puss_simple_unpack(L, msg->buf, msg->len);
	return lua_gettop(L) - 1;
}

static int tqueue_pop(lua_State* L) {
	TQueue* q = *((TQueue**)luaL_checkudata(L, 1, PUSS_NAME_QUEUE_MT));
	uint32_t wait_time = (uint32_t)luaL_optinteger(L, 2, 0);
	TMsg* msg = NULL;
	int res;
	if( !q ) return 0;

#ifdef _WIN32
	if( ((msg = queue_pop(q))==NULL) && (wait_time > 0) ) {
		if( WaitForSingleObject(q->ev, wait_time)!=WAIT_OBJECT_0 )
			return 0;
		msg = queue_pop(q);
	}
#else
	if( wait_time==0 ) {
		puss_mutex_lock(&q->mutex);
		if( (msg = q->head) != NULL ) {
			q->head = msg->next;
			msg->next = NULL;
		}
		puss_mutex_unlock(&q->mutex);
	} else {
		struct timespec timeout;
		fill_timeout(&timeout, wait_time);
		puss_mutex_lock(&q->mutex);
		while( (msg = q->head)==NULL ) {
			if( pthread_cond_timedwait(&q->cond, &q->mutex, &timeout) )
				break;	// timeout or error
		}
		if( msg ) {
			q->head = msg->next;
			msg->next = NULL;
		}
		puss_mutex_unlock(&q->mutex);
	}
#endif

	if( !msg ) return 0;

	lua_settop(L, 0);
	lua_pushcfunction(L, simple_unpack_msg);
	lua_pushlightuserdata(L, msg);
	res = lua_pcall(L, 1, LUA_MULTRET, 0);
	free(msg);
	return res ? lua_error(L) : lua_gettop(L);
}

static luaL_Reg tqueue_methods[] =
	{ {"__gc", tqueue_close}
	, {"close", tqueue_close}
	, {"push", tqueue_push}
	, {"pop", tqueue_pop}
	, { NULL, NULL }
	};

static TQueue** tqueue_create(lua_State* L, TQueue* q, int ensure_queue) {
	TQueue** ud = (TQueue**)lua_newuserdata(L, sizeof(TQueue*));
	*ud = NULL;
	if( luaL_newmetatable(L, PUSS_NAME_QUEUE_MT) ) {
		luaL_setfuncs(L, tqueue_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);

	if( !q && (ensure_queue) )
		q = queue_new();
	*ud = queue_ref(q);
	return ud;
}

static PUSS_THREAD_DECLARE(thread_main_wrapper, arg) {
	lua_State* L = (lua_State*)arg;
	// fprintf(stderr, "thread start: %p\n", L);
	assert( lua_isfunction(L, 1) );
	assert( lua_isfunction(L, 2) );
	lua_pcall(L, lua_gettop(L)-2, 0, 1);	// thread main
	lua_close(L);
	// fprintf(stderr, "thread exit: %p\n", L);
	return 0;
}

static int puss_lua_queue_create(lua_State* L) {
	return (*tqueue_create(L, NULL, 1)) ? 1 : luaL_error(L, "tqueue_create failed!");
}

static void queue_detach(THandle* ud) {
	TQueue* tq = ud->q;
	if( tq ) {
		tq->_detached = 1;
#ifdef _WIN32
		SetEvent(tq->ev);
#else
		pthread_cond_signal(&tq->cond);
		if( ud->tid )
			pthread_kill(ud->tid, PUSS_THREAD_SIGNAL);
#endif
		ud->q = queue_unref(tq);
	}
}

static int thread_detach(lua_State* L) {
	THandle* ud = (THandle*)luaL_checkudata(L, 1, PUSS_NAME_THREAD_MT);
	queue_detach(ud);
	if( ud->tid ) {
		puss_thread_detach(ud->tid);
		ud->tid = 0;
	}
	return 0;
}

static int thread_join(lua_State* L) {
	THandle* ud = (THandle*)luaL_checkudata(L, 1, PUSS_NAME_THREAD_MT);
	queue_detach(ud);
	if( ud->tid ) {
#ifdef _WIN32
		WaitForSingleObject(ud->tid, INFINITE);
#else
		void* ret = NULL;
		pthread_join(ud->tid, &ret);
#endif
		ud->tid = 0;
	}
	return 0;
}

static int thread_post(lua_State* L) {
	THandle* ud = (THandle*)luaL_checkudata(L, 1, PUSS_NAME_THREAD_MT);
	int ok = queue_pack_push(L, ud->q, 2);
#ifndef _WIN32
	if( ok && ud->tid )
		pthread_kill(ud->tid, PUSS_THREAD_SIGNAL);
#endif
	lua_pushboolean(L, ok);
	return 1;
}

static luaL_Reg thread_methods[] =
	{ {"__gc", thread_detach}
	, {"detach", thread_detach}
	, {"join", thread_join}
	, {"post", thread_post}
	, { NULL, NULL }
	};

typedef struct _TArg {
	int				type;
	size_t			len;
	union {
		int			b;
		lua_Integer	i;
		lua_Number	n;
		void*		p;
		TQueue*		q;
		const char*	s;
	};
} TArg;

static void targs_build(lua_State* L, TArg* a, int i, int n) {
	for( ; i<=n; ++i ) {
		a->type = lua_type(L, i);
		switch( lua_type(L, i) ) {
		case LUA_TNIL:
			break;
		case LUA_TBOOLEAN:
			a->b = lua_toboolean(L, i);
			break;
		case LUA_TLIGHTUSERDATA:
			a->p = lua_touserdata(L, i);
			break;	
		case LUA_TNUMBER:
			if( lua_isinteger(L, i) ) {
				a->len = 1;
				a->i = lua_tointeger(L, i);
			} else {
				a->len = 0;
				a->n = lua_tonumber(L, i);
			}
			break;
		case LUA_TSTRING:
			a->s = lua_tolstring(L, i, &a->len);
			break;
		case LUA_TTABLE:
			a->p = puss_simple_pack(&a->len, L, i, i);
			a->s = lua_pushlstring(L, (const char*)a->p, a->len);
			lua_replace(L, i);
			break;
		case LUA_TUSERDATA:
			a->q = *((TQueue**)luaL_checkudata(L, i, PUSS_NAME_QUEUE_MT));
			break;
		default:
			luaL_error(L, "not support arg type(%s)", lua_typename(L, a->type));
			break;
		}
		++a;
	}
	a->type = LUA_TNONE;
}

static void targs_parse(lua_State* L, const TArg* a) {
	for( ; a->type!=LUA_TNONE; ++a ) {
		switch( a->type ) {
		case LUA_TNIL:
			lua_pushnil(L);
			break;
		case LUA_TBOOLEAN:
			lua_pushboolean(L, a->b);
			break;
		case LUA_TLIGHTUSERDATA:
			lua_pushlightuserdata(L, a->p);
			break;	
		case LUA_TNUMBER:
			if( a->len ) {
				lua_pushinteger(L, a->i);
			} else {
				lua_pushnumber(L, a->n);
			}
			break;
		case LUA_TSTRING:
			lua_pushlstring(L, a->s, a->len);
			break;
		case LUA_TTABLE:
			puss_simple_unpack(L, a->s, a->len);
			break;
		case LUA_TUSERDATA:
			tqueue_create(L, a->q, 0);
			break;
		default:
			break;
		}
	}
}

static int do_thread_queue_event(lua_State* L) {
	TMsg* msg = (TMsg*)lua_touserdata(L, 2);
	lua_settop(L, 1);
	puss_simple_unpack(L, msg->buf, msg->len);
	lua_call(L, lua_gettop(L)-1, 0);
	return 0;
}

static int tqueue_handle_default(lua_State* L) {
	if( lua_type(L, 1)==LUA_TSTRING ) {
		puss_get_value(L, lua_tostring(L, 1));
		lua_replace(L, 1);
	} else {
		lua_pushglobaltable(L);
		lua_pushvalue(L, 1);
		lua_gettable(L, -2);
		lua_replace(L, 1);
		lua_pop(L, 1);
	}
	lua_call(L, lua_gettop(L)-1, 0);
	return 0;
}

static void thread_queue_handle(lua_State* L, int custom_handle, TMsg* msg) {
	int top = lua_gettop(L);
	puss_lua_get(L, PUSS_KEY_ERROR_HANDLE);
	lua_pushcfunction(L, do_thread_queue_event);
	if( custom_handle )
		lua_pushvalue(L, custom_handle);
	else
		lua_pushcfunction(L, tqueue_handle_default);
	lua_pushlightuserdata(L, msg);
	lua_pcall(L, 2, 0, top+1);
	free(msg);
	lua_settop(L, top);
}

static inline int check_thread_queue_handle(lua_State* L, int custom_handle, TQueue* tq) {
	TMsg* msg = tq ? queue_pop(tq) : NULL;
	if( !msg )
		return 0;
	thread_queue_handle(L, custom_handle, msg);
	return 1;
}

static int thread_wait(lua_State* L) {
	TQueue* tq = *((TQueue**)lua_touserdata(L, lua_upvalueindex(1)));
	int custom_handle = lua_isfunction(L, 1);
	int arg;
	uint32_t wait_time;
	TQueue** wait_ud;
	TQueue* wq;
	if( check_thread_queue_handle(L, custom_handle, tq) )
		goto final_label;

	arg = custom_handle ? 2 : 1;
	wait_time = (uint32_t)luaL_optinteger(L, arg++, 0);
	if( wait_time==0 )
		goto final_label;

	wait_ud = luaL_testudata(L, arg, PUSS_NAME_QUEUE_MT);
	wq = wait_ud ? *wait_ud : NULL;
	if( wq && wq->head )
		goto final_label;

#ifdef _WIN32
	if( wq ) {
		if( wq->head==NULL ) {
			if( tq ) {
				HANDLE evs[2] = { tq->ev, wq->ev };	// wait for tq & wq
				switch( WaitForMultipleObjects(2, evs, FALSE, wait_time) ) {
				case WAIT_OBJECT_0:
					check_thread_queue_handle(L, custom_handle, tq);
					break;
				default:
					break;
				}
			} else {
				WaitForSingleObject(wq->ev, wait_time);
			}
		}
	} else if( tq ) {
		if( WaitForSingleObject(tq->ev, wait_time)==WAIT_OBJECT_0 )
			check_thread_queue_handle(L, custom_handle, tq);
	} else {
		Sleep(wait_time);
	}
#else
	if( wq ) {
		puss_mutex_lock(&wq->mutex);
		if( wq->head==NULL ) {
			struct timespec timeout;
			fill_timeout(&timeout, wait_time);
			thread_wait_queue = wq;
			pthread_cond_timedwait(&wq->cond, &wq->mutex, &timeout);
			thread_wait_queue = NULL;
		}
		puss_mutex_unlock(&wq->mutex);
		check_thread_queue_handle(L, custom_handle, tq);
	} else if( tq ) {
		TMsg* msg;
		puss_mutex_lock(&tq->mutex);
		if( (msg = tq->head)==NULL ) {
			struct timespec timeout;
			fill_timeout(&timeout, wait_time);
			pthread_cond_timedwait(&tq->cond, &tq->mutex, &timeout);
			msg = tq->head;
		}
		if( msg ) {
			tq->head = msg->next;
			msg->next = NULL;
		}
		puss_mutex_unlock(&tq->mutex);
		if( msg )
			thread_queue_handle(L, custom_handle, msg);
	} else {
		usleep(wait_time*1000);
	}
#endif

final_label:
	lua_pushboolean(L, tq ? tq->_detached : 1);
	return 1;
}

static int thread_prepare(lua_State* L) {
	TArg* args = (TArg*)lua_touserdata(L, 1);
	TQueue* tq = (TQueue*)lua_touserdata(L, 2);
	if( tq ) {
		puss_lua_get(L, PUSS_KEY_PUSS);
		if( lua_getfield(L, -1, "thread_wait")==LUA_TFUNCTION ) {
			tqueue_create(L, tq, 0);
			lua_setupvalue(L, -2, 1);
		} else {
			tqueue_create(L, tq, 0);
			lua_pushcclosure(L, thread_wait, 1);
			lua_setfield(L, -2, "thread_wait");	// replace 
		}
	}
	lua_settop(L, 0);
	puss_lua_get(L, PUSS_KEY_ERROR_HANDLE);
	assert( args->type==LUA_TSTRING );
	puss_get_value(L, args->s);
	targs_parse(L, ++args);
	return lua_gettop(L);
}

#define THREAD_ARGS_MAX	32

static int puss_lua_thread_create(lua_State* L) {
	int n = lua_gettop(L);
	int enable_thread_event = lua_toboolean(L, 1);
	THandle* ud = lua_newuserdata(L, sizeof(THandle));
	lua_State* new_state;
	TArg args[THREAD_ARGS_MAX+1];
	if( n > THREAD_ARGS_MAX )
		return luaL_error(L, "too many args!");
	luaL_checktype(L, enable_thread_event ? 1 : 2, LUA_TSTRING);

	ud->tid = 0;
	ud->q = NULL;
	if( luaL_newmetatable(L, PUSS_NAME_THREAD_MT) ) {
		luaL_setfuncs(L, thread_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);

	if( enable_thread_event ) {
		TQueue* q = queue_new();
		if( !q )
			return luaL_error(L, "create thread queue failed!");
		ud->q = queue_ref(q);
	}

	memset(args, 0, sizeof(args));
	targs_build(L, args, enable_thread_event ? 1 : 2, n);

	new_state = puss_lua_newstate();
	if( !new_state )
		return luaL_error(L, "new state failed!");

	lua_settop(new_state, 0);
	lua_pushcfunction(new_state, thread_prepare);
	lua_pushlightuserdata(new_state, args);
	lua_pushlightuserdata(new_state, ud->q);
	if( lua_pcall(new_state, 2, LUA_MULTRET, 0) ) {
		lua_close(new_state);
		return luaL_error(L, "new state prepare failed!");
	}

	if( !puss_thread_create(&ud->tid, thread_main_wrapper, new_state) ) {
		lua_close(new_state);
		return luaL_error(L, "create thread failed!");
	}

	return 1;
}

static luaL_Reg thread_service_methods[] =
	{ {"queue_create", puss_lua_queue_create}
	, {"thread_create", puss_lua_thread_create}
	, {NULL, NULL}
	};

int puss_reg_thread_service(lua_State* L) {
#ifndef _WIN32
	if( !thread_signal_installed ) {
		signal(PUSS_THREAD_SIGNAL, thread_queue_signal);
		thread_signal_installed = 1;
	}
#endif
	tqueue_create(L, NULL, 0);
	lua_pushcclosure(L, thread_wait, 1);
	lua_setfield(L, -2, "thread_wait");
	luaL_setfuncs(L, thread_service_methods, 0);
	return 0;
}
