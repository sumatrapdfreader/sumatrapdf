/******************************************************************************
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2024, Nathan Egge
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
 *****************************************************************************/

#include "src/riscv/asm.S"

// This function detects non-compliant RVV 0.7.1 hardware which reports support
//  for the V extension through HWCAP, by intentionally setting tail and mask
//  agnostic vector configurations that were only introduced in RVV 0.9 spec.
// Existing non-compliant (pre RVV 1.0) hardware will set the VILL bit in VTYPE
//  (indicating an illegal vector configuration) which is stored in the XLEN-1
//  bit position, thus a simple sign check is sufficient for detection.
// This work around is inexpensive and harmless on compliant hardware, but we
//  should still consider removing it once all non-compliant RVV 0.7.1 hardware
//  is out of service.
function has_compliant_rvv, export=1, ext=v
  vsetvli t0, zero, e8, m1, ta, ma
  csrr a0, vtype
  sgtz a0, a0
  ret
endfunc

function get_vlenb, export=1
  csrr a0, vlenb
  ret
endfunc
