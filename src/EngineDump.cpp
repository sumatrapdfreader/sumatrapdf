/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/File.h"
#include "base/GdiPlus.h"
#include "base/TgaReader.h"
#include "base/Win.h"

#include "wingui/UIModels.h"

#include "Settings.h"
#include "Flags.h"
#include "DocProperties.h"
#include "DocController.h"
#include "EngineBase.h"

#define Out(msg, ...) printf(msg, __VA_ARGS__)

static void Out1(Str msg) {
    printf("%s", msg.s);
}

static bool NeedsEscape(Str s) {
    // TODO: optimize to do a single loop over s
    if (str::ContainsChar(s, '<')) {
        return true;
    }
    if (str::ContainsChar(s, '&')) {
        return true;
    }
    if (str::ContainsChar(s, '"')) {
        return true;
    }
    return false;
}

static TempStr EscapeTemp(Str str) {
    if (len(str) == 0) {
        return {};
    }

    if (!NeedsEscape(str)) {
        return str;
    }

    str::Builder escaped(256);
    for (int i = 0; i < str.len; i++) {
        switch (str.s[i]) {
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
                escaped.AppendChar(str.s[i]);
                break;
        }
    }
    return ToStrTemp(escaped);
}

void DumpProperties(EngineBase* engine, bool fullDump) {
    Out1("\t<Properties\n");
    TempStr str = EscapeTemp(engine->FilePath());
    Out("\t\tFilePath=\"%s\"\n", str.s);
    str = EscapeTemp(engine->GetPropertyTemp(kPropTitle));
    if (str) {
        Out("\t\tTitle=\"%s\"\n", str.s);
    }
    str = EscapeTemp(engine->GetPropertyTemp(kPropSubject));
    if (str) {
        Out("\t\tSubject=\"%s\"\n", str.s);
    }
    str = EscapeTemp(engine->GetPropertyTemp(kPropAuthor));
    if (str) {
        Out("\t\tAuthor=\"%s\"\n", str.s);
    }
    str = EscapeTemp(engine->GetPropertyTemp(kPropCopyright));
    if (str) {
        Out("\t\tCopyright=\"%s\"\n", str.s);
    }
    str = EscapeTemp(engine->GetPropertyTemp(kPropCreationDate));
    if (str) {
        Out("\t\tCreationDate=\"%s\"\n", str.s);
    }
    str = EscapeTemp(engine->GetPropertyTemp(kPropModificationDate));
    if (str) {
        Out("\t\tModDate=\"%s\"\n", str.s);
    }
    str = EscapeTemp(engine->GetPropertyTemp(kPropCreatorApp));
    if (str) {
        Out("\t\tCreator=\"%s\"\n", str.s);
    }
    str = EscapeTemp(engine->GetPropertyTemp(kPropPdfProducer));
    if (str) {
        Out("\t\tPdfProducer=\"%s\"\n", str.s);
    }
    str = EscapeTemp(engine->GetPropertyTemp(kPropPdfVersion));
    if (str) {
        Out("\t\tPdfVersion=\"%s\"\n", str.s);
    }
    str = EscapeTemp(engine->GetPropertyTemp(kPropPdfFileStructure));
    if (str) {
        Out("\t\tPdfFileStructure=\"%s\"\n", str.s);
    }
    str = EscapeTemp(engine->GetPropertyTemp(kPropUnsupportedFeatures));
    if (str) {
        Out("\t\tUnsupportedFeatures=\"%s\"\n", str.s);
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
    TempStr fontlist = engine->GetPropertyTemp(kPropFontList);
    if (fontlist) {
        StrVec fonts;
        Split(&fonts, fontlist, "\n");
        str = EscapeTemp(Join(&fonts, "\n\t\t"));
        Out("\t<FontList>\n\t\t%s\n\t</FontList>\n", str.s);
    }
}

static TempStr DestRectToStrTemp(EngineBase* engine, IPageDestination* dest) {
    Str destName = PageDestGetName(dest);
    if (destName) {
        TempStr name = EscapeTemp(destName);
        return fmt("Name=\"%s\"", name);
    }
    // as handled by LinkHandler::ScrollTo in MainWindow.cpp
    int pageNo = PageDestGetPageNo(dest);
    if (pageNo <= 0 || pageNo > engine->PageCount()) {
        return nullptr;
    }
    RectF rect = PageDestGetRect(dest);
    if (rect.IsEmpty()) {
        PointF pt = engine->Transform(rect.TL(), pageNo, 1.0, 0);
        return fmt("Point=\"%.0f %.0f\"", pt.x, pt.y);
    }
    if (rect.dx != DEST_USE_DEFAULT && rect.dy != DEST_USE_DEFAULT) {
        Rect rc = engine->Transform(rect, pageNo, 1.0, 0).Round();
        return fmt("Rect=\"%d %d %d %d\"", rc.x, rc.y, rc.dx, rc.dy);
    }
    if (rect.y != DEST_USE_DEFAULT) {
        PointF pt = engine->Transform(rect.TL(), pageNo, 1.0, 0);
        return fmt("Point=\"x %.0f\"", pt.y);
    }
    return nullptr;
}

void DumpTocItem(EngineBase* engine, TocItem* item, int level, int& idCounter) {
    for (; item; item = item->next) {
        TempStr title = EscapeTemp(item->title);
        for (int i = 0; i < level; i++) {
            Out1("\t");
        }
        Out("<Item Title=\"%s\"", title.s);
        if (item->pageNo) {
            Out(" Page=\"%d\"", item->pageNo);
        }
        if (item->id != ++idCounter) {
            Out(" Id=\"%d\"", item->id);
        }
        if (item->GetPageDestination()) {
            IPageDestination* dest = item->GetPageDestination();
            TempStr target = EscapeTemp(PageDestGetValue(dest));
            if (target) {
                Out(" Target=\"%s\"", target.s);
            }
            if (item->pageNo != PageDestGetPageNo(dest)) {
                Out(" TargetPage=\"%d\"", PageDestGetPageNo(dest));
            }
            TempStr rectStr = DestRectToStrTemp(engine, dest);
            if (rectStr) {
                Out(" Target%s", rectStr.s);
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

Str ElementTypeToStr(IPageElement* el) {
    Kind kind = el->GetKind();
    if (kind) {
        return Str(kind);
    }
    return StrL("unknown");
}

Str PageDestToStr(Kind kind) {
    return Str(kind);
}

void DumpPageContent(EngineBase* engine, int pageNo, bool fullDump) {
    // ensure that the page is loaded
    engine->BenchLoadPage(pageNo);

    Out("\t<Page Number=\"%d\"\n", pageNo);
    if (engine->HasPageLabels()) {
        TempStr label = EscapeTemp(engine->GetPageLabeTemp(pageNo));
        Out("\t\tLabel=\"%s\"\n", label.s);
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
        if (pageText.text) {
            TempStr text = EscapeTemp(pageText.text.s);
            if (text) {
                Out("\t\t<TextContent>\n%s\t\t</TextContent>\n", text.s);
            }
        }
        FreePageText(&pageText);
    }

    Vec<IPageElement*> els = engine->GetElements(pageNo);
    if (len(els) > 0) {
        Out1("\t\t<PageElements>\n");
        for (auto& el : els) {
            RectF rect = el->GetRect();
            Out("\t\t\t<Element Type=\"%s\"\n\t\t\t\tRect=\"%.0f %.0f %.0f %.0f\"\n", ElementTypeToStr(el).s, rect.x,
                rect.y, rect.dx, rect.dy);
            IPageDestination* dest = el->AsLink();
            if (dest) {
                if (dest->GetKind() != nullptr) {
                    Out("\t\t\t\tLinkType=\"%s\"\n", dest->GetKind());
                }
                TempStr value = EscapeTemp(PageDestGetValue(dest));
                if (value) {
                    Out("\t\t\t\tLinkTarget=\"%s\"\n", value.s);
                }
                if (PageDestGetPageNo(dest)) {
                    Out("\t\t\t\tLinkedPage=\"%d\"\n", PageDestGetPageNo(dest));
                }
                TempStr rectStr = DestRectToStrTemp(engine, dest);
                if (rectStr) {
                    Out("\t\t\t\tLinked%s\n", rectStr.s);
                }
            }
            TempStr name = EscapeTemp(el->GetValue());
            if (name) {
                Out("\t\t\t\tLabel=\"%s\"\n", name.s);
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
    Pixmap* bmp = engine->RenderPage(args);
    if (!bmp) {
        Out1("\t<Thumbnail />\n");
        return;
    }

    Str imgData = tga::SerializeBitmap(bmp->hbmp);
    TempStr hexData = imgData.s ? str::MemToHexTemp(imgData) : Str{};
    if (hexData) {
        Out("\t<Thumbnail>\n\t\t%s\n\t</Thumbnail>\n", hexData.s);
    } else {
        Out1("\t<Thumbnail />\n");
    }
    str::Free(imgData);
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

static bool CheckRenderPath(Str path) {
    ReportIf(!path);
    bool hasArg = false;
    int i = 0;
    while (i < path.len) {
        Str rest(path.s + i, path.len - i);
        int pIdx = str::IndexOfChar(rest, '%');
        if (pIdx < 0) {
            break;
        }
        i = i + pIdx + 1;
        if (i >= path.len) {
            break;
        }
        if (path.s[i] == '%') {
            i++;
            continue;
        }
        if (path.s[i] == '0' && i + 1 < path.len && '1' <= path.s[i + 1] && path.s[i + 1] <= '9') {
            i += 2;
        }
        if (hasArg || i >= path.len || path.s[i] != 'd') {
            ErrOut1("Error: Render path may contain '%%d' only once, other '%%' signs must be doubled!");
            return false;
        }
        hasArg = true;
        i++;
    }
    return true;
}

// static
bool RenderDocument(EngineBase* engine, Str renderPath, float zoom = 1.f, bool silent = false) {
    if (!CheckRenderPath(renderPath)) {
        return false;
    }

    if (str::EndsWithI(renderPath, ".txt")) {
        str::Builder text(1024);
        for (int pageNo = 1; pageNo <= engine->PageCount(); pageNo++) {
            PageText pageText = engine->ExtractPageText(pageNo);
            if (pageText.text) {
                text.Append(pageText.text.s);
            }
            FreePageText(&pageText);
        }
        if (silent) {
            return true;
        }
        TempStr txtFilePath = fmt(renderPath.s, 0);
        TempStr textCrLf = str::ReplaceTemp(ToStr(text), StrL("\n"), StrL("\r\n"));
        TempStr textUTF8BOM = str::JoinTemp(UTF8_BOM, textCrLf);
        return file::WriteFile(txtFilePath, textUTF8BOM);
    }

    bool success = true;
    for (int pageNo = 1; pageNo <= engine->PageCount(); pageNo++) {
        RenderPageArgs args(pageNo, zoom, 0);
        Pixmap* bmp = engine->RenderPage(args);
        success &= bmp != nullptr;
        if (!bmp && !silent) {
            ErrOut("Error: Failed to render page %d for %s!", pageNo, engine->FilePath().s);
        }
        if (!bmp || silent) {
            FreePixmap(bmp);
            continue;
        }
        TempStr pageBmpPath = fmt(renderPath.s, pageNo);
        if (str::EndsWithI(pageBmpPath, ".png")) {
            Gdiplus::Bitmap gbmp(bmp->hbmp, nullptr);
            CLSID pngEncId = GetGdiPlusEncoderClsid(L"image/png");
            WCHAR* pageBmpPathW = CWStrTemp(pageBmpPath);
            gbmp.Save(pageBmpPathW, &pngEncId, nullptr);
        } else if (str::EndsWithI(pageBmpPath, ".bmp")) {
            Str imgData = SerializeBitmap(bmp->hbmp);
            if (len(imgData) > 0) {
                file::WriteFile(pageBmpPath, imgData);
                str::Free(imgData);
            }
        } else { // render as TGA for all other file extensions
            // a serialized TGA starts with a 0 byte
            Str imgData = tga::SerializeBitmap(bmp->hbmp);
            if (len(imgData) > 0) {
                file::WriteFile(pageBmpPath, imgData);
                str::Free(imgData);
            }
        }
        FreePixmap(bmp);
    }

    return success;
}

class PasswordHolder : public PasswordUI {
    Str password;

  public:
    explicit PasswordHolder(Str password) : password(password) {}
    Str GetPassword(Str, u8*, __unused u8 decryptionKeyOut[32], bool*) override { return str::Dup(password); }
};

void EngineDump(const Flags& flags) {
#if 0
    setlocale(LC_ALL, "C");
    DisableDataExecution();

    CmdLineArgsIter argList(GetCommandLine());
    int nArgs = argList.nArgs;

    if (nArgs < 2) {
    Usage:
        ErrOut("%s [-pwd <password>][-quick][-render <path-%%d.tga>] <filename>",
               path::GetBaseNameTemp(Str(argList.args[0])));
        return 2;
    }

    Str filePath = {};
    Str password = {};
    bool fullDump = true;
    Str renderPath = {};
    float renderZoom = 1.f;
    bool loadOnly = false, silent = false;

    for (int i = 1; i < nArgs; i++) {
        if (str::Eq(argList[i], "-pwd") && i + 1 < nArgs && !password) {
            password = argList[++i];
        } else if (str::Eq(argList[i], "-quick")) {
            fullDump = false;
        } else if (str::Eq(argList[i], "-render") && i + 1 < nArgs && !renderPath) {
            // optional zoom argument (e.g. -render 50% file.pdf)
            float zoom;
            if (i + 2 < nArgs && !str::IsNull(str::Parse(argList[i + 1], "%f%%%$", &zoom)) && zoom > 0.f) {
                renderZoom = zoom / 100.f;
                i++;
            }
            renderPath = argList[++i];
        } else if (str::Eq(argList[i], "-loadonly")) {
            // -loadonly and -silent are only meant for profiling
            loadOnly = true;
        } else if (str::Eq(argList[i], "-silent")) {
            silent = true;
        } else if (str::Eq(argList[i], "-full")) {
            // -full is for backward compatibility
            fullDump = true;
        } else if (!filePath) {
            filePath = argList[i];
        } else {
            goto Usage;
        }
    }
    if (!filePath) {
        goto Usage;
    }
#endif
    if (flags.silent) {
        FILE* nul;
        freopen_s(&nul, "NUL", "w", stdout);
        freopen_s(&nul, "NUL", "w", stderr);
    }

#if 0
    ScopedGdiPlus gdiPlus;
    ScopedMui miniMui;

    WIN32_FIND_DATA fdata;
    WCHAR* pathW = CWStrTemp(filePath);
    HANDLE hfind = FindFirstFileW(pathW, &fdata);
    // embedded documents are referred to by an invalid path
    // containing more information after a colon (e.g. "C:\file.pdf:3:0")
    if (INVALID_HANDLE_VALUE != hfind) {
        TempStr dir = path::GetDirTemp(filePath);
        TempStr name = ToUtf8Temp(fdata.cFileName);
        filePath = path::JoinTemp(dir, name);
        FindClose(hfind);
    }

    PasswordHolder pwdUI(password);
    EngineBase* engine = CreateEngineFromFile(filePath, &pwdUI, false);
    if (!engine) {
        ErrOut("Error: Couldn't create an engine for %s!", path::GetBaseNameTemp(filePath).s);
        return 1;
    }
    if (!loadOnly) {
        DumpData(engine, fullDump);
    }
    if (renderPath) {
        RenderDocument(engine, renderPath, renderZoom, flags.silent);
    }
    SafeEngineRelease(&engine);
#endif
}
