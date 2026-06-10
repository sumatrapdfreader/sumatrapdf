/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Janne Grunau
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

#include "common/attributes.h"

#include "src/cpu.h"
#include "src/arm/cpu.h"

#if HAVE_GETAUXVAL || HAVE_ELF_AUX_INFO
#include <sys/auxv.h>

#if ARCH_AARCH64

#define HWCAP_AARCH64_ASIMDDP (1 << 20)
#define HWCAP_AARCH64_SVE     (1 << 22)
#define HWCAP2_AARCH64_SVE2   (1 << 1)
#define HWCAP2_AARCH64_I8MM   (1 << 13)

COLD unsigned dav1d_get_cpu_flags_arm(void) {
    unsigned long hw_cap = dav1d_getauxval(AT_HWCAP);
    unsigned long hw_cap2 = dav1d_getauxval(AT_HWCAP2);

    unsigned flags = dav1d_get_default_cpu_flags();
    flags |= (hw_cap & HWCAP_AARCH64_ASIMDDP) ? DAV1D_ARM_CPU_FLAG_DOTPROD : 0;
    flags |= (hw_cap2 & HWCAP2_AARCH64_I8MM) ? DAV1D_ARM_CPU_FLAG_I8MM : 0;
    flags |= (hw_cap & HWCAP_AARCH64_SVE) ? DAV1D_ARM_CPU_FLAG_SVE : 0;
    flags |= (hw_cap2 & HWCAP2_AARCH64_SVE2) ? DAV1D_ARM_CPU_FLAG_SVE2 : 0;
    return flags;
}
#else  /* !ARCH_AARCH64 */

#ifndef HWCAP_ARM_NEON
#define HWCAP_ARM_NEON    (1 << 12)
#endif
#define HWCAP_ARM_ASIMDDP (1 << 24)
#define HWCAP_ARM_I8MM    (1 << 27)

COLD unsigned dav1d_get_cpu_flags_arm(void) {
    unsigned long hw_cap = dav1d_getauxval(AT_HWCAP);

    unsigned flags = dav1d_get_default_cpu_flags();
    flags |= (hw_cap & HWCAP_ARM_NEON) ? DAV1D_ARM_CPU_FLAG_NEON : 0;
    flags |= (hw_cap & HWCAP_ARM_ASIMDDP) ? DAV1D_ARM_CPU_FLAG_DOTPROD : 0;
    flags |= (hw_cap & HWCAP_ARM_I8MM) ? DAV1D_ARM_CPU_FLAG_I8MM : 0;
    return flags;
}
#endif /* ARCH_AARCH64 */

#elif defined(__APPLE__)
#include <sys/sysctl.h>

static int have_feature(const char *feature) {
    int supported = 0;
    size_t size = sizeof(supported);
    if (sysctlbyname(feature, &supported, &size, NULL, 0) != 0) {
        return 0;
    }
    return supported;
}

COLD unsigned dav1d_get_cpu_flags_arm(void) {
    unsigned flags = dav1d_get_default_cpu_flags();
    if (have_feature("hw.optional.arm.FEAT_DotProd"))
        flags |= DAV1D_ARM_CPU_FLAG_DOTPROD;
    if (have_feature("hw.optional.arm.FEAT_I8MM"))
        flags |= DAV1D_ARM_CPU_FLAG_I8MM;
    /* No SVE and SVE2 feature detection available on Apple platforms. */
    return flags;
}

#elif defined(__OpenBSD__) && ARCH_AARCH64
#include <machine/armreg.h>
#include <machine/cpu.h>
#include <sys/types.h>
#include <sys/sysctl.h>

COLD unsigned dav1d_get_cpu_flags_arm(void) {
     unsigned flags = dav1d_get_default_cpu_flags();

#ifdef CPU_ID_AA64ISAR0
     int mib[2];
     uint64_t isar0;
     uint64_t isar1;
     size_t len;

     mib[0] = CTL_MACHDEP;
     mib[1] = CPU_ID_AA64ISAR0;
     len = sizeof(isar0);
     if (sysctl(mib, 2, &isar0, &len, NULL, 0) != -1) {
         if (ID_AA64ISAR0_DP(isar0) >= ID_AA64ISAR0_DP_IMPL)
             flags |= DAV1D_ARM_CPU_FLAG_DOTPROD;
     }

     mib[0] = CTL_MACHDEP;
     mib[1] = CPU_ID_AA64ISAR1;
     len = sizeof(isar1);
     if (sysctl(mib, 2, &isar1, &len, NULL, 0) != -1) {
#ifdef ID_AA64ISAR1_I8MM_IMPL
         if (ID_AA64ISAR1_I8MM(isar1) >= ID_AA64ISAR1_I8MM_IMPL)
             flags |= DAV1D_ARM_CPU_FLAG_I8MM;
#endif
     }
#endif

     return flags;
}

#elif defined(_WIN32)
#include <windows.h>

COLD unsigned dav1d_get_cpu_flags_arm(void) {
    unsigned flags = dav1d_get_default_cpu_flags();
#ifdef PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE
    if (IsProcessorFeaturePresent(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE))
        flags |= DAV1D_ARM_CPU_FLAG_DOTPROD;
#endif
#ifdef PF_ARM_SVE_INSTRUCTIONS_AVAILABLE
    if (IsProcessorFeaturePresent(PF_ARM_SVE_INSTRUCTIONS_AVAILABLE))
        flags |= DAV1D_ARM_CPU_FLAG_SVE;
#endif
#ifdef PF_ARM_SVE2_INSTRUCTIONS_AVAILABLE
    if (IsProcessorFeaturePresent(PF_ARM_SVE2_INSTRUCTIONS_AVAILABLE))
        flags |= DAV1D_ARM_CPU_FLAG_SVE2;
#endif
#ifdef PF_ARM_SVE_I8MM_INSTRUCTIONS_AVAILABLE
    /* There's no PF_* flag that indicates whether plain I8MM is available
     * or not. But if SVE_I8MM is available, that also implies that
     * regular I8MM is available. */
    if (IsProcessorFeaturePresent(PF_ARM_SVE_I8MM_INSTRUCTIONS_AVAILABLE))
        flags |= DAV1D_ARM_CPU_FLAG_I8MM;
#endif
    return flags;
}

#elif defined(__ANDROID__) || defined(__linux__)
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static unsigned parse_proc_cpuinfo(const char *flag) {
    FILE *file = fopen("/proc/cpuinfo", "r");
    if (!file)
        return 0;

    char line_buffer[120];
    const char *line;

    size_t flaglen = strlen(flag);
    while ((line = fgets(line_buffer, sizeof(line_buffer), file))) {
        // check all occurances as whole words
        const char *found = line;
        while ((found = strstr(found, flag))) {
            if ((found == line_buffer || !isgraph(found[-1])) &&
                (isspace(found[flaglen]) || feof(file))) {
                fclose(file);
                return 1;
            }
            found += flaglen;
        }
        // if line is incomplete seek back to avoid splitting the search
        // string into two buffers
        if (!strchr(line, '\n') && strlen(line) > flaglen) {
            // use fseek since the 64 bit fseeko is only available since
            // Android API level 24 and meson defines _FILE_OFFSET_BITS
            // by default 64
            if (fseek(file, -flaglen, SEEK_CUR))
                break;
        }
    }

    fclose(file);

    return 0;
}

COLD unsigned dav1d_get_cpu_flags_arm(void) {
    unsigned flags = dav1d_get_default_cpu_flags();
    flags |= parse_proc_cpuinfo("neon") ? DAV1D_ARM_CPU_FLAG_NEON : 0;
    flags |= parse_proc_cpuinfo("asimd") ? DAV1D_ARM_CPU_FLAG_NEON : 0;
    flags |= parse_proc_cpuinfo("asimddp") ? DAV1D_ARM_CPU_FLAG_DOTPROD : 0;
    flags |= parse_proc_cpuinfo("i8mm") ? DAV1D_ARM_CPU_FLAG_I8MM : 0;
#if ARCH_AARCH64
    flags |= parse_proc_cpuinfo("sve") ? DAV1D_ARM_CPU_FLAG_SVE : 0;
    flags |= parse_proc_cpuinfo("sve2") ? DAV1D_ARM_CPU_FLAG_SVE2 : 0;
#endif /* ARCH_AARCH64 */
    return flags;
}

#else  /* Unsupported OS */

COLD unsigned dav1d_get_cpu_flags_arm(void) {
    return dav1d_get_default_cpu_flags();
}

#endif
