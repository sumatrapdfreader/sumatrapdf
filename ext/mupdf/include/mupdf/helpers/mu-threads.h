// Copyright (C) 2004-2021 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#ifndef MUPDF_HELPERS_MU_THREADS_H
#define MUPDF_HELPERS_MU_THREADS_H

/*
	Simple threading helper library.
	Includes implementations for Windows, pthreads,
	and "no threads".

	The "no threads" implementation simply provides types
	and stub functions so that things will build, but abort
	if we try to call them. This simplifies the job for
	calling functions.

	To build this library on a platform with no threading,
	define DISABLE_MUTHREADS (or extend the ifdeffery below
	so that it does so).

	To build this library on a platform that uses a
	threading model other than windows threads or pthreads,
	extend the #ifdeffery below to set MUTHREAD_IMPL_TYPE
	to an unused value, and modify mu-threads.c
	appropriately.
*/

#if !defined(DISABLE_MUTHREADS)
#ifdef _WIN32
#define MU_THREAD_IMPL_TYPE 1
#elif defined(HAVE_PTHREAD)
#define MU_THREAD_IMPL_TYPE 2
#else
#define DISABLE_MUTHREADS
#endif
#endif

/*
	Types
*/
typedef struct mu_thread mu_thread;
typedef struct mu_semaphore mu_semaphore;
typedef struct mu_mutex mu_mutex;

/*
	Semaphores

	Created with a value of 0. Triggering a semaphore
	increments the value. Waiting on a semaphore reduces
	the value, blocking if it would become negative.

	Never increment the value of a semaphore above 1, as
	this has undefined meaning in this implementation.
*/

/*
	Create a semaphore.

	sem: Pointer to a mu_semaphore to populate.

	Returns non-zero for error.
*/
int mu_create_semaphore(mu_semaphore *sem);

/*
	Destroy a semaphore.
	Semaphores may safely be destroyed multiple
	times. Any semaphore initialised to zeros is
	safe to destroy.

	Never destroy a semaphore that may be being waited
	upon, as this has undefined meaning in this
	implementation.

	sem: Pointer to a mu_semaphore to destroy.
*/
void mu_destroy_semaphore(mu_semaphore *sem);

/*
	Increment the value of the
	semaphore. Never blocks.

	sem: The semaphore to increment.

	Returns non-zero on error.
*/
int mu_trigger_semaphore(mu_semaphore *sem);

/*
	Decrement the value of the
	semaphore, blocking if this would involve making
	the value negative.

	sem: The semaphore to decrement.

	Returns non-zero on error.
*/
int mu_wait_semaphore(mu_semaphore *sem);

/*
	Threads
*/

/*
	The type for the function that a thread runs.

	arg: User supplied data.
*/
typedef void (mu_thread_fn)(void *arg);

/*
	Create a thread to run the
	supplied function with the supplied argument.

	th: Pointer to mu_thread to populate with created
	threads information.

	fn: The function for the thread to run.

	arg: The argument to pass to fn.
*/
int mu_create_thread(mu_thread *th, mu_thread_fn *fn, void *arg);

/*
	Destroy a thread. This function
	blocks until a thread has terminated normally, and
	destroys its storage. A mu_thread may safely be destroyed
	multiple times, as may any mu_thread initialised with
	zeros.

	th: Pointer to mu_thread to destroy.
*/
void mu_destroy_thread(mu_thread *th);

/*
	Mutexes

	This implementation does not specify whether
	mutexes are recursive or not.
*/

/*
	Create a mutex.

	mutex: pointer to a mu_mutex to populate.

	Returns non-zero on error.
*/
int mu_create_mutex(mu_mutex *mutex);

/*
	Destroy a mutex. A mu_mutex may
	safely be destroyed several times, as may a mu_mutex
	initialised with zeros. Never destroy locked mu_mutex.

	mutex: Pointer to mu_mutex to destroy.
*/
void mu_destroy_mutex(mu_mutex *mutex);

/*
	Lock a mutex.

	mutex: Mutex to lock.
*/
void mu_lock_mutex(mu_mutex *mutex);

/*
	Unlock a mutex.

	mutex: Mutex to unlock.
*/
void mu_unlock_mutex(mu_mutex *mutex);

/*
	Everything under this point is implementation specific.
	Only people looking to extend the capabilities of this
	helper module should need to look below here.
*/

#ifdef DISABLE_MUTHREADS

/* Null implementation */
struct mu_semaphore
{
	int dummy;
};

struct mu_thread
{
	int dummy;
};

struct mu_mutex
{
	int dummy;
};

#elif MU_THREAD_IMPL_TYPE == 1

#include <windows.h>

/* Windows threads */
struct mu_semaphore
{
	HANDLE handle;
};

struct mu_thread
{
	HANDLE handle;
	mu_thread_fn *fn;
	void *arg;
};

struct mu_mutex
{
	CRITICAL_SECTION mutex;
};

#elif MU_THREAD_IMPL_TYPE == 2

/*
	PThreads - without working unnamed semaphores.

	Neither ios nor OSX supports unnamed semaphores.
	Named semaphores are a pain to use, so we implement
	our own semaphores using condition variables and
	mutexes.
*/

#include <pthread.h>

struct mu_semaphore
{
	int count;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
};

struct mu_thread
{
	pthread_t thread;
	mu_thread_fn *fn;
	void *arg;
};

struct mu_mutex
{
	pthread_mutex_t mutex;
};

/*
	Add new threading implementations here, with
	#elif MU_THREAD_IMPL_TYPE == 3... etc.
*/

#else
#error Unknown MU_THREAD_IMPL_TYPE setting
#endif

#endif /* MUPDF_HELPERS_MU_THREADS_H */
