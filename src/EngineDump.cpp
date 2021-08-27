/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/CmdLineArgsIter.h"
#include "utils/FileUtil.h"
#include "utils/GdiPlusUtil.h"
#include "mui/Mui.h"
#include "utils/TgaReader.h"
#include "utils/WinUtil.h"

#include "wingui/TreeModel.h"
#include "DisplayMode.h"
#include "Controller.h"
#include "EngineBase.h"
#include "EngineDjVu.h"
#include "EngineAll.h"
#include "PdfCreator.h"

void _submitDebugReportIfFunc(__unused bool cond, __unused const char* condStr) {
    // no-op implementation to satisfy SubmitBugReport()
}

#define Out(msg, ...) printf(msg, __VA_ARGS__)

static void Out1(const char* msg) {
    printf("%s", msg);
}

static bool NeedsEscape(const WCHAR* s) {
    if (str::FindChar(s, '<')) {
        return true;
    }
    if (str::FindChar(s, '&')) {
        return true;
    }
    if (str::FindChar(s, '"')) {
        return true;
    }
    return false;
}

// TODO: we leak because in the past Escape() was freeing str
// and now we don't but I didn't update all the code
// doesn't matter because engine dump does its job and quits
static std::string_view Escape(const WCHAR* str) {
    if (str::IsEmpty(str)) {
        return {};
    }

    if (!NeedsEscape(str)) {
        return strconv::WstrToUtf8(str);
    }

    str::WStr escaped(256);
    for (const WCHAR* s = str; *s; s++) {
        switch (*s) {
            case '&':
                escaped.Append(L"&amp;");
                break;
            case '<':
                escaped.Append(L"&lt;");
                break;
            case '>':
                escaped.Append(L"&gt;");
                break;
            case '"':
                escaped.Append(L"&quot;");
                break;
            case '\'':
                escaped.Append(L"&amp;");
                break;
            default:
                escaped.Append(*s);
                break;
        }
    }
    return strconv::WstrToUtf8(escaped.Get());
}

void DumpProperties(EngineBase* engine, bool fullDump) {
    Out1("\t<Properties\n");
    AutoFree str = Escape(engine->FileName());
    Out("\t\tFilePath=\"%s\"\n", str.Get());
    str = Escape(engine->GetProperty(DocumentProperty::Title));
    if (str.Get()) {
        Out("\t\tTitle=\"%s\"\n", str.Get());
    }
    str = Escape(engine->GetProperty(DocumentProperty::Subject));
    if (str.Get()) {
        Out("\t\tSubject=\"%s\"\n", str.Get());
    }
    str = Escape(engine->GetProperty(DocumentProperty::Author));
    if (str.Get()) {
        Out("\t\tAuthor=\"%s\"\n", str.Get());
    }
    str = Escape(engine->GetProperty(DocumentProperty::Copyright));
    if (str.Get()) {
        Out("\t\tCopyright=\"%s\"\n", str.Get());
    }
    str = Escape(engine->GetProperty(DocumentProperty::CreationDate));
    if (str.Get()) {
        Out("\t\tCreationDate=\"%s\"\n", str.Get());
    }
    str = Escape(engine->GetProperty(DocumentProperty::ModificationDate));
    if (str.Get()) {
        Out("\t\tModDate=\"%s\"\n", str.Get());
    }
    str = Escape(engine->GetProperty(DocumentProperty::CreatorApp));
    if (str.Get()) {
        Out("\t\tCreator=\"%s\"\n", str.Get());
    }
    str = Escape(engine->GetProperty(DocumentProperty::PdfProducer));
    if (str.Get()) {
        Out("\t\tPdfProducer=\"%s\"\n", str.Get());
    }
    str = Escape(engine->GetProperty(DocumentProperty::PdfVersion));
    if (str.Get()) {
        Out("\t\tPdfVersion=\"%s\"\n", str.Get());
    }
    str = Escape(engine->GetProperty(DocumentProperty::PdfFileStructure));
    if (str.Get()) {
        Out("\t\tPdfFileStructure=\"%s\"\n", str.Get());
    }
    str = Escape(engine->GetProperty(DocumentProperty::UnsupportedFeatures));
    if (str.Get()) {
        Out("\t\tUnsupportedFeatures=\"%s\"\n", str.Get());
    }
    if (!engine->AllowsPrinting()) {
        Out1("\t\tPrintingAllowed=\"no\"\n");
    }
    if (!engine->AllowsCopyingText()) {
        Out1("\t\tCopyingTextAllowed=\"no\"\n");
    }
    if (engine->IsImageCollection()) {
        Out("\t\tImageFileDPI=\"%g\"\n", engine->GetFileDPI());
    }
#if 0
    if (engine->preferredLayout.t) {
        Out("\t\tPreferredLayout=\"%d\"\n", engine->preferredLayout);
    }
#endif
    Out1("\t/>\n");

    if (!fullDump) {
        return;
    }
    AutoFreeWstr fontlist(engine->GetProperty(DocumentProperty::FontList));
    if (fontlist) {
        WStrVec fonts;
        fonts.Split(fontlist, L"\n");
        str = Escape(fonts.Join(L"\n\t\t"));
        Out("\t<FontList>\n\t\t%s\n\t</FontList>\n", str.Get());
    }
}

// caller must free() the result
static char* DestRectToStr(EngineBase* engine, IPageDestination* dest) {
    WCHAR* destName = dest->GetName();
    if (destName) {
        AutoFree name = Escape(destName);
        return str::Format("Name=\"%s\"", name.Get());
    }
    // as handled by LinkHandler::ScrollTo in WindowInfo.cpp
    int pageNo = dest->GetPageNo();
    if (pageNo <= 0 || pageNo > engine->PageCount()) {
        return nullptr;
    }
    RectF rect = dest->GetRect();
    if (rect.IsEmpty()) {
        PointF pt = engine->Transform(rect.TL(), pageNo, 1.0, 0);
        return str::Format("Point=\"%.0f %.0f\"", pt.x, pt.y);
    }
    if (rect.dx != DEST_USE_DEFAULT && rect.dy != DEST_USE_DEFAULT) {
        Rect rc = engine->Transform(rect, pageNo, 1.0, 0).Round();
        return str::Format("Rect=\"%d %d %d %d\"", rc.x, rc.y, rc.dx, rc.dy);
    }
    if (rect.y != DEST_USE_DEFAULT) {
        PointF pt = engine->Transform(rect.TL(), pageNo, 1.0, 0);
        return str::Format("Point=\"x %.0f\"", pt.y);
    }
    return nullptr;
}

void DumpTocItem(EngineBase* engine, TocItem* item, int level, int& idCounter) {
    for (; item; item = item->next) {
        AutoFree title(Escape(item->title));
        for (int i = 0; i < level; i++) {
            Out1("\t");
        }
        Out("<Item Title=\"%s\"", title.Get());
        if (item->pageNo) {
            Out(" Page=\"%d\"", item->pageNo);
        }
        if (item->id != ++idCounter) {
            Out(" Id=\"%d\"", item->id);
        }
        if (item->GetPageDestination()) {
            IPageDestination* dest = item->GetPageDestination();
            AutoFree target = Escape(dest->GetValue());
            if (target.Get()) {
                Out(" Target=\"%s\"", target.Get());
            }
            if (item->pageNo != dest->GetPageNo()) {
                Out(" TargetPage=\"%d\"", dest->GetPageNo());
            }
            AutoFree rectStr(DestRectToStr(engine, dest));
            if (rectStr) {
                Out(" Target%s", rectStr.Get());
            }
        }
        if (!item->child) {
            Out1(" />\n");
        } else {
            if (item->isOpenDefault) {
                Out1(" Expanded=\"yes\"");
            }
            Out1(">\n");
            DumpTocItem(engine, item->child, level + 1, idCounter);
            for (int i = 0; i < level; i++) {
                Out1("\t");
            }
            Out1("</Item>\n");
        }
    }
}

void DumpToc(EngineBase* engine) {
    TocTree* tree = engine->GetToc();
    if (!tree) {
        return;
    }
    auto* root = tree->root;
    if (root) {
        Out("\t<TocTree%s>\n", engine->HacToc() ? "" : " Expected=\"no\"");
        int idCounter = 0;
        DumpTocItem(engine, root, 2, idCounter);
        Out1("\t</TocTree>\n");
    } else if (engine->HacToc()) {
        Out1("\t<TocTree />\n");
    }
}

const char* ElementTypeToStr(IPageElement* el) {
    Kind kind = el->GetKind();
    if (kind) {
        return kind;
    }
    return "unknown";
}

const char* PageDestToStr(Kind kind) {
    return kind;
}

void DumpPageContent(EngineBase* engine, int pageNo, bool fullDump) {
    // ensure that the page is loaded
    engine->BenchLoadPage(pageNo);

    Out("\t<Page Number=\"%d\"\n", pageNo);
    if (engine->HasPageLabels()) {
        AutoFree label(Escape(engine->GetPageLabel(pageNo)));
        Out("\t\tLabel=\"%s\"\n", label.Get());
    }
    Rect bbox = engine->PageMediabox(pageNo).Round();
    Out("\t\tMediaBox=\"%d %d %d %d\"\n", bbox.x, bbox.y, bbox.dx, bbox.dy);
    Rect cbox = engine->PageContentBox(pageNo).Round();
    if (cbox != bbox) {
        Out("\t\tContentBox=\"%d %d %d %d\"\n", cbox.x, cbox.y, cbox.dx, cbox.dy);
    }
    if (!engine->HasClipOptimizations(pageNo)) {
        Out1("\t\tHasClipOptimizations=\"no\"\n");
    }
    Out1("\t>\n");

    if (fullDump) {
        PageText pageText = engine->ExtractPageText(pageNo);
        if (pageText.text != nullptr) {
            AutoFree text(Escape(pageText.text));
            if (text.Get()) {
                Out("\t\t<TextContent>\n%s\t\t</TextContent>\n", text.Get());
            }
        }
        FreePageText(&pageText);
    }

    Vec<IPageElement*> els = engine->GetElements(pageNo);
    if (els.size() > 0) {
        Out1("\t\t<PageElements>\n");
        for (auto& el : els) {
            RectF rect = el->GetRect();
            Out("\t\t\t<Element Type=\"%s\"\n\t\t\t\tRect=\"%.0f %.0f %.0f %.0f\"\n", ElementTypeToStr(el), rect.x,
                rect.y, rect.dx, rect.dy);
            IPageDestination* dest = el->AsLink();
            if (dest) {
                if (dest->GetKind() != nullptr) {
                    Out("\t\t\t\tLinkType=\"%s\"\n", dest->GetKind());
                }
                AutoFree value(Escape(dest->GetValue()));
                if (value.Get()) {
                    Out("\t\t\t\tLinkTarget=\"%s\"\n", value.Get());
                }
                if (dest->GetPageNo()) {
                    Out("\t\t\t\tLinkedPage=\"%d\"\n", dest->GetPageNo());
                }
                AutoFree rectStr(DestRectToStr(engine, dest));
                if (rectStr) {
                    Out("\t\t\t\tLinked%s\n", rectStr.Get());
                }
            }
            AutoFree name(Escape(el->GetValue()));
            if (name.Get()) {
                Out("\t\t\t\tLabel=\"%s\"\n", name.Get());
            }
            Out1("\t\t\t/>\n");
        }
        Out1("\t\t</PageElements>\n");
    }

    Out1("\t</Page>\n");
}

void DumpThumbnail(EngineBase* engine) {
    RectF rect = engine->Transform(engine->PageMediabox(1), 1, 1.0, 0);
    if (rect.IsEmpty()) {
        Out1("\t<Thumbnail />\n");
        return;
    }

    float zoom = std::min(128 / (float)rect.dx, 128 / (float)rect.dy) - 0.001f;
    Rect thumb = RectF(0, 0, rect.dx * zoom, rect.dy * zoom).Round();
    rect = engine->Transform(ToRectF(thumb), 1, zoom, 0, true);
    RenderPageArgs args(1, zoom, 0, &rect);
    RenderedBitmap* bmp = engine->RenderPage(args);
    if (!bmp) {
        Out1("\t<Thumbnail />\n");
        return;
    }

    ByteSlice imgData = tga::SerializeBitmap(bmp->GetBitmap());
    size_t len = imgData.size();
    u8* data = imgData.data();
    AutoFree hexData(data ? str::MemToHex(data, len) : nullptr);
    if (hexData) {
        Out("\t<Thumbnail>\n\t\t%s\n\t</Thumbnail>\n", hexData.Get());
    } else {
        Out1("\t<Thumbnail />\n");
    }
    str::Free(data);
    delete bmp;
}

void DumpData(EngineBase* engine, bool fullDump) {
    Out1(UTF8_BOM);
    Out1("<?xml version=\"1.0\"?>\n");
    Out1("<EngineDump>\n");
    DumpProperties(engine, fullDump);
    DumpToc(engine);
    for (int i = 1; i <= engine->PageCount(); i++) {
        DumpPageContent(engine, i, fullDump);
    }
    if (fullDump) {
        DumpThumbnail(engine);
    }
    Out1("</EngineDump>\n");
}

#define ErrOut(msg, ...) fwprintf(stderr, TEXT(msg) TEXT("\n"), __VA_ARGS__)
#define ErrOut1(msg) fwprintf(stderr, TEXT("%s"), TEXT(msg) TEXT("\n"))

bool CheckRenderPath(const WCHAR* path) {
    CrashIf(!path);
    bool hasArg = false;
    const WCHAR* p = path - 1;
    while ((p = str::FindChar(p + 1, '%')) != nullptr) {
        p++;
        if (*p == '%') {
            continue;
        }
        if (*p == '0' && '1' <= *(p + 1) && *(p + 1) <= '9') {
            p += 2;
        }
        if (hasArg || *p != 'd') {
            ErrOut1("Error: Render path may contain '%%d' only once, other '%%' signs must be doubled!");
            return false;
        }
        hasArg = true;
    }
    return true;
}

bool RenderDocument(EngineBase* engine, const WCHAR* renderPath, float zoom = 1.f, bool silent = false) {
    if (!CheckRenderPath(renderPath)) {
        return false;
    }

    if (str::EndsWithI(renderPath, L".txt")) {
        str::WStr text(1024);
        for (int pageNo = 1; pageNo <= engine->PageCount(); pageNo++) {
            PageText pageText = engine->ExtractPageText(pageNo);
            if (pageText.text != nullptr) {
                text.Append(pageText.text);
            }
            FreePageText(&pageText);
        }
        Replace(text, L"\n", L"\r\n");
        if (silent) {
            return true;
        }
        AutoFreeWstr txtFilePath(str::Format(renderPath, 0));
        auto textA = ToUtf8Temp(text.Get());
        AutoFree textUTF8BOM(str::Join(UTF8_BOM, textA.Get()));
        return file::WriteFile(txtFilePath, textUTF8BOM.AsSpan());
    }

    if (str::EndsWithI(renderPath, L".pdf")) {
        if (silent) {
            return false;
        }
        AutoFreeWstr pdfFilePath(str::Format(renderPath, 0));
        auto pathA(ToUtf8Temp(pdfFilePath.Get()));
        if (engine->SaveFileAsPDF(pathA.Get())) {
            return true;
        }
        return PdfCreator::RenderToFile(pathA.Get(), engine);
    }

    bool success = true;
    for (int pageNo = 1; pageNo <= engine->PageCount(); pageNo++) {
        RenderPageArgs args(pageNo, zoom, 0);
        RenderedBitmap* bmp = engine->RenderPage(args);
        success &= bmp != nullptr;
        if (!bmp && !silent) {
            ErrOut("Error: Failed to render page %d for %s!", pageNo, engine->FileName());
        }
        if (!bmp || silent) {
            delete bmp;
            continue;
        }
        AutoFreeWstr pageBmpPath(str::Format(renderPath, pageNo));
        if (str::EndsWithI(pageBmpPath, L".png")) {
            Gdiplus::Bitmap gbmp(bmp->GetBitmap(), nullptr);
            CLSID pngEncId = GetEncoderClsid(L"image/png");
            gbmp.Save(pageBmpPath, &pngEncId);
        } else if (str::EndsWithI(pageBmpPath, L".bmp")) {
            ByteSlice imgData = SerializeBitmap(bmp->GetBitmap());
            if (!imgData.empty()) {
                file::WriteFile(pageBmpPath, imgData);
                str::Free(imgData.data());
            }
        } else { // render as TGA for all other file extensions
            ByteSlice imgData = tga::SerializeBitmap(bmp->GetBitmap());
            if (!imgData.empty()) {
                file::WriteFile(pageBmpPath, imgData);
                str::Free(imgData.data());
            }
        }
        delete bmp;
    }

    return success;
}

class PasswordHolder : public PasswordUI {
    const WCHAR* password;

  public:
    explicit PasswordHolder(const WCHAR* password) : password(password) {
    }
    WCHAR* GetPassword(__unused const WCHAR* fileName, __unused u8* fileDigest, __unused u8 decryptionKeyOut[32],
                       __unused bool* saveKey) override {
        return str::Dup(password);
    }
};

int main(__unused int argc, __unused char** argv) {
    setlocale(LC_ALL, "C");
    DisableDataExecution();

    CmdLineArgsIter argList(GetCommandLine());
    int nArgs = argList.nArgs;

    if (nArgs < 2) {
    Usage:
        ErrOut("%s [-pwd <password>][-quick][-render <path-%%d.tga>] <filename>",
               path::GetBaseNameTemp(argList.args[0]));
        return 2;
    }

    AutoFreeWstr filePath;
    WCHAR* password = nullptr;
    bool fullDump = true;
    WCHAR* renderPath = nullptr;
    float renderZoom = 1.f;
    bool loadOnly = false, silent = false;

    for (int i = 1; i < nArgs; i++) {
        if (str::Eq(argList.at(i), L"-pwd") && i + 1 < nArgs && !password) {
            password = argList.at(++i);
        } else if (str::Eq(argList.at(i), L"-quick")) {
            fullDump = false;
        } else if (str::Eq(argList.at(i), L"-render") && i + 1 < nArgs && !renderPath) {
            // optional zoom argument (e.g. -render 50% file.pdf)
            float zoom;
            if (i + 2 < nArgs && str::Parse(argList.at(i + 1), L"%f%%%$", &zoom) && zoom > 0.f) {
                renderZoom = zoom / 100.f;
                i++;
            }
            renderPath = argList.at(++i);
        } else if (str::Eq(argList.at(i), L"-loadonly")) {
            // -loadonly and -silent are only meant for profiling
            loadOnly = true;
        } else if (str::Eq(argList.at(i), L"-silent")) {
            silent = true;
        } else if (str::Eq(argList.at(i), L"-full")) {
            // -full is for backward compatibility
            fullDump = true;
        } else if (!filePath) {
            filePath.SetCopy(argList.at(i));
        } else {
            goto Usage;
        }
    }
    if (!filePath) {
        goto Usage;
    }

    if (silent) {
        FILE* nul;
        freopen_s(&nul, "NUL", "w", stdout);
        freopen_s(&nul, "NUL", "w", stderr);
    }

    ScopedGdiPlus gdiPlus;
    ScopedMui miniMui;

    WIN32_FIND_DATA fdata;
    HANDLE hfind = FindFirstFile(filePath, &fdata);
    // embedded documents are referred to by an invalid path
    // containing more information after a colon (e.g. "C:\file.pdf:3:0")
    if (INVALID_HANDLE_VALUE != hfind) {
        AutoFreeWstr dir(path::GetDir(filePath));
        filePath.Set(path::Join(dir, fdata.cFileName));
        FindClose(hfind);
    }

    PasswordHolder pwdUI(password);
    EngineBase* engine = CreateEngine(filePath, &pwdUI);
    if (!engine) {
        ErrOut("Error: Couldn't create an engine for %s!", path::GetBaseNameTemp(filePath));
        return 1;
    }
    if (!loadOnly) {
        DumpData(engine, fullDump);
    }
    if (renderPath) {
        RenderDocument(engine, renderPath, renderZoom, silent);
    }
    delete engine;

    return 0;
}
