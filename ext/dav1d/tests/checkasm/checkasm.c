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

#include <stdio.h>

#include <checkasm/checkasm.h>

#include "src/cpu.h"
#include "tests/checkasm/internal.h"

/* List of tests to invoke */
static const CheckasmTest tests[] = {
    { "msac", checkasm_check_msac },
    { "pal", checkasm_check_pal },
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
static const CheckasmCpuInfo flags[] = {
#if ARCH_X86
    { "SSE2",               "sse2",      DAV1D_X86_CPU_FLAG_SSE2 },
    { "SSSE3",              "ssse3",     DAV1D_X86_CPU_FLAG_SSSE3 },
    { "SSE4.1",             "sse4",      DAV1D_X86_CPU_FLAG_SSE41 },
    { "AVX2",               "avx2",      DAV1D_X86_CPU_FLAG_AVX2 },
    { "AVX-512 (Ice Lake)", "avx512icl", DAV1D_X86_CPU_FLAG_AVX512ICL },
#elif ARCH_AARCH64 || ARCH_ARM
    { "NEON",               "neon",      DAV1D_ARM_CPU_FLAG_NEON },
    { "DOTPROD",            "dotprod",   DAV1D_ARM_CPU_FLAG_DOTPROD },
    { "I8MM",               "i8mm",      DAV1D_ARM_CPU_FLAG_I8MM },
#if ARCH_AARCH64
    { "SVE",                "sve",       DAV1D_ARM_CPU_FLAG_SVE },
    { "SVE2",               "sve2",      DAV1D_ARM_CPU_FLAG_SVE2 },
#endif /* ARCH_AARCH64 */
#elif ARCH_LOONGARCH
    { "LSX",                "lsx",       DAV1D_LOONGARCH_CPU_FLAG_LSX },
    { "LASX",               "lasx",      DAV1D_LOONGARCH_CPU_FLAG_LASX },
#elif ARCH_PPC64LE
    { "VSX",                "vsx",       DAV1D_PPC_CPU_FLAG_VSX },
    { "PWR9",               "pwr9",      DAV1D_PPC_CPU_FLAG_PWR9 },
#elif ARCH_RISCV
    { "RVV",                "rvv",       DAV1D_RISCV_CPU_FLAG_V },
#endif
    { 0 }
};

static void set_cpu_flags(CheckasmCpu flags)
{
    dav1d_set_cpu_flags_mask((unsigned) flags);
}

int main(int argc, const char *argv[])
{
#if TRIM_DSP_FUNCTIONS
    fprintf(stderr, "checkasm: reference functions unavailable, reconfigure using '-Dtrim_dsp=false'\n");
    return 0;
#endif

    CheckasmConfig cfg = {
        .cpu_flags      = flags,
        .tests          = tests,
        .set_cpu_flags  = set_cpu_flags,
    };

    dav1d_init_cpu();
    cfg.cpu = dav1d_get_cpu_flags();

    return checkasm_main(&cfg, argc, argv);
}
