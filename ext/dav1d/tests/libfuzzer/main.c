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

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dav1d_fuzzer.h"

// expects ivf input

int main(int argc, char *argv[]) {
    int ret = -1;
    FILE *f = NULL;
    int64_t fsize;
    const char *filename = NULL;
    uint8_t *data = NULL;
    size_t size = 0;

    if (LLVMFuzzerInitialize(&argc, &argv)) {
        return 1;
    }

    if (argc != 2) {
        fprintf(stdout, "Usage:\n%s fuzzing_testcase.ivf\n", argv[0]);
        return -1;
    }
    filename = argv[1];

    if (!(f = fopen(filename, "rb"))) {
        fprintf(stderr, "failed to open %s: %s\n", filename, strerror(errno));
        goto error;
    }

    if (fseeko(f, 0, SEEK_END) == -1) {
        fprintf(stderr, "fseek(%s, 0, SEEK_END) failed: %s\n", filename,
                strerror(errno));
        goto error;
    }
    if ((fsize = ftello(f)) == -1) {
        fprintf(stderr, "ftell(%s) failed: %s\n", filename, strerror(errno));
        goto error;
    }
    rewind(f);

    if (fsize < 0 || fsize > INT_MAX) {
        fprintf(stderr, "%s is too large: %"PRId64"\n", filename, fsize);
        goto error;
    }
    size = (size_t)fsize;

    if (!(data = malloc(size))) {
        fprintf(stderr, "failed to allocate: %zu bytes\n", size);
        goto error;
    }

    if (fread(data, size, 1, f) == size) {
        fprintf(stderr, "failed to read %zu bytes from %s: %s\n", size,
                filename, strerror(errno));
        goto error;
    }

    ret = LLVMFuzzerTestOneInput(data, size);

error:
    free(data);
    if (f) fclose(f);
    return ret;
}
