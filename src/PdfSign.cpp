/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Digitally signing a PDF with a PKCS#7 certificate. Kept out of
// EngineMupdf.cpp because PdfFilter / PdfPreview compile that file too and
// import mupdf through libsumatrapdf.def; signing is an app-only feature and
// dragging pdf_sign_signature & co. into those shell extensions would mean
// exporting them for no one.

#include "base/Base.h"
#include "base/File.h"
#include "base/ScopedWin.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <wincrypt.h>

extern "C" {
#include <mupdf/pdf.h>
#include <mupdf/helpers/pkcs7-windows.h>
}

#include "gui/UIModels.h"

#include "Annotation.h"
#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "EngineMupdf.h"

// True when this widget is a signature field the document's author left for
// someone to sign, i.e. a signature field with no signature in it yet. Its
// field name (which may legitimately be empty) goes to fieldNameOut, so the
// Sign Document dialog can preselect that field (issue #5964).
bool IsUnsignedSignatureWidget(Annotation* widget, TempStr* fieldNameOut) {
    if (!AnnotationIsLive(widget) || widget->type != AnnotationType::Widget) {
        return false;
    }
    EngineMupdf* e = widget->engine;
    pdf_annot* a = widget->pdfannot;
    if (!e || !a) {
        return false;
    }
    fz_context* ctx = e->Ctx();
    ScopedRecursiveMutex scope(&e->docLock);
    bool res = false;
    fz_try(ctx) {
        if (pdf_widget_type(ctx, a) == PDF_WIDGET_TYPE_SIGNATURE && !pdf_widget_is_signed(ctx, a)) {
            res = true;
            if (fieldNameOut) {
                char* name = pdf_load_field_name(ctx, pdf_annot_obj(ctx, a));
                *fieldNameOut = name ? str::DupTemp(Str(name)) : StrL("");
                fz_free(ctx, name);
            }
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        res = false;
    }
    return res;
}

// Names of the signature fields that are present but not signed yet, plus the
// 1-based page each sits on. A PDF prepared for signing has such a field, and
// signing it (rather than adding another) is what the author intended.
void EngineMupdfGetUnsignedSignatureFields(EngineBase* engine, StrVec& names, Vec<int>& pageNos) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf || !epdf->pdfdoc) {
        return;
    }
    fz_context* ctx = epdf->BaseCtx();
    ScopedRecursiveMutex scope(&epdf->docLock);

    // finding the widgets means loading every page, which is O(pages). The
    // form's field tree says up front whether any signature field exists, so
    // documents without one (most of them) don't pay for it.
    int nSigFields = 0;
    fz_try(ctx) {
        nSigFields = pdf_count_signatures(ctx, epdf->pdfdoc);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        return;
    }
    if (nSigFields == 0) {
        return;
    }

    pdf_page* page = nullptr;
    fz_var(page);
    fz_try(ctx) {
        int nPages = pdf_count_pages(ctx, epdf->pdfdoc);
        for (int pageIdx = 0; pageIdx < nPages; pageIdx++) {
            page = pdf_load_page(ctx, epdf->pdfdoc, pageIdx);
            for (pdf_annot* w = pdf_first_widget(ctx, page); w; w = pdf_next_widget(ctx, w)) {
                if (pdf_widget_type(ctx, w) != PDF_WIDGET_TYPE_SIGNATURE) {
                    continue;
                }
                if (pdf_widget_is_signed(ctx, w)) {
                    continue;
                }
                char* name = pdf_load_field_name(ctx, pdf_annot_obj(ctx, w));
                names.Append(name ? Str(name) : StrL(""));
                VecAppend(pageNos, pageIdx + 1);
                fz_free(ctx, name);
            }
            fz_drop_page(ctx, (fz_page*)page);
            page = nullptr;
        }
    }
    fz_always(ctx) {
        fz_drop_page(ctx, (fz_page*)page);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
    }
}

// Finds the unsigned signature widget named name; returns a kept reference the
// caller drops. The page that owns it is left loaded and returned in pageOut
// (caller drops it). Dropping that page before signing unbinds the annot
// ("annotation not bound to any page") if nothing else is holding it.
static pdf_annot* FindUnsignedSignatureWidget(fz_context* ctx, pdf_document* doc, Str name, int* pageNoOut,
                                              pdf_page** pageOut) {
    pdf_page* page = nullptr;
    pdf_annot* found = nullptr;
    fz_var(page);
    fz_var(found);
    fz_try(ctx) {
        int nPages = pdf_count_pages(ctx, doc);
        for (int pageIdx = 0; !found && pageIdx < nPages; pageIdx++) {
            page = pdf_load_page(ctx, doc, pageIdx);
            for (pdf_annot* w = pdf_first_widget(ctx, page); w; w = pdf_next_widget(ctx, w)) {
                if (pdf_widget_type(ctx, w) != PDF_WIDGET_TYPE_SIGNATURE || pdf_widget_is_signed(ctx, w)) {
                    continue;
                }
                char* wName = pdf_load_field_name(ctx, pdf_annot_obj(ctx, w));
                bool isMatch = str::Eq(Str(wName), name);
                fz_free(ctx, wName);
                if (isMatch) {
                    found = pdf_keep_annot(ctx, w);
                    if (pageNoOut) {
                        *pageNoOut = pageIdx + 1;
                    }
                    if (pageOut) {
                        *pageOut = page;
                    }
                    page = nullptr;
                    break;
                }
            }
            fz_drop_page(ctx, (fz_page*)page);
            page = nullptr;
        }
    }
    fz_always(ctx) {
        fz_drop_page(ctx, (fz_page*)page);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
    return found;
}

// Signing changed what the page draws, so the cached display list (captured
// before the signature appearance existed) has to go. The annotation list is
// left alone: the document is re-loaded right after saving.
static void DropCachedPageRendering(EngineMupdf* e, int pageNo) {
    ScopedRecursiveMutex scope(&e->pagesLock);
    FzPageInfo* pageInfo = e->PageInfoByPageNo(pageNo);
    if (!pageInfo) {
        return;
    }
    pageInfo->elementsNeedRebuilding = true;
    ScopedMutex rl(&e->renderLock);
    if (pageInfo->displayList) {
        fz_drop_display_list(e->Ctx(), pageInfo->displayList);
        pageInfo->displayList = nullptr;
    }
}

// Signs the document with a PKCS#7 certificate from a .pfx / .p12 file, either
// filling in an existing (unsigned) signature field or creating a new one.
// The signature isn't complete until the document is saved: mupdf reserves
// space for it here and computes the digest over the saved bytes, so the caller
// must follow this with EngineMupdfSaveUpdated() (incremental, so earlier
// signatures stay valid).
bool EngineMupdfSignDocument(EngineBase* engine, const PdfSignArgs& args, Str* errOut) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    if (!epdf || !epdf->pdfdoc) {
        return false;
    }
    // signing changes a form field value, which can regenerate appearances and
    // run form JavaScript, so it must happen on the base context (see BaseCtx)
    fz_context* ctx = epdf->BaseCtx();
    ScopedRecursiveMutex scope(&epdf->docLock);

    pdf_pkcs7_signer* signer = nullptr;
    pdf_annot* widget = nullptr;
    pdf_page* page = nullptr;
    fz_image* graphic = nullptr;
    fz_buffer* imgBuf = nullptr;
    int pageNo = args.pageNo;
    bool ok = false;
    fz_var(signer);
    fz_var(widget);
    fz_var(page);
    fz_var(graphic);
    fz_var(imgBuf);
    fz_var(ok);
    fz_var(pageNo);

    static_assert(kPdfSignShowLabels == PDF_SIGNATURE_SHOW_LABELS);
    static_assert(kPdfSignShowDN == PDF_SIGNATURE_SHOW_DN);
    static_assert(kPdfSignShowDate == PDF_SIGNATURE_SHOW_DATE);
    static_assert(kPdfSignShowTextName == PDF_SIGNATURE_SHOW_TEXT_NAME);
    static_assert(kPdfSignShowGraphicName == PDF_SIGNATURE_SHOW_GRAPHIC_NAME);

    fz_try(ctx) {
        if (args.certThumbprint) {
            signer = pkcs7_windows_read_store(ctx, CStrTemp(args.certThumbprint));
        } else {
            signer = pkcs7_windows_read_pfx(ctx, CStrTemp(args.certPath), CStrTemp(args.certPassword));
        }
        if (args.fieldName) {
            widget = FindUnsignedSignatureWidget(ctx, epdf->pdfdoc, args.fieldName, &pageNo, &page);
            if (!widget) {
                fz_throw(ctx, FZ_ERROR_ARGUMENT, "signature field '%s' is gone", CStrTemp(args.fieldName));
            }
        } else {
            page = pdf_load_page(ctx, epdf->pdfdoc, pageNo - 1);
            // the name only has to be unique; mupdf doesn't check, so make it
            // unlikely to collide with fields the document already has
            TempStr name = fmt("SumatraSignature%d", pdf_count_signatures(ctx, epdf->pdfdoc) + 1);
            widget = pdf_create_signature_widget(ctx, page, CStrTemp(name));
            if (!args.rect.IsEmpty()) {
                pdf_set_annot_rect(ctx, widget, ToFzRect(args.rect));
            }
        }
        if (args.imagePath) {
            Str imgData = file::ReadFile(args.imagePath);
            if (len(imgData) == 0) {
                fz_throw(ctx, FZ_ERROR_ARGUMENT, "could not read signature image");
            }
            imgBuf = fz_new_buffer_from_copied_data(ctx, (const unsigned char*)imgData.s, (size_t)len(imgData));
            str::Free(imgData);
            graphic = fz_new_image_from_buffer(ctx, imgBuf);
        }
        const char* reason = args.reason ? CStrTemp(args.reason) : nullptr;
        const char* location = args.location ? CStrTemp(args.location) : nullptr;
        // PDF_SIGNATURE_DEFAULT_APPEARANCE, minus the logo: that draws mupdf's
        // logo into the signature, which has no business in a user's document
        int appearance = args.appearanceFlags >= 0 ? args.appearanceFlags : kPdfSignDefaultAppearance;
        appearance &= ~PDF_SIGNATURE_SHOW_LOGO;
        // note: mupdf draws the reason / location into the signature's
        // appearance but doesn't put /Reason and /Location in the signature
        // dictionary, and they can't be added afterwards: signing already
        // reserved a /ByteRange for the bytes as they are now, so any extra
        // dictionary entry shifts the offsets and invalidates the signature.
        pdf_sign_signature(ctx, widget, signer, appearance, graphic, reason, location);
        ok = true;
    }
    fz_always(ctx) {
        fz_drop_image(ctx, graphic);
        fz_drop_buffer(ctx, imgBuf);
        pdf_drop_annot(ctx, widget);
        fz_drop_page(ctx, (fz_page*)page);
        pdf_drop_signer(ctx, signer);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        if (errOut) {
            *errOut = str::Dup(Str(fz_caught_message(ctx)));
        }
        return false;
    }
    if (ok) {
        DropCachedPageRendering(epdf, pageNo);
        epdf->modifiedAnnotations = true;
    }
    return ok;
}

// Certificates in the current user's Personal store that have a private key
// and are currently valid. Labels are what the Sign Document drop-down shows;
// thumbprints are what EngineMupdfSignDocument wants in certThumbprint.
void ListWindowsSigningCertificates(StrVec& thumbprints, StrVec& labels) {
    thumbprints.Reset();
    labels.Reset();
    HCERTSTORE hStore = CertOpenSystemStoreW(0, L"MY");
    if (!hStore) {
        return;
    }
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    PCCERT_CONTEXT cert = nullptr;
    while ((cert = CertEnumCertificatesInStore(hStore, cert)) != nullptr) {
        DWORD provSz = 0;
        if (!CertGetCertificateContextProperty(cert, CERT_KEY_PROV_INFO_PROP_ID, nullptr, &provSz)) {
            continue;
        }
        if (cert->pCertInfo) {
            if (CompareFileTime(&cert->pCertInfo->NotBefore, &now) > 0) {
                continue;
            }
            if (CompareFileTime(&cert->pCertInfo->NotAfter, &now) < 0) {
                continue;
            }
            BYTE ku[2]{};
            if (CertGetIntendedKeyUsage(X509_ASN_ENCODING, cert->pCertInfo, ku, sizeof(ku))) {
                if ((ku[0] & CERT_DIGITAL_SIGNATURE_KEY_USAGE) == 0) {
                    continue;
                }
            }
        }
        BYTE hash[20];
        DWORD hashLen = sizeof(hash);
        if (!CertGetCertificateContextProperty(cert, CERT_HASH_PROP_ID, hash, &hashLen) || hashLen != 20) {
            continue;
        }
        char hex[41];
        for (DWORD i = 0; i < 20; i++) {
            snprintf(hex + (i * 2), 3, "%02X", hash[i]);
        }
        WCHAR nameW[256]{};
        CertGetNameStringW(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, nameW, dimof(nameW));
        TempStr name = ToUtf8Temp(WStr(nameW));
        if (len(name) == 0) {
            name = StrL("(unnamed)");
        }
        SYSTEMTIME st{};
        if (cert->pCertInfo) {
            FileTimeToSystemTime(&cert->pCertInfo->NotAfter, &st);
        }
        TempStr label = st.wYear ? fmt("%s (expires %04d-%02d-%02d)", name, st.wYear, st.wMonth, st.wDay) : name;
        thumbprints.Append(Str(hex, 40));
        labels.Append(label);
    }
    CertCloseStore(hStore, 0);
}
