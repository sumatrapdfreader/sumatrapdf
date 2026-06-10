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

#include <stdint.h>
#include <string.h>

#include "common/attributes.h"

#include "src/cpu.h"
#include "src/x86/cpu.h"

typedef struct {
    uint32_t eax, ebx, edx, ecx;
} CpuidRegisters;

void dav1d_cpu_cpuid(CpuidRegisters *regs, unsigned leaf, unsigned subleaf);
uint64_t dav1d_cpu_xgetbv(unsigned xcr);

#define X(reg, mask) (((reg) & (mask)) == (mask))

COLD unsigned dav1d_get_cpu_flags_x86(void) {
    union {
        CpuidRegisters r;
        struct {
            uint32_t max_leaf;
            char vendor[12];
        };
    } cpu;
    dav1d_cpu_cpuid(&cpu.r, 0, 0);
    unsigned flags = dav1d_get_default_cpu_flags();

    if (cpu.max_leaf >= 1) {
        CpuidRegisters r;
        dav1d_cpu_cpuid(&r, 1, 0);
        const unsigned family = ((r.eax >> 8) & 0x0f) + ((r.eax >> 20) & 0xff);

        if (X(r.edx, 0x06008000)) /* CMOV/SSE/SSE2 */ {
            flags |= DAV1D_X86_CPU_FLAG_SSE2;
            if (X(r.ecx, 0x00000201)) /* SSE3/SSSE3 */ {
                flags |= DAV1D_X86_CPU_FLAG_SSSE3;
                if (X(r.ecx, 0x00080000)) /* SSE4.1 */
                    flags |= DAV1D_X86_CPU_FLAG_SSE41;
            }
        }
#if ARCH_X86_64
        /* We only support >128-bit SIMD on x86-64. */
        if (X(r.ecx, 0x18000000)) /* OSXSAVE/AVX */ {
            const uint64_t xcr0 = dav1d_cpu_xgetbv(0);
            if (X(xcr0, 0x00000006)) /* XMM/YMM */ {
                if (cpu.max_leaf >= 7) {
                    dav1d_cpu_cpuid(&r, 7, 0);
                    if (X(r.ebx, 0x00000128)) /* BMI1/BMI2/AVX2 */ {
                        flags |= DAV1D_X86_CPU_FLAG_AVX2;
                        if (X(xcr0, 0x000000e0)) /* ZMM/OPMASK */ {
                            if (X(r.ebx, 0xd0230000) && X(r.ecx, 0x00005f42))
                                flags |= DAV1D_X86_CPU_FLAG_AVX512ICL;
                        }
                    }
                }
            }
        }
#endif
        if (!memcmp(cpu.vendor, "AuthenticAMD", sizeof(cpu.vendor))) {
            if ((flags & DAV1D_X86_CPU_FLAG_AVX2) && family <= 0x19) {
                /* Excavator, Zen, Zen+, Zen 2, Zen 3, Zen 3+, Zen 4 */
                flags |= DAV1D_X86_CPU_FLAG_SLOW_GATHER;
            }
        }
    }

    return flags;
}
