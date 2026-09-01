/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#if IS_DEBUG
int RunAppUnitTests();
#else
constexpr int RunAppUnitTests() {
    return 0;
}
#endif