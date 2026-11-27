#ifndef CMARK_GFM_AUTOHEADERID_H
#define CMARK_GFM_AUTOHEADERID_H

#include "cmark-gfm-core-extensions.h"

cmark_syntax_extension *create_autoheaderid_extension(void);

/* SumatraPDF: GitHub's anchor for a heading's text (github-slugger): letters
 * lowercased, punctuation and symbols dropped except '-' and '_', each space a
 * '-'. Returns a NUL-terminated string from cmark's default allocator (free it
 * with cmark_get_default_mem_allocator()->free) and its length in *out_len. */
char *cmark_gfm_heading_slug(const char *s, size_t len, size_t *out_len);

#endif
