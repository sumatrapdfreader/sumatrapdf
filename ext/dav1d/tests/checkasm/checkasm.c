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
#include "tests/checkasm/checkasm.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "src/cpu.h"

#ifdef _WIN32
#include <windows.h>
#define COLOR_RED    FOREGROUND_RED
#define COLOR_GREEN  FOREGROUND_GREEN
#define COLOR_YELLOW (FOREGROUND_RED|FOREGROUND_GREEN)

static unsigned get_seed(void) {
    return GetTickCount();
}
#else
#include <unistd.h>
#include <signal.h>
#include <time.h>
#ifdef __APPLE__
#include <mach/mach_time.h>
#endif
#define COLOR_RED    1
#define COLOR_GREEN  2
#define COLOR_YELLOW 3

static unsigned get_seed(void) {
#ifdef __APPLE__
    return (unsigned) mach_absolute_time();
#elif defined(HAVE_CLOCK_GETTIME)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned) (1000000000ULL * ts.tv_sec + ts.tv_nsec);
#endif
}
#endif

/* List of tests to invoke */
static const struct {
    const char *name;
    void (*func)(void);
} tests[] = {
    { "msac", checkasm_check_msac },
    { "refmvs", checkasm_check_refmvs },
#if CONFIG_8BPC
    { "cdef_8bpc", checkasm_check_cdef_8bpc },
    { "filmgrain_8bpc", checkasm_check_filmgrain_8bpc },
    { "ipred_8bpc", checkasm_check_ipred_8bpc },
    { "itx_8bpc", checkasm_check_itx_8bpc },
    { "loopfilter_8bpc", checkasm_check_loopfilter_8bpc },
    { "looprestoration_8bpc", checkasm_check_looprestoration_8bpc },
    { "mc_8bpc", checkasm_check_mc_8bpc },
#endif
#if CONFIG_16BPC
    { "cdef_16bpc", checkasm_check_cdef_16bpc },
    { "filmgrain_16bpc", checkasm_check_filmgrain_16bpc },
    { "ipred_16bpc", checkasm_check_ipred_16bpc },
    { "itx_16bpc", checkasm_check_itx_16bpc },
    { "loopfilter_16bpc", checkasm_check_loopfilter_16bpc },
    { "looprestoration_16bpc", checkasm_check_looprestoration_16bpc },
    { "mc_16bpc", checkasm_check_mc_16bpc },
#endif
    { 0 }
};

/* List of cpu flags to check */
static const struct {
    const char *name;
    const char *suffix;
    unsigned flag;
} cpus[] = {
#if ARCH_X86
    { "SSE2",               "sse2",      DAV1D_X86_CPU_FLAG_SSE2 },
    { "SSSE3",              "ssse3",     DAV1D_X86_CPU_FLAG_SSSE3 },
    { "SSE4.1",             "sse4",      DAV1D_X86_CPU_FLAG_SSE41 },
    { "AVX2",               "avx2",      DAV1D_X86_CPU_FLAG_AVX2 },
    { "AVX-512 (Ice Lake)", "avx512icl", DAV1D_X86_CPU_FLAG_AVX512ICL },
#elif ARCH_AARCH64 || ARCH_ARM
    { "NEON",               "neon",      DAV1D_ARM_CPU_FLAG_NEON },
#elif ARCH_PPC64LE
    { "VSX",                "vsx",       DAV1D_PPC_CPU_FLAG_VSX },
#endif
    { 0 }
};

typedef struct CheckasmFuncVersion {
    struct CheckasmFuncVersion *next;
    void *func;
    int ok;
    unsigned cpu;
    int iterations;
    uint64_t cycles;
} CheckasmFuncVersion;

/* Binary search tree node */
typedef struct CheckasmFunc {
    struct CheckasmFunc *child[2];
    CheckasmFuncVersion versions;
    uint8_t color; /* 0 = red, 1 = black */
    char name[];
} CheckasmFunc;

/* Internal state */
static struct {
    CheckasmFunc *funcs;
    CheckasmFunc *current_func;
    CheckasmFuncVersion *current_func_ver;
    const char *current_test_name;
    const char *bench_pattern;
    size_t bench_pattern_len;
    int num_checked;
    int num_failed;
    int nop_time;
    unsigned cpu_flag;
    const char *cpu_flag_name;
    const char *test_name;
    unsigned seed;
    int bench_c;
    int verbose;
    int function_listing;
#if ARCH_X86_64
    void (*simd_warmup)(void);
#endif
} state;

/* float compare support code */
typedef union {
    float f;
    uint32_t i;
} intfloat;

static uint32_t xs_state[4];

static void xor128_srand(unsigned seed) {
    xs_state[0] = seed;
    xs_state[1] = ( seed & 0xffff0000) | (~seed & 0x0000ffff);
    xs_state[2] = (~seed & 0xffff0000) | ( seed & 0x0000ffff);
    xs_state[3] = ~seed;
}

// xor128 from Marsaglia, George (July 2003). "Xorshift RNGs".
//             Journal of Statistical Software. 8 (14).
//             doi:10.18637/jss.v008.i14.
int xor128_rand(void) {
    const uint32_t x = xs_state[0];
    const uint32_t t = x ^ (x << 11);

    xs_state[0] = xs_state[1];
    xs_state[1] = xs_state[2];
    xs_state[2] = xs_state[3];
    uint32_t w = xs_state[3];

    w = (w ^ (w >> 19)) ^ (t ^ (t >> 8));
    xs_state[3] = w;

    return w >> 1;
}

static int is_negative(const intfloat u) {
    return u.i >> 31;
}

int float_near_ulp(const float a, const float b, const unsigned max_ulp) {
    intfloat x, y;

    x.f = a;
    y.f = b;

    if (is_negative(x) != is_negative(y)) {
        // handle -0.0 == +0.0
        return a == b;
    }

    if (llabs((int64_t)x.i - y.i) <= max_ulp)
        return 1;

    return 0;
}

int float_near_ulp_array(const float *const a, const float *const b,
                         const unsigned max_ulp, const int len)
{
    for (int i = 0; i < len; i++)
        if (!float_near_ulp(a[i], b[i], max_ulp))
            return 0;

    return 1;
}

int float_near_abs_eps(const float a, const float b, const float eps) {
    return fabsf(a - b) < eps;
}

int float_near_abs_eps_array(const float *const a, const float *const b,
                             const float eps, const int len)
{
    for (int i = 0; i < len; i++)
        if (!float_near_abs_eps(a[i], b[i], eps))
            return 0;

    return 1;
}

int float_near_abs_eps_ulp(const float a, const float b, const float eps,
                           const unsigned max_ulp)
{
    return float_near_ulp(a, b, max_ulp) || float_near_abs_eps(a, b, eps);
}

int float_near_abs_eps_array_ulp(const float *const a, const float *const b,
                                 const float eps, const unsigned max_ulp,
                                 const int len)
{
    for (int i = 0; i < len; i++)
        if (!float_near_abs_eps_ulp(a[i], b[i], eps, max_ulp))
            return 0;

    return 1;
}

/* Print colored text to stderr if the terminal supports it */
static void color_printf(const int color, const char *const fmt, ...) {
    static int8_t use_color = -1;
    va_list arg;

#ifdef _WIN32
    static HANDLE con;
    static WORD org_attributes;

    if (use_color < 0) {
        CONSOLE_SCREEN_BUFFER_INFO con_info;
        con = GetStdHandle(STD_ERROR_HANDLE);
        if (con && con != INVALID_HANDLE_VALUE &&
            GetConsoleScreenBufferInfo(con, &con_info))
        {
            org_attributes = con_info.wAttributes;
            use_color = 1;
        } else
            use_color = 0;
    }
    if (use_color)
        SetConsoleTextAttribute(con, (org_attributes & 0xfff0) |
                                (color & 0x0f));
#else
    if (use_color < 0) {
        const char *const term = getenv("TERM");
        use_color = term && strcmp(term, "dumb") && isatty(2);
    }
    if (use_color)
        fprintf(stderr, "\x1b[%d;3%dm", (color & 0x08) >> 3, color & 0x07);
#endif

    va_start(arg, fmt);
    vfprintf(stderr, fmt, arg);
    va_end(arg);

    if (use_color) {
#ifdef _WIN32
        SetConsoleTextAttribute(con, org_attributes);
#else
        fprintf(stderr, "\x1b[0m");
#endif
    }
}

/* Deallocate a tree */
static void destroy_func_tree(CheckasmFunc *const f) {
    if (f) {
        CheckasmFuncVersion *v = f->versions.next;
        while (v) {
            CheckasmFuncVersion *next = v->next;
            free(v);
            v = next;
        }

        destroy_func_tree(f->child[0]);
        destroy_func_tree(f->child[1]);
        free(f);
    }
}

/* Allocate a zero-initialized block, clean up and exit on failure */
static void *checkasm_malloc(const size_t size) {
    void *const ptr = calloc(1, size);
    if (!ptr) {
        fprintf(stderr, "checkasm: malloc failed\n");
        destroy_func_tree(state.funcs);
        exit(1);
    }
    return ptr;
}

/* Get the suffix of the specified cpu flag */
static const char *cpu_suffix(const unsigned cpu) {
    for (int i = (int)(sizeof(cpus) / sizeof(*cpus)) - 2; i >= 0; i--)
        if (cpu & cpus[i].flag)
            return cpus[i].suffix;

    return "c";
}

#ifdef readtime
static int cmp_nop(const void *a, const void *b) {
    return *(const uint16_t*)a - *(const uint16_t*)b;
}

/* Measure the overhead of the timing code (in decicycles) */
static int measure_nop_time(void) {
    uint16_t nops[10000];
    int nop_sum = 0;

    for (int i = 0; i < 10000; i++) {
        uint64_t t = readtime();
        nops[i] = (uint16_t) (readtime() - t);
    }

    qsort(nops, 10000, sizeof(uint16_t), cmp_nop);
    for (int i = 2500; i < 7500; i++)
        nop_sum += nops[i];

    return nop_sum / 500;
}

/* Print benchmark results */
static void print_benchs(const CheckasmFunc *const f) {
    if (f) {
        print_benchs(f->child[0]);

        /* Only print functions with at least one assembly version */
        if (state.bench_c || f->versions.cpu || f->versions.next) {
            const CheckasmFuncVersion *v = &f->versions;
            do {
                if (v->iterations) {
                    const int decicycles = (int) (10*v->cycles/v->iterations -
                                                  state.nop_time) / 4;
                    printf("%s_%s: %d.%d\n", f->name, cpu_suffix(v->cpu),
                           decicycles/10, decicycles%10);
                }
            } while ((v = v->next));
        }

        print_benchs(f->child[1]);
    }
}
#endif

static void print_functions(const CheckasmFunc *const f) {
    if (f) {
        print_functions(f->child[0]);
        printf("%s\n", f->name);
        print_functions(f->child[1]);
    }
}

#define is_digit(x) ((x) >= '0' && (x) <= '9')

/* ASCIIbetical sort except preserving natural order for numbers */
static int cmp_func_names(const char *a, const char *b) {
    const char *const start = a;
    int ascii_diff, digit_diff;

    for (; !(ascii_diff = *(const unsigned char*)a -
                          *(const unsigned char*)b) && *a; a++, b++);
    for (; is_digit(*a) && is_digit(*b); a++, b++);

    if (a > start && is_digit(a[-1]) &&
        (digit_diff = is_digit(*a) - is_digit(*b)))
    {
        return digit_diff;
    }

    return ascii_diff;
}

/* Perform a tree rotation in the specified direction and return the new root */
static CheckasmFunc *rotate_tree(CheckasmFunc *const f, const int dir) {
    CheckasmFunc *const r = f->child[dir^1];
    f->child[dir^1] = r->child[dir];
    r->child[dir] = f;
    r->color = f->color;
    f->color = 0;
    return r;
}

#define is_red(f) ((f) && !(f)->color)

/* Balance a left-leaning red-black tree at the specified node */
static void balance_tree(CheckasmFunc **const root) {
    CheckasmFunc *const f = *root;

    if (is_red(f->child[0]) && is_red(f->child[1])) {
        f->color ^= 1;
        f->child[0]->color = f->child[1]->color = 1;
    }
    else if (!is_red(f->child[0]) && is_red(f->child[1]))
        *root = rotate_tree(f, 0); /* Rotate left */
    else if (is_red(f->child[0]) && is_red(f->child[0]->child[0]))
        *root = rotate_tree(f, 1); /* Rotate right */
}

/* Get a node with the specified name, creating it if it doesn't exist */
static CheckasmFunc *get_func(CheckasmFunc **const root, const char *const name) {
    CheckasmFunc *f = *root;

    if (f) {
        /* Search the tree for a matching node */
        const int cmp = cmp_func_names(name, f->name);
        if (cmp) {
            f = get_func(&f->child[cmp > 0], name);

            /* Rebalance the tree on the way up if a new node was inserted */
            if (!f->versions.func)
                balance_tree(root);
        }
    } else {
        /* Allocate and insert a new node into the tree */
        const size_t name_length = strlen(name) + 1;
        f = *root = checkasm_malloc(offsetof(CheckasmFunc, name) + name_length);
        memcpy(f->name, name, name_length);
    }

    return f;
}

checkasm_context checkasm_context_buf;

/* Crash handling: attempt to catch crashes and handle them
 * gracefully instead of just aborting abruptly. */
#ifdef _WIN32
static LONG NTAPI signal_handler(EXCEPTION_POINTERS *const e) {
    const char *err;
    switch (e->ExceptionRecord->ExceptionCode) {
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        err = "fatal arithmetic error";
        break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
        err = "illegal instruction";
        break;
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_STACK_OVERFLOW:
        err = "segmentation fault";
        break;
    default:
        return EXCEPTION_CONTINUE_SEARCH;
    }
    RemoveVectoredExceptionHandler(signal_handler);
    checkasm_fail_func(err);
    checkasm_load_context();
    return EXCEPTION_CONTINUE_EXECUTION; /* never reached, but shuts up gcc */
}
#else
static void signal_handler(const int s) {
    checkasm_set_signal_handler_state(0);
    checkasm_fail_func(s == SIGFPE ? "fatal arithmetic error" :
                       s == SIGILL ? "illegal instruction" :
                                     "segmentation fault");
    checkasm_load_context();
}
#endif

/* Perform tests and benchmarks for the specified
 * cpu flag if supported by the host */
static void check_cpu_flag(const char *const name, unsigned flag) {
    const unsigned old_cpu_flag = state.cpu_flag;

    flag |= old_cpu_flag;
    dav1d_set_cpu_flags_mask(flag);
    state.cpu_flag = dav1d_get_cpu_flags();

    if (!flag || state.cpu_flag != old_cpu_flag) {
        state.cpu_flag_name = name;
        for (int i = 0; tests[i].func; i++) {
            if (state.test_name && strcmp(tests[i].name, state.test_name))
                continue;
            xor128_srand(state.seed);
            state.current_test_name = tests[i].name;
            tests[i].func();
        }
    }
}

/* Print the name of the current CPU flag, but only do it once */
static void print_cpu_name(void) {
    if (state.cpu_flag_name) {
        color_printf(COLOR_YELLOW, "%s:\n", state.cpu_flag_name);
        state.cpu_flag_name = NULL;
    }
}

int main(int argc, char *argv[]) {
    state.seed = get_seed();

    while (argc > 1) {
        if (!strncmp(argv[1], "--help", 6)) {
            fprintf(stderr,
                    "checkasm [options] <random seed>\n"
                    "    <random seed>       Numeric value to seed the rng\n"
                    "Options:\n"
                    "    --test=<test_name>  Test only <test_name>\n"
                    "    --bench=<pattern>   Test and benchmark the functions matching <pattern>\n"
                    "    --list-functions    List available functions\n"
                    "    --list-tests        List available tests\n"
                    "    --bench-c           Benchmark the C-only functions\n"
                    "    --verbose -v        Print failures verbosely\n");
            return 0;
        } else if (!strncmp(argv[1], "--bench-c", 9)) {
            state.bench_c = 1;
        } else if (!strncmp(argv[1], "--bench", 7)) {
#ifndef readtime
            fprintf(stderr,
                    "checkasm: --bench is not supported on your system\n");
            return 1;
#endif
            if (argv[1][7] == '=') {
                state.bench_pattern = argv[1] + 8;
                state.bench_pattern_len = strlen(state.bench_pattern);
            } else
                state.bench_pattern = "";
        } else if (!strncmp(argv[1], "--test=", 7)) {
            state.test_name = argv[1] + 7;
        } else if (!strcmp(argv[1], "--list-functions")) {
            state.function_listing = 1;
        } else if (!strcmp(argv[1], "--list-tests")) {
            for (int i = 0; tests[i].name; i++)
                printf("%s\n", tests[i].name);
            return 0;
        } else if (!strcmp(argv[1], "--verbose") || !strcmp(argv[1], "-v")) {
            state.verbose = 1;
        } else {
            state.seed = (unsigned) strtoul(argv[1], NULL, 10);
        }

        argc--;
        argv++;
    }

    dav1d_init_cpu();

#ifdef readtime
    if (state.bench_pattern) {
        static int testing = 0;
        checkasm_save_context();
        if (!testing) {
            checkasm_set_signal_handler_state(1);
            testing = 1;
            readtime();
            checkasm_set_signal_handler_state(0);
        } else {
            fprintf(stderr, "checkasm: unable to access cycle counter\n");
            return 1;
        }
    }
#endif

    int ret = 0;

    if (!state.function_listing) {
        fprintf(stderr, "checkasm: using random seed %u\n", state.seed);
#if ARCH_X86_64
        void checkasm_warmup_avx2(void);
        void checkasm_warmup_avx512(void);
        const unsigned cpu_flags = dav1d_get_cpu_flags();
        if (cpu_flags & DAV1D_X86_CPU_FLAG_AVX512ICL)
            state.simd_warmup = checkasm_warmup_avx512;
        else if (cpu_flags & DAV1D_X86_CPU_FLAG_AVX2)
            state.simd_warmup = checkasm_warmup_avx2;
        checkasm_simd_warmup();
#endif
    }

    check_cpu_flag(NULL, 0);

    if (state.function_listing) {
        print_functions(state.funcs);
    } else {
        for (int i = 0; cpus[i].flag; i++)
            check_cpu_flag(cpus[i].name, cpus[i].flag);
        if (!state.num_checked) {
            fprintf(stderr, "checkasm: no tests to perform\n");
        } else if (state.num_failed) {
            fprintf(stderr, "checkasm: %d of %d tests have failed\n",
                    state.num_failed, state.num_checked);
            ret = 1;
        } else {
            fprintf(stderr, "checkasm: all %d tests passed\n", state.num_checked);
#ifdef readtime
            if (state.bench_pattern) {
                state.nop_time = measure_nop_time();
                printf("nop: %d.%d\n", state.nop_time/10, state.nop_time%10);
                print_benchs(state.funcs);
            }
#endif
        }
    }

    destroy_func_tree(state.funcs);
    return ret;
}

/* Decide whether or not the specified function needs to be tested and
 * allocate/initialize data structures if needed. Returns a pointer to a
 * reference function if the function should be tested, otherwise NULL */
void *checkasm_check_func(void *const func, const char *const name, ...) {
    char name_buf[256];
    va_list arg;

    va_start(arg, name);
    const int name_length = vsnprintf(name_buf, sizeof(name_buf), name, arg);
    va_end(arg);

    if (!func || name_length <= 0 || (size_t)name_length >= sizeof(name_buf))
        return NULL;

    state.current_func = get_func(&state.funcs, name_buf);

    if (state.function_listing) /* Save function names without running tests */
        return NULL;

    state.funcs->color = 1;
    CheckasmFuncVersion *v = &state.current_func->versions;
    void *ref = func;

    if (v->func) {
        CheckasmFuncVersion *prev;
        do {
            /* Only test functions that haven't already been tested */
            if (v->func == func)
                return NULL;

            if (v->ok)
                ref = v->func;

            prev = v;
        } while ((v = v->next));

        v = prev->next = checkasm_malloc(sizeof(CheckasmFuncVersion));
    }

    v->func = func;
    v->ok = 1;
    v->cpu = state.cpu_flag;
    state.current_func_ver = v;
    xor128_srand(state.seed);

    if (state.cpu_flag || state.bench_c)
        state.num_checked++;

    return ref;
}

/* Decide whether or not the current function needs to be benchmarked */
int checkasm_bench_func(void) {
    return !state.num_failed && state.bench_pattern &&
           !strncmp(state.current_func->name, state.bench_pattern,
                    state.bench_pattern_len);
}

/* Indicate that the current test has failed, return whether verbose printing
 * is requested. */
int checkasm_fail_func(const char *const msg, ...) {
    if (state.current_func_ver && state.current_func_ver->cpu &&
        state.current_func_ver->ok)
    {
        va_list arg;

        print_cpu_name();
        fprintf(stderr, "   %s_%s (", state.current_func->name,
                cpu_suffix(state.current_func_ver->cpu));
        va_start(arg, msg);
        vfprintf(stderr, msg, arg);
        va_end(arg);
        fprintf(stderr, ")\n");

        state.current_func_ver->ok = 0;
        state.num_failed++;
    }
    return state.verbose;
}

/* Update benchmark results of the current function */
void checkasm_update_bench(const int iterations, const uint64_t cycles) {
    state.current_func_ver->iterations += iterations;
    state.current_func_ver->cycles += cycles;
}

/* Print the outcome of all tests performed since
 * the last time this function was called */
void checkasm_report(const char *const name, ...) {
    static int prev_checked, prev_failed;
    static size_t max_length;

    if (state.num_checked > prev_checked) {
        int pad_length = (int) max_length + 4;
        va_list arg;

        print_cpu_name();
        pad_length -= fprintf(stderr, " - %s.", state.current_test_name);
        va_start(arg, name);
        pad_length -= vfprintf(stderr, name, arg);
        va_end(arg);
        fprintf(stderr, "%*c", imax(pad_length, 0) + 2, '[');

        if (state.num_failed == prev_failed)
            color_printf(COLOR_GREEN, "OK");
        else
            color_printf(COLOR_RED, "FAILED");
        fprintf(stderr, "]\n");

        prev_checked = state.num_checked;
        prev_failed  = state.num_failed;
    } else if (!state.cpu_flag) {
        /* Calculate the amount of padding required
         * to make the output vertically aligned */
        size_t length = strlen(state.current_test_name);
        va_list arg;

        va_start(arg, name);
        length += vsnprintf(NULL, 0, name, arg);
        va_end(arg);

        if (length > max_length)
            max_length = length;
    }
}

void checkasm_set_signal_handler_state(const int enabled) {
#ifdef _WIN32
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    if (enabled)
        AddVectoredExceptionHandler(0, signal_handler);
    else
        RemoveVectoredExceptionHandler(signal_handler);
#endif
#else
    void (*const handler)(int) = enabled ? signal_handler : SIG_DFL;
    signal(SIGBUS,  handler);
    signal(SIGFPE,  handler);
    signal(SIGILL,  handler);
    signal(SIGSEGV, handler);
#endif
}

static int check_err(const char *const file, const int line,
                     const char *const name, const int w, const int h,
                     int *const err)
{
    if (*err)
        return 0;
    if (!checkasm_fail_func("%s:%d", file, line))
        return 1;
    *err = 1;
    fprintf(stderr, "%s (%dx%d):\n", name, w, h);
    return 0;
}

#define DEF_CHECKASM_CHECK_FUNC(type, fmt) \
int checkasm_check_##type(const char *const file, const int line, \
                          const type *buf1, ptrdiff_t stride1, \
                          const type *buf2, ptrdiff_t stride2, \
                          const int w, int h, const char *const name, \
                          const int align_w, const int align_h, \
                          const int padding) \
{ \
    int aligned_w = (w + align_w - 1) & ~(align_w - 1); \
    int aligned_h = (h + align_h - 1) & ~(align_h - 1); \
    int err = 0; \
    stride1 /= sizeof(*buf1); \
    stride2 /= sizeof(*buf2); \
    int y = 0; \
    for (y = 0; y < h; y++) \
        if (memcmp(&buf1[y*stride1], &buf2[y*stride2], w*sizeof(*buf1))) \
            break; \
    if (y != h) { \
        if (check_err(file, line, name, w, h, &err)) \
            return 1; \
        for (y = 0; y < h; y++) { \
            for (int x = 0; x < w; x++) \
                fprintf(stderr, " " fmt, buf1[x]); \
            fprintf(stderr, "    "); \
            for (int x = 0; x < w; x++) \
                fprintf(stderr, " " fmt, buf2[x]); \
            fprintf(stderr, "    "); \
            for (int x = 0; x < w; x++) \
                fprintf(stderr, "%c", buf1[x] != buf2[x] ? 'x' : '.'); \
            buf1 += stride1; \
            buf2 += stride2; \
            fprintf(stderr, "\n"); \
        } \
        buf1 -= h*stride1; \
        buf2 -= h*stride2; \
    } \
    for (y = -padding; y < 0; y++) \
        if (memcmp(&buf1[y*stride1 - padding], &buf2[y*stride2 - padding], \
                   (w + 2*padding)*sizeof(*buf1))) { \
            if (check_err(file, line, name, w, h, &err)) \
                return 1; \
            fprintf(stderr, " overwrite above\n"); \
            break; \
        } \
    for (y = aligned_h; y < aligned_h + padding; y++) \
        if (memcmp(&buf1[y*stride1 - padding], &buf2[y*stride2 - padding], \
                   (w + 2*padding)*sizeof(*buf1))) { \
            if (check_err(file, line, name, w, h, &err)) \
                return 1; \
            fprintf(stderr, " overwrite below\n"); \
            break; \
        } \
    for (y = 0; y < h; y++) \
        if (memcmp(&buf1[y*stride1 - padding], &buf2[y*stride2 - padding], \
                   padding*sizeof(*buf1))) { \
            if (check_err(file, line, name, w, h, &err)) \
                return 1; \
            fprintf(stderr, " overwrite left\n"); \
            break; \
        } \
    for (y = 0; y < h; y++) \
        if (memcmp(&buf1[y*stride1 + aligned_w], &buf2[y*stride2 + aligned_w], \
                   padding*sizeof(*buf1))) { \
            if (check_err(file, line, name, w, h, &err)) \
                return 1; \
            fprintf(stderr, " overwrite right\n"); \
            break; \
        } \
    return err; \
}

DEF_CHECKASM_CHECK_FUNC(int8_t,   "%4d")
DEF_CHECKASM_CHECK_FUNC(int16_t,  "%6d")
DEF_CHECKASM_CHECK_FUNC(int32_t,  "%9d")
DEF_CHECKASM_CHECK_FUNC(uint8_t,  "%02x")
DEF_CHECKASM_CHECK_FUNC(uint16_t, "%04x")
DEF_CHECKASM_CHECK_FUNC(uint32_t, "%08x")

#if ARCH_X86_64
void checkasm_simd_warmup(void)
{
    if (state.simd_warmup)
        state.simd_warmup();
}
#endif
