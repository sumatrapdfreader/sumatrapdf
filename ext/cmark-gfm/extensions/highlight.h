#ifndef CMARK_GFM_HIGHLIGHT_H
#define CMARK_GFM_HIGHLIGHT_H

#include "cmark-gfm-core-extensions.h"

extern cmark_node_type CMARK_NODE_HIGHLIGHT;
cmark_syntax_extension *create_highlight_extension(void);

#endif
