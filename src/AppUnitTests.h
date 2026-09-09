/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#if IS_DEBUG
int RunAppUnitTests(bool forAi);
#else
constexpr int RunAppUnitTests(bool) {
    return 0;
}
#endif
