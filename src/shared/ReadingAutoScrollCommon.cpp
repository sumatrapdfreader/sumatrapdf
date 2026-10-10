/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "AppSettings.h"
#include "Commands.h"
#include "Translations.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "Theme.h"
#include "FindBar.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "SumatraPDF.h"
#include "ReadingBar.h"
#include "ReadingAutoScroll.h"
#include "ReadingAutoScrollCommon.h"

#include "SumatraLog.h"

int SpeedCount() {
    return dimofi(kSpeeds);
}

static float ClampSpeed(float s) {
    if (s < kSpeeds[0]) {
        return kSpeeds[0];
    }
    if (s > kSpeeds[SpeedCount() - 1]) {
        return kSpeeds[SpeedCount() - 1];
    }
    return s;
}

float CurrentSpeed() {
    if (!gSettings) {
        return 40;
    }
    return ClampSpeed(gSettings->readingAutoScrollSpeed);
}

int ClosestSpeedIdx(float s) {
    int best = 0;
    float bestD = fabsf(kSpeeds[0] - s);
    for (int i = 1; i < SpeedCount(); i++) {
        float d = fabsf(kSpeeds[i] - s);
        if (d < bestD) {
            best = i;
            bestD = d;
        }
    }
    return best;
}

void SetSpeed(float s) {
    if (!gSettings) {
        return;
    }
    s = ClampSpeed(s);
    if (gSettings->readingAutoScrollSpeed == s) {
        return;
    }
    gSettings->readingAutoScrollSpeed = s;
    ScheduleSaveSettings();
}

void StepSpeed(int dir) {
    int idx = ClosestSpeedIdx(CurrentSpeed()) + dir;
    idx = limitValue(idx, 0, SpeedCount() - 1);
    SetSpeed(kSpeeds[idx]);
}

TempStr StatusTextTemp(WindowTab* tab) {
    if (!tab) {
        return {};
    }
    if (tab->autoScroll.atEnd) {
        return str::DupTemp(Tr("End of document"));
    }
    if (tab->autoScroll.paused) {
        return str::DupTemp(Tr("Paused"));
    }
    return str::DupTemp(Tr("Scrolling"));
}

DisplayModel* ScrollModel(WindowTab* tab) {
    if (!tab) {
        return nullptr;
    }
    return tab->AsFixed();
}

WindowTab* CurrentDocTab(MainWindow* win) {
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    if (!tab || tab->IsNonDocumentTab()) {
        return nullptr;
    }
    return tab;
}

WindowTab* ActiveTab(MainWindow* win) {
    WindowTab* tab = CurrentDocTab(win);
    if (!tab || !tab->autoScroll.on) {
        return nullptr;
    }
    return tab;
}

bool AtScrollLimit(DisplayModel* dm, int dir) {
    if (!dm) {
        return true;
    }
    if (dir > 0) {
        int maxY = std::max(0, dm->canvasSize.dy - dm->viewPort.dy);
        if (dm->viewPort.y < maxY) {
            return false;
        }
        if (IsContinuous(dm->GetDisplayMode())) {
            return true;
        }
        return dm->CurrentPageNo() >= dm->PageCount();
    }
    if (dm->viewPort.y > 0) {
        return false;
    }
    if (IsContinuous(dm->GetDisplayMode())) {
        return true;
    }
    return dm->CurrentPageNo() <= 1;
}

void ClearTabScroll(WindowTab* tab) {
    if (tab) {
        tab->autoScroll = {};
    }
}

bool ReadingAutoScrollIsOn(MainWindow* win) {
    return ActiveTab(win) != nullptr;
}

void ReadingAutoScrollForgetTab(WindowTab* tab) {
    if (!tab) {
        return;
    }
    MainWindow* win = tab->win;
    bool wasSession = win && SessionTab(win) == tab;
    ClearTabScroll(tab);
    if (wasSession) {
        ReadingAutoScrollHideBar(win);
    }
}

// Acrobat maps 0 (slowest) .. 9 (fastest) onto these.
static const float kDigitSpeeds[] = {8, 12, 16, 24, 36, 48, 72, 108, 160, 240};

TempStr SpeedLabelTemp(WindowTab* tab) {
    const char* arrow = (!tab || tab->autoScroll.dir >= 0) ? "\xE2\x86\x93" : "\xE2\x86\x91";
    return fmt("%s %d px/s", Str(arrow), (int)(CurrentSpeed() + 0.5f));
}

void ReadingAutoScrollStop(MainWindow* win) {
    WindowTab* tab = ActiveTab(win);
    if (!tab) {
        ReadingAutoScrollHideBar(win);
        return;
    }
    ClearTabScroll(tab);
    logf("ReadingAutoScroll: stop\n");
    ReadingAutoScrollHideBar(win);
}

void ReadingAutoScrollFaster(MainWindow* win) {
    if (!ActiveTab(win)) {
        return;
    }
    StepSpeed(1);
    BarUpdate(win);
}

void ReadingAutoScrollSlower(MainWindow* win) {
    if (!ActiveTab(win)) {
        return;
    }
    StepSpeed(-1);
    BarUpdate(win);
}

void ApplyArrowSpeed(MainWindow* win, WindowTab* tab, int keyDir) {
    // Acrobat: the arrow that matches the pan direction speeds up; the other slows.
    if (keyDir == tab->autoScroll.dir) {
        StepSpeed(1);
    } else {
        StepSpeed(-1);
    }
    BarUpdate(win);
}

void ApplyDigitSpeed(MainWindow* win, int digit) {
    if (digit < 0 || digit > 9) {
        return;
    }
    SetSpeed(kDigitSpeeds[digit]);
    logf("ReadingAutoScroll: digit %d -> %d px/s\n", digit, (int)CurrentSpeed());
    BarUpdate(win);
}
