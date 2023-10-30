/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/CmdLineArgsIter.h"
#include "utils/FileUtil.h"
#include "utils/GdiPlusUtil.h"
#include "mui/Mui.h"
#include "utils/TgaReader.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "PdfCreator.h"

void _uploadDebugReportIfFunc(bool, const char*) {
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

static bool NeedsEscape(const char* s) {
    // TODO: optimize to do a single loop over s
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
static char* Escape(const WCHAR* str) {
    if (str::IsEmpty(str)) {
        return {};
    }

    if (!NeedsEscape(str)) {
        return ToUtf8(str);
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
                escaped.AppendChar(*s);
                break;
        }
    }
    return ToUtf8(escaped.Get());
}

static TempStr EscapeTemp(const char* str) {
    if (str::IsEmpty(str)) {
        return nullptr;
    }

    if (!NeedsEscape(str)) {
        return (TempStr)str;
    }

    str::Str escaped(256);
    for (const char* s = str; *s; s++) {
        switch (*s) {
            case '&':
                escaped.Append("&amp;");
                break;
            case '<':
                escaped.Append("&lt;");
                break;
            case '>':
                escaped.Append("&gt;");
                break;
            case '"':
                escaped.Append("&quot;");
                break;
            case '\'':
                escaped.Append("&amp;");
                break;
            default:
                escaped.AppendChar(*s);
                break;
        }
    }
    return str::DupTemp(escaped.Get());
}

void DumpProperties(EngineBase* engine, bool fullDump) {
    Out1("\t<Properties\n");
    TempStr str = EscapeTemp(engine->FilePath());
    Out("\t\tFilePath=\"%s\"\n", str);
    str = EscapeTemp(engine->GetPropertyTemp(DocumentProperty::Title));
    if (str) {
        Out("\t\tTitle=\"%s\"\n", str);
    }
    str = EscapeTemp(engine->GetPropertyTemp(DocumentProperty::Subject));
    if (str) {
        Out("\t\tSubject=\"%s\"\n", str);
    }
    str = EscapeTemp(engine->GetPropertyTemp(DocumentProperty::Author));
    if (str) {
        Out("\t\tAuthor=\"%s\"\n", str);
    }
    str = EscapeTemp(engine->GetPropertyTemp(DocumentProperty::Copyright));
    if (str) {
        Out("\t\tCopyright=\"%s\"\n", str);
    }
    str = EscapeTemp(engine->GetPropertyTemp(DocumentProperty::CreationDate));
    if (str) {
        Out("\t\tCreationDate=\"%s\"\n", str);
    }
    str = EscapeTemp(engine->GetPropertyTemp(DocumentProperty::ModificationDate));
    if (str) {
        Out("\t\tModDate=\"%s\"\n", str);
    }
    str = EscapeTemp(engine->GetPropertyTemp(DocumentProperty::CreatorApp));
    if (str) {
        Out("\t\tCreator=\"%s\"\n", str);
    }
    str = EscapeTemp(engine->GetPropertyTemp(DocumentProperty::PdfProducer));
    if (str) {
        Out("\t\tPdfProducer=\"%s\"\n", str);
    }
    str = EscapeTemp(engine->GetPropertyTemp(DocumentProperty::PdfVersion));
    if (str) {
        Out("\t\tPdfVersion=\"%s\"\n", str);
    }
    str = EscapeTemp(engine->GetPropertyTemp(DocumentProperty::PdfFileStructure));
    if (str) {
        Out("\t\tPdfFileStructure=\"%s\"\n", str);
    }
    str = EscapeTemp(engine->GetPropertyTemp(DocumentProperty::UnsupportedFeatures));
    if (str) {
        Out("\t\tUnsupportedFeatures=\"%s\"\n", str);
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
    TempStr fontlist = engine->GetPropertyTemp(DocumentProperty::FontList);
    if (fontlist) {
        StrVec fonts;
        Split(fonts, fontlist, "\n");
        str = EscapeTemp(Join(fonts, "\n\t\t"));
        Out("\t<FontList>\n\t\t%s\n\t</FontList>\n", str);
    }
}

// caller must free() the result
static char* DestRectToStr(EngineBase* engine, IPageDestination* dest) {
    char* destName = dest->GetName();
    if (destName) {
        TempStr name = EscapeTemp(destName);
        return str::Format("Name=\"%s\"", name);
    }
    // as handled by LinkHandler::ScrollTo in MainWindow.cpp
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
        TempStr title = EscapeTemp(item->title);
        for (int i = 0; i < level; i++) {
            Out1("\t");
        }
        Out("<Item Title=\"%s\"", title);
        if (item->pageNo) {
            Out(" Page=\"%d\"", item->pageNo);
        }
        if (item->id != ++idCounter) {
            Out(" Id=\"%d\"", item->id);
        }
        if (item->GetPageDestination()) {
            IPageDestination* dest = item->GetPageDestination();
            TempStr target = EscapeTemp(dest->GetValue());
            if (target) {
                Out(" Target=\"%s\"", target);
            }
            if (item->pageNo != dest->GetPageNo()) {
                Out(" TargetPage=\"%d\"", dest->GetPageNo());
            }
            AutoFreeStr rectStr = DestRectToStr(engine, dest);
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
        Out("\t<TocTree%s>\n", engine->HasToc() ? "" : " Expected=\"no\"");
        int idCounter = 0;
        DumpTocItem(engine, root, 2, idCounter);
        Out1("\t</TocTree>\n");
    } else if (engine->HasToc()) {
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
        TempStr label = EscapeTemp(engine->GetPageLabeTemp(pageNo));
        Out("\t\tLabel=\"%s\"\n", label);
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
            AutoFreeStr text = Escape(pageText.text);
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
                TempStr value = EscapeTemp(dest->GetValue());
                if (value) {
                    Out("\t\t\t\tLinkTarget=\"%s\"\n", value);
                }
                if (dest->GetPageNo()) {
                    Out("\t\t\t\tLinkedPage=\"%d\"\n", dest->GetPageNo());
                }
                AutoFreeStr rectStr = DestRectToStr(engine, dest);
                if (rectStr) {
                    Out("\t\t\t\tLinked%s\n", rectStr.Get());
                }
            }
            TempStr name = EscapeTemp(el->GetValue());
            if (name) {
                Out("\t\t\t\tLabel=\"%s\"\n", name);
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

#define ErrOut(msg, ...) fprintf(stderr, msg "\n", __VA_ARGS__)
#define ErrOut1(msg) fprintf(stderr, "%s", msg "\n")

static bool CheckRenderPath(const char* path) {
    CrashIf(!path);
    bool hasArg = false;
    const char* p = path - 1;
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

static bool RenderDocument(EngineBase* engine, const char* renderPath, float zoom = 1.f, bool silent = false) {
    if (!CheckRenderPath(renderPath)) {
        return false;
    }

    if (str::EndsWithI(renderPath, ".txt")) {
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
        TempStr txtFilePath = str::FormatTemp(renderPath, 0);
        char* textA = ToUtf8Temp(text.Get());
        char* textUTF8BOM = str::JoinTemp(UTF8_BOM, textA);
        return file::WriteFile(txtFilePath, textUTF8BOM);
    }

    bool success = true;
    for (int pageNo = 1; pageNo <= engine->PageCount(); pageNo++) {
        RenderPageArgs args(pageNo, zoom, 0);
        RenderedBitmap* bmp = engine->RenderPage(args);
        success &= bmp != nullptr;
        if (!bmp && !silent) {
            ErrOut("Error: Failed to render page %d for %s!", pageNo, engine->FilePath());
        }
        if (!bmp || silent) {
            delete bmp;
            continue;
        }
        TempStr pageBmpPath = str::FormatTemp(renderPath, pageNo);
        if (str::EndsWithI(pageBmpPath, ".png")) {
            Gdiplus::Bitmap gbmp(bmp->GetBitmap(), nullptr);
            CLSID pngEncId = GetEncoderClsid(L"image/png");
            WCHAR* pageBmpPathW = ToWStrTemp(pageBmpPath);
            gbmp.Save(pageBmpPathW, &pngEncId);
        } else if (str::EndsWithI(pageBmpPath, ".bmp")) {
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
    const char* password;

  public:
    explicit PasswordHolder(const char* password) : password(password) {
    }
    char* GetPassword(const char*, u8*, __unused u8 decryptionKeyOut[32], bool*) override {
        return str::Dup(password);
    }
};

int main(int, char**) {
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

    char* filePath = nullptr;
    char* password = nullptr;
    bool fullDump = true;
    char* renderPath = nullptr;
    float renderZoom = 1.f;
    bool loadOnly = false, silent = false;

    for (int i = 1; i < nArgs; i++) {
        if (str::Eq(argList.at(i), "-pwd") && i + 1 < nArgs && !password) {
            password = argList.at(++i);
        } else if (str::Eq(argList.at(i), "-quick")) {
            fullDump = false;
        } else if (str::Eq(argList.at(i), "-render") && i + 1 < nArgs && !renderPath) {
            // optional zoom argument (e.g. -render 50% file.pdf)
            float zoom;
            if (i + 2 < nArgs && str::Parse(argList.at(i + 1), "%f%%%$", &zoom) && zoom > 0.f) {
                renderZoom = zoom / 100.f;
                i++;
            }
            renderPath = argList.at(++i);
        } else if (str::Eq(argList.at(i), "-loadonly")) {
            // -loadonly and -silent are only meant for profiling
            loadOnly = true;
        } else if (str::Eq(argList.at(i), "-silent")) {
            silent = true;
        } else if (str::Eq(argList.at(i), "-full")) {
            // -full is for backward compatibility
            fullDump = true;
        } else if (!filePath) {
            filePath = argList.at(i);
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
    WCHAR* pathW = ToWStrTemp(filePath);
    HANDLE hfind = FindFirstFileW(pathW, &fdata);
    // embedded documents are referred to by an invalid path
    // containing more information after a colon (e.g. "C:\file.pdf:3:0")
    if (INVALID_HANDLE_VALUE != hfind) {
        char* dir = path::GetDirTemp(filePath);
        char* name = ToUtf8Temp(fdata.cFileName);
        filePath = path::JoinTemp(dir, name);
        FindClose(hfind);
    }

    PasswordHolder pwdUI(password);
    EngineBase* engine = CreateEngineFromFile(filePath, &pwdUI, false);
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
