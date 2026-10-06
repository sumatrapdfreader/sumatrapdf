/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "Theme.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "PdfDarkMode.h"

bool DarkModeProfileUsesLegacyPostProcess(const DarkModeProfile* profile) {
    if (!profile) {
        return false;
    }
    return profile->mode == PageColorMode::LegacyInvert || profile->mode == PageColorMode::PreserveImages;
}

static u32 HashDarkModeProfile(const DarkModeProfile& profile) {
    auto mix = [](u32 h, u32 v) -> u32 { return (h * 31) + v; };
    u32 h = 0;
    h = mix(h, (u32)profile.mode);
    h = mix(h, (u32)profile.foreground);
    h = mix(h, (u32)profile.pageBackground);
    h = mix(h, (u32)profile.linkColor);
    h = mix(h, (u32)profile.preservePdfImages);
    return h;
}

void BuildViewDarkModeProfile(EngineBase* engine, DarkModeProfile* profile) {
    ReportIf(!profile);
    if (!profile) {
        return;
    }
    *profile = DarkModeProfile{};

    // unlike the fork's themes, master's themes never touch page colors:
    // dark pages come from DocumentColorsFollowTheme or custom dark
    // FixedPageUI colors, so key the dark modes off the effective page
    // background rather than the window chrome
    Color bgCol;
    Color textCol = ThemePageRenderColors(bgCol);
    bool pagesDark = !IsLightColor(bgCol);
    profile->foreground = textCol;
    profile->pageBackground = bgCol;
    profile->linkColor = pagesDark ? ThemeWindowLinkColor() : 0;
    profile->preservePdfImages = GetPreservePdfImagesInDarkMode();

    // Reflowable documents get theme colors through CSS; bitmap recoloring would invert their images.
    if (pagesDark && EngineUsesDocumentColorsFollowTheme(engine) && !EngineUsesReflowThemeCss(engine)) {
        bool preserve =
            GetDocumentColorsFollowTheme() != DocumentColorsFollowTheme::Legacy && profile->preservePdfImages;
        profile->mode = preserve ? PageColorMode::PreserveImages : PageColorMode::LegacyInvert;
    }

    profile->hash = HashDarkModeProfile(*profile);
}

bool EngineUsesDocumentColorsFollowTheme(EngineBase* engine) {
    if (!engine || engine->isImageCollection) {
        return false;
    }
    if (engine->kind == kindEngineMupdf || engine->kind == kindEngineDjVu) {
        return true;
    }
    // Native HTML-layout engines paint black-on-white pages. Recolor them with
    // FixedPageUI colors the same way as PDF (issue #6030: CHM went white when
    // recolor was narrowed to MuPDF+DjVu).
    return engine->kind == kindEngineChm || engine->kind == kindEngineEpub || engine->kind == kindEngineFb2 ||
           engine->kind == kindEngineMobi || engine->kind == kindEnginePdb || engine->kind == kindEngineHtml;
}

bool EngineUsesReflowThemeCss(EngineBase* engine) {
    return engine && engine->kind == kindEngineMupdf && engine->isReflowable;
}
