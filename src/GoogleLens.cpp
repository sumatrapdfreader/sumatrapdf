/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/GdiPlusUtil.h"
#include "base/Pixmap.h"
#include "base/ScopedWin.h"
#include "base/UITask.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "GlobalPrefs.h"
#include "ChmModel.h"
#include "MarkdownModel.h"
#include "DisplayModel.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Notifications.h"
#include "Selection.h"
#include "Translations.h"
#include "WindowTab.h"
#include "GoogleLens.h"

constexpr i64 kMaxGoogleLensPngBytes = 32 * 1024 * 1024;

static void GoogleLensNotify(WindowTab* tab, Str message) {
    if (!tab || !tab->win) {
        return;
    }
    NotificationCreateArgs args;
    args.hwndParent = tab->win->hwndCanvas;
    args.tab = tab;
    args.warning = true;
    args.timeoutMs = 5000;
    args.msg = message;
    ShowNotification(args);
}

static void AppendBase64(str::Builder& out, const u8* data, size_t size) {
    static constexpr char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (size_t i = 0; i < size; i += 3) {
        u32 value = (u32)data[i] << 16;
        if (i + 1 < size) {
            value |= (u32)data[i + 1] << 8;
        }
        if (i + 2 < size) {
            value |= data[i + 2];
        }
        out.AppendChar(table[(value >> 18) & 63]);
        out.AppendChar(table[(value >> 12) & 63]);
        out.AppendChar(i + 1 < size ? table[(value >> 6) & 63] : '=');
        out.AppendChar(i + 2 < size ? table[value & 63] : '=');
    }
}

static bool WriteGoogleLensPage(WindowTab* tab, const u8* png, size_t pngSize) {
    if (pngSize == 0 || pngSize > kMaxGoogleLensPngBytes) {
        GoogleLensNotify(tab, _TRA("The selected area is too large for Google Lens."));
        return false;
    }

    str::Builder html;
    html.Append(
        StrL("<!doctype html><meta charset=\"utf-8\"><title>Google Lens</title>\n"
             "<p>Opening Google Lens...</p><script>\n"
             "const b=atob('"));
    AppendBase64(html, png, pngSize);
    html.Append(
        StrL("');const a=Uint8Array.from(b,c=>c.charCodeAt(0));"
             "const f=new File([a],'sumatra.png',{type:'image/png'});"
             "const i=document.createElement('input');i.type='file';i.name='encoded_image';"
             "const d=new DataTransfer();d.items.add(f);i.files=d.files;"
             "const form=document.createElement('form');form.method='post';"
             "form.enctype='multipart/form-data';form.action='https://lens.google.com/v3/upload?ep=cntpubb&re=df&s=4';"
             "form.appendChild(i);document.body.appendChild(form);form.submit();</script>\n"));

    TempStr path = GetTempFilePathTemp(StrL("SumatraPDF-Lens"));
    if (!path) {
        GoogleLensNotify(tab, _TRA("Could not create a temporary file for Google Lens."));
        return false;
    }
    TempStr htmlPath = str::JoinTemp(path, StrL(".html"));
    if (!file::Rename(htmlPath, path) || !file::WriteFile(htmlPath, ToStr(html))) {
        file::Delete(path);
        file::Delete(htmlPath);
        GoogleLensNotify(tab, _TRA("Could not create a temporary file for Google Lens."));
        return false;
    }
    if (!LaunchFileShell(htmlPath, {}, StrL("open"))) {
        file::Delete(htmlPath);
        GoogleLensNotify(tab, _TRA("Could not open Google Lens in the web browser."));
        return false;
    }
    return true;
}

static bool EncodePng(RenderedBitmap* bitmap, Vec<u8>& out) {
    Gdiplus::Bitmap image(bitmap->GetBitmap(), nullptr);
    CLSID pngClsid = GetGdiPlusEncoderClsid(WStrL(L"image/png"));
    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream))) {
        return false;
    }
    bool ok = image.Save(stream, &pngClsid, nullptr) == Gdiplus::Ok;
    STATSTG stat{};
    HGLOBAL global = nullptr;
    if (ok && (FAILED(stream->Stat(&stat, STATFLAG_NONAME)) || stat.cbSize.QuadPart <= 0 ||
               stat.cbSize.QuadPart > kMaxGoogleLensPngBytes || FAILED(GetHGlobalFromStream(stream, &global)))) {
        ok = false;
    }
    if (ok) {
        size_t size = (size_t)stat.cbSize.QuadPart;
        void* data = GlobalLock(global);
        if (!data) {
            ok = false;
        } else {
            out.Reset();
            out.Append((u8*)data, (int)size);
            GlobalUnlock(global);
        }
    }
    stream->Release();
    return ok;
}

void SearchWithGoogleLens(WindowTab* tab) {
    if (!tab || !tab->win || !HasPermission(Perm::InternetAccess) || !HasPermission(Perm::CopySelection)) {
        return;
    }
    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        GoogleLensNotify(tab, _TRA("Google Lens is only available for document pages."));
        return;
    }

    RenderedBitmap* bitmap = nullptr;
    if (tab->selectionOnPage && len(*tab->selectionOnPage) > 0) {
        bitmap = RenderSelectionsAsRenderedBitmap(dm, *tab->selectionOnPage);
    } else {
        int pageNo = dm->CurrentPageNo();
        if (dm->ValidPageNo(pageNo)) {
            float zoom = dm->GetZoomReal(pageNo);
            RectF pageRect = dm->GetEngine()->PageMediabox(pageNo);
            RenderPageArgs args(pageNo, zoom, dm->GetRotation(), &pageRect, RenderTarget::Export);
            bitmap = RenderedBitmapFromPixmap(dm->GetEngine()->RenderPage(args));
        }
    }
    if (!bitmap) {
        GoogleLensNotify(tab, _TRA("Could not render the page for Google Lens."));
        return;
    }

    Vec<u8> png;
    bool ok = EncodePng(bitmap, png);
    delete bitmap;
    if (!ok) {
        GoogleLensNotify(tab, _TRA("Could not encode the page for Google Lens."));
        return;
    }
    WriteGoogleLensPage(tab, png.els, len(png));
}
