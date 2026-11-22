/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#if defined(SUMATRA_NG)
#include "VirtKeys.h"
#endif
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
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "SumatraPDF.h"
#include "ReadingBar.h"
#include "ReadingAutoScroll.h"
#include "ReadAloudBarCommon.h"
#include "ReadingAutoScrollCommon.h"

#include "SumatraLog.h"

static void ReadingAutoScrollStart(MainWindow* win);

// Arrow keys step these. Finer than Acrobat's 0-9 so speed is tunable.
static const float kSpeeds[] = {1, 2, 3, 4, 6, 8, 12, 16, 20, 24, 32, 40, 48, 56, 64, 80, 96, 120, 160, 200, 260, 320};

int ReadingAutoScrollSpeedCount() {
    return dimofi(kSpeeds);
}

float ReadingAutoScrollSpeedAt(int idx) {
    return kSpeeds[idx];
}

static float ClampSpeed(float s) {
    if (s < kSpeeds[0]) {
        return kSpeeds[0];
    }
    if (s > kSpeeds[ReadingAutoScrollSpeedCount() - 1]) {
        return kSpeeds[ReadingAutoScrollSpeedCount() - 1];
    }
    return s;
}

// the document's own speed when it has one, else the global setting
float ReadingAutoScrollCurSpeed(WindowTab* tab) {
    DisplayModel* dm = ReadingAutoScrollModel(tab);
    if (dm && dm->GetAutoScrollSpeed() > 0) {
        return ClampSpeed(dm->GetAutoScrollSpeed());
    }
    if (!gSettings) {
        return 40;
    }
    return ClampSpeed(gSettings->readingAutoScrollSpeed);
}

int ReadingAutoScrollSpeedIdx(float s) {
    int best = 0;
    float bestD = fabsf(kSpeeds[0] - s);
    for (int i = 1; i < ReadingAutoScrollSpeedCount(); i++) {
        float d = fabsf(kSpeeds[i] - s);
        if (d < bestD) {
            best = i;
            bestD = d;
        }
    }
    return best;
}

// Remembered for the document and as the default for documents without one.
void ReadingAutoScrollSetSpeed(WindowTab* tab, float s) {
    s = ClampSpeed(s);
    DisplayModel* dm = ReadingAutoScrollModel(tab);
    if (dm) {
        dm->SetAutoScrollSpeed(s);
    }
    if (!gSettings || gSettings->readingAutoScrollSpeed == s) {
        return;
    }
    gSettings->readingAutoScrollSpeed = s;
    ScheduleSaveSettings();
}

void ReadingAutoScrollStepSpeed(WindowTab* tab, int dir) {
    int idx = ReadingAutoScrollSpeedIdx(ReadingAutoScrollCurSpeed(tab)) + dir;
    idx = limitValue(idx, 0, ReadingAutoScrollSpeedCount() - 1);
    ReadingAutoScrollSetSpeed(tab, kSpeeds[idx]);
}

// Pixels left to scroll toward dir. Outside continuous view only the current
// row is laid out, so the remaining rows are assumed to be as tall.
static i64 RemainingPx(DisplayModel* dm, int dir) {
    bool horiz = dm->ContentScrollHorizontal();
    int toward = dir * dm->ContentScrollSign();
    int pos = horiz ? dm->viewPort.x : dm->viewPort.y;
    int view = horiz ? dm->viewPort.dx : dm->viewPort.dy;
    int total = horiz ? dm->canvasSize.dx : dm->canvasSize.dy;
    i64 px = toward > 0 ? total - pos - view : pos;
    px = std::max(px, (i64)0);
    DisplayMode mode = dm->GetDisplayMode();
    if (IsContinuous(mode)) {
        return px;
    }
    int pageNo = dm->CurrentPageNo();
    int pagesLeft = dir > 0 ? dm->PageCount() - pageNo : pageNo - 1;
    int perRow = IsSingle(mode) ? 1 : 2;
    i64 rowsLeft = (pagesLeft + perRow - 1) / perRow;
    return px + (rowsLeft * total);
}

// "12 min" / "< 1 min"
static TempStr MinutesTemp(i64 sec) {
    if (sec < 60) {
        return str::DupTemp(Tr("< 1 min"));
    }
    return fmt(Tr("%d min").s, (int)((sec + 30) / 60));
}

// time left to the end of the document and, with ReadingAutoScrollMinutes,
// until the timer pauses scrolling: "12 min left, pauses in 5 min"
static TempStr ScrollTimesTemp(WindowTab* tab) {
    DisplayModel* dm = ReadingAutoScrollModel(tab);
    if (!dm) {
        return {};
    }
    int dir = tab->autoScroll.dir >= 0 ? 1 : -1;
    i64 etaSec = (i64)((float)RemainingPx(dm, dir) / ReadingAutoScrollCurSpeed(tab));
    TempStr res = fmt(Tr("%s left").s, MinutesTemp(etaSec));
    float minutes = gSettings ? gSettings->readingAutoScrollMinutes : 0;
    if (minutes <= 0) {
        return res;
    }
    i64 timerSec = std::max((i64)0, (i64)(minutes * 60) - (i64)tab->autoScroll.runSec);
    if (timerSec >= etaSec) {
        return res;
    }
    return fmt("%s \xC2\xB7 %s", res, fmt(Tr("pauses in %s").s, MinutesTemp(timerSec)));
}

TempStr ReadingAutoScrollStatusTemp(WindowTab* tab) {
    if (!tab) {
        return {};
    }
    switch (tab->autoScroll.stop) {
        case AutoScrollStop::AtEnd:
            return str::DupTemp(Tr("End of document"));
        case AutoScrollStop::TimeUp:
            return str::DupTemp(Tr("Time is up"));
        case AutoScrollStop::User:
        case AutoScrollStop::Minimized:
            return str::DupTemp(Tr("Paused"));
        case AutoScrollStop::None:
            break;
    }
    return fmt("%s \xC2\xB7 %s", Tr("Scrolling"), ScrollTimesTemp(tab));
}

DisplayModel* ReadingAutoScrollModel(WindowTab* tab) {
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

WindowTab* ReadingAutoScrollActiveTab(MainWindow* win) {
    WindowTab* tab = CurrentDocTab(win);
    if (!tab || !tab->autoScroll.on) {
        return nullptr;
    }
    return tab;
}

bool ReadingAutoScrollAtLimit(DisplayModel* dm, int dir) {
    if (!dm) {
        return true;
    }
    if (dm->ScrollFollowsRotation()) {
        if (!dm->AtContentEdge(dir)) {
            return false;
        }
        if (IsContinuous(dm->GetDisplayMode())) {
            return true;
        }
        if (dir > 0) {
            return dm->CurrentPageNo() >= dm->PageCount();
        }
        return dm->CurrentPageNo() <= 1;
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

void ReadingAutoScrollClearTab(WindowTab* tab) {
    if (tab) {
        tab->autoScroll = {};
    }
}

bool ReadingAutoScrollIsOn(MainWindow* win) {
    return ReadingAutoScrollActiveTab(win) != nullptr;
}

void ReadingAutoScrollForgetTab(WindowTab* tab) {
    if (!tab) {
        return;
    }
    MainWindow* win = tab->win;
    bool wasSession = win && ReadingAutoScrollSessionTab(win) == tab;
    ReadingAutoScrollClearTab(tab);
    if (wasSession) {
        ReadingAutoScrollHideBar(win);
    }
}

// Acrobat maps 0 (slowest) .. 9 (fastest) onto these.
static const float kDigitSpeeds[] = {8, 12, 16, 24, 36, 48, 72, 108, 160, 240};

TempStr ReadingAutoScrollLabelTemp(WindowTab* tab) {
    const char* arrow = (!tab || tab->autoScroll.dir >= 0) ? "\xE2\x86\x93" : "\xE2\x86\x91";
    return fmt("%s %d px/s", Str(arrow), (int)(ReadingAutoScrollCurSpeed(tab) + 0.5f));
}

void ReadingAutoScrollStop(MainWindow* win) {
    WindowTab* tab = ReadingAutoScrollActiveTab(win);
    if (!tab) {
        ReadingAutoScrollHideBar(win);
        return;
    }
    ReadingAutoScrollClearTab(tab);
    logf("ReadingAutoScroll: stop\n");
    ReadingAutoScrollHideBar(win);
}

void ReadingAutoScrollFaster(MainWindow* win) {
    WindowTab* tab = ReadingAutoScrollActiveTab(win);
    if (!tab) {
        return;
    }
    ReadingAutoScrollStepSpeed(tab, 1);
    ReadingAutoScrollBarUpdate(win);
}

void ReadingAutoScrollSlower(MainWindow* win) {
    WindowTab* tab = ReadingAutoScrollActiveTab(win);
    if (!tab) {
        return;
    }
    ReadingAutoScrollStepSpeed(tab, -1);
    ReadingAutoScrollBarUpdate(win);
}

void ReadingAutoScrollArrowSpeed(MainWindow* win, WindowTab* tab, int keyDir) {
    // Acrobat: the arrow that matches the pan direction speeds up; the other slows.
    if (keyDir == tab->autoScroll.dir) {
        ReadingAutoScrollStepSpeed(tab, 1);
    } else {
        ReadingAutoScrollStepSpeed(tab, -1);
    }
    ReadingAutoScrollBarUpdate(win);
}

void ReadingAutoScrollDigitSpeed(MainWindow* win, int digit) {
    if (digit < 0 || digit > 9) {
        return;
    }
    WindowTab* tab = ReadingAutoScrollActiveTab(win);
    ReadingAutoScrollSetSpeed(tab, kDigitSpeeds[digit]);
    logf("ReadingAutoScroll: digit %d -> %d px/s\n", digit, (int)ReadingAutoScrollCurSpeed(tab));
    ReadingAutoScrollBarUpdate(win);
}

void ReadingAutoScrollHideBar(MainWindow* win) {
    KillReadingTimer(win);
    ReadingAutoScrollBarHide(win);
}

void ReadingAutoScrollToggle(MainWindow* win) {
    if (!win) {
        return;
    }
    if (ReadingAutoScrollActiveTab(win)) {
        ReadingAutoScrollStop(win);
        return;
    }
    ReadingAutoScrollStart(win);
}

void ReadingAutoScrollPause(MainWindow* win) {
    WindowTab* tab = ReadingAutoScrollActiveTab(win);
    if (!tab) {
        return;
    }
    AutoScroll& as = tab->autoScroll;
    if (as.stop == AutoScrollStop::AtEnd) {
        return;
    }
    if (as.stop == AutoScrollStop::None) {
        as.stop = AutoScrollStop::User;
        KillReadingTimer(win);
        as.accum = 0;
    } else {
        if (as.stop == AutoScrollStop::TimeUp) {
            // resuming after the timer ran out starts another timed run
            as.runSec = 0;
        }
        bool atEnd = ReadingAutoScrollAtLimit(ReadingAutoScrollModel(tab), as.dir);
        as.stop = atEnd ? AutoScrollStop::AtEnd : AutoScrollStop::None;
        if (!atEnd) {
            ArmReadingTimer(win, tab);
        }
    }
    logf("ReadingAutoScroll: stop %d\n", (int)as.stop);
    ReadingAutoScrollBarUpdate(win, true);
}

void ReadingAutoScrollReverse(MainWindow* win) {
    WindowTab* tab = ReadingAutoScrollActiveTab(win);
    if (!tab) {
        return;
    }
    tab->autoScroll.dir = tab->autoScroll.dir >= 0 ? -1 : 1;
    logf("ReadingAutoScroll: dir %d\n", tab->autoScroll.dir);
    AutoScroll& as = tab->autoScroll;
    if (ReadingAutoScrollAtLimit(ReadingAutoScrollModel(tab), as.dir)) {
        as.stop = AutoScrollStop::AtEnd;
        KillReadingTimer(win);
    } else if (as.stop == AutoScrollStop::AtEnd) {
        // turned away from the end: wait for Resume, as before
        as.stop = AutoScrollStop::User;
    } else if (!as.IsStopped()) {
        ArmReadingTimer(win, tab);
    }
    ReadingAutoScrollBarUpdate(win, true);
}

static void ReadingAutoScrollStart(MainWindow* win) {
    WindowTab* tab = CurrentDocTab(win);
    if (!tab || !ReadingAutoScrollModel(tab)) {
        return;
    }
    StopMiddleClickScroll(win);
    AutoScroll& as = tab->autoScroll;
    as.on = true;
    bool atEnd = ReadingAutoScrollAtLimit(ReadingAutoScrollModel(tab), as.dir);
    as.stop = atEnd ? AutoScrollStop::AtEnd : AutoScrollStop::None;
    as.accum = 0;
    as.runSec = 0;
    if (!atEnd) {
        ArmReadingTimer(win, tab);
    } else {
        KillReadingTimer(win);
    }
    ReadingAutoScrollBarSetTab(win, tab);
    logf("ReadingAutoScroll: start, speed %d px/s, atEnd %d\n", (int)ReadingAutoScrollCurSpeed(tab), (int)atEnd);
    ReadingAutoScrollBarUpdate(win, true);
}

void ReadingAutoScrollSyncToTab(WindowTab* tab) {
    if (!tab || !tab->win) {
        return;
    }
    MainWindow* win = tab->win;
    if (tab->IsNonDocumentTab() || !tab->autoScroll.on || !ReadingAutoScrollModel(tab)) {
        ReadingAutoScrollHideBar(win);
        return;
    }
    if (!tab->autoScroll.IsStopped()) {
        ArmReadingTimer(win, tab);
    } else {
        KillReadingTimer(win);
    }
    ReadingAutoScrollBarSetTab(win, tab);
    ReadingAutoScrollBarUpdate(win, true);
}

bool ReadingAutoScrollOnKey(MainWindow* win, int key, bool ctrl, bool shift, bool alt) {
    WindowTab* tab = ReadingAutoScrollActiveTab(win);
    if (!tab) {
        return false;
    }
    if (ctrl || shift || alt) {
        return false;
    }
    if (IsFindUIVisible(win)) {
        return false;
    }

    if (key == VK_ESCAPE) {
        ReadingAutoScrollStop(win);
        return true;
    }
    if (key == VK_SPACE) {
        ReadingAutoScrollPause(win);
        return true;
    }
    if (key == VK_DOWN) {
        ReadingAutoScrollArrowSpeed(win, tab, 1);
        return true;
    }
    if (key == VK_UP) {
        ReadingAutoScrollArrowSpeed(win, tab, -1);
        return true;
    }
    if (key == VK_LEFT) {
        if (win->ctrl) {
            win->ctrl->GoToPrevPage();
        }
        return true;
    }
    if (key == VK_RIGHT) {
        if (win->ctrl) {
            win->ctrl->GoToNextPage();
        }
        return true;
    }
    if (key == VK_OEM_MINUS || key == VK_SUBTRACT) {
        ReadingAutoScrollReverse(win);
        return true;
    }
    if (key >= '0' && key <= '9') {
        ReadingAutoScrollDigitSpeed(win, (int)(key - '0'));
        return true;
    }
    if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9) {
        ReadingAutoScrollDigitSpeed(win, (int)(key - VK_NUMPAD0));
        return true;
    }
    return false;
}

void ReadingAutoScrollTick(MainWindow* win, int elapsedMs) {
    WindowTab* tab = ReadingAutoScrollActiveTab(win);
    if (!tab || tab != ReadingAutoScrollSessionTab(win) || tab->autoScroll.IsStopped()) {
        KillReadingTimer(win);
        return;
    }
    DisplayModel* dm = ReadingAutoScrollModel(tab);
    if (!dm) {
        ReadingAutoScrollStop(win);
        return;
    }
    int dir = tab->autoScroll.dir >= 0 ? 1 : -1;
    if (ReadingAutoScrollAtLimit(dm, dir)) {
        tab->autoScroll.stop = AutoScrollStop::AtEnd;
        KillReadingTimer(win);
        ReadingAutoScrollBarUpdate(win, true);
        return;
    }

    float dt = ReadingTickSeconds(tab, elapsedMs);
    if (dt <= 0) {
        return;
    }
    if (dt > 0.1f) {
        dt = 0.1f;
    }

    int prevSec = (int)tab->autoScroll.runSec;
    tab->autoScroll.runSec += dt;
    float minutes = gSettings ? gSettings->readingAutoScrollMinutes : 0;
    if (minutes > 0 && tab->autoScroll.runSec >= minutes * 60) {
        logf("ReadingAutoScroll: %.1f min timer is up\n", minutes);
        tab->autoScroll.stop = AutoScrollStop::TimeUp;
        KillReadingTimer(win);
        ReadingAutoScrollBarUpdate(win, true);
        return;
    }
    // refresh the time left once a second
    if ((int)tab->autoScroll.runSec != prevSec) {
        ReadingAutoScrollBarUpdate(win, true);
    }

    tab->autoScroll.accum += ReadingAutoScrollCurSpeed(tab) * (float)dir * dt;
    int dy = (int)tab->autoScroll.accum;
    tab->autoScroll.accum -= (float)dy;
    if (dy == 0) {
        return;
    }
    dm->ScrollContentBy(dy, true);
    ScheduleRepaint(win, 0);
    if (ReadingAutoScrollAtLimit(dm, dir)) {
        tab->autoScroll.stop = AutoScrollStop::AtEnd;
        KillReadingTimer(win);
        ReadingAutoScrollBarUpdate(win, true);
    }
}

// Nobody reads a minimized window: pause, and resume on restore unless the
// user paused or resumed in between.
void ReadingAutoScrollOnMinimize(MainWindow* win, bool minimized) {
    WindowTab* tab = ReadingAutoScrollActiveTab(win);
    if (!tab) {
        return;
    }
    if (minimized) {
        if (tab->autoScroll.IsStopped()) {
            return;
        }
        ReadingAutoScrollPause(win);
        tab->autoScroll.stop = AutoScrollStop::Minimized;
        return;
    }
    if (tab->autoScroll.stop == AutoScrollStop::Minimized) {
        ReadingAutoScrollPause(win);
    }
}

// the tab the bar scrolls; null when the bar is hidden or doesn't exist
WindowTab* ReadingAutoScrollSessionTab(MainWindow* win) {
    if (!win || !win->readingAutoScrollBar) {
        return nullptr;
    }
    return win->readingAutoScrollTab;
}

void ReadingAutoScrollBarSetTab(MainWindow* win, WindowTab* tab) {
    if (ReadingAutoScrollBarEnsure(win)) {
        win->readingAutoScrollTab = tab;
    }
}
