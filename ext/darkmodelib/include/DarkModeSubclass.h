/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Compatibility shim: darkmodelib 0.75 renamed the public header and
// namespace (kept through 0.76). Keep the old include/name so the rest
// of Sumatra does not have to churn with every upstream rename.

#pragma once

#include "Darkmodelib.h"

/* SumatraPDF: need with WINVER=0x601 */
#ifndef WM_DPICHANGED_AFTERPARENT
#define WM_DPICHANGED_AFTERPARENT 0x02E3
#endif

namespace DarkMode = dmlib;
