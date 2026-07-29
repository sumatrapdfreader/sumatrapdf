/* SumatraPDF: avx2-only subset of cdef.h (no SSE/AVX-512 asm). */
#include "src/cpu.h"
#include "src/cdef.h"

#define decl_cdef_fns(ext) \
    decl_cdef_fn(BF(dav1d_cdef_filter_4x4, ext)); \
    decl_cdef_fn(BF(dav1d_cdef_filter_4x8, ext)); \
    decl_cdef_fn(BF(dav1d_cdef_filter_8x8, ext))

decl_cdef_fns(avx2);
decl_cdef_dir_fn(BF(dav1d_cdef_dir, avx2));

static ALWAYS_INLINE void cdef_dsp_init_x86(Dav1dCdefDSPContext *const c) {
    const unsigned flags = dav1d_get_cpu_flags();

#if ARCH_X86_64
    if (!(flags & DAV1D_X86_CPU_FLAG_AVX2)) return;

    c->dir = BF(dav1d_cdef_dir, avx2);
    c->fb[0] = BF(dav1d_cdef_filter_8x8, avx2);
    c->fb[1] = BF(dav1d_cdef_filter_4x8, avx2);
    c->fb[2] = BF(dav1d_cdef_filter_4x4, avx2);
#else
    (void)flags;
    (void)c;
#endif
}
