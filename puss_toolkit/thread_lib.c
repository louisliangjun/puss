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

typedef struct _QHandle {
	TQueue*			q;
#ifdef _WIN32
	TQueue*			tq;
#endif
} QHandle;

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

static int do_thread_event(lua_State* L) {
	TMsg* msg = (TMsg*)lua_touserdata(L, 1);
	int top;
	lua_settop(L, 0);
	puss_lua_get(L, PUSS_KEY_THREAD_EVENT_HANDLE);
	puss_simple_unpack(L, msg->buf, msg->len);
	top = lua_gettop(L);
	if( lua_isfunction(L, 1) ) {
		lua_call(L, top-1, 0);
		return 0;
	}

	if( lua_type(L, 2)==LUA_TSTRING ) {
		puss_get_value(L, lua_tostring(L, 2));
		lua_replace(L, 2);
	} else {
		lua_pushglobaltable(L);
		lua_replace(L, 1);
		lua_pushvalue(L, 2);
		lua_gettable(L, 1);
		lua_replace(L, 2);
	}
	lua_call(L, top-2, 0);
	return 0;
}

static void thread_signal_handle(TQueue* tq, lua_State* L) {
	while( tq->head ) {
		TMsg* msg = queue_pop(tq);
		if( !msg )
			break;
		if( L ) {
			int top = lua_gettop(L);
			puss_lua_get(L, PUSS_KEY_ERROR_HANDLE);
			lua_pushcfunction(L, do_thread_event);
			lua_pushlightuserdata(L, msg);
			lua_pcall(L, 1, 0, top+1);
			lua_settop(L, top);
		}
		free(msg);
	}
}

#ifndef _WIN32
static __thread TQueue* thread_Q;
static __thread lua_State* thread_L;

static void thread_queue_signal(int sig) {
	// fprintf(stderr, "recv signal: %d\n", sig);
	if( thread_L ) {
		assert( thread_Q );
		thread_signal_handle(thread_Q, thread_L);	
	}
}
#endif

static int tqueue_close(lua_State* L) {
	QHandle* ud = (QHandle*)luaL_checkudata(L, 1, PUSS_NAME_QUEUE_MT);
	ud->q = queue_unref(ud->q);
#ifdef _WIN32
	ud->tq = queue_unref(ud->tq);
#endif
	return 0;
}

static int tqueue_push(lua_State* L) {
	QHandle* ud = (QHandle*)luaL_checkudata(L, 1, PUSS_NAME_QUEUE_MT);
	lua_pushboolean(L, queue_pack_push(L, ud->q, 2));
	return 1;
}

static int simple_unpack_msg(lua_State* L) {
	TMsg* msg = (TMsg*)lua_touserdata(L, 1);
	puss_simple_unpack(L, msg->buf, msg->len);
	return lua_gettop(L) - 1;
}

static int tqueue_pop(lua_State* L) {
	QHandle* ud = (QHandle*)luaL_checkudata(L, 1, PUSS_NAME_QUEUE_MT);
	uint32_t wait_time = (uint32_t)luaL_optinteger(L, 2, 0);
	TQueue* q = ud->q;
#ifdef _WIN32
	TQueue* tq = ud->tq;
#else
	TQueue* tq = thread_Q
#endif
	TMsg* msg = NULL;
	int res;
	if( tq ) thread_signal_handle(tq, L);
	if( !q ) return 0;

#ifdef _WIN32
	if( ((msg = queue_pop(q))==NULL) && (wait_time > 0) ) {
		if( tq ) {
			HANDLE evs[2] = { tq->ev, q->ev };	// wait for tq & q
			switch( WaitForMultipleObjects(2, evs, FALSE, wait_time) ) {
			case WAIT_OBJECT_0:
				thread_signal_handle(tq, L);
				break;
			case WAIT_OBJECT_0 + 1:
				break;
			default:
				return 0;
			}
		} else if( WaitForSingleObject(q->ev, wait_time)!=WAIT_OBJECT_0 ) {
			return 0;
		}
		msg = queue_pop(q);
	}
#else
	puss_mutex_lock(&q->mutex);
	if( (msg = q->head) != NULL ) {
		q->head = msg->next;
		msg->next = NULL;
	} else if( wait_time > 0 ) {
		struct timespec timeout;
		uint64_t ns = wait_time;
		clock_gettime(CLOCK_REALTIME, &timeout);
		ns = ns * 1000000 + timeout.tv_nsec;
		timeout.tv_sec += ns / 1000000000;
		timeout.tv_nsec = ns % 1000000000;
		thread_L = L;
		pthread_cond_timedwait(&q->cond, &q->mutex, &timeout);
		thread_L = NULL;
		if( (msg = q->head) != NULL ) {
			q->head = msg->next;
			msg->next = NULL;
		}
	}
	puss_mutex_unlock(&q->mutex);
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

static QHandle* tqueue_create(lua_State* L, TQueue* q, int ensure_queue) {
	QHandle* ud = (QHandle*)lua_newuserdata(L, sizeof(QHandle));
	ud->q = NULL;
	ud->tq = 0;
	if( luaL_newmetatable(L, PUSS_NAME_QUEUE_MT) ) {
		luaL_setfuncs(L, tqueue_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);

	if( !q && (ensure_queue) )
		q = queue_new();
	ud->q = queue_ref(q);
	return ud;
}

static PUSS_THREAD_DECLARE(thread_main_wrapper, arg) {
	lua_State* L = (lua_State*)arg;
	// fprintf(stderr, "thread start: %p\n", L);
	assert( lua_isfunction(L, 1) );
	assert( lua_isfunction(L, 2) );

#ifdef _WIN32
	lua_pop(L, 1);	// pop tq lightuserdata
	lua_pcall(L, lua_gettop(L)-2, 0, 1);	// thread main
#else
	thread_Q = queue_ref(lua_touserdata(L, -1));
	if( thread_Q )
		signal(SIGUSR1, thread_signal_handle);
	lua_pop(L, 1);	// pop tq lightuserdata
	lua_pcall(L, lua_gettop(L)-2, 0, 1);	// thread main
	thread_Q = queue_unref(thread_Q);
#endif
	lua_close(L);
	// fprintf(stderr, "thread exit: %p\n", L);
	return 0;
}

static int puss_lua_queue_create(lua_State* L) {
	return tqueue_create(L, NULL, 1)->q ? 1 : luaL_error(L, "tqueue_create failed!");
}

static int thread_detach(lua_State* L) {
	THandle* ud = (THandle*)luaL_checkudata(L, 1, PUSS_NAME_THREAD_MT);
	if( ud->q ) {
		ud->q->_detached = 1;
		ud->q = queue_unref(ud->q);
	}
	if( ud->tid ) {
		puss_thread_detach(ud->tid);
		ud->tid = 0;
	}
	return 0;
}

static int thread_join(lua_State* L) {
	THandle* ud = (THandle*)luaL_checkudata(L, 1, PUSS_NAME_THREAD_MT);
	if( ud->q ) {
		ud->q->_detached = 1;
		ud->q = queue_unref(ud->q);
	}
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
	if( ok && ud->tid ) {
		pthread_kill(ud->tid, SIGUSR1);
		return 1;
	}
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
			break;
		case LUA_TUSERDATA:
			a->q = ((QHandle*)luaL_checkudata(L, i, PUSS_NAME_QUEUE_MT))->q;
			break;
		default:
			luaL_error(L, "not support arg type(%s)", lua_typename(L, a->type));
			break;
		}
		++a;
	}
	a->type = LUA_TNONE;
}

static void targs_parse(lua_State* L
#ifdef _WIN32
	, TQueue* tq
#endif
	, const TArg* a)
{
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
			tqueue_create(L, a->q, 0)
#ifdef _WIN32
				->tq = queue_ref(tq)
#endif
			;
			break;
		default:
			break;
		}
	}
}

static int thread_wait(lua_State* L) {
	QHandle* ud = (QHandle*)lua_touserdata(L, lua_upvalueindex(1));
	lua_Integer ms = luaL_optinteger(L, 1, 0);
	if( ud->q ) {
		thread_signal_handle(ud->q, L);
#ifdef _WIN32
		if( (ms > 0) && WaitForSingleObject(ud->q->ev, (DWORD)ms)==WAIT_OBJECT_0 )
			thread_signal_handle(ud->q, L);
#else
		if( ms > 0 ) {
			thread_L = L;
			usleep(ms*1000);
			thread_L = NULL;
		}
#endif
	}
	lua_pushboolean(L, ud->q ? ud->q->_detached : 1);
	return 1;
}

static int thread_signal(lua_State* L) {
	luaL_checkany(L, 1);
	lua_settop(L, 1);
	__puss_config__.state_set_key(L, PUSS_KEY_THREAD_EVENT_HANDLE);
	return 0;
}

static int thread_prepare(lua_State* L) {
	TArg* args = (TArg*)lua_touserdata(L, 1);
	TQueue* tq = (TQueue*)lua_touserdata(L, 2);
	if( tq ) {
		puss_lua_get(L, PUSS_KEY_PUSS);
		tqueue_create(L, tq, 0);
		lua_pushcclosure(L, thread_wait, 1);
		lua_setfield(L, -2, "thread_wait");
		lua_pushcfunction(L, thread_signal);
		lua_setfield(L, -2, "thread_signal");
	}
	lua_settop(L, 0);
	puss_lua_get(L, PUSS_KEY_ERROR_HANDLE);
	assert( args->type==LUA_TSTRING );
	puss_get_value(L, args->s);
#ifdef _WIN32
	targs_parse(L, tq, ++args);
#else
	targs_parse(L, ++args);
	lua_pushlightuserdata(L, tq);
#endif
	lua_pushlightuserdata(L, tq);
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

void puss_reg_thread_service(lua_State* L) {
	luaL_setfuncs(L, thread_service_methods, 0);
}
