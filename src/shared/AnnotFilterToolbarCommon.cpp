/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/UITask.h"
#include "gui/Dpi.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "AppSettings.h"
#include "Annotation.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "Theme.h"
#include "Translations.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Commands.h"
#include "Toolbar.h"
#include "FilterUtil.h"
#include "AnnotSearch.h"
#include "AnnotEditToolbar.h"
#include "AnnotFilterToolbar.h"
#include "AppHelpersCommon.h"
#include "AnnotFilterToolbarCommon.h"

WindowTab* FilterTab(AnnotFilterToolbarBase* f) {
    return f && f->win ? f->win->CurrentTab() : nullptr;
}

void LoadAnnotations(AnnotFilterToolbarBase* f) {
    VecReset(f->annotations);
    WindowTab* tab = FilterTab(f);
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return;
    }
    EngineMupdfGetLoadedAnnotations(engine, f->annotations);
}

Annotation* VisibleAnnotAt(AnnotFilterToolbarBase* f, int idx) {
    if (!f || !VecIsValidIndex(f->visibleAnnots, idx)) {
        return nullptr;
    }
    return f->visibleAnnots[idx];
}

void ToggleFloatingAnnotList(MainWindow* win) {
    if (!win) {
        return;
    }
    if (IsFloatingAnnotListVisible(win)) {
        HideAnnotFilterWindow(win);
        return;
    }
    ShowAnnotFilterWindow(win);
}
