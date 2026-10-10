/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "SumatraDialogs.h"
#include "PageGridDialogCommon.h"

SeqStrings kPageGridUnitTok = "pt\0in\0mm\0cm\0";

SeqStrings kPageGridStyleTok = "dots\0dotted\0solid\0";

constexpr float kPageGridPtPerIn = 72.f;

constexpr float kPageGridMmPerIn = 25.4f;

float PageGridToPt(float v, int unit) {
    switch (unit) {
        case 1:
            return v * kPageGridPtPerIn;
        case 2:
            return v * kPageGridPtPerIn / kPageGridMmPerIn;
        case 3:
            return v * kPageGridPtPerIn / 2.54f;
        default:
            return v;
    }
}

float PageGridFromPt(float pt, int unit) {
    switch (unit) {
        case 1:
            return pt / kPageGridPtPerIn;
        case 2:
            return pt * kPageGridMmPerIn / kPageGridPtPerIn;
        case 3:
            return pt * 2.54f / kPageGridPtPerIn;
        default:
            return pt;
    }
}

TempStr PageGridNumTemp(float v) {
    return fmt("%.4g", v);
}

Str PageGridUnitName(int i) {
    switch (i) {
        case 1:
            return Tr("inches");
        case 2:
            return Tr("millimeters");
        case 3:
            return Tr("centimeters");
        default:
            return Tr("points");
    }
}

Str PageGridStyleName(int i) {
    if (i == 1) {
        return Tr("Dotted lines");
    }
    if (i == 2) {
        return Tr("Solid lines");
    }
    return Tr("Dots");
}

PageGrid* PageGridPrefs() {
    return gSettings ? &gSettings->fixedPageUI.pageGrid : nullptr;
}

void CopyPageGridSnap(PageGridSnap& dst, const PageGrid& src, bool showGrid) {
    dst.width = src.width > 0 ? src.width : kPageGridDefaultSizePt;
    dst.height = src.height > 0 ? src.height : kPageGridDefaultSizePt;
    dst.subdivisions = src.subdivisions > 0 ? src.subdivisions : kPageGridDefaultSubdivisions;
    dst.offsetX = src.offsetX;
    dst.offsetY = src.offsetY;
    str::ReplaceWithCopy(&dst.color, src.color.s);
    str::ReplaceWithCopy(&dst.style, src.style);
    str::ReplaceWithCopy(&dst.units, src.units);
    dst.showGrid = showGrid;
}

// Restore the shipped appearance settings as a live preview. Show Grid is a
// session toggle rather than a saved setting, so leave it unchanged.
void ResetPageGridToDefaults() {
    PageGrid* pg = PageGridPrefs();
    if (!pg) {
        return;
    }
    pg->width = kPageGridDefaultSizePt;
    pg->height = kPageGridDefaultSizePt;
    pg->subdivisions = kPageGridDefaultSubdivisions;
    pg->offsetX = 0;
    pg->offsetY = 0;
    SetColorText(pg->color, SerializeColorTemp(kPageGridDefaultColor));
    str::ReplaceWithCopy(&pg->style, SeqStrByIndex(kPageGridStyleTok, kPageGridDefaultStyleIdx));
    str::ReplaceWithCopy(&pg->units, SeqStrByIndex(kPageGridUnitTok, kPageGridDefaultUnitIdx));
}

TempStr PageGridStateTemp() {
    PageGrid* pg = PageGridPrefs();
    if (!pg) {
        return str::DupTemp(StrL("ERROR no-settings"));
    }
    return fmt("show=%d width=%g height=%g subdiv=%d ox=%g oy=%g color=%s style=%s units=%s checker=%d\n",
               ShowPageGrid() ? 1 : 0, pg->width, pg->height, pg->subdivisions, pg->offsetX, pg->offsetY,
               pg->color.s ? pg->color.s : StrL(""), pg->style.s ? pg->style : StrL(""),
               pg->units.s ? pg->units : StrL(""), ShowTransparencyGrid() ? 1 : 0);
}
