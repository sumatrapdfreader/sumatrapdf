/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern "C" {
#include "../ext/a-gumbo/gumbo.h"
}

bool GumboTagNameIs(const GumboNode* node, Str name);
bool GumboTagNameIsNS(const GumboNode* node, Str name, Str ns);

const GumboNode* GumboFindChildByTag(const GumboNode* node, Str name);

const GumboNode* GumboFindDescendantByTag(const GumboNode* node, Str name);
const GumboNode* GumboFindDescendantByTagNS(const GumboNode* node, Str name, Str ns);

TempStr GumboAttributeValueTemp(const GumboNode* node, const char* name);

TempStr GumboTextContentTemp(const GumboNode* node);

// Returns a GumboOptions struct configured with our malloc/free wrappers
// and otherwise-default values. We avoid the kGumboDefaultOptions data
// extern because it's awkward to import across the libsumatrapdf.dll boundary.
GumboOptions GumboMakeOptions();
GumboOptions GumboMakeXmlFragmentOptions();
