/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"
#include "base/Launch.h"
#include "base/Pixmap.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "AppSettings.h"
#include "AppTools.h"
#include "DisplayModel.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Notifications.h"
#include "PngOptimizer.h"
#include "Selection.h"
#include "Translations.h"
#include "WindowTab.h"
#include "GoogleLens.h"

constexpr int kMaxGoogleLensPngBytes = 32 * 1024 * 1024;

static void GoogleLensNotify(WindowTab* tab, Str message) {
    if (!tab || !tab->win) {
        return;
    }
    NotificationCreateArgs args;
    SetNotifWindow(args, tab->win);
    args.tab = tab;
    args.warning = true;
    args.timeoutMs = kNotif5SecsTimeOut;
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
        GoogleLensNotify(tab, Tr("The selected area is too large for Google Lens."));
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

    TempStr dir = GetTempDirPathTemp();
    if (len(dir) == 0) {
        GoogleLensNotify(tab, Tr("Could not create a temporary file for Google Lens."));
        return false;
    }
    TempStr name = fmt("SumatraPDF-Lens-%d.html", CurrentProcessId());
    TempStr htmlPath = path::JoinTemp(dir, name);
    if (!file::WriteFile(htmlPath, ToStr(html))) {
        file::Delete(htmlPath);
        GoogleLensNotify(tab, Tr("Could not create a temporary file for Google Lens."));
        return false;
    }
    if (!LaunchFileShell(htmlPath, {}, StrL("open"))) {
        file::Delete(htmlPath);
        GoogleLensNotify(tab, Tr("Could not open Google Lens in the web browser."));
        return false;
    }
    return true;
}

enum class GoogleLensSrc {
    Auto,
    Selection,
    Page,
    Image
};

static Pixmap* PixmapForImageElement(EngineBase* engine, IPageElement* imageElement) {
    RenderedBitmap* bmp = engine->GetImageForPageElement(imageElement);
    if (!bmp) {
        return nullptr;
    }
#if OS_WIN
    Pixmap* px = PixmapFromRenderedBitmap(bmp);
    if (px && px->format == PixmapFormat::Native) {
        Pixmap* copy = PixmapCopyAs32bppDIB(px);
        FreePixmap(px);
        px = copy;
    }
    return px;
#else
    // ng: a RenderedBitmap is win32-only, so bmp is always null here
    return nullptr;
#endif
}

static Pixmap* RenderLensSelection(DisplayModel* dm, const Vec<SelectionOnPage>& selections) {
#ifdef SUMATRA_NG
    return RenderSelectionsAsPixmap(dm, selections);
#else
    return PixmapFromRenderedBitmap(RenderSelectionsAsRenderedBitmap(dm, selections));
#endif
}

static void SearchGoogleLensSrc(WindowTab* tab, GoogleLensSrc src, IPageElement* imageElement, int pageNo) {
    if (!tab || !tab->win || !HasPermission(Perm::InternetAccess) || !HasPermission(Perm::CopySelection)) {
        return;
    }
    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        GoogleLensNotify(tab, Tr("Google Lens is only available for document pages."));
        return;
    }

    Pixmap* bitmap = nullptr;
    bool isImage = imageElement && imageElement->Is(kindPageElementImage);
    if (src == GoogleLensSrc::Image || (src == GoogleLensSrc::Auto && isImage)) {
        if (isImage) {
            bitmap = PixmapForImageElement(dm->GetEngine(), imageElement);
        } else if (src == GoogleLensSrc::Image && dm->GetEngine()->kind != kindEngineImage) {
            GoogleLensNotify(tab, Tr("No image under the cursor."));
            return;
        }
    }

    bool wantSel = src == GoogleLensSrc::Selection ||
                   (src == GoogleLensSrc::Auto && !bitmap && dm->GetEngine()->kind != kindEngineImage &&
                    tab->selectionOnPage && len(*tab->selectionOnPage) > 0);
    if (!bitmap && wantSel && tab->selectionOnPage && len(*tab->selectionOnPage) > 0) {
        bitmap = RenderLensSelection(dm, *tab->selectionOnPage);
    }

    bool wantPage =
        src == GoogleLensSrc::Page || src == GoogleLensSrc::Auto || (src == GoogleLensSrc::Image && !bitmap);
    if (!bitmap && wantPage) {
        if (pageNo <= 0) {
            pageNo = dm->CurrentPageNo();
        }
        if (dm->ValidPageNo(pageNo)) {
            float zoom = dm->GetZoomReal(pageNo);
            RectF pageRect = dm->GetEngine()->PageMediabox(pageNo);
            RenderPageArgs args(pageNo, zoom, dm->GetRotation(), &pageRect, RenderTarget::Export);
            bitmap = dm->GetEngine()->RenderPage(args);
            if (bitmap && bitmap->format != PixmapFormat::BGRA8) {
#if OS_WIN
                Pixmap* converted = PixmapCopyAs32bppDIB(bitmap);
                FreePixmap(bitmap);
                bitmap = converted;
#else
                // ng: a Native (palette DIB) pixmap can only be read through GDI
                FreePixmap(bitmap);
                bitmap = nullptr;
#endif
            }
        }
    }
    if (!bitmap) {
        GoogleLensNotify(tab, Tr("Could not render the page for Google Lens."));
        return;
    }

    Str png = EncodePngFromPixmap(bitmap);
    FreePixmap(bitmap);
    if (len(png) == 0 || len(png) > kMaxGoogleLensPngBytes) {
        str::Free(png);
        GoogleLensNotify(tab, Tr("Could not encode the page for Google Lens."));
        return;
    }
    WriteGoogleLensPage(tab, (const u8*)png.s, (size_t)len(png));
    str::Free(png);
}

void SearchWithGoogleLens(WindowTab* tab, IPageElement* imageElement, int pageNo) {
    SearchGoogleLensSrc(tab, GoogleLensSrc::Auto, imageElement, pageNo);
}

void SearchGoogleLensSelection(WindowTab* tab) {
    SearchGoogleLensSrc(tab, GoogleLensSrc::Selection, nullptr, 0);
}

void SearchGoogleLensPage(WindowTab* tab, int pageNo) {
    SearchGoogleLensSrc(tab, GoogleLensSrc::Page, nullptr, pageNo);
}

void SearchGoogleLensImage(WindowTab* tab, IPageElement* imageElement) {
    SearchGoogleLensSrc(tab, GoogleLensSrc::Image, imageElement, 0);
}
