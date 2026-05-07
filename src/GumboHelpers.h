/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern "C" {
#include "../ext/gumbo-parser/src/gumbo.h"
}

// True if `node` is an element whose tag name matches `name`
// (case-insensitive). Handles both standard HTML tags (via
// gumbo_normalized_tagname) and unknown tags (case-preserved in
// original_tag) -- the latter covers e.g. PascalCase XML element names
// like <ComicInfo>'s <Title>, <Year>, ...
bool GumboTagNameIs(const GumboNode* node, const char* name);

// First direct element child of `node` whose tag matches `name`.
// Returns nullptr if `node` isn't an element or no matching child exists.
const GumboNode* GumboFindChildByTag(const GumboNode* node, const char* name);

// Depth-first search for the first element under `node` with the given
// tag name. Walks both ELEMENT and DOCUMENT nodes.
const GumboNode* GumboFindDescendantByTag(const GumboNode* node, const char* name);

// Concatenated text content (TEXT/WHITESPACE/CDATA children) of an
// element. Returns nullptr for non-element nodes or empty content.
TempStr GumboTextContentTemp(const GumboNode* node);

// Returns a GumboOptions struct configured with our malloc/free wrappers
// and otherwise-default values. We avoid the kGumboDefaultOptions data
// extern because it's awkward to import across the libmupdf.dll boundary.
GumboOptions GumboMakeOptions();
