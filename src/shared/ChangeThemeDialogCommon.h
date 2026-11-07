/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- constants shared by orig's and ng's ChangeThemeDialog.cpp ---

static constexpr int kFollowWindowsThemeListIndex = 0;

inline int FollowThemeToIndex(DocumentColorsFollowTheme mode) {
    if (mode == DocumentColorsFollowTheme::Smart) {
        return 1;
    }
    if (mode == DocumentColorsFollowTheme::Legacy) {
        return 2;
    }
    return 0;
}

inline DocumentColorsFollowTheme FollowThemeFromIndex(int idx) {
    if (idx == 1) {
        return DocumentColorsFollowTheme::Smart;
    }
    if (idx == 2) {
        return DocumentColorsFollowTheme::Legacy;
    }
    return DocumentColorsFollowTheme::Off;
}
