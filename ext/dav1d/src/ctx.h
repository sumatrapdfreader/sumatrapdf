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

#ifndef DAV1D_SRC_CTX_H
#define DAV1D_SRC_CTX_H

#include <stdint.h>

#include "common/attributes.h"

union alias64 { uint64_t u64; uint8_t u8[8]; } ATTR_ALIAS;
union alias32 { uint32_t u32; uint8_t u8[4]; } ATTR_ALIAS;
union alias16 { uint16_t u16; uint8_t u8[2]; } ATTR_ALIAS;
union alias8 { uint8_t u8; } ATTR_ALIAS;

#define set_ctx_rep4(type, var, off, val) do { \
        const uint64_t const_val = val; \
        ((union alias64 *) &var[off +  0])->u64 = const_val; \
        ((union alias64 *) &var[off +  8])->u64 = const_val; \
        ((union alias64 *) &var[off + 16])->u64 = const_val; \
        ((union alias64 *) &var[off + 24])->u64 = const_val; \
    } while (0)
#define set_ctx_rep2(type, var, off, val) do { \
        const uint64_t const_val = val; \
        ((union alias64 *) &var[off + 0])->u64 = const_val; \
        ((union alias64 *) &var[off + 8])->u64 = const_val; \
    } while (0)
#define set_ctx_rep1(typesz, var, off, val) \
    ((union alias##typesz *) &var[off])->u##typesz = val
#define case_set(var, dir, diridx, off) \
    switch (var) { \
    case  1: set_ctx( 8, dir, diridx, off, 0x01, set_ctx_rep1); break; \
    case  2: set_ctx(16, dir, diridx, off, 0x0101, set_ctx_rep1); break; \
    case  4: set_ctx(32, dir, diridx, off, 0x01010101U, set_ctx_rep1); break; \
    case  8: set_ctx(64, dir, diridx, off, 0x0101010101010101ULL, set_ctx_rep1); break; \
    case 16: set_ctx(  , dir, diridx, off, 0x0101010101010101ULL, set_ctx_rep2); break; \
    case 32: set_ctx(  , dir, diridx, off, 0x0101010101010101ULL, set_ctx_rep4); break; \
    }
#define case_set_upto16(var, dir, diridx, off) \
    switch (var) { \
    case  1: set_ctx( 8, dir, diridx, off, 0x01, set_ctx_rep1); break; \
    case  2: set_ctx(16, dir, diridx, off, 0x0101, set_ctx_rep1); break; \
    case  4: set_ctx(32, dir, diridx, off, 0x01010101U, set_ctx_rep1); break; \
    case  8: set_ctx(64, dir, diridx, off, 0x0101010101010101ULL, set_ctx_rep1); break; \
    case 16: set_ctx(  , dir, diridx, off, 0x0101010101010101ULL, set_ctx_rep2); break; \
    }
#define case_set_upto32_with_default(var, dir, diridx, off) \
    switch (var) { \
    case  1: set_ctx( 8, dir, diridx, off, 0x01, set_ctx_rep1); break; \
    case  2: set_ctx(16, dir, diridx, off, 0x0101, set_ctx_rep1); break; \
    case  4: set_ctx(32, dir, diridx, off, 0x01010101U, set_ctx_rep1); break; \
    case  8: set_ctx(64, dir, diridx, off, 0x0101010101010101ULL, set_ctx_rep1); break; \
    case 16: set_ctx(  , dir, diridx, off, 0x0101010101010101ULL, set_ctx_rep2); break; \
    case 32: set_ctx(  , dir, diridx, off, 0x0101010101010101ULL, set_ctx_rep4); break; \
    default: default_memset(dir, diridx, off, var); break; \
    }
#define case_set_upto16_with_default(var, dir, diridx, off) \
    switch (var) { \
    case  1: set_ctx( 8, dir, diridx, off, 0x01, set_ctx_rep1); break; \
    case  2: set_ctx(16, dir, diridx, off, 0x0101, set_ctx_rep1); break; \
    case  4: set_ctx(32, dir, diridx, off, 0x01010101U, set_ctx_rep1); break; \
    case  8: set_ctx(64, dir, diridx, off, 0x0101010101010101ULL, set_ctx_rep1); break; \
    case 16: set_ctx(  , dir, diridx, off, 0x0101010101010101ULL, set_ctx_rep2); break; \
    default: default_memset(dir, diridx, off, var); break; \
    }

#endif /* DAV1D_SRC_CTX_H */
