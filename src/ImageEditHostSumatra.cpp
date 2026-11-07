/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// What the shared image editor (ImageSaveCropResize.cpp) needs from SumatraPDF:
// decoding an image file, writing a PDF, opening what was saved, translations
// and dark mode. Another app fills the same hooks in with its own.

#include "base/Base.h"
#include "base/File.h"
#include "base/GdiPlusUtil.h"
#include "PdfDate.h"
#include "base/Pixmap.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"

#include "ImageReader.h"
#include "SumatraConfig.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"
#include "PdfCreator.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Theme.h"
#include "DarkMode.h"
#include "Translations.h"
#include "ImageSaveCropResize.h"
#include "AppHelpersCommon.h"

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

// Create a single-page PDF from a bitmap using PdfCreator. The image is
// converted to a 24-bit RGB pixmap (a format PDF supports) and stamped with
// the current time as CreationDate/ModDate (issue #949).
static bool SaveBitmapAsPdf(Bitmap* bmp, Str destPath) {
    if (!bmp || len(destPath) == 0) {
        return false;
    }
    PdfCreator* c = new PdfCreator();
    bool ok = c->AddPageFromGdiplusBitmap(bmp, 0);
    if (ok) {
        TempStr now = FormatPdfDateNowTemp();
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
    gImageEditHost.escToExit = gSettings->escToExit;
}
