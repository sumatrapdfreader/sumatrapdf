/*
 * Copyright © 2018-2021, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if defined(_WIN32)

#include <process.h>
#include <stdlib.h>
#include <windows.h>

#include "common/attributes.h"

#include "src/thread.h"

static HRESULT (WINAPI *set_thread_description)(HANDLE, PCWSTR);

COLD void dav1d_init_thread(void) {
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    HANDLE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (kernel32)
        set_thread_description =
            (void*)GetProcAddress(kernel32, "SetThreadDescription");
#endif
}

#undef dav1d_set_thread_name
COLD void dav1d_set_thread_name(const wchar_t *const name) {
    if (set_thread_description) /* Only available since Windows 10 1607 */
        set_thread_description(GetCurrentThread(), name);
}

static COLD unsigned __stdcall thread_entrypoint(void *const data) {
    pthread_t *const t = data;
    t->arg = t->func(t->arg);
    return 0;
}

COLD int dav1d_pthread_create(pthread_t *const thread,
                              const pthread_attr_t *const attr,
                              void *(*const func)(void*), void *const arg)
{
    const unsigned stack_size = attr ? attr->stack_size : 0;
    thread->func = func;
    thread->arg = arg;
    thread->h = (HANDLE)_beginthreadex(NULL, stack_size, thread_entrypoint, thread,
                                       STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    return !thread->h;
}

COLD int dav1d_pthread_join(pthread_t *const thread, void **const res) {
    if (WaitForSingleObject(thread->h, INFINITE))
        return 1;

    if (res)
        *res = thread->arg;

    return !CloseHandle(thread->h);
}

COLD int dav1d_pthread_once(pthread_once_t *const once_control,
                            void (*const init_routine)(void))
{
    BOOL pending = FALSE;

    if (InitOnceBeginInitialize(once_control, 0, &pending, NULL) != TRUE)
        return 1;

    if (pending == TRUE)
        init_routine();

    return !InitOnceComplete(once_control, 0, NULL);
}

#endif
