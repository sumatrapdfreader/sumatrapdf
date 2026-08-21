/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// What the shared image editor (ImageSaveCropResize.cpp) needs from SumatraPDF:
// decoding an image file, writing a PDF, opening what was saved, translations
// and dark mode. Another app fills the same hooks in with its own.

#include "base/Base.h"
#include "base/File.h"
#include "base/GdiPlusUtil.h"
#include "base/Pixmap.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"

#include "ImageReader.h"
#include "SumatraConfig.h"
#include "Settings.h"
#include "GlobalPrefs.h"
#include "AppSettings.h"
#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "PdfCreator.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Theme.h"
#include "DarkMode_win.h"
#include "Translations.h"
#include "ImageSaveCropResize.h"

using Gdiplus::Bitmap;

static Bitmap* LoadImageFile(Str path) {
    Str data = file::ReadFile(path);
    if (len(data) == 0) {
        return nullptr;
    }
    Bitmap* bmp = NewGdiplusBitmapFromPixmap(PixmapFromData(data));
    str::Free(data);
    return bmp;
}

// PDF date format is "D:YYYYMMDDHHmmSSOHH'mm'" where O is the relationship of
// local time to UTC (+, - or Z). Used to stamp CreationDate/ModDate (issue #949).
static TempStr FormatPdfDateTemp() {
    SYSTEMTIME lt{};
    GetLocalTime(&lt);

    TIME_ZONE_INFORMATION tzi{};
    DWORD r = GetTimeZoneInformation(&tzi);
    LONG bias = tzi.Bias; // UTC = local + bias (minutes)
    if (r == TIME_ZONE_ID_DAYLIGHT) {
        bias += tzi.DaylightBias;
    } else if (r == TIME_ZONE_ID_STANDARD) {
        bias += tzi.StandardBias;
    }
    // offset of local time from UTC, in minutes
    int off = -(int)bias;
    char sign = '+';
    if (off < 0) {
        sign = '-';
        off = -off;
    }
    int offH = off / 60;
    int offM = off % 60;
    return fmt("D:%04d%02d%02d%02d%02d%02d%c%02d'%02d'", (int)lt.wYear, (int)lt.wMonth, (int)lt.wDay, (int)lt.wHour,
               (int)lt.wMinute, (int)lt.wSecond, sign, offH, offM);
}

// Create a single-page PDF from a bitmap using PdfCreator. The image is
// converted to a 24-bit RGB pixmap (a format PDF supports) and stamped with
// the current time as CreationDate/ModDate (issue #949).
static bool SaveBitmapAsPdf(Bitmap* bmp, Str destPath) {
    if (!bmp || !destPath) {
        return false;
    }
    PdfCreator* c = new PdfCreator();
    bool ok = c->AddPageFromGdiplusBitmap(bmp, 0);
    if (ok) {
        TempStr now = FormatPdfDateTemp();
        c->SetProperty(DocProp::CreationDate, now);
        c->SetProperty(DocProp::ModificationDate, now);
        c->SetProperty(DocProp::CreatorApp, StrL("SumatraPDF"));
        ok = c->SaveToFile(destPath);
    }
    delete c;
    return ok;
}

static void OpenSavedFile(HWND parent, Str path) {
    MainWindow* win = FindMainWindowByHwnd(parent);
    if (!win && len(gWindows) > 0) {
        win = gWindows[0];
    }
    if (!win) {
        return;
    }
    LoadArgs args(path, win);
    StartLoadDocument(&args);
}

static Str TranslateStr(Str s) {
    return _TRA(s);
}

static void ApplyDarkMode(HWND hwnd) {
    DarkModeApplyToWindowAndEraseBg(hwnd);
}

static HWND GetOwnerHwnd() {
    return len(gWindows) > 0 ? gWindows[0]->hwndFrame : nullptr;
}

// fills the hooks above in with SumatraPDF's implementations
void InitImageEditHost() {
    gImageEditHost.LoadImageFile = LoadImageFile;
    gImageEditHost.SaveBitmapAsPdf = SaveBitmapAsPdf;
    gImageEditHost.OpenSavedFile = OpenSavedFile;
    gImageEditHost.Translate = TranslateStr;
    gImageEditHost.ApplyDarkMode = ApplyDarkMode;
    gImageEditHost.GetFont = GetAppFont;
    gImageEditHost.GetOwnerHwnd = GetOwnerHwnd;
    gImageEditHost.appIconId = GetAppIconID();
    gImageEditHost.escToExit = gGlobalPrefs->escToExit;
}
