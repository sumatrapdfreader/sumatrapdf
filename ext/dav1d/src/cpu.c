/*
 * Copyright © 2018, VideoLAN and dav1d authors
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

#include <errno.h>
#include <stdint.h>

#include "src/cpu.h"
#include "src/log.h"

#ifdef _WIN32
#include <windows.h>
#endif
#ifdef __APPLE__
#include <sys/sysctl.h>
#include <sys/types.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_PTHREAD_GETAFFINITY_NP
#include <pthread.h>
#if HAVE_PTHREAD_NP_H
#include <pthread_np.h>
#endif
#if defined(__FreeBSD__)
#define cpu_set_t cpuset_t
#endif
#endif

#if HAVE_GETAUXVAL || HAVE_ELF_AUX_INFO
#include <sys/auxv.h>
#endif

unsigned dav1d_cpu_flags = 0U;
unsigned dav1d_cpu_flags_mask = ~0U;

COLD void dav1d_init_cpu(void) {
#if HAVE_ASM && !__has_feature(memory_sanitizer)
// memory sanitizer is inherently incompatible with asm
#if ARCH_AARCH64 || ARCH_ARM
    dav1d_cpu_flags = dav1d_get_cpu_flags_arm();
#elif ARCH_LOONGARCH
    dav1d_cpu_flags = dav1d_get_cpu_flags_loongarch();
#elif ARCH_PPC64LE
    dav1d_cpu_flags = dav1d_get_cpu_flags_ppc();
#elif ARCH_RISCV
    dav1d_cpu_flags = dav1d_get_cpu_flags_riscv();
#elif ARCH_X86
    dav1d_cpu_flags = dav1d_get_cpu_flags_x86();
#endif
#endif
}

COLD void dav1d_set_cpu_flags_mask(const unsigned mask) {
    dav1d_cpu_flags_mask = mask;
}

COLD int dav1d_num_logical_processors(Dav1dContext *const c) {
#ifdef _WIN32
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    GROUP_AFFINITY affinity;
    if (GetThreadGroupAffinity(GetCurrentThread(), &affinity)) {
        int num_processors = 1;
        while (affinity.Mask &= affinity.Mask - 1)
            num_processors++;
        return num_processors;
    }
#else
    SYSTEM_INFO system_info;
    GetNativeSystemInfo(&system_info);
    return system_info.dwNumberOfProcessors;
#endif
#elif HAVE_PTHREAD_GETAFFINITY_NP && defined(CPU_COUNT)
    cpu_set_t affinity;
    if (!pthread_getaffinity_np(pthread_self(), sizeof(affinity), &affinity))
        return CPU_COUNT(&affinity);
#elif defined(__APPLE__)
    int num_processors;
    size_t length = sizeof(num_processors);
    if (!sysctlbyname("hw.logicalcpu", &num_processors, &length, NULL, 0))
        return num_processors;
#elif defined(_SC_NPROCESSORS_ONLN)
    return (int)sysconf(_SC_NPROCESSORS_ONLN);
#endif
    if (c)
        dav1d_log(c, "Unable to detect thread count, defaulting to single-threaded mode\n");
    return 1;
}

COLD unsigned long dav1d_getauxval(unsigned long type) {
#if HAVE_GETAUXVAL
    return getauxval(type);
#elif HAVE_ELF_AUX_INFO
    unsigned long aux = 0;
    int ret = elf_aux_info(type, &aux, sizeof(aux));
    if (ret != 0)
        errno = ret;
    return aux;
#else
    errno = ENOSYS;
    return 0;
#endif
}
