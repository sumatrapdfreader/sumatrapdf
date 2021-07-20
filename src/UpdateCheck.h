/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

enum class UpdateCheck {
    UserInitiated, // user used menu "Check update"
    Automatic,     // an automatic, periodic check done at startup
};

void CheckForUpdateAsync(WindowInfo* win, UpdateCheck updateCheckType);
void UpdateSelfTo(const WCHAR* path);
