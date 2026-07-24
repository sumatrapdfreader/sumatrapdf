#include "src/cpu.h"
#include "src/cdef.h"

decl_cdef_fn(BF(dav1d_cdef_filter_4x4, rvv));
decl_cdef_fn(BF(dav1d_cdef_filter_4x8, rvv));
decl_cdef_fn(BF(dav1d_cdef_filter_8x8, rvv));

static ALWAYS_INLINE void cdef_dsp_init_riscv(Dav1dCdefDSPContext *const c) {
    const unsigned flags = dav1d_get_cpu_flags();
    if (!(flags & DAV1D_RISCV_CPU_FLAG_V)) return;

    // c->dir = BF(dav1d_cdef_dir, rvv);
    c->fb[0] = BF(dav1d_cdef_filter_8x8, rvv);
    c->fb[1] = BF(dav1d_cdef_filter_4x8, rvv);
    c->fb[2] = BF(dav1d_cdef_filter_4x4, rvv);
}
