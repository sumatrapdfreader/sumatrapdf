/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"

#include "wingui/UIModels.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "DocController.h"
#include "Theme.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "PdfDarkMode.h"

static float ColorChannel01(byte v) {
    return v / 255.f;
}

static DarkModePalette BuildPaletteFromColors(COLORREF textCol, COLORREF bgCol, COLORREF linkCol) {
    byte tr, tg, tb, br, bg, bb, lr, lg, lb;
    UnpackColor(textCol, tr, tg, tb);
    UnpackColor(bgCol, br, bg, bb);
    UnpackColor(linkCol, lr, lg, lb);

    DarkModePalette p;
    p.textR = ColorChannel01(tr);
    p.textG = ColorChannel01(tg);
    p.textB = ColorChannel01(tb);
    p.bgR = ColorChannel01(br);
    p.bgG = ColorChannel01(bg);
    p.bgB = ColorChannel01(bb);
    p.linkR = ColorChannel01(lr);
    p.linkG = ColorChannel01(lg);
    p.linkB = ColorChannel01(lb);
    p.diffR = p.bgR - p.textR;
    p.diffG = p.bgG - p.textG;
    p.diffB = p.bgB - p.textB;
    return p;
}

bool DarkModeProfileUsesObjectLevel(const DarkModeProfile* profile) {
    return profile && profile->mode == PageColorMode::SmartDark;
}

bool DarkModeProfileUsesLegacyPostProcess(const DarkModeProfile* profile) {
    if (!profile) {
        return false;
    }
    return profile->mode == PageColorMode::LegacyInvert || profile->mode == PageColorMode::PreserveImages;
}

u32 PdfDarkModeComputeProfileHash(const DarkModeProfile* profile) {
    if (!profile) {
        return 0;
    }
    auto mix = [](u32 h, u32 v) -> u32 { return h * 31 + v; };
    u32 h = 0;
    h = mix(h, (u32)profile->mode);
    h = mix(h, (u32)profile->foreground);
    h = mix(h, (u32)profile->pageBackground);
    h = mix(h, (u32)profile->linkColor);
    h = mix(h, (u32)profile->preservePdfImages);
    h = mix(h, (u32)profile->preservePdfImagesMinSize);
    h = mix(h, *(u32*)&profile->options.scanImageCoverageThreshold);
    h = mix(h, *(u32*)&profile->options.minScanDominantCoverage);
    h = mix(h, *(u32*)&profile->options.maxScanAspectSkew);
    h = mix(h, (u32)profile->options.maxTextOpsForScanPage);
    h = mix(h, (u32)profile->options.maxVectorOpsForScanPage);
    h = mix(h, *(u32*)&profile->options.preserveImagePaperSoftening);
    h = mix(h, *(u32*)&profile->options.lightFillChromaThreshold);
    h = mix(h, *(u32*)&profile->options.lightFillLuminanceThreshold);
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
    COLORREF bgCol;
    COLORREF textCol = ThemePageRenderColors(bgCol);
    bool pagesDark = !IsLightColor(bgCol);
    profile->foreground = textCol;
    profile->pageBackground = bgCol;
    profile->linkColor = pagesDark ? ThemeWindowLinkColor() : 0;
    profile->strength = 1.f;
    profile->preservePdfImages = GetPreservePdfImagesInDarkMode();
    profile->preservePdfImagesMinSize = GetPreservePdfImagesMinSize();
    profile->options = PdfDarkModeCurrentOptions();
    profile->palette = BuildPaletteFromColors(textCol, bgCol, profile->linkColor);

    if (!pagesDark) {
        // mode stays Normal: the render cache's default recolor pass still
        // applies custom (light) page colors from the cache colors
        profile->hash = PdfDarkModeComputeProfileHash(profile);
        return;
    }

    if (EngineUsesDocumentColorsFollowTheme(engine)) {
        switch (GetDocumentColorsFollowTheme()) {
            case DocumentColorsFollowTheme::Legacy:
                profile->mode = PageColorMode::LegacyInvert;
                break;
            case DocumentColorsFollowTheme::Smart:
            default:
                if (EngineSupportsSmartDarkMode(engine) && PdfDarkModeUsesObjectLevel()) {
                    profile->mode = PageColorMode::SmartDark;
                } else if (profile->preservePdfImages) {
                    profile->mode = PageColorMode::PreserveImages;
                } else {
                    profile->mode = PageColorMode::LegacyInvert;
                }
                break;
        }
    }

    profile->hash = PdfDarkModeComputeProfileHash(profile);
}

bool EngineUsesDocumentColorsFollowTheme(EngineBase* engine) {
    return engine && (engine->kind == kindEngineMupdf || engine->kind == kindEngineDjVu);
}
