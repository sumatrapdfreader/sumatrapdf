/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// this is for adding temporary code for testing

// TODO: remove this
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

#include "utils/BaseUtil.h"
#include "utils/Archive.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/GuessFileType.h"
#include "utils/Timer.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/wingui2.h"
#include "wingui/Window.h"

#include "Settings.h"
#include "Controller.h"
#include "PalmDbReader.h"
#include "EbookBase.h"
#include "EbookDoc.h"
#include "MobiDoc.h"
#include "AppTools.h"
#include "Scratch.h"

#include "utils/Log.h"

#include "../../ext/unrar/dll.hpp"

/*
extern "C" {
#include <unarr.h>
}
*/

// META-INF/container.xml
static const char* metaInfContainerXML = R"(<?xml version="1.0"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
   <rootfiles>
      <rootfile full-path="content.opf" media-type="application/oebps-package+xml"/>
      
   </rootfiles>
</container>)";

// mimetype

static const char* mimeType = R"(application/epub+zip)";

// content.opf
static const char* contentOpf = R"(<?xml version='1.0' encoding='utf-8'?>
<package xmlns="http://www.idpf.org/2007/opf" unique-identifier="uuid_id" version="2.0">
  <metadata xmlns:calibre="http://calibre.kovidgoyal.net/2009/metadata" xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dcterms="http://purl.org/dc/terms/" xmlns:opf="http://www.idpf.org/2007/opf" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
    <dc:title>input</dc:title>
  </metadata>
  <manifest>
    <item href="input.htm" id="html" media-type="application/xhtml+xml"/>
    <item href="toc.ncx" id="ncx" media-type="application/x-dtbncx+xml"/>
  </manifest>
  <spine toc="ncx">
    <itemref idref="html"/>
  </spine>
</package>
)";

// toc.ncx
static const char* tocNcx = R"--(<?xml version='1.0' encoding='utf-8'?>
<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1" xml:lang="en-US">
  <head>
    <meta content="d3bfbfa8-a0b5-4e1c-9297-297fb130bcbc" name="dtb:uid"/>
    <meta content="2" name="dtb:depth"/>
    <meta content="0" name="dtb:totalPageCount"/>
    <meta content="0" name="dtb:maxPageNumber"/>
  </head>
  <docTitle>
    <text>input</text>
  </docTitle>
  <navMap>
    <navPoint id="urvn6G6FwfJA4zq7INOgGaD" playOrder="1">
      <navLabel>
        <text>Start</text>
      </navLabel>
      <content src="input.htm"/>
    </navPoint>
  </navMap>
</ncx>
)--";

Vec<FileData*> MobiToEpub2(const char* path) {
    Vec<FileData*> res;
    MobiDoc* doc = MobiDoc::CreateFromFile(path);
    if (!doc) {
        return res;
    }
    ByteSlice d = doc->GetHtmlData();
    {
        auto e = new FileData();
        e->name = str::Dup("input.htm");
        e->data = d.Clone();
        res.Append(e);
        logf("name: '%s', size: %d, %d images\n", e->name, (int)e->data.size(), doc->imagesCount);
    }
    {
        auto e = new FileData();
        e->name = str::Dup("META-INF\\container.xml");
        e->data = ByteSlice(metaInfContainerXML).Clone();
        res.Append(e);
    }

    {
        auto e = new FileData();
        e->name = str::Dup("mimetype");
        e->data = ByteSlice(mimeType).Clone();
        res.Append(e);
    }

    {
        auto e = new FileData();
        e->name = str::Dup("content.opf");
        e->data = ByteSlice(contentOpf).Clone();
        res.Append(e);
    }

    {
        auto e = new FileData();
        e->name = str::Dup("toc.ncx");
        e->data = ByteSlice(tocNcx).Clone();
        res.Append(e);
    }

    for (size_t i = 1; i <= doc->imagesCount; i++) {
        auto imageData = doc->GetImage(i);
        if (!imageData) {
            logf("image %d is missing\n", (int)i);
            continue;
        }
        const char* extA = GfxFileExtFromData(*imageData);
        logf("image %d, size: %d, ext: %s\n", (int)i, (int)imageData->size(), extA);
        auto e = new FileData();
        e->name = str::Format("image-%d%s", (int)i, extA);
        e->data = imageData->Clone();
        e->imageNo = (int)i;
        res.Append(e);
    }
    return res;
}

Vec<FileData*> MobiToEpub(const char* path) {
    auto files = MobiToEpub2(path);
    const WCHAR* dstDir = LR"(C:\Users\kjk\Downloads\mobiToEpub)";
    bool failed = false;
    for (auto& f : files) {
        if (failed) {
            break;
        }
        WCHAR* name = ToWstrTemp(f->name);
        AutoFreeWstr dstPath = path::Join(dstDir, name);
        bool ok = dir::CreateForFile(dstPath);
        if (!ok) {
            logf("Failed to create directory for file '%s'\n", ToUtf8Temp(dstPath));
            failed = true;
            continue;
        }
        ok = file::WriteFile(dstPath.Get(), f->data);
        if (!ok) {
            logf("Failed to write '%s'\n", ToUtf8Temp(dstPath));
            failed = true;
        } else {
            logf("Wrote '%s'\n", ToUtf8Temp(dstPath));
        }
    }
    return files;
}

constexpr const char* rarFilePath =
    R"__(x:\comics\!new4\Bride of Graphic Novels, Hardcovers and Trade Paperbacks\ABSOLUTE WATCHMEN (2005) (DC) (Minutemen-TheKid).cbr)__";

void LoadFile() {
    auto timeStart = TimeGet();
    defer {
        auto dur = TimeSinceInMs(timeStart);
        logf("LoadFile() took %.2f ms\n", dur);
    };
    ByteSlice d = file::ReadFile(rarFilePath);
    d.Free();
}

// return 1 on success. Other values for msg that we don't handle: UCM_CHANGEVOLUME, UCM_NEEDPASSWORD
static int CALLBACK unrarCallback2(UINT msg, LPARAM userData, LPARAM rarBuffer, LPARAM bytesProcessed) {
    if (UCM_PROCESSDATA != msg || !userData) {
        return -1;
    }
    ByteSlice* buf = (ByteSlice*)userData;
    size_t bytesGot = (size_t)bytesProcessed;
    if (bytesGot > buf->Left()) {
        return -1;
    }
    memcpy(buf->curr, (char*)rarBuffer, bytesGot);
    buf->curr += bytesGot;
    return 1;
}

void LoadRar() {
    auto timeStart = TimeGet();
    defer {
        auto dur = TimeSinceInMs(timeStart);
        logf("LoadRar() took %.2f ms\n", dur);
    };

    ByteSlice uncompressedBuf;

    RAROpenArchiveDataEx arcData = {nullptr};
    arcData.ArcNameW = (WCHAR*)rarFilePath;
    arcData.OpenMode = RAR_OM_EXTRACT;
    arcData.Callback = unrarCallback2;
    arcData.UserData = (LPARAM)&uncompressedBuf;

    HANDLE hArc = RAROpenArchiveEx(&arcData);
    if (!hArc || arcData.OpenResult != 0) {
        return;
    }
    size_t fileId = 0;
    while (true) {
        RARHeaderDataEx rarHeader{};
        int res = RARReadHeaderEx(hArc, &rarHeader);
        if (0 != res) {
            break;
        }

        str::TransCharsInPlace(rarHeader.FileNameW, L"\\", L"/");
        auto name = ToUtf8Temp(rarHeader.FileNameW);

        size_t fileSizeUncompressed = (size_t)rarHeader.UnpSize;
        char* data = AllocArray<char>(fileSizeUncompressed + 3);
        if (!data) {
            return;
        }
        uncompressedBuf.Set(data, fileSizeUncompressed);
        RARProcessFile(hArc, RAR_EXTRACT, nullptr, nullptr);
    }

    RARCloseArchive(hArc);
}

// ----------------

using namespace wg;

struct BrowserTestWnd : Wnd {
    Webview2Wnd* webView = nullptr;
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
    ~BrowserTestWnd() {
        delete webView;
    }
};

LRESULT BrowserTestWnd::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_CLOSE) {
        OnClose();
        return 0;
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    if (msg == WM_SIZE && webView) {
        Rect rc = ClientRect(hwnd);
        rc.x += 10;
        rc.y += 10;
        rc.dx -= 20;
        rc.dy -= 20;
        webView->SetBounds(rc);
    }
    return WndProcDefault(hwnd, msg, wparam, lparam);
}

void TestBrowser() {
    int dx = 480;
    int dy = 640;
    auto w = new BrowserTestWnd();
    {
        CreateCustomArgs args;
        args.pos = {CW_USEDEFAULT, CW_USEDEFAULT, dx, dy};
        args.title = L"test browser";
        // TODO: if set, navigate to url doesn't work
        // args.visible = false;
        HWND hwnd = w->CreateCustom(args);
        CrashIf(!hwnd);
    }

    {
        Rect rc = ClientRect(w->hwnd);
        w->webView = new Webview2Wnd();
        w->webView->dataDir = str::Dup(AppGenDataFilenameTemp("webViewData"));
        CreateCustomArgs args;
        args.parent = w->hwnd;
        dx = rc.dx;
        dy = rc.dy;
        args.pos = {10, 10, dx - 20, dy - 20};
        HWND hwnd = w->webView->Create(args);
        CrashIf(!hwnd);
        w->webView->SetIsVisible(true);
    }

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    w->webView->Navigate("https://blog.kowalczyk.info/");
    w->SetIsVisible(true);
    auto res = RunMessageLoop(nullptr, w->hwnd);
    delete w;
}
