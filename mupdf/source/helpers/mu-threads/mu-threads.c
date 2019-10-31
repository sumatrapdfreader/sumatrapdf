#include "mupdf/helpers/mu-threads.h"

#ifdef DISABLE_MUTHREADS

#include <stdlib.h>

/* Null implementation. Just error out. */

int mu_create_semaphore(mu_semaphore *sem)
{
	return 1; /* Just Error */
}

void mu_destroy_semaphore(mu_semaphore *sem)
{
}

int mu_trigger_semaphore(mu_semaphore *sem)
{
	abort();
	return 1;
}

int mu_wait_semaphore(mu_semaphore *sem)
{
	abort();
	return 1;
}

int mu_create_thread(mu_thread *th, mu_thread_fn *fn, void *arg)
{
	return 1;
}

void mu_destroy_thread(mu_thread *th)
{
}

int mu_create_mutex(mu_mutex *mutex)
{
	return 1;
}

void mu_destroy_mutex(mu_mutex *mutex)
{
}

void mu_lock_mutex(mu_mutex *mutex)
{
	abort();
}

void mu_unlock_mutex(mu_mutex *mutex)
{
	abort();
}

#elif MU_THREAD_IMPL_TYPE == 1

/* Windows threads */
int mu_create_semaphore(mu_semaphore *sem)
{
	sem->handle = CreateSemaphore(NULL, 0, 1, NULL);
	return (sem->handle == NULL);
}

void mu_destroy_semaphore(mu_semaphore *sem)
{
	if (sem->handle == NULL)
		return;
	/* We can't sensibly handle this failing */
	(void)CloseHandle(sem->handle);
}

int mu_trigger_semaphore(mu_semaphore *sem)
{
	if (sem->handle == NULL)
		return 0;
	/* We can't sensibly handle this failing */
	return !ReleaseSemaphore(sem->handle, 1, NULL);
}

int mu_wait_semaphore(mu_semaphore *sem)
{
	if (sem->handle == NULL)
		return 0;
	/* We can't sensibly handle this failing */
	return !WaitForSingleObject(sem->handle, INFINITE);
}

static DWORD WINAPI thread_starter(LPVOID arg)
{
	mu_thread *th = (mu_thread *)arg;

	th->fn(th->arg);

	return 0;
}

int mu_create_thread(mu_thread *th, mu_thread_fn *fn, void *arg)
{
	th->fn = fn;
	th->arg = arg;
	th->handle = CreateThread(NULL, 0, thread_starter, th, 0, NULL);

	return (th->handle == NULL);
}

void mu_destroy_thread(mu_thread *th)
{
	if (th->handle == NULL)
		return;
	/* We can't sensibly handle this failing */
	(void)WaitForSingleObject(th->handle, INFINITE);
	(void)CloseHandle(th->handle);
	th->handle = NULL;
}

int mu_create_mutex(mu_mutex *mutex)
{
	InitializeCriticalSection(&mutex->mutex);
	return 0; /* Magic function, never fails */
}

void mu_destroy_mutex(mu_mutex *mutex)
{
	const static CRITICAL_SECTION empty = { 0 };
	if (memcmp(&mutex->mutex, &empty, sizeof(empty)) == 0)
		return;
	DeleteCriticalSection(&mutex->mutex);
	mutex->mutex = empty;
}

void mu_lock_mutex(mu_mutex *mutex)
{
	EnterCriticalSection(&mutex->mutex);
}

void mu_unlock_mutex(mu_mutex *mutex)
{
	LeaveCriticalSection(&mutex->mutex);
}

#elif MU_THREAD_IMPL_TYPE == 2

/*
	PThreads - without working unnamed semaphores.

	Neither ios nor OSX supports unnamed semaphores.
	Named semaphores are a pain to use, so we implement
	our own semaphores using condition variables and
	mutexes.
*/

#include <string.h>

struct mu_semaphore
{
	int count;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
};

int
mu_create_semaphore(mu_semaphore *sem)
{
	int scode;

	sem->count = 0;
	scode = pthread_mutex_init(&sem->mutex, NULL);
	if (scode == 0)
	{
		scode = pthread_cond_init(&sem->cond, NULL);
		if (scode)
			pthread_mutex_destroy(&sem->mutex);
	}
	if (scode)
		memset(sem, 0, sizeof(*sem));
	return scode;
}

void
mu_destroy_semaphore(mu_semaphore *sem)
{
	const static mu_semaphore empty = { 0 };

	if (memcmp(sem, &empty, sizeof(empty)) == 0)
		return;
	(void)pthread_cond_destroy(&sem->cond);
	(void)pthread_mutex_destroy(&sem->mutex);
	*sem = empty;
}

int
mu_wait_semaphore(mu_semaphore *sem)
{
	int scode, scode2;

	scode = pthread_mutex_lock(&sem->mutex);
	if (scode)
		return scode;
	while (sem->count == 0) {
		scode = pthread_cond_wait(&sem->cond, &sem->mutex);
		if (scode)
			break;
	}
	if (scode == 0)
		--sem->count;
	scode2 = pthread_mutex_unlock(&sem->mutex);
	if (scode == 0)
		scode = scode2;
	return scode;
}

int
mu_trigger_semaphore(mu_semaphore * sem)
{
	int scode, scode2;

	scode = pthread_mutex_lock(&sem->mutex);
	if (scode)
		return scode;
	if (sem->count++ == 0)
		scode = pthread_cond_signal(&sem->cond);
	scode2 = pthread_mutex_unlock(&sem->mutex);
	if (scode == 0)
		scode = scode2;
	return scode;
}

static void *thread_starter(void *arg)
{
	mu_thread *th = (mu_thread *)arg;

	th->fn(th->arg);

	return NULL;
}

int mu_create_thread(mu_thread *th, mu_thread_fn *fn, void *arg)
{
	th->fn = fn;
	th->arg = arg;
	return pthread_create(&th->thread, NULL, thread_starter, th);
}

void mu_destroy_thread(mu_thread *th)
{
	const static mu_thread empty; /* static objects are always initialized to zero */

	if (memcmp(th, &empty, sizeof(empty)) == 0)
		return;

	(void)pthread_join(th->thread, NULL);
	*th = empty;
}

int mu_create_mutex(mu_mutex *mutex)
{
	return pthread_mutex_init(&mutex->mutex, NULL);
}

void mu_destroy_mutex(mu_mutex *mutex)
{
	const static mu_mutex empty; /* static objects are always initialized to zero */

	if (memcmp(mutex, &empty, sizeof(empty)) == 0)
		return;

	(void)pthread_mutex_destroy(&mutex->mutex);
	*mutex = empty;
}

void mu_lock_mutex(mu_mutex *mutex)
{
	(void)pthread_mutex_lock(&mutex->mutex);
}

void mu_unlock_mutex(mu_mutex *mutex)
{
	(void)pthread_mutex_unlock(&mutex->mutex);
}

#else
#error Unknown MU_THREAD_IMPL_TYPE setting
#endif
