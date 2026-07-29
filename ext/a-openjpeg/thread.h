#ifndef THREAD_H
#define THREAD_H

#include "openjpeg.h"

typedef struct opj_mutex_t opj_mutex_t;

opj_mutex_t* opj_mutex_create(void);

void opj_mutex_lock(opj_mutex_t* mutex);

void opj_mutex_unlock(opj_mutex_t* mutex);

void opj_mutex_destroy(opj_mutex_t* mutex);

typedef struct opj_cond_t opj_cond_t;

opj_cond_t* opj_cond_create(void);

void opj_cond_wait(opj_cond_t* cond, opj_mutex_t* mutex);

void opj_cond_signal(opj_cond_t* cond);

void opj_cond_destroy(opj_cond_t* cond);

typedef struct opj_thread_t opj_thread_t;

typedef void (*opj_thread_fn)(void* user_data);

opj_thread_t* opj_thread_create(opj_thread_fn thread_fn, void* user_data);

void opj_thread_join(opj_thread_t* thread);

typedef struct opj_tls_t opj_tls_t;

void* opj_tls_get(opj_tls_t* tls, int key);

typedef void (*opj_tls_free_func)(void* value);

OPJ_BOOL opj_tls_set(opj_tls_t* tls, int key, void* value,
                     opj_tls_free_func free_func);

typedef struct opj_thread_pool_t opj_thread_pool_t;

opj_thread_pool_t* opj_thread_pool_create(int num_threads);

typedef void (*opj_job_fn)(void* user_data, opj_tls_t* tls);

OPJ_BOOL opj_thread_pool_submit_job(opj_thread_pool_t* tp, opj_job_fn job_fn,
                                    void* user_data);

void opj_thread_pool_wait_completion(opj_thread_pool_t* tp,
                                     int max_remaining_jobs);

int opj_thread_pool_get_thread_count(opj_thread_pool_t* tp);

void opj_thread_pool_destroy(opj_thread_pool_t* tp);

#endif
