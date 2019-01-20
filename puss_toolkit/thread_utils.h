// thread_utils.h

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <process.h>

	typedef CRITICAL_SECTION	PussMutex;
	typedef HANDLE				PussThreadID;
	typedef unsigned(__stdcall* PussThreadFun)(void* arg);
	#define KS_THREAD_DECLARE(fun, arg) unsigned __stdcall func(void* arg)

	static inline void puss_mutex_init(PussMutex* mutex)	{ InitializeCriticalSection(mutex); }
	static inline void puss_mutex_uninit(PussMutex* mutex)	{ DeleteCriticalSection(mutex); }
	static inline void puss_mutex_lock(PussMutex* mutex)	{ EnterCriticalSection(mutex); }
	static inline void puss_mutex_unlock(PussMutex* mutex)	{ LeaveCriticalSection(mutex); }

	static inline int puss_thread_create(PussThreadID* ptid, PussThreadFun fun, void* arg) {
		*ptid = (PussThreadID)_beginthreadex(NULL, 0, fun, arg, 0, NULL);
		return (*ptid) != 0;
	}

	static inline void puss_thread_destroy(PussThreadID ptid) { CloseHandle(ptid); }
#else
	#include <stdlib.h>
	#include <pthread.h>
	#include <stdint.h>
	#include <unistd.h>
	#include <time.h>
	#include <sys/time.h>

	typedef pthread_mutex_t		PussMutex;
	typedef pthread_t			PussThreadID;
	typedef void* (*PussThreadFun)(void* arg);
	#define KS_THREAD_DECLARE(func, arg) void* func(void* arg)

	static inline void puss_mutex_init(PussMutex* mutex) {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ADAPTIVE_NP);	
		pthread_mutex_init(mutex, &attr);
	}

	static inline void puss_mutex_uninit(PussMutex* mutex)	{ pthread_mutex_destroy(mutex); }
	static inline void puss_mutex_lock(PussMutex* mutex)	{ pthread_mutex_lock(mutex); }
	static inline void puss_mutex_unlock(PussMutex* mutex)	{ pthread_mutex_unlock(mutex); }

	static inline int puss_pthread_wait_cond(PussMutex* lock, pthread_cond_t* cond)
		{ return pthread_cond_wait(cond, lock)==0; }

	static inline int puss_pthread_wait_cond_timed(PussMutex* lock, pthread_cond_t* cond, uint32_t wait_time_ms) {
		struct timespec timeout;
		uint64_t ns = wait_time_ms;
		clock_gettime(CLOCK_REALTIME, &timeout);
		ns = ns * 1000 + timeout.tv_nsec;
		timeout.tv_sec += ns / 1000000000;
		timeout.tv_nsec = ns % 1000000000;
		return pthread_cond_timedwait(cond, lock, &timeout)!=ETIMEDOUT;
	}

	static inline int puss_thread_create(PussThreadID* ptid, PussThreadFun func, void* arg) {
		int ret = pthread_create(ptid, NULL, func, arg);
		return ret==0;
	}

	static inline void puss_thread_destroy(PussThreadID ptid) {
		void* ret = NULL;
		pthread_join(ptid, &ret);
	}
#endif
