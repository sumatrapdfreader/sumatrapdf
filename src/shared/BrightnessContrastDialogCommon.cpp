/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "RenderCache.h"
#include "WindowTab.h"
#include "FileHistory.h"
#include "SumatraPDF.h"
#include "BrightnessContrastDialogCommon.h"

// Applies brightness / contrast to tab's rendered pages; Remember also stores
// them for the file, so they come back when it is opened again.
void SetTabPageAdjust(WindowTab* tab, int brightness, int contrast, PageAdjustSave save) {
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    if (!dm) {
        return;
    }
    if (dm->brightness != brightness || dm->contrast != contrast) {
        dm->SetPageAdjust(brightness, contrast);
        gRenderCache->InvalidateAll(dm);
        dm->RepaintDisplay();
    }
    if (save != PageAdjustSave::Remember) {
        return;
    }
    FileState* fs = FileHistoryFindByPath(tab->filePath);
    if (fs) {
        fs->brightness = dm->brightness;
        fs->contrast = dm->contrast;
    }
    ScheduleSaveSettings();
}

// -dbg-control's TestPageAdjust: applies the values as the dialog's OK does
// (when `set`) and reports what the tab and its FileState have
TempStr PageAdjustTestTemp(WindowTab* tab, bool set, int brightness, int contrast) {
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    if (!dm) {
        return {};
    }
    if (set) {
        SetTabPageAdjust(tab, brightness, contrast, PageAdjustSave::Remember);
    }
    FileState* fs = FileHistoryFindByPath(tab->filePath);
    int fsBrightness = fs ? fs->brightness : 0;
    int fsContrast = fs ? fs->contrast : 0;
    return fmt("brightness=%d contrast=%d fs=%d,%d\n", dm->brightness, dm->contrast, fsBrightness, fsContrast);
}
