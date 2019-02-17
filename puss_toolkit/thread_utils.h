// thread_utils.h

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <process.h>

	typedef CRITICAL_SECTION	PussMutex;
	typedef HANDLE				PussThreadID;
	typedef unsigned(__stdcall* PussThreadFun)(void* arg);
	#define PUSS_THREAD_DECLARE(fun, arg) unsigned __stdcall fun(void* arg)

	#define puss_mutex_init		InitializeCriticalSection
	#define puss_mutex_uninit	DeleteCriticalSection
	#define puss_mutex_lock		EnterCriticalSection
	#define puss_mutex_unlock	LeaveCriticalSection

	static inline int puss_thread_create(PussThreadID* ptid, PussThreadFun fun, void* arg) {
		*ptid = (PussThreadID)_beginthreadex(NULL, 0, fun, arg, 0, NULL);
		return (*ptid) != 0;
	}

	#define puss_thread_detach	CloseHandle
	#define puss_thread_self	GetCurrentThread
#else
	#include <stdlib.h>
	#include <pthread.h>
	#include <stdint.h>
	#include <unistd.h>
	#include <errno.h>
	#include <time.h>
	#include <sys/time.h>
	#include <sys/signal.h>

	typedef pthread_mutex_t		PussMutex;
	typedef pthread_t			PussThreadID;
	typedef void* (*PussThreadFun)(void* arg);
	#define PUSS_THREAD_DECLARE(fun, arg) void* fun(void* arg)

	static inline void puss_mutex_init(PussMutex* mutex) {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ADAPTIVE_NP);	
		pthread_mutex_init(mutex, &attr);
	}

	#define puss_mutex_uninit	pthread_mutex_destroy
	#define puss_mutex_lock		pthread_mutex_lock
	#define puss_mutex_unlock	pthread_mutex_unlock

	static inline int puss_thread_create(PussThreadID* ptid, PussThreadFun fun, void* arg) {
		int ret = pthread_create(ptid, NULL, fun, arg);
		return ret==0;
	}

	#define puss_thread_detach	pthread_detach
	#define puss_thread_self	pthread_self
#endif
