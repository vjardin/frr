// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Minimal POSIX threads on top of native Win32 primitives (FRR subset).
 *
 * pthread_t is the Win32 thread id, so it is a scalar and can be compared
 * with == like FRR does. Mutexes are SRW locks (non-recursive, like the
 * default pthread mutex), condition variables are CONDITION_VARIABLEs and
 * thread-specific keys use fiber local storage, which runs destructors on
 * thread exit.
 *
 * XXX: can we use a legacy pthread() shim for Win32 ?
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_PTHREAD_H
#define _FRR_WIN_PTHREAD_H

#include <time.h>
#include <sched.h>

typedef uintptr_t pthread_t;

typedef struct {
	int detachstate;
	size_t stacksize;
} pthread_attr_t;

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

typedef SRWLOCK pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT

typedef struct {
	int type;
} pthread_mutexattr_t;
#define PTHREAD_MUTEX_NORMAL 0
#define PTHREAD_MUTEX_DEFAULT 0
#define PTHREAD_MUTEX_ERRORCHECK 2

typedef struct {
	CONDITION_VARIABLE cv;
	clockid_t clock;
} pthread_cond_t;
#define PTHREAD_COND_INITIALIZER { CONDITION_VARIABLE_INIT, CLOCK_REALTIME }

typedef struct {
	clockid_t clock;
} pthread_condattr_t;

typedef struct {
	SRWLOCK lock;
	volatile DWORD writer;
} pthread_rwlock_t;
#define PTHREAD_RWLOCK_INITIALIZER { SRWLOCK_INIT, 0 }
typedef struct {
	int dummy;
} pthread_rwlockattr_t;

typedef DWORD pthread_key_t;

typedef INIT_ONCE pthread_once_t;
#define PTHREAD_ONCE_INIT INIT_ONCE_STATIC_INIT

#define PTHREAD_CANCEL_ENABLE 0
#define PTHREAD_CANCEL_DISABLE 1
#define PTHREAD_CANCEL_DEFERRED 0
#define PTHREAD_CANCEL_ASYNCHRONOUS 1
#define PTHREAD_CANCELED ((void *)-1)

/* threads */
extern int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
			  void *(*start)(void *), void *arg);
extern int pthread_join(pthread_t thread, void **retval);
extern int pthread_detach(pthread_t thread);
extern __declspec(noreturn) void pthread_exit(void *retval);
extern int pthread_kill(pthread_t thread, int sig);
extern int pthread_cancel(pthread_t thread);
static __forceinline pthread_t pthread_self(void)
{
	return (pthread_t)GetCurrentThreadId();
}
static __forceinline int pthread_equal(pthread_t a, pthread_t b)
{
	return a == b;
}
static __forceinline int pthread_getthreadid_np(void)
{
	return (int)GetCurrentThreadId();
}
extern int pthread_setname_np(pthread_t thread, const char *name);
extern int pthread_getname_np(pthread_t thread, char *name, size_t len);

extern int pthread_attr_init(pthread_attr_t *attr);
extern int pthread_attr_destroy(pthread_attr_t *attr);
extern int pthread_attr_setdetachstate(pthread_attr_t *attr, int state);
extern int pthread_attr_setstacksize(pthread_attr_t *attr, size_t size);

static __forceinline int pthread_setcancelstate(int state, int *oldstate)
{
	(void)state;
	if (oldstate)
		*oldstate = PTHREAD_CANCEL_ENABLE;
	return 0;
}
static __forceinline int pthread_setcanceltype(int type, int *oldtype)
{
	(void)type;
	if (oldtype)
		*oldtype = PTHREAD_CANCEL_DEFERRED;
	return 0;
}
static __forceinline void pthread_testcancel(void)
{
}

/* no cancellation, so cleanup handlers only run through pop(1) */
#define pthread_cleanup_push(routine, arg)                                     \
	{                                                                      \
		void (*_pcl_routine)(void *) = (routine);                      \
		void *_pcl_arg = (arg);
#define pthread_cleanup_pop(execute)                                           \
		if (execute)                                                   \
			_pcl_routine(_pcl_arg);                                \
	}

/* mutexes */
static __forceinline int pthread_mutex_init(pthread_mutex_t *m,
					    const pthread_mutexattr_t *attr)
{
	(void)attr;
	InitializeSRWLock(m);
	return 0;
}
static __forceinline int pthread_mutex_destroy(pthread_mutex_t *m)
{
	(void)m;
	return 0;
}
static __forceinline int pthread_mutex_lock(pthread_mutex_t *m)
{
	AcquireSRWLockExclusive(m);
	return 0;
}
static __forceinline int pthread_mutex_trylock(pthread_mutex_t *m)
{
	return TryAcquireSRWLockExclusive(m) ? 0 : EBUSY;
}
static __forceinline int pthread_mutex_unlock(pthread_mutex_t *m)
{
	ReleaseSRWLockExclusive(m);
	return 0;
}
static __forceinline int pthread_mutexattr_init(pthread_mutexattr_t *a)
{
	a->type = PTHREAD_MUTEX_DEFAULT;
	return 0;
}
static __forceinline int pthread_mutexattr_destroy(pthread_mutexattr_t *a)
{
	(void)a;
	return 0;
}
static __forceinline int pthread_mutexattr_settype(pthread_mutexattr_t *a,
						   int type)
{
	a->type = type;
	return 0;
}

/* condition variables */
static __forceinline int pthread_condattr_init(pthread_condattr_t *a)
{
	a->clock = CLOCK_REALTIME;
	return 0;
}
static __forceinline int pthread_condattr_destroy(pthread_condattr_t *a)
{
	(void)a;
	return 0;
}
static __forceinline int pthread_condattr_setclock(pthread_condattr_t *a,
						   clockid_t clock)
{
	a->clock = clock;
	return 0;
}
static __forceinline int pthread_cond_init(pthread_cond_t *c,
					   const pthread_condattr_t *a)
{
	InitializeConditionVariable(&c->cv);
	c->clock = a ? a->clock : CLOCK_REALTIME;
	return 0;
}
static __forceinline int pthread_cond_destroy(pthread_cond_t *c)
{
	(void)c;
	return 0;
}
static __forceinline int pthread_cond_signal(pthread_cond_t *c)
{
	WakeConditionVariable(&c->cv);
	return 0;
}
static __forceinline int pthread_cond_broadcast(pthread_cond_t *c)
{
	WakeAllConditionVariable(&c->cv);
	return 0;
}
static __forceinline int pthread_cond_wait(pthread_cond_t *c,
					   pthread_mutex_t *m)
{
	SleepConditionVariableSRW(&c->cv, m, INFINITE, 0);
	return 0;
}
extern int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m,
				  const struct timespec *abstime);

/* read-write locks */
static __forceinline int pthread_rwlock_init(pthread_rwlock_t *l,
					     const pthread_rwlockattr_t *a)
{
	(void)a;
	InitializeSRWLock(&l->lock);
	l->writer = 0;
	return 0;
}
static __forceinline int pthread_rwlock_destroy(pthread_rwlock_t *l)
{
	(void)l;
	return 0;
}
static __forceinline int pthread_rwlock_rdlock(pthread_rwlock_t *l)
{
	AcquireSRWLockShared(&l->lock);
	return 0;
}
static __forceinline int pthread_rwlock_wrlock(pthread_rwlock_t *l)
{
	AcquireSRWLockExclusive(&l->lock);
	l->writer = GetCurrentThreadId();
	return 0;
}
static __forceinline int pthread_rwlock_unlock(pthread_rwlock_t *l)
{
	if (l->writer == GetCurrentThreadId()) {
		l->writer = 0;
		ReleaseSRWLockExclusive(&l->lock);
	} else
		ReleaseSRWLockShared(&l->lock);
	return 0;
}

/* thread specific data */
extern int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
static __forceinline int pthread_key_delete(pthread_key_t key)
{
	return FlsFree(key) ? 0 : EINVAL;
}
static __forceinline void *pthread_getspecific(pthread_key_t key)
{
	return FlsGetValue(key);
}
static __forceinline int pthread_setspecific(pthread_key_t key,
					     const void *value)
{
	return FlsSetValue(key, (void *)value) ? 0 : EINVAL;
}

/* one-time initialization */
extern int pthread_once(pthread_once_t *once, void (*init)(void));

#endif /* _FRR_WIN_PTHREAD_H */
