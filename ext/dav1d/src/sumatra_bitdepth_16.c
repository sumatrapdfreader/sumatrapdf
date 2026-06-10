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
/* looprestoration_tmpl.c is in sumatra_bitdepth_16_2.c */
#include "lr_apply_tmpl.c"
/* x86/ipred.h and x86/mc.h both define decl_fn */
#undef decl_fn
#include "mc_tmpl.c"
#include "recon_tmpl.c"
