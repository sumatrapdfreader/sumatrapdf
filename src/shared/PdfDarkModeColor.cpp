/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Theme.h"

#include "PdfDarkMode.h"

// Hardcoded PDF dark mode defaults (not persisted in settings file).
static constexpr int kPreservePdfImagesMinSize = 72;

static bool gPreservePdfImagesInDarkMode = true;

DocumentColorsFollowTheme DocumentColorsFollowThemeFromString(Str v) {
    if (len(v) == 0 || str::EqI(v, StrL("off"))) {
        return DocumentColorsFollowTheme::Off;
    }
    if (str::EqI(v, StrL("smart"))) {
        return DocumentColorsFollowTheme::Smart;
    }
    if (str::EqI(v, StrL("legacy"))) {
        return DocumentColorsFollowTheme::Legacy;
    }
    // migrate pre-3.7 DocumentColorMode values
    if (str::EqI(v, StrL("auto"))) {
        return DocumentColorsFollowTheme::Smart;
    }
    if (str::EqI(v, StrL("black"))) {
        return DocumentColorsFollowTheme::Legacy;
    }
    if (str::EqI(v, StrL("none")) || str::EqI(v, StrL("light"))) {
        return DocumentColorsFollowTheme::Off;
    }
    return DocumentColorsFollowTheme::Off;
}

static const char* DocumentColorsFollowThemeToString(DocumentColorsFollowTheme mode) {
    if (mode == DocumentColorsFollowTheme::Smart) {
        return "smart";
    }
    if (mode == DocumentColorsFollowTheme::Legacy) {
        return "legacy";
    }
    return "off";
}

// PDF dark mode runtime options (not stored in settings file)
bool GetPreservePdfImagesInDarkMode() {
    return gPreservePdfImagesInDarkMode;
}

void SetPreservePdfImagesInDarkMode(bool preserve) {
    gPreservePdfImagesInDarkMode = preserve;
}

int GetPreservePdfImagesMinSize() {
    return kPreservePdfImagesMinSize;
}

bool DocumentColorsFollowThemeEnabled() {
    return GetDocumentColorsFollowTheme() != DocumentColorsFollowTheme::Off;
}

static TempStr ColorToCssHexTemp(Color c) {
    u8 r, g, b;
    UnpackColor(c, r, g, b);
    return fmt("#%02x%02x%02x", r, g, b);
}

// User CSS overlay for MuPDF reflowable documents (EPUB, HTML, FB2, MOBI, TXT).
// Empty when the effective page colors are black-on-white (nothing to override).
TempStr ReflowDocumentThemeCssTemp() {
    Color bgCol;
    Color txtCol = ThemePageRenderColors(bgCol);
    if (bgCol == kColWhite && txtCol == kColBlack) {
        return {};
    }
    TempStr bg = ColorToCssHexTemp(bgCol);
    TempStr fg = ColorToCssHexTemp(txtCol);
    TempStr link = ColorToCssHexTemp(ThemeWindowLinkColor());
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
    if (mode < DocumentColorsFollowTheme::Off || mode > DocumentColorsFollowTheme::Legacy) {
        mode = DocumentColorsFollowTheme::Off;
    }
    if (!gSettings) {
        return;
    }
    Str name(DocumentColorsFollowThemeToString(mode));
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
    if (imgRect.IsEmpty() || pageBounds.IsEmpty()) {
        return false;
    }
    float w = imgRect.dx;
    float h = imgRect.dy;
    if (w <= 0.f || h <= 0.f) {
        return false;
    }
    float pageW = pageBounds.dx;
    float pageH = pageBounds.dy;
    if (pageW <= 0.f || pageH <= 0.f) {
        return false;
    }

    float wFrac = w / pageW;
    float hFrac = h / pageH;
    float minDim = w < h ? w : h;
    float maxDim = w > h ? w : h;
    float aspect = minDim / maxDim;

    // Tall narrow or wide shallow strips (spiral margins, side shadows).
    if (aspect < 0.22f) {
        return true;
    }
    // Edge-aligned column/row spanning a substantial part of the page.
    if (wFrac < 0.20f && hFrac > 0.30f) {
        return true;
    }
    if (hFrac < 0.20f && wFrac > 0.30f) {
        return true;
    }
    return false;
}
