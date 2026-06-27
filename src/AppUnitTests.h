/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#if defined(DEBUG)
int RunAppUnitTests();
#else
constexpr int RunAppUnitTests() {
    return 0;
}
#endif