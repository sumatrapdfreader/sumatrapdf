/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Crypto.h"
#include "base/File.h"
#include "base/GdiPlusUtil.h"
#include "base/Pixmap.h"
#include "base/UITask.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/win/HtmlWindow.h"

#include "Settings.h"
#include "EbookBase.h"
#include "ChmFile.h"
#include "ImageReader.h"

#include "AppTools.h"
#include "FileThumbnails.h"
#include "FileThumbnailsCommon.h"

bool HasThumbnail(FileState* fs) {
    if (!fs || len(fs->filePath) == 0) {
        return false;
    }
    // Prefer the in-memory thumbnail; only hit disk when missing.
    if (!fs->thumbnail && !LoadThumbnail(fs)) {
        return false;
    }

    TempStr bmpPath = GetThumbnailPathTemp(fs->filePath);
    if (len(bmpPath) == 0) {
        return fs->thumbnail != nullptr;
    }
    FILETIME bmpTime = file::GetModificationTime(bmpPath);
    FILETIME fileTime = file::GetModificationTime(fs->filePath);
    // delete the thumbnail if the file is newer than the thumbnail
    if (FileTimeDiffInSecs(fileTime, bmpTime) > 0) {
        FreePixmap(fs->thumbnail);
        fs->thumbnail = nullptr;
    }

    return fs->thumbnail != nullptr;
}

// takes ownership of bmp
void SetThumbnail(FileState* fs, Pixmap* bmp) {
    ReportIf(bmp && PixmapIsEmpty(bmp));
    if (!fs || len(fs->filePath) == 0 || PixmapIsEmpty(bmp)) {
        FreePixmap(bmp);
        return;
    }
    FreePixmap(fs->thumbnail);
    fs->thumbnail = PixmapToBgr(bmp);
    SaveThumbnail(fs);
}

void SaveThumbnail(FileState* fs) {
    if (!fs || !fs->thumbnail || len(fs->filePath) == 0) {
        return;
    }

    TempStr thumbnailPath = GetThumbnailPathTemp(fs->filePath);
    if (len(thumbnailPath) == 0) {
        return;
    }
    // failing to create the cache dir is environmental (antivirus, ACLs, disk full,
    // a file occupying the name) rather than a bug, so log and skip the thumbnail -
    // same as the other dir::CreateForFile callers. err == 0 means the create
    // reported success but the directory was gone when we looked.
    int err = 0;
    if (!dir::CreateForFile(thumbnailPath, &err)) {
        logf("SaveThumbnail: dir::CreateForFile('%s') failed, err=%d, file path: '%s'\n", thumbnailPath, err,
             fs->filePath);
        return;
    }
    ReportIf(!str::EndsWithI(thumbnailPath, StrL(".png")));

    Pixmap* thumbnail = fs->thumbnail;
    if (PixmapIsEmpty(thumbnail)) {
        return;
    }
    // the engine renders pages to an 8-bit palette DIB when it can, and those
    // pixels can only be read through the platform bitmap
    Pixmap* converted = nullptr;
    defer {
        FreePixmap(converted);
    };
    if (thumbnail->format == PixmapFormat::Native) {
        converted = PixmapCopyAs32bppDIB(thumbnail);
        if (!converted) {
            logf("SaveThumbnail: PixmapCopyAs32bppDIB() failed for '%s'\n", fs->filePath);
            return;
        }
        thumbnail = converted;
    }
    // Wrap (don't take ownership) so we can encode the in-memory thumbnail as PNG.
    Gdiplus::Bitmap* bmp = WrapPixmapGdiplus(thumbnail);
    if (!bmp) {
        return;
    }
    CLSID tmpClsid = GetGdiPlusEncoderClsid(L"image/png");
    WCHAR* pathW = CWStrTemp(thumbnailPath);
    Gdiplus::Status st = bmp->Save(pathW, &tmpClsid, nullptr);
    delete bmp;
    if (st != Gdiplus::Ok) {
        // gdi+ creates the file before it encodes, so a failure leaves a 0-byte
        // png behind, which reads back as a blank thumbnail (issue #5932)
        logf("SaveThumbnail: Save('%s') failed with %d\n", thumbnailPath, (int)st);
        file::Delete(thumbnailPath);
    }
}

void RemoveThumbnail(FileState* fs) {
    if (!fs || len(fs->filePath) == 0) {
        return;
    }
    if (!HasThumbnail(fs)) {
        return;
    }

    TempStr bmpPath = GetThumbnailPathTemp(fs->filePath);
    if (bmpPath) {
        file::Delete(bmpPath);
    }
    FreePixmap(fs->thumbnail);
    fs->thumbnail = nullptr;
}

struct ChmThumbnailTask : HtmlWindowCallback {
    ChmFile* doc = nullptr;
    HWND hwnd = nullptr;
    HtmlWindow* hw = nullptr;
    bool didSave = false;
    Size size;
    const OnBitmapRendered* saveThumbnail = nullptr;
    Str homeUrl;
    Vec<Str> data;
    Mutex docAccess;

    ChmThumbnailTask(ChmFile* doc, HWND hwnd, Size size, const OnBitmapRendered* saveThumbnail);
    ~ChmThumbnailTask() override;
    void StartCreateThumbnail(HtmlWindow* hw);
    bool OnBeforeNavigate(Str url, bool newWindow) override;
    void OnDocumentComplete(Str url) override;
    Str GetDataForUrl(Str url) override;
    void OnLButtonDown() override;
    void DownloadData(Str url, Str data) override;
};

static void SafeDeleteChmThumbnailTask(ChmThumbnailTask* d) {
    logf("SafeDeleteChmThumbnailTask: about to delete ChmThumbnailTask: 0x%p\n", (void*)d);
    delete d;
}

ChmThumbnailTask::ChmThumbnailTask(ChmFile* doc, HWND hwnd, Size size, const OnBitmapRendered* saveThumbnail) {
    this->doc = doc;
    this->hwnd = hwnd;
    this->size = size;
    this->saveThumbnail = saveThumbnail;
    this->didSave = false;
}

ChmThumbnailTask::~ChmThumbnailTask() {
    docAccess.Lock();
    delete hw;
    DestroyWindow(hwnd);
    delete doc;
    for (auto&& d : data) {
        str::Free(d);
    }
    docAccess.Unlock();
    delete saveThumbnail;
    str::Free(homeUrl);
}

bool ChmThumbnailTask::OnBeforeNavigate(Str /*url*/, bool newWindow) {
    return !newWindow;
}

void ChmThumbnailTask::StartCreateThumbnail(HtmlWindow* hw) {
    this->hw = hw;
    homeUrl = strconv::AnsiToUtf8(doc->homePath);
    Str trimmedHomeUrl = homeUrl;
    if (str::TrimPrefix(trimmedHomeUrl, StrL("/"))) {
        str::ReplaceWithCopy(&homeUrl, trimmedHomeUrl);
    }
    hw->NavigateToDataUrl(homeUrl);
}

Str ChmThumbnailTask::GetDataForUrl(Str url) {
    AutoUnlockMutex scope(&docAccess);
    TempStr plainUrl = url::GetFullPathTemp(url);
    Str d = str::Dup(doc->GetDataTemp(plainUrl));
    VecAppend(data, d);
    return d;
}

void ChmThumbnailTask::OnDocumentComplete(Str url) {
    if (url && url.s[0] == '/') {
        url = Str(url.s + 1, url.len - 1);
    }
    if (!str::Eq(url, homeUrl)) {
        return;
    }
    logf("ChmThumbnailTask::OnDocumentComplete: '%s'\n", url);
    if (didSave) {
        // don't crash creating .chm thumbnail
        // https://github.com/sumatrapdfreader/sumatrapdf/issues/4519
        // https://github.com/sumatrapdfreader/sumatrapdf/issues/4833
        return;
    }
    didSave = true;
    Rect area(0, 0, size.dx * 2, size.dy * 2);
    HBITMAP hbmp = hw->TakeScreenshot(area, size);
    if (hbmp) {
        RenderedBitmap* bmp = new RenderedBitmap(hbmp, size);
        saveThumbnail->Call(bmp);
    }
    // delay deleting because ~ChmThumbnailTask() deletes HtmlWindow
    // and we're currently processing HtmlWindow messages
    auto fn = MkFunc0<ChmThumbnailTask>(SafeDeleteChmThumbnailTask, this);
    uitask::Post(fn, "SafeDeleteChmThumbnailTask");
}

void ChmThumbnailTask::OnLButtonDown() {}

void ChmThumbnailTask::DownloadData(Str /*url*/, Str /*data*/) {}

void CreateChmThumbnail(Str path, const Size& size, const OnBitmapRendered* saveThumbnail) {
    // doc and window will be destroyed by the callback once it's invoked
    ChmFile* doc = ChmFile::CreateFromFile(path);
    if (!doc) {
        delete saveThumbnail;
        return;
    }

    // We render twice the size of thumbnail and scale it down
    int dx = (size.dx * 2) + GetSystemMetrics(SM_CXVSCROLL);
    int dy = (size.dy * 2) + GetSystemMetrics(SM_CYHSCROLL);
    // reusing WC_STATICW. I don't think exact class matters (WndProc
    // will be taken over by HtmlWindow anyway) but it can't be nullptr.
    HWND hwnd =
        CreateWindowExW(0, WC_STATICW, L"BrowserCapture", WS_POPUP, 0, 0, dx, dy, nullptr, nullptr, nullptr, nullptr);
    if (!hwnd) {
        delete doc;
        return;
    }
#if 0 // when debugging set to 1 to see the window
    ShowWindow(hwnd, SW_SHOW);
#endif

    ChmThumbnailTask* thumbnailTask = new ChmThumbnailTask(doc, hwnd, size, saveThumbnail);
    HtmlWindow* hw = HtmlWindow::Create(hwnd, thumbnailTask);
    if (!hw) {
        delete thumbnailTask;
        return;
    }
    // is deleted in ChmThumbnailTask::OnDocumentComplete
    thumbnailTask->StartCreateThumbnail(hw);
}
