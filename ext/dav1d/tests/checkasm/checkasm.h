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

#ifndef DAV1D_TESTS_CHECKASM_CHECKASM_H
#define DAV1D_TESTS_CHECKASM_CHECKASM_H

#include "config.h"

#include <stdint.h>
#include <stdlib.h>

#if ARCH_X86_64 && defined(_WIN32)
/* setjmp/longjmp on 64-bit Windows will try to use SEH to unwind the stack,
 * which doesn't work for assembly functions without unwind information. */
#include <windows.h>
#define checkasm_context CONTEXT
#define checkasm_save_context() RtlCaptureContext(&checkasm_context_buf)
#define checkasm_load_context() RtlRestoreContext(&checkasm_context_buf, NULL)
#else
#include <setjmp.h>
#define checkasm_context jmp_buf
#define checkasm_save_context() setjmp(checkasm_context_buf)
#define checkasm_load_context() longjmp(checkasm_context_buf, 1)
#endif

#include "include/common/attributes.h"
#include "include/common/bitdepth.h"
#include "include/common/intops.h"

int xor128_rand(void);
#define rnd xor128_rand

#define decl_check_bitfns(name) \
name##_8bpc(void); \
name##_16bpc(void)

void checkasm_check_msac(void);
void checkasm_check_refmvs(void);
decl_check_bitfns(void checkasm_check_cdef);
decl_check_bitfns(void checkasm_check_filmgrain);
decl_check_bitfns(void checkasm_check_ipred);
decl_check_bitfns(void checkasm_check_itx);
decl_check_bitfns(void checkasm_check_loopfilter);
decl_check_bitfns(void checkasm_check_looprestoration);
decl_check_bitfns(void checkasm_check_mc);

void *checkasm_check_func(void *func, const char *name, ...);
int checkasm_bench_func(void);
int checkasm_fail_func(const char *msg, ...);
void checkasm_update_bench(int iterations, uint64_t cycles);
void checkasm_report(const char *name, ...);
void checkasm_set_signal_handler_state(int enabled);
extern checkasm_context checkasm_context_buf;

/* float compare utilities */
int float_near_ulp(float a, float b, unsigned max_ulp);
int float_near_abs_eps(float a, float b, float eps);
int float_near_abs_eps_ulp(float a, float b, float eps, unsigned max_ulp);
int float_near_ulp_array(const float *a, const float *b, unsigned max_ulp,
                         int len);
int float_near_abs_eps_array(const float *a, const float *b, float eps,
                             int len);
int float_near_abs_eps_array_ulp(const float *a, const float *b, float eps,
                                 unsigned max_ulp, int len);

#define BENCH_RUNS (1 << 12) /* Trade-off between accuracy and speed */

/* Decide whether or not the specified function needs to be tested */
#define check_func(func, ...)\
    (func_ref = checkasm_check_func((func_new = func), __VA_ARGS__))

/* Declare the function prototype. The first argument is the return value,
 * the remaining arguments are the function parameters. Naming parameters
 * is optional. */
#define declare_func(ret, ...)\
    declare_new(ret, __VA_ARGS__)\
    void *func_ref, *func_new;\
    typedef ret func_type(__VA_ARGS__);\
    checkasm_save_context()

/* Indicate that the current test has failed */
#define fail() checkasm_fail_func("%s:%d", __FILE__, __LINE__)

/* Print the test outcome */
#define report checkasm_report

/* Call the reference function */
#define call_ref(...)\
    (checkasm_set_signal_handler_state(1),\
     ((func_type *)func_ref)(__VA_ARGS__));\
    checkasm_set_signal_handler_state(0)

#if HAVE_ASM
#if ARCH_X86
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#define readtime() (_mm_lfence(), __rdtsc())
#else
static inline uint64_t readtime(void) {
    uint32_t eax, edx;
    __asm__ __volatile__("lfence\nrdtsc" : "=a"(eax), "=d"(edx));
    return (((uint64_t)edx) << 32) | eax;
}
#define readtime readtime
#endif
#elif (ARCH_AARCH64 || ARCH_ARM) && defined(__APPLE__)
#include <mach/mach_time.h>
#define readtime() mach_absolute_time()
#elif ARCH_AARCH64
#ifdef _MSC_VER
#include <windows.h>
#define readtime() (_InstructionSynchronizationBarrier(), ReadTimeStampCounter())
#else
static inline uint64_t readtime(void) {
    uint64_t cycle_counter;
    /* This requires enabling user mode access to the cycle counter (which
     * can only be done from kernel space).
     * This could also read cntvct_el0 instead of pmccntr_el0; that register
     * might also be readable (depending on kernel version), but it has much
     * worse precision (it's a fixed 50 MHz timer). */
    __asm__ __volatile__("isb\nmrs %0, pmccntr_el0"
                         : "=r"(cycle_counter)
                         :: "memory");
    return cycle_counter;
}
#define readtime readtime
#endif
#elif ARCH_ARM && !defined(_MSC_VER) && __ARM_ARCH >= 7
static inline uint64_t readtime(void) {
    uint32_t cycle_counter;
    /* This requires enabling user mode access to the cycle counter (which
     * can only be done from kernel space). */
    __asm__ __volatile__("isb\nmrc p15, 0, %0, c9, c13, 0"
                         : "=r"(cycle_counter)
                         :: "memory");
    return cycle_counter;
}
#define readtime readtime
#elif ARCH_PPC64LE
static inline uint64_t readtime(void) {
    uint32_t tbu, tbl, temp;

    __asm__ __volatile__(
        "1:\n"
        "mfspr %2,269\n"
        "mfspr %0,268\n"
        "mfspr %1,269\n"
        "cmpw   %2,%1\n"
        "bne    1b\n"
    : "=r"(tbl), "=r"(tbu), "=r"(temp)
    :
    : "cc");

    return (((uint64_t)tbu) << 32) | (uint64_t)tbl;
}
#define readtime readtime
#endif

/* Verifies that clobbered callee-saved registers
 * are properly saved and restored */
void checkasm_checked_call(void *func, ...);

#if ARCH_X86_64
/* Evil hack: detect incorrect assumptions that 32-bit ints are zero-extended
 * to 64-bit. This is done by clobbering the stack with junk around the stack
 * pointer and calling the assembly function through checked_call() with added
 * dummy arguments which forces all real arguments to be passed on the stack
 * and not in registers. For 32-bit arguments the upper half of the 64-bit
 * register locations on the stack will now contain junk which will cause
 * misbehaving functions to either produce incorrect output or segfault. Note
 * that even though this works extremely well in practice, it's technically
 * not guaranteed and false negatives is theoretically possible, but there
 * can never be any false positives. */
void checkasm_stack_clobber(uint64_t clobber, ...);
/* YMM and ZMM registers on x86 are turned off to save power when they haven't
 * been used for some period of time. When they are used there will be a
 * "warmup" period during which performance will be reduced and inconsistent
 * which is problematic when trying to benchmark individual functions. We can
 * work around this by periodically issuing "dummy" instructions that uses
 * those registers to keep them powered on. */
void checkasm_simd_warmup(void);
#define declare_new(ret, ...)\
    ret (*checked_call)(void *, int, int, int, int, int, __VA_ARGS__,\
                        int, int, int, int, int, int, int, int,\
                        int, int, int, int, int, int, int) =\
    (void *)checkasm_checked_call;
#define CLOB (UINT64_C(0xdeadbeefdeadbeef))
#ifdef _WIN32
#define STACKARGS 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0
#else
#define STACKARGS 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0
#endif
#define call_new(...)\
    (checkasm_set_signal_handler_state(1),\
     checkasm_simd_warmup(),\
     checkasm_stack_clobber(CLOB, CLOB, CLOB, CLOB, CLOB, CLOB, CLOB,\
                            CLOB, CLOB, CLOB, CLOB, CLOB, CLOB, CLOB,\
                            CLOB, CLOB, CLOB, CLOB, CLOB, CLOB, CLOB),\
     checked_call(func_new, 0, 0, 0, 0, 0, __VA_ARGS__, STACKARGS));\
    checkasm_set_signal_handler_state(0)
#elif ARCH_X86_32
#define declare_new(ret, ...)\
    ret (*checked_call)(void *, __VA_ARGS__, int, int, int, int, int, int,\
                        int, int, int, int, int, int, int, int, int) =\
        (void *)checkasm_checked_call;
#define call_new(...)\
    (checkasm_set_signal_handler_state(1),\
     checked_call(func_new, __VA_ARGS__, 15, 14, 13, 12,\
                  11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1));\
    checkasm_set_signal_handler_state(0)
#elif ARCH_ARM
/* Use a dummy argument, to offset the real parameters by 2, not only 1.
 * This makes sure that potential 8-byte-alignment of parameters is kept
 * the same even when the extra parameters have been removed. */
void checkasm_checked_call_vfp(void *func, int dummy, ...);
#define declare_new(ret, ...)\
    ret (*checked_call)(void *, int dummy, __VA_ARGS__,\
                        int, int, int, int, int, int, int, int,\
                        int, int, int, int, int, int, int) =\
    (void *)checkasm_checked_call_vfp;
#define call_new(...)\
    (checkasm_set_signal_handler_state(1),\
     checked_call(func_new, 0, __VA_ARGS__, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0));\
    checkasm_set_signal_handler_state(0)
#elif ARCH_AARCH64 && !defined(__APPLE__)
void checkasm_stack_clobber(uint64_t clobber, ...);
#define declare_new(ret, ...)\
    ret (*checked_call)(void *, int, int, int, int, int, int, int,\
                        __VA_ARGS__, int, int, int, int, int, int, int, int,\
                        int, int, int, int, int, int, int) =\
    (void *)checkasm_checked_call;
#define CLOB (UINT64_C(0xdeadbeefdeadbeef))
#define call_new(...)\
    (checkasm_set_signal_handler_state(1),\
     checkasm_stack_clobber(CLOB, CLOB, CLOB, CLOB, CLOB, CLOB,\
                            CLOB, CLOB, CLOB, CLOB, CLOB, CLOB,\
                            CLOB, CLOB, CLOB, CLOB, CLOB, CLOB,\
                            CLOB, CLOB, CLOB, CLOB, CLOB),\
     checked_call(func_new, 0, 0, 0, 0, 0, 0, 0, __VA_ARGS__,\
                  7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0));\
    checkasm_set_signal_handler_state(0)
#else
#define declare_new(ret, ...)
#define call_new(...)\
    (checkasm_set_signal_handler_state(1),\
     ((func_type *)func_new)(__VA_ARGS__));\
    checkasm_set_signal_handler_state(0)
#endif
#else /* HAVE_ASM */
#define declare_new(ret, ...)
/* Call the function */
#define call_new(...)\
    (checkasm_set_signal_handler_state(1),\
     ((func_type *)func_new)(__VA_ARGS__));\
    checkasm_set_signal_handler_state(0)
#endif /* HAVE_ASM */

/* Benchmark the function */
#ifdef readtime
#define bench_new(...)\
    do {\
        if (checkasm_bench_func()) {\
            func_type *const tfunc = func_new;\
            checkasm_set_signal_handler_state(1);\
            uint64_t tsum = 0;\
            int tcount = 0;\
            for (int ti = 0; ti < BENCH_RUNS; ti++) {\
                uint64_t t = readtime();\
                int talt = 0; (void)talt;\
                tfunc(__VA_ARGS__);\
                talt = 1;\
                tfunc(__VA_ARGS__);\
                talt = 0;\
                tfunc(__VA_ARGS__);\
                talt = 1;\
                tfunc(__VA_ARGS__);\
                t = readtime() - t;\
                if (t*tcount <= tsum*4 && ti > 0) {\
                    tsum += t;\
                    tcount++;\
                }\
            }\
            checkasm_set_signal_handler_state(0);\
            checkasm_update_bench(tcount, tsum);\
        } else {\
            const int talt = 0; (void)talt;\
            call_new(__VA_ARGS__);\
        }\
    } while (0)
#else
#define bench_new(...) do {} while (0)
#endif

/* Alternates between two pointers. Intended to be used within bench_new()
 * calls for functions which modifies their input buffer(s) to ensure that
 * throughput, and not latency, is measured. */
#define alternate(a, b) (talt ? (b) : (a))

#define ROUND_UP(x,a) (((x)+((a)-1)) & ~((a)-1))
#define PIXEL_RECT(name, w, h) \
    ALIGN_STK_64(pixel, name##_buf, ((h)+32)*(ROUND_UP(w,64)+64) + 64,); \
    ptrdiff_t name##_stride = sizeof(pixel)*(ROUND_UP(w,64)+64); \
    (void)name##_stride; \
    pixel *name = name##_buf + (ROUND_UP(w,64)+64)*16 + 64

#define CLEAR_PIXEL_RECT(name) \
    memset(name##_buf, 0x99, sizeof(name##_buf)) \

#define DECL_CHECKASM_CHECK_FUNC(type) \
int checkasm_check_##type(const char *const file, const int line, \
                          const type *const buf1, const ptrdiff_t stride1, \
                          const type *const buf2, const ptrdiff_t stride2, \
                          const int w, const int h, const char *const name, \
                          const int align_w, const int align_h, \
                          const int padding)

DECL_CHECKASM_CHECK_FUNC(int8_t);
DECL_CHECKASM_CHECK_FUNC(int16_t);
DECL_CHECKASM_CHECK_FUNC(int32_t);
DECL_CHECKASM_CHECK_FUNC(uint8_t);
DECL_CHECKASM_CHECK_FUNC(uint16_t);
DECL_CHECKASM_CHECK_FUNC(uint32_t);

#define CONCAT(a,b) a ## b

#define checkasm_check2(prefix, ...) CONCAT(checkasm_check_, prefix)(__FILE__, __LINE__, __VA_ARGS__)
#define checkasm_check(prefix, ...) checkasm_check2(prefix, __VA_ARGS__, 0, 0, 0)

#ifdef BITDEPTH
#define checkasm_check_pixel(...) checkasm_check(PIXEL_TYPE, __VA_ARGS__)
#define checkasm_check_pixel_padded(...) checkasm_check2(PIXEL_TYPE, __VA_ARGS__, 1, 1, 8)
#define checkasm_check_pixel_padded_align(...) checkasm_check2(PIXEL_TYPE, __VA_ARGS__, 8)
#define checkasm_check_coef(...)  checkasm_check(COEF_TYPE,  __VA_ARGS__)
#endif

#endif /* DAV1D_TESTS_CHECKASM_CHECKASM_H */
