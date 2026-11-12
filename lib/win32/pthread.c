// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Minimal POSIX threads on top of native Win32 primitives (FRR subset).
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

struct win_thread {
	struct win_thread *next;
	DWORD tid;
	HANDLE handle;
	void *(*start)(void *);
	void *arg;
	void *retval;
	bool detached;
	bool exited;
};

static SRWLOCK threads_lock = SRWLOCK_INIT;
static struct win_thread *threads;

static struct win_thread *thread_find(DWORD tid)
{
	struct win_thread *t;

	for (t = threads; t; t = t->next)
		if (t->tid == tid && !t->exited)
			return t;
	return NULL;
}

static void thread_unlink(struct win_thread *wt)
{
	struct win_thread **pp;

	for (pp = &threads; *pp; pp = &(*pp)->next)
		if (*pp == wt) {
			*pp = wt->next;
			return;
		}
}

static unsigned __stdcall thread_trampoline(void *arg)
{
	struct win_thread *wt = arg;
	void *rv;

	rv = wt->start(wt->arg);
	AcquireSRWLockExclusive(&threads_lock);
	wt->retval = rv;
	ReleaseSRWLockExclusive(&threads_lock);
	return 0;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
		   void *(*start)(void *), void *arg)
{
	struct win_thread *wt;
	unsigned tid;
	uintptr_t h;

	wt = calloc(1, sizeof(*wt));
	if (!wt)
		return ENOMEM;
	wt->start = start;
	wt->arg = arg;
	wt->detached = attr && attr->detachstate == PTHREAD_CREATE_DETACHED;

	/* hold the lock so the new thread can't finish before registration */
	AcquireSRWLockExclusive(&threads_lock);
	h = _beginthreadex(NULL, attr ? (unsigned)attr->stacksize : 0,
			   thread_trampoline, wt, 0, &tid);
	if (!h) {
		ReleaseSRWLockExclusive(&threads_lock);
		free(wt);
		return EAGAIN;
	}
	wt->tid = tid;
	wt->handle = (HANDLE)h;
	if (wt->detached) {
		CloseHandle(wt->handle);
		wt->handle = NULL;
	} else {
		wt->next = threads;
		threads = wt;
	}
	ReleaseSRWLockExclusive(&threads_lock);

	*thread = tid;
	return 0;
}

int pthread_join(pthread_t thread, void **retval)
{
	struct win_thread *wt;
	HANDLE h;

	if (thread == pthread_self())
		return EDEADLK;

	AcquireSRWLockShared(&threads_lock);
	wt = thread_find((DWORD)thread);
	h = wt ? wt->handle : NULL;
	ReleaseSRWLockShared(&threads_lock);
	if (!wt || !h)
		return ESRCH;

	WaitForSingleObject(h, INFINITE);

	AcquireSRWLockExclusive(&threads_lock);
	if (retval)
		*retval = wt->retval;
	thread_unlink(wt);
	ReleaseSRWLockExclusive(&threads_lock);

	CloseHandle(h);
	free(wt);
	return 0;
}

int pthread_detach(pthread_t thread)
{
	struct win_thread *wt;

	AcquireSRWLockExclusive(&threads_lock);
	wt = thread_find((DWORD)thread);
	if (wt) {
		thread_unlink(wt);
		CloseHandle(wt->handle);
	}
	ReleaseSRWLockExclusive(&threads_lock);
	/* the trampoline still references wt; leak it rather than race */
	return wt ? 0 : ESRCH;
}

void pthread_exit(void *retval)
{
	struct win_thread *wt;

	AcquireSRWLockExclusive(&threads_lock);
	wt = thread_find(GetCurrentThreadId());
	if (wt)
		wt->retval = retval;
	ReleaseSRWLockExclusive(&threads_lock);
	_endthreadex(0);
}

int pthread_kill(pthread_t thread, int sig)
{
	(void)thread;
	(void)sig;
	return ENOTSUP;
}

int pthread_cancel(pthread_t thread)
{
	(void)thread;
	return ENOTSUP;
}

int pthread_setname_np(pthread_t thread, const char *name)
{
	wchar_t wname[64];
	HANDLE h;
	HRESULT hr;

	if (MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, 64) == 0)
		return EINVAL;
	if (thread == pthread_self())
		return FAILED(SetThreadDescription(GetCurrentThread(), wname))
			       ? EINVAL
			       : 0;

	h = OpenThread(THREAD_SET_LIMITED_INFORMATION, FALSE, (DWORD)thread);
	if (!h)
		return ESRCH;
	hr = SetThreadDescription(h, wname);
	CloseHandle(h);
	return FAILED(hr) ? EINVAL : 0;
}

int pthread_getname_np(pthread_t thread, char *name, size_t len)
{
	wchar_t *wname = NULL;
	HANDLE h;
	HRESULT hr;

	h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)thread);
	if (!h)
		return ESRCH;
	hr = GetThreadDescription(h, &wname);
	CloseHandle(h);
	if (FAILED(hr))
		return EINVAL;
	WideCharToMultiByte(CP_UTF8, 0, wname, -1, name, (int)len, NULL, NULL);
	LocalFree(wname);
	return 0;
}

int pthread_attr_init(pthread_attr_t *attr)
{
	attr->detachstate = PTHREAD_CREATE_JOINABLE;
	attr->stacksize = 0;
	return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr)
{
	(void)attr;
	return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *attr, int state)
{
	attr->detachstate = state;
	return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t size)
{
	attr->stacksize = size;
	return 0;
}

int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m,
			   const struct timespec *abstime)
{
	struct timespec now;
	long long ms;

	clock_gettime(c->clock, &now);
	ms = (abstime->tv_sec - now.tv_sec) * 1000LL +
	     (abstime->tv_nsec - now.tv_nsec) / 1000000LL;
	if (ms < 0)
		ms = 0;
	if (ms > 0x7ffffffeLL)
		ms = 0x7ffffffeLL;

	if (SleepConditionVariableSRW(&c->cv, m, (DWORD)ms, 0))
		return 0;
	return GetLastError() == ERROR_TIMEOUT ? ETIMEDOUT : EINVAL;
}

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *))
{
	DWORD k;

	/* PFLS_CALLBACK_FUNCTION and a pthread destructor share the ABI on
	 * x64 and ARM64 (one pointer argument, no return value)
	 */
	k = FlsAlloc((PFLS_CALLBACK_FUNCTION)destructor);
	if (k == FLS_OUT_OF_INDEXES)
		return EAGAIN;
	*key = k;
	return 0;
}

static BOOL CALLBACK once_trampoline(PINIT_ONCE once, PVOID param,
				     PVOID *ctx)
{
	void (*init)(void) = (void (*)(void))param;

	(void)once;
	(void)ctx;
	init();
	return TRUE;
}

int pthread_once(pthread_once_t *once, void (*init)(void))
{
	return InitOnceExecuteOnce(once, once_trampoline, (PVOID)init, NULL)
		       ? 0
		       : EINVAL;
}
