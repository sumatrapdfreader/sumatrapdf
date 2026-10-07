/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Theme.h"

#include "PdfDarkMode.h"

static bool gPreservePdfImagesInDarkMode = true;

// Accept current names and pre-3.7 DocumentColorMode aliases.
DocumentColorsFollowTheme DocumentColorsFollowThemeFromString(Str v) {
    if (str::EqI(v, StrL("smart")) || str::EqI(v, StrL("auto"))) {
        return DocumentColorsFollowTheme::Smart;
    }
    if (str::EqI(v, StrL("legacy")) || str::EqI(v, StrL("black"))) {
        return DocumentColorsFollowTheme::Legacy;
    }
    return DocumentColorsFollowTheme::Off;
}

static Str DocumentColorModeName(DocumentColorsFollowTheme mode) {
    return mode == DocumentColorsFollowTheme::Smart    ? StrL("smart")
           : mode == DocumentColorsFollowTheme::Legacy ? StrL("legacy")
                                                       : StrL("off");
}

// PDF dark mode runtime options (not stored in settings file)
bool GetPreservePdfImagesInDarkMode() {
    return gPreservePdfImagesInDarkMode;
}

void SetPreservePdfImagesInDarkMode(bool preserve) {
    gPreservePdfImagesInDarkMode = preserve;
}

bool DocumentColorsFollowThemeEnabled() {
    return GetDocumentColorsFollowTheme() != DocumentColorsFollowTheme::Off;
}

// User CSS overlay for MuPDF reflowable documents (EPUB, HTML, FB2, MOBI, TXT).
// Empty when the effective page colors are black-on-white (nothing to override).
TempStr ReflowDocumentThemeCssTemp() {
    Color bgCol;
    Color txtCol = ThemePageRenderColors(bgCol);
    if (bgCol == kColWhite && txtCol == kColBlack) {
        return {};
    }
    TempStr bg = ColorToCssTemp(bgCol);
    TempStr fg = ColorToCssTemp(txtCol);
    TempStr link = ColorToCssTemp(ThemeWindowLinkColor());
    // * first so html/body's background wins if MuPDF treats later rules as
    // stronger (a trailing * { background: transparent } would leave the
    // pixmap's white clear color showing through). Images are unaffected.
    return fmt(
        "* { color: %s !important; background-color: transparent !important; }\n"
        "html, body { background-color: %s !important; color: %s !important; }\n"
        "a, a * { color: %s !important; }\n",
        fg, bg, fg, link);
}

// an unsaved value the advanced settings dialog is previewing; -1 when there is
// none and the saved setting applies
static int gDocumentColorsFollowThemePreview = -1;

DocumentColorsFollowTheme GetDocumentColorsFollowTheme() {
    if (gDocumentColorsFollowThemePreview >= 0) {
        return (DocumentColorsFollowTheme)gDocumentColorsFollowThemePreview;
    }
    if (!gSettings || len(gSettings->documentColorsFollowTheme) == 0) {
        return DocumentColorsFollowTheme::Off;
    }
    return DocumentColorsFollowThemeFromString(gSettings->documentColorsFollowTheme);
}

// Render pages as if the setting had this value, without touching gSettings,
// so the advanced settings dialog can show what a value does before it's saved
// (and go back to the saved one when it's cancelled). The caller re-renders.
void SetDocumentColorsFollowThemePreview(DocumentColorsFollowTheme mode) {
    if (mode < DocumentColorsFollowTheme::Off || mode > DocumentColorsFollowTheme::Legacy) {
        mode = DocumentColorsFollowTheme::Off;
    }
    gDocumentColorsFollowThemePreview = (int)mode;
}

void ClearDocumentColorsFollowThemePreview() {
    gDocumentColorsFollowThemePreview = -1;
}

void SetDocumentColorsFollowTheme(DocumentColorsFollowTheme mode) {
    if (!gSettings) {
        return;
    }
    Str name = DocumentColorModeName(mode);
    if (!str::EqI(gSettings->documentColorsFollowTheme, name)) {
        str::ReplaceWithCopy(&gSettings->documentColorsFollowTheme, name);
    }
}

u32 PdfDarkModeComputeOptionsHash() {
    DarkModeProfile profile;
    BuildViewDarkModeProfile(nullptr, &profile);
    return profile.hash;
}

bool PdfDarkModeIsDecorativeStripImage(const RectF& imgRect, const RectF& pageBounds) {
    float w = imgRect.dx;
    float h = imgRect.dy;
    float pageW = pageBounds.dx;
    float pageH = pageBounds.dy;
    if (w <= 0.f || h <= 0.f || pageW <= 0.f || pageH <= 0.f) {
        return false;
    }

    float wFrac = w / pageW;
    float hFrac = h / pageH;
    float minDim = w < h ? w : h;
    float maxDim = w > h ? w : h;
    float aspect = minDim / maxDim;

    // Narrow strips or thin columns/rows spanning a substantial part of the page.
    return aspect < 0.22f || (wFrac < 0.20f && hFrac > 0.30f) || (hFrac < 0.20f && wFrac > 0.30f);
}
