/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/WinDynCalls.h"

/*
A centrialized location for all APIs that we need to load dynamically.
The convention is: for a function like SetThreadDescription(), we define
a function pointer DynSetThreadDescription() (with a signature matching
SetThreadDescription()).

You can test if a function is available with if (DynSetThreadDescription).

APIs available on our minimum OS (Windows 7) are called directly, not via Dyn*.
*/
void InitDynCalls() {}
