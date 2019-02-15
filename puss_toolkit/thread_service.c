// thread_service.c

#include "puss_toolkit.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

#include "thread_utils.h"

#define PUSS_NAME_QHANDLE_MT	"_@PussQHandle@_"
#define PUSS_NAME_THANDLE_MT	"_@PussTHandle@_"

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

typedef struct _QHandle {
	TQueue*			q;
} QHandle;

typedef struct _THandle {
	PussThreadID	tid;
} THandle;

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

static int tqueue_gc(lua_State* L) {
	QHandle* ud = (QHandle*)luaL_checkudata(L, 1, PUSS_NAME_QHANDLE_MT);
	TQueue* q = ud->q;
	if( ud->tid ) {
		puss_thread_detach(ud->tid);
		ud->tid = 0;
	}
	ud->q = NULL;
	queue_unref(q);
	return 0;
}

static int tqueue_refcount(lua_State* L) {
	QHandle* ud = (QHandle*)luaL_checkudata(L, 1, PUSS_NAME_QHANDLE_MT);
	TQueue* q = ud->q;
	lua_pushinteger(L, q ? q->_ref : 0);
	return 1;
}

static int tqueue_push(lua_State* L) {
	int top = lua_gettop(L);
	size_t len = 0;
	void* pkt;
	TMsg* msg;
	QHandle* ud = (QHandle*)luaL_testudata(L, 1, PUSS_NAME_QHANDLE_MT);
	if( top < 2 )
		return luaL_error(L, "thread post bad args!");
	if( !ud->q )
		return 0;
	pkt = puss_simple_pack(&len, L, 2, -1);
	msg = (TMsg*)malloc(sizeof(TMsg) + len);
	if( !msg )
		return luaL_error(L, "no memory!");
	msg->next = NULL;
	msg->len = len;
	memcpy(msg->buf, pkt, len);
	queue_push(ud->q, msg);
	return 0;
}

static int simple_unpack_msg(lua_State* L) {
	TMsg* msg = (TMsg*)lua_touserdata(L, 1);
	puss_simple_unpack(L, msg->buf, msg->len);
	return lua_gettop(L) - 1;
}

static int tqueue_pop(lua_State* L) {
	QHandle* ud = (QHandle*)luaL_testudata(L, 1, PUSS_NAME_QHANDLE_MT);
	uint32_t wait_time = (uint32_t)luaL_optinteger(L, 2, 0);
	TMsg* msg;
	int res;
	if( !ud->q )
		return 0;
	if( (msg = queue_wait(ud->q, wait_time))==NULL )
		return 0;
	lua_settop(L, 0);
	lua_pushcfunction(L, simple_unpack_msg);
	lua_pushlightuserdata(L, msg);
	res = lua_pcall(L, 1, LUA_MULTRET, 0);
	free(msg);
	return res ? lua_error(L) : lua_gettop(L);
}

static luaL_Reg tqueue_methods[] =
	{ {"__gc", tqueue_gc}
	, {"close", tqueue_gc}
	, {"refcount", tqueue_refcount}
	, {"push", tqueue_push}
	, {"pop", tqueue_pop}
	, { NULL, NULL }
	};

static QHandle* tqueue_create(lua_State* L, TQueue* q, int ensure_queue) {
	QHandle* ud = (QHandle*)lua_newuserdata(L, sizeof(QHandle));
	memset(ud, 0, sizeof(QHandle));
	if( luaL_newmetatable(L, PUSS_NAME_QHANDLE_MT) ) {
		luaL_setfuncs(L, tqueue_methods, 0);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);

	if( !q && (ensure_queue) ) {
		q = (TQueue*)malloc(sizeof(TQueue));
		if( q ) {
			memset(q, 0, sizeof(TQueue));
#ifdef _WIN32
			q->ev = CreateEventA(NULL, TRUE, FALSE, NULL);
#else
			pthread_cond_init(&q->cond, NULL);
#endif
			puss_mutex_init(&q->mutex);
		}
	}
	ud->q = queue_ref(q);
	return ud;
}

static PUSS_THREAD_DECLARE(thread_main_wrapper, arg) {
	lua_State* L = (lua_State*)arg;

	// thread main
	assert( lua_isfunction(L, 1) );
	assert( lua_isfunction(L, 2) );
	lua_pcall(L, lua_gettop(L)-2, 0, 1);

	// close state
	lua_close(L);
	return 0;
}

static int puss_lua_queue_create(lua_State* L) {
	return tqueue_create(L, NULL, 1)->q ? 1 : luaL_error(L, "tqueue_create failed!");
}

static int thread_close(lua_State* L) {
	THandle* ud = (THandle*)luaL_checkudata(L, 1, PUSS_NAME_THANDLE_MT);
	if( ud->tid ) {
		puss_thread_detach(ud->tid);
		ud->tid = 0;
	}
	return 0;
}

static int thread_join(lua_State* L) {
}

static luaL_Reg tqueue_methods[] =
	{ {"__gc", thread_close}
	, {"join", thread_join}
	, {"close", thread_close}
	, { NULL, NULL }
	};

#define THREAD_ARGS_MAX	32

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

static void targs_build(lua_State* L, TArg* a, int n) {
	int i;
	for( i=2; i<=n; ++i ) {
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
			a->q = ((QHandle*)luaL_checkudata(L, i, PUSS_NAME_QHANDLE_MT))->q;
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

static int thread_prepare(lua_State* L) {
	TArg* args = (TArg*)lua_touserdata(L, 1);
	lua_settop(L, 0);
	puss_lua_get(L, PUSS_KEY_ERROR_HANDLE);
	assert( args->type==LUA_TSTRING );
	puss_get_value(L, args->s);
	targs_parse(L, ++args);
	return lua_gettop(L);
}

static int puss_lua_thread_create(lua_State* L) {
	QHandle* tq = luaL_testudata(L, 1, PUSS_NAME_QHANDLE_MT);
	lua_State* new_state;
	QHandle* ud;
	TArg args[THREAD_ARGS_MAX];
	int n = lua_gettop(L);
	if( n > THREAD_ARGS_MAX )
		luaL_error(L, "too many args!");
	luaL_checktype(L, 2, LUA_TSTRING);
	memset(args, 0, sizeof(args));
	targs_build(L, args, n);
	ud = tqueue_create(L, tq->q, 0);
	new_state = puss_lua_newstate();
	if( !new_state )
		return luaL_error(L, "new state failed!");

	lua_settop(new_state, 0);
	lua_pushcfunction(new_state, thread_prepare);
	lua_pushlightuserdata(new_state, args);
	if( lua_pcall(new_state, 1, LUA_MULTRET, 0) ) {
		queue_unref(ud->q);
		ud->q = NULL;
		lua_close(new_state);
		return luaL_error(L, "new state prepare failed!");
	}

	if( !puss_thread_create(&(ud->tid), thread_main_wrapper, new_state) ) {
		queue_unref(ud->q);
		ud->q = NULL;
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
