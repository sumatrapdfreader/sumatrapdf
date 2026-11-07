/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/SettingsUtil.h"

#include "Settings.h"
#include "AppSettings.h"
#include "SumatraPDF.h"

int ScrollbarModeFromPrefs() {
    int idx = SeqStrIndexIS(gScrollbarModeNames, gSettings->scrollbars);
    // ng has no native Windows scrollbar; a saved "windows" value is smart
    if (idx < 0 || idx == kScrollbarWindows) {
        idx = kScrollbarSmart;
    }
    return idx;
}
