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

#include "src/arm/cpu.h"

#if defined(__ARM_NEON) || defined(__APPLE__) || defined(_WIN32) || ARCH_AARCH64
// NEON is always available; runtime tests are not needed.
#elif defined(HAVE_GETAUXVAL) && ARCH_ARM
#include <sys/auxv.h>

#ifndef HWCAP_ARM_NEON
#define HWCAP_ARM_NEON (1 << 12)
#endif
#define NEON_HWCAP HWCAP_ARM_NEON

#elif defined(HAVE_ELF_AUX_INFO) && ARCH_ARM
#include <sys/auxv.h>

#define NEON_HWCAP HWCAP_NEON

#elif defined(__ANDROID__)
#include <stdio.h>
#include <string.h>

static unsigned parse_proc_cpuinfo(const char *flag) {
    FILE *file = fopen("/proc/cpuinfo", "r");
    if (!file)
        return 0;

    char line_buffer[120];
    const char *line;

    while ((line = fgets(line_buffer, sizeof(line_buffer), file))) {
        if (strstr(line, flag)) {
            fclose(file);
            return 1;
        }
        // if line is incomplete seek back to avoid splitting the search
        // string into two buffers
        if (!strchr(line, '\n') && strlen(line) > strlen(flag)) {
            // use fseek since the 64 bit fseeko is only available since
            // Android API level 24 and meson defines _FILE_OFFSET_BITS
            // by default 64
            if (fseek(file, -strlen(flag), SEEK_CUR))
                break;
        }
    }

    fclose(file);

    return 0;
}
#endif

COLD unsigned dav1d_get_cpu_flags_arm(void) {
    unsigned flags = 0;
#if defined(__ARM_NEON) || defined(__APPLE__) || defined(_WIN32) || ARCH_AARCH64
    flags |= DAV1D_ARM_CPU_FLAG_NEON;
#elif defined(HAVE_GETAUXVAL) && ARCH_ARM
    unsigned long hw_cap = getauxval(AT_HWCAP);
    flags |= (hw_cap & NEON_HWCAP) ? DAV1D_ARM_CPU_FLAG_NEON : 0;
#elif defined(HAVE_ELF_AUX_INFO) && ARCH_ARM
    unsigned long hw_cap = 0;
    elf_aux_info(AT_HWCAP, &hw_cap, sizeof(hw_cap));
    flags |= (hw_cap & NEON_HWCAP) ? DAV1D_ARM_CPU_FLAG_NEON : 0;
#elif defined(__ANDROID__)
    flags |= parse_proc_cpuinfo("neon") ? DAV1D_ARM_CPU_FLAG_NEON : 0;
#endif

    return flags;
}
