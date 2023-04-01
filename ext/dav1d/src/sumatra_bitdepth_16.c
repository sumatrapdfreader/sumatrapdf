#define BITDEPTH 16

#include "cdef_apply_tmpl.c"
#include "cdef_tmpl.c"
#include "fg_apply_tmpl.c"
#include "filmgrain_tmpl.c"
#include "ipred_prepare_tmpl.c"
#include "ipred_tmpl.c"
#include "itx_tmpl.c"
#include "lf_apply_tmpl.c"
#include "loopfilter_tmpl.c"
// #include ".c"

#include "lr_apply_tmpl.c"
#include "mc_tmpl.c"
#include "recon_tmpl.c"

#if !defined(_M_ARM64)
#include "x86/cdef_init_tmpl.c"
#include "x86/filmgrain_init_tmpl.c"
#include "x86/ipred_init_tmpl.c"
#include "x86/itx_init_tmpl.c"
#include "x86/loopfilter_init_tmpl.c"
#include "x86/looprestoration_init_tmpl.c"
#undef decl_fn
#include "x86/mc_init_tmpl.c"
#endif
