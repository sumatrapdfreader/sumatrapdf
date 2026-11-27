#ifndef CMARK_GFM_TEXMATH_H
#define CMARK_GFM_TEXMATH_H

#include "cmark-gfm-core-extensions.h"

extern cmark_node_type CMARK_NODE_MATH;
cmark_syntax_extension *create_math_extension(void);

// functions, not CMARK_NODE_MATH: libsumatrapdf.dll exports functions only
int cmark_gfm_extensions_is_math(cmark_node *node);
// 1 for $$...$$ (display), 0 for $...$ and $`...`$ (inline)
int cmark_gfm_extensions_math_is_display(cmark_node *node);

#endif
