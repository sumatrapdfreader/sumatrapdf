/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "base/File.h"
#include "base/UITask.h"
#include "base/Win.h"
#include "base/Http.h"
#include "base/Pixmap.h"
#include "base/GdiPlusUtil.h"
#include "base/GuessFileType.h"

#include <shlobj.h>
#include <commctrl.h>

#include "gui/UIModels.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Selection.h"
#include "OleDragDropCommon.h"

#include "SumatraLog.h"

// orig's OLE drag and drop helpers for Canvas.cpp: the data objects and drop
// sources for dragging text or an image out of the canvas, and reading text,
// URLs and files from a dropped data object.

// data object of the most recent drag-out, kept connected because cross-process
// drop targets can still extract data after DoDragDrop returns (e.g. Explorer
// fetches CFSTR_FILECONTENTS after IDropTarget::Drop returns)
static IDataObject* gLastDragDataObj = nullptr;

// DoDragDrop marshals the data object (CoMarshalInterface) for cross-process
// targets and never releases the stub's references, not even in
// OleUninitialize, which would leak the object. Disconnecting right after
// DoDragDrop returns breaks targets that extract data after Drop() returns,
// so we keep the object connected until the next drag-out or app exit.
void DisconnectLastDragDataObject() {
    if (!gLastDragDataObj) {
        return;
    }
    CoDisconnectObject(gLastDragDataObj, 0);
    gLastDragDataObj->Release();
    gLastDragDataObj = nullptr;
}

void FinishDragDrop(IDataObject* dataObj) {
    DisconnectLastDragDataObject();
    gLastDragDataObj = dataObj; // transfers our reference
}

// encode HBITMAP to PNG in memory using GDI+ IStream
HGLOBAL EncodeBitmapToPngGlobal(HBITMAP hbmp) {
    Gdiplus::Bitmap gdipBmp(hbmp, nullptr);
    if (gdipBmp.GetLastStatus() != Gdiplus::Ok) {
        return nullptr;
    }
    CLSID pngClsid = GetGdiPlusEncoderClsid(L"image/png");
    IStream* stream = nullptr;
    HRESULT hr = CreateStreamOnHGlobal(nullptr, FALSE, &stream);
    if (FAILED(hr) || !stream) {
        return nullptr;
    }
    Gdiplus::Status status = gdipBmp.Save(stream, &pngClsid, nullptr);
    HGLOBAL hMem = nullptr;
    if (status == Gdiplus::Ok) {
        GetHGlobalFromStream(stream, &hMem);
    }
    stream->Release();
    if (status != Gdiplus::Ok) {
        return nullptr;
    }
    return hMem;
}

// Proportional drag thumbnail (longest edge capped), Chrome-like.
// GDI+ scales the source (StretchBlt on some DIB/mapped bitmaps leaves pure white).
// Top-down 32bpp DIB with a 1px border so light pages stay visible. Caller owns HBITMAP.
HBITMAP CreateProportionalDragThumbnail(HBITMAP src, int maxEdge) {
    if (!src || maxEdge < 16) {
        return nullptr;
    }
    BITMAP bm{};
    if (!GetObject(src, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) {
        return nullptr;
    }
    int sw = bm.bmWidth;
    int sh = bm.bmHeight;
    int maxDim = sw > sh ? sw : sh;
    int dw = sw;
    int dh = sh;
    if (maxDim > maxEdge) {
        dw = (int)((i64)sw * maxEdge / maxDim);
        dh = (int)((i64)sh * maxEdge / maxDim);
        dw = std::max(dw, 1);
        dh = std::max(dh, 1);
    }

    Gdiplus::Bitmap srcGdip(src, nullptr);
    if (srcGdip.GetLastStatus() != Gdiplus::Ok) {
        return nullptr;
    }
    Gdiplus::Bitmap scaled(dw, dh, PixelFormat32bppARGB);
    if (scaled.GetLastStatus() != Gdiplus::Ok) {
        return nullptr;
    }
    {
        Gdiplus::Graphics g(&scaled);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        g.Clear(Gdiplus::Color(255, 255, 255, 255));
        g.DrawImage(&srcGdip, 0, 0, dw, dh);
        Gdiplus::Pen border(Gdiplus::Color(255, 60, 60, 60), 1.0f);
        g.DrawRectangle(&border, 0, 0, dw - 1, dh - 1);
    }

    Gdiplus::BitmapData bd{};
    Gdiplus::Rect lockRc(0, 0, dw, dh);
    if (scaled.LockBits(&lockRc, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bd) != Gdiplus::Ok) {
        return nullptr;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = dw;
    bmi.bmiHeader.biHeight = -dh; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC screenDc = GetDC(nullptr);
    HBITMAP dib = screenDc ? CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr;
    if (screenDc) {
        ReleaseDC(nullptr, screenDc);
    }
    if (!dib || !bits) {
        scaled.UnlockBits(&bd);
        if (dib) {
            DeleteObject(dib);
        }
        return nullptr;
    }

    auto* dst = (BYTE*)bits;
    const auto* srcRow = (const BYTE*)bd.Scan0;
    for (int y = 0; y < dh; y++) {
        const auto* s = srcRow + ((size_t)y * bd.Stride);
        auto* d = dst + ((size_t)y * dw * 4);
        for (int x = 0; x < dw; x++) {
            // GDI+ 32bppARGB is B,G,R,A in memory on Windows; full opacity
            d[0] = s[0];
            d[1] = s[1];
            d[2] = s[2];
            d[3] = 0xFF;
            s += 4;
            d += 4;
        }
    }
    scaled.UnlockBits(&bd);
    return dib;
}

// Build an imagelist from the thumbnail for ImageList_BeginDrag.
// Takes ownership of hbmp (always destroyed before return).
HIMAGELIST CreateDragImageList(HBITMAP hbmp) {
    if (!hbmp) {
        return nullptr;
    }
    BITMAP bm{};
    if (!GetObject(hbmp, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) {
        DeleteObject(hbmp);
        return nullptr;
    }
    HIMAGELIST himl = ImageList_Create(bm.bmWidth, bm.bmHeight, ILC_COLOR32, 1, 1);
    if (!himl) {
        DeleteObject(hbmp);
        return nullptr;
    }
    int idx = ImageList_Add(himl, hbmp, nullptr);
    DeleteObject(hbmp);
    if (idx < 0) {
        ImageList_Destroy(himl);
        return nullptr;
    }
    return himl;
}

// returns true if url looks like it could be an image URL
static bool IsImageUrl(Str url) {
    // strip query string / fragment for extension check
    int qIdx = str::IndexOfChar(url, '?');
    int hIdx = str::IndexOfChar(url, '#');
    int n = url.len;
    if (qIdx >= 0 && qIdx < n) {
        n = qIdx;
    }
    if (hIdx >= 0 && hIdx < n) {
        n = hIdx;
    }
    // check for common image extensions
    Str exts[] = {StrL(".png"),  StrL(".jpg"),  StrL(".jpeg"), StrL(".gif"), StrL(".bmp"), StrL(".tiff"), StrL(".tif"),
                  StrL(".webp"), StrL(".avif"), StrL(".heic"), StrL(".jxr"), StrL(".jp2"), StrL(".tga"),  StrL(".ico")};
    for (Str ext : exts) {
        if (n >= ext.len) {
            Str ending(url.s + n - ext.len, ext.len);
            if (str::EqI(ending, ext)) {
                return true;
            }
        }
    }
    return false;
}

// Extract text from IDataObject (tries CF_UNICODETEXT, then CF_TEXT)
TempStr GetTextFromDataObject(IDataObject* dataObj) {
    FORMATETC fmtUnicode = {CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    FORMATETC fmtAnsi = {CF_TEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM medium{};
    HRESULT hr = dataObj->GetData(&fmtUnicode, &medium);
    TempStr res;
    if (SUCCEEDED(hr) && medium.hGlobal) {
        WCHAR* w = (WCHAR*)GlobalLock(medium.hGlobal);
        res = w ? ToUtf8Temp(w) : TempStr();
        goto Cleanup;
    }
    hr = dataObj->GetData(&fmtAnsi, &medium);
    if (SUCCEEDED(hr) && medium.hGlobal) {
        char* s = (char*)GlobalLock(medium.hGlobal);
        res = s ? str::DupTemp(Str(s)) : TempStr();
        goto Cleanup;
    }
    return {};
Cleanup:
    GlobalUnlock(medium.hGlobal);
    ReleaseStgMedium(&medium);
    return res;
}

// Check if IDataObject contains a URL (registered format "UniformResourceLocatorW" or "UniformResourceLocator")
TempStr GetUrlFromDataObject(IDataObject* dataObj) {
    // try wide URL format first
    static CLIPFORMAT cfUrlW = (CLIPFORMAT)RegisterClipboardFormatW(L"UniformResourceLocatorW");
    if (cfUrlW) {
        FORMATETC fmt = {cfUrlW, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM medium{};
        HRESULT hr = dataObj->GetData(&fmt, &medium);
        if (SUCCEEDED(hr) && medium.hGlobal) {
            WCHAR* w = (WCHAR*)GlobalLock(medium.hGlobal);
            TempStr res = w ? ToUtf8Temp(w) : TempStr();
            GlobalUnlock(medium.hGlobal);
            ReleaseStgMedium(&medium);
            if (res && (str::StartsWithI(res, StrL("http://")) || str::StartsWithI(res, StrL("https://")))) {
                return res;
            }
        }
    }
    // try ANSI URL format
    static CLIPFORMAT cfUrl = (CLIPFORMAT)RegisterClipboardFormatW(L"UniformResourceLocator");
    if (cfUrl) {
        FORMATETC fmt = {cfUrl, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        STGMEDIUM medium{};
        HRESULT hr = dataObj->GetData(&fmt, &medium);
        if (SUCCEEDED(hr) && medium.hGlobal) {
            char* s = (char*)GlobalLock(medium.hGlobal);
            TempStr res = s ? str::DupTemp(Str(s)) : TempStr();
            GlobalUnlock(medium.hGlobal);
            ReleaseStgMedium(&medium);
            if (res && (str::StartsWithI(res, StrL("http://")) || str::StartsWithI(res, StrL("https://")))) {
                return res;
            }
        }
    }
    return {};
}

bool DataObjectHasFiles(IDataObject* dataObj) {
    FORMATETC fmt = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    return dataObj->QueryGetData(&fmt) == S_OK;
}

bool DataObjectHasUrl(IDataObject* dataObj) {
    TempStr url = GetUrlFromDataObject(dataObj);
    if (url && IsImageUrl(url)) {
        return true;
    }
    // also check plain text that looks like an image URL
    TempStr text = GetTextFromDataObject(dataObj);
    if (text && (str::StartsWithI(text, StrL("http://")) || str::StartsWithI(text, StrL("https://"))) &&
        IsImageUrl(text)) {
        return true;
    }
    return false;
}
