/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "FileHistory.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "SumatraDialogs.h"
#include "EbookSettingsDialogCommon.h"

// the dropdown's first entry: no font of our own, whatever the engine picks
Str FontDefaultLabel() {
    return Tr("(default)");
}

// the margin is typed as CSS writes it: one number for all four sides, two
// for top/bottom and left/right, or four in top-right-bottom-left order.
// anything else (or out of range) is left empty, i.e. unset
void ParseMargin(Str s, Vec<float>& out) {
    VecReset(out);
    if (len(s) == 0) {
        return;
    }
    StrVec parts;
    Split(&parts, s, StrL(" "), true);
    bool ok = true;
    for (Str part : parts) {
        if (len(part) == 0) {
            continue; // Split can hand back an empty piece
        }
        // strtof, not atof: a token that isn't a plain number (or is null,
        // which atof faults on) has to invalidate the whole value, not read
        // as 0 - that would silently mean "no margin at all"
        const char* cs = CStrTemp(part);
        char* end = nullptr;
        float v = strtof(cs, &end);
        if (end == cs || (end && *end != 0)) {
            ok = false;
            break;
        }
        VecAppend(out, v);
    }
    int n = len(out);
    ok = ok && (n == 1 || n == 2 || n == 4);
    for (int i = 0; ok && i < n; i++) {
        ok = out[i] >= 0 && out[i] <= 200;
    }
    if (!ok) {
        VecReset(out);
    }
}

TempStr MarginTextTemp(const Vec<float>* margin) {
    int n = margin ? len(*margin) : 0;
    TempStr res;
    for (int i = 0; i < n; i++) {
        TempStr one = fmt("%g", (*margin)[i]);
        res = res ? str::JoinTemp(res, StrL(" "), one) : one;
    }
    return res;
}

bool MarginEq(const Vec<float>& a, const Vec<float>* b) {
    int n = len(a);
    if (n != (b ? len(*b) : 0)) {
        return false;
    }
    for (int i = 0; i < n; i++) {
        if (a[i] != (*b)[i]) {
            return false;
        }
    }
    return true;
}
