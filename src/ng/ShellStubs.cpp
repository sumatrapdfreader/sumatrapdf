/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: what the layers below the shell still ask for and no ported file
// answers yet. Each entry names the step that replaces it. Delete this file
// when it is empty. `src/tools/AppStubs.cpp` is the same idea for the console
// tools, which have no shell at all.

#include "base/Base.h"

#include "DocProperties.h"
#include "Settings.h"
#include "AppSettings.h"
#include "EmbeddedResources.h"
#include "SumatraPDF.h"

bool IsMenuFontSizeDefault() {
    return true;
}
