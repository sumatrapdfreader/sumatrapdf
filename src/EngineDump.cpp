/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/File.h"
#include "base/GdiPlusUtil.h"
#include "base/TgaReader.h"
#include "base/Win.h"

#include "gui/UIModels.h"

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
    return str::ContainsCharAny(s, StrL("<&\""));
}

static TempStr EscapeTemp(Str str) {
    if (len(str) == 0) {
        return {};
    }

    if (!NeedsEscape(str)) {
        return str;
    }

    str::Builder escaped;
    str::BuilderReserve(escaped, 256);
    for (int i = 0; i < str.len; i++) {
        switch (str.s[i]) {
            case '&':
                escaped.Append(StrL("&amp;"));
                break;
            case '<':
                escaped.Append(StrL("&lt;"));
                break;
            case '>':
                escaped.Append(StrL("&gt;"));
                break;
            case '"':
                escaped.Append(StrL("&quot;"));
                break;
            case '\'':
                escaped.Append(StrL("&amp;"));
                break;
            default:
                escaped.AppendChar(str.s[i]);
                break;
        }
    }
    return ToStrTemp(escaped);
}

static void DumpProperties(EngineBase* engine, bool fullDump) {
    Out1(StrL("\t<Properties\n"));
    TempStr str = EscapeTemp(engine->FilePath());
    Out("\t\tFilePath=\"%s\"\n", str.s);
    str = EscapeTemp(engine->GetPropertyTemp(DocProp::Title));
    if (str) {
        Out("\t\tTitle=\"%s\"\n", str.s);
    }
    str = EscapeTemp(engine->GetPropertyTemp(DocProp::Subject));
    if (str) {
        Out("\t\tSubject=\"%s\"\n", str.s);
    }
    str = EscapeTemp(engine->GetPropertyTemp(DocProp::Author));
    if (str) {
        Out("\t\tAuthor=\"%s\"\n", str.s);
    }
    str = EscapeTemp(engine->GetPropertyTemp(DocProp::Copyright));
    if (str) {
        Out("\t\tCopyright=\"%s\"\n", str.s);
    }
    str = EscapeTemp(engine->GetPropertyTemp(DocProp::CreationDate));
    if (str) {
        Out("\t\tCreationDate=\"%s\"\n", str.s);
    }
    str = EscapeTemp(engine->GetPropertyTemp(DocProp::ModificationDate));
    if (str) {
        Out("\t\tModDate=\"%s\"\n", str.s);
    }
    str = EscapeTemp(engine->GetPropertyTemp(DocProp::CreatorApp));
    if (str) {
        Out("\t\tCreator=\"%s\"\n", str.s);
    }
    str = EscapeTemp(engine->GetPropertyTemp(DocProp::PdfProducer));
    if (str) {
        Out("\t\tPdfProducer=\"%s\"\n", str.s);
    }
    str = EscapeTemp(engine->GetPropertyTemp(DocProp::PdfVersion));
    if (str) {
        Out("\t\tPdfVersion=\"%s\"\n", str.s);
    }
    str = EscapeTemp(engine->GetPropertyTemp(DocProp::PdfFileStructure));
    if (str) {
        Out("\t\tPdfFileStructure=\"%s\"\n", str.s);
    }
    str = EscapeTemp(engine->GetPropertyTemp(DocProp::UnsupportedFeatures));
    if (str) {
        Out("\t\tUnsupportedFeatures=\"%s\"\n", str.s);
    }
    if (!engine->AllowsPrinting()) {
        Out1(StrL("\t\tPrintingAllowed=\"no\"\n"));
    }
    if (!engine->AllowsCopyingText()) {
        Out1(StrL("\t\tCopyingTextAllowed=\"no\"\n"));
    }
    if (engine->IsImageCollection()) {
        Out("\t\tImageFileDPI=\"%g\"\n", engine->GetFileDPI());
    }
#if 0
    if (engine->preferredLayout.t) {
        Out("\t\tPreferredLayout=\"%d\"\n", engine->preferredLayout);
    }
#endif
    Out1(StrL("\t/>\n"));

    if (!fullDump) {
        return;
    }
    TempStr fontlist = engine->GetPropertyTemp(DocProp::FontList);
    if (fontlist) {
        StrVec fonts;
        Split(&fonts, fontlist, StrL("\n"));
        str = EscapeTemp(Join(&fonts, StrL("\n\t\t")));
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
        return {};
    }
    RectF rect = PageDestGetRect(dest);
    if (rect.IsEmpty()) {
        PointF pt = engine->Transform(rect.TL(), pageNo, 1.0, 0);
        return fmt("Point=\"%.0f %.0f\"", pt.x, pt.y);
    }
    if (rect.dx != kDestUseDefault && rect.dy != kDestUseDefault) {
        Rect rc = engine->Transform(rect, pageNo, 1.0, 0).Round();
        return fmt("Rect=\"%d %d %d %d\"", rc.x, rc.y, rc.dx, rc.dy);
    }
    if (rect.y != kDestUseDefault) {
        PointF pt = engine->Transform(rect.TL(), pageNo, 1.0, 0);
        return fmt("Point=\"x %.0f\"", pt.y);
    }
    return {};
}

static void DumpTocItem(EngineBase* engine, TocItem* item, int level, int& idCounter) {
    for (; item; item = item->next) {
        TempStr title = EscapeTemp(item->title);
        for (int i = 0; i < level; i++) {
            Out1(StrL("\t"));
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
            Out1(StrL(" />\n"));
        } else {
            if (item->isOpenDefault) {
                Out1(StrL(" Expanded=\"yes\""));
            }
            Out1(StrL(">\n"));
            DumpTocItem(engine, item->child, level + 1, idCounter);
            for (int i = 0; i < level; i++) {
                Out1(StrL("\t"));
            }
            Out1(StrL("</Item>\n"));
        }
    }
}

static void DumpToc(EngineBase* engine) {
    TocTree* tree = engine->GetToc();
    if (!tree) {
        return;
    }
    auto* root = tree->root;
    if (root) {
        Out("\t<TocTree%s>\n", engine->HasToc() ? "" : " Expected=\"no\"");
        int idCounter = 0;
        DumpTocItem(engine, root, 2, idCounter);
        Out1(StrL("\t</TocTree>\n"));
    } else if (engine->HasToc()) {
        Out1(StrL("\t<TocTree />\n"));
    }
}

static Str ElementTypeToStr(IPageElement* el) {
    Kind kind = el->GetKind();
    if (kind) {
        return Str(kind);
    }
    return StrL("unknown");
}

__unused static Str PageDestToStr(Kind kind) {
    return Str(kind);
}

static void DumpPageContent(EngineBase* engine, int pageNo, bool fullDump) {
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
        Out1(StrL("\t\tHasClipOptimizations=\"no\"\n"));
    }
    Out1(StrL("\t>\n"));

    if (fullDump) {
        PageText pageText = engine->ExtractPageText(pageNo);
        if (pageText.text) {
            TempStr text = EscapeTemp(Str(pageText.text.s));
            if (text) {
                Out("\t\t<TextContent>\n%s\t\t</TextContent>\n", text.s);
            }
        }
        FreePageText(&pageText);
    }

    Vec<IPageElement*> els = engine->GetElements(pageNo);
    if (len(els) > 0) {
        Out1(StrL("\t\t<PageElements>\n"));
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
            Out1(StrL("\t\t\t/>\n"));
        }
        Out1(StrL("\t\t</PageElements>\n"));
    }

    Out1(StrL("\t</Page>\n"));
}

static void DumpThumbnail(EngineBase* engine) {
    RectF rect = engine->Transform(engine->PageMediabox(1), 1, 1.0, 0);
    if (rect.IsEmpty()) {
        Out1(StrL("\t<Thumbnail />\n"));
        return;
    }

    float zoom = std::min(128 / rect.dx, 128 / rect.dy) - 0.001f;
    Rect thumb = RectF(0, 0, rect.dx * zoom, rect.dy * zoom).Round();
    rect = engine->Transform(ToRectF(thumb), 1, zoom, 0, true);
    RenderPageArgs args(1, zoom, 0, &rect);
    Pixmap* bmp = engine->RenderPage(args);
    if (!bmp) {
        Out1(StrL("\t<Thumbnail />\n"));
        return;
    }

    Str imgData = tga::PixmapToTgaFormat(bmp);
    TempStr hexData = imgData.s ? str::MemToHexTemp(imgData) : Str{};
    if (hexData) {
        Out("\t<Thumbnail>\n\t\t%s\n\t</Thumbnail>\n", hexData.s);
    } else {
        Out1(StrL("\t<Thumbnail />\n"));
    }
    str::Free(imgData);
    FreePixmap(bmp);
}

__unused static void DumpData(EngineBase* engine, bool fullDump) {
    EnsureFullLayout(engine);
    ResolveTocPages(engine, engine->GetToc());
    Out1(StrL(kUtf8Bom));
    Out1(StrL("<?xml version=\"1.0\"?>\n"));
    Out1(StrL("<EngineDump>\n"));
    DumpProperties(engine, fullDump);
    DumpToc(engine);
    for (int i = 1; i <= engine->PageCount(); i++) {
        DumpPageContent(engine, i, fullDump);
    }
    if (fullDump) {
        DumpThumbnail(engine);
    }
    Out1(StrL("</EngineDump>\n"));
}

#define ErrOut(msg, ...) fprintf(stderr, msg "\n", __VA_ARGS__)
#define ErrOut1(msg) fprintf(stderr, "%s", msg "\n")

static bool CheckRenderPath(Str path) {
    ReportIf(len(path) == 0);
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
__unused static bool RenderDocument(EngineBase* engine, Str renderPath, float zoom = 1.f, bool silent = false) {
    if (!CheckRenderPath(renderPath)) {
        return false;
    }

    if (str::EndsWithI(renderPath, StrL(".txt"))) {
        str::Builder text;
        str::BuilderReserve(text, 1024);
        for (int pageNo = 1; pageNo <= engine->PageCount(); pageNo++) {
            PageText pageText = engine->ExtractPageText(pageNo);
            if (pageText.text) {
                text.Append(Str(pageText.text.s));
            }
            FreePageText(&pageText);
        }
        if (silent) {
            return true;
        }
        TempStr txtFilePath = fmt(renderPath.s, 0);
        TempStr textCrLf = str::ReplaceTemp(ToStr(text), StrL("\n"), StrL("\r\n"));
        TempStr textUTF8BOM = str::JoinTemp(StrL(kUtf8Bom), textCrLf);
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
        if (str::EndsWithI(pageBmpPath, StrL(".png"))) {
            Gdiplus::Bitmap gbmp(bmp->hbmp, nullptr);
            CLSID pngEncId = GetGdiPlusEncoderClsid(L"image/png");
            WCHAR* pageBmpPathW = CWStrTemp(pageBmpPath);
            gbmp.Save(pageBmpPathW, &pngEncId, nullptr);
        } else if (str::EndsWithI(pageBmpPath, StrL(".bmp"))) {
            Str imgData = PixmapToBmpFormat(bmp);
            if (len(imgData) > 0) {
                file::WriteFile(pageBmpPath, imgData);
                str::Free(imgData);
            }
        } else { // render as TGA for all other file extensions
            // a serialized TGA starts with a 0 byte
            Str imgData = tga::PixmapToTgaFormat(bmp);
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
    Str GetPassword(Str /*path*/, u8* /*fileDigest*/, __unused u8 decryptionKeyOut[32], bool* /*saveKey*/) override {
        return str::Dup(password);
    }
};

void EngineDump(const Flags& flags) {
    if (flags.silent) {
        FILE* nul;
        freopen_s(&nul, "NUL", "w", stdout);
        freopen_s(&nul, "NUL", "w", stderr);
    }

#if 0
    ScopedGdiPlus gdiPlus;

    // Normalize casing / short names when the path exists (embedded docs may use
    // "C:\file.pdf:3:0" which does not exist as a real file path).
    if (file::Exists(filePath)) {
        TempStr norm = path::NormalizeTemp(filePath);
        if (norm) {
            filePath = norm;
        }
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
