/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "BaseEngine.h"
#include "ChmEngine.h"
#include "CmdLineParser.h"
#include "Doc.h"
#include "FileModifications.h"
#include "FileUtil.h"
using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "PdfEngine.h"
#include "TgaReader.h"
#include "WinUtil.h"

#define Out(msg, ...) printf(msg, __VA_ARGS__)

// caller must free() the result
char *Escape(WCHAR *string)
{
    ScopedMem<WCHAR> freeOnReturn(string);

    if (str::IsEmpty(string))
        return NULL;

    if (!str::FindChar(string, '<') && !str::FindChar(string, '&') && !str::FindChar(string, '"'))
        return str::conv::ToUtf8(string);

    str::Str<WCHAR> escaped(256);
    for (const WCHAR *s = string; *s; s++) {
        switch (*s) {
        case '&': escaped.Append(L"&amp;"); break;
        case '<': escaped.Append(L"&lt;"); break;
        case '>': escaped.Append(L"&gt;"); break;
        case '"': escaped.Append(L"&quot;"); break;
        case '\'': escaped.Append(L"&amp;"); break;
        default: escaped.Append(*s); break;
        }
    }
    return str::conv::ToUtf8(escaped.Get());
}

void DumpProperties(BaseEngine *engine, bool fullDump)
{
    Out("\t<Properties\n");
    ScopedMem<char> str;
    str.Set(Escape(str::Dup(engine->FileName())));
    Out("\t\tFilePath=\"%s\"\n", str.Get());
    str.Set(Escape(engine->GetProperty(Prop_Title)));
    if (str)
        Out("\t\tTitle=\"%s\"\n", str.Get());
    str.Set(Escape(engine->GetProperty(Prop_Subject)));
    if (str)
        Out("\t\tSubject=\"%s\"\n", str.Get());
    str.Set(Escape(engine->GetProperty(Prop_Author)));
    if (str)
        Out("\t\tAuthor=\"%s\"\n", str.Get());
    str.Set(Escape(engine->GetProperty(Prop_Copyright)));
    if (str)
        Out("\t\tCopyright=\"%s\"\n", str.Get());
    str.Set(Escape(engine->GetProperty(Prop_CreationDate)));
    if (str)
        Out("\t\tCreationDate=\"%s\"\n", str.Get());
    str.Set(Escape(engine->GetProperty(Prop_ModificationDate)));
    if (str)
        Out("\t\tModDate=\"%s\"\n", str.Get());
    str.Set(Escape(engine->GetProperty(Prop_CreatorApp)));
    if (str)
        Out("\t\tCreator=\"%s\"\n", str.Get());
    str.Set(Escape(engine->GetProperty(Prop_PdfProducer)));
    if (str)
        Out("\t\tPdfProducer=\"%s\"\n", str.Get());
    str.Set(Escape(engine->GetProperty(Prop_PdfVersion)));
    if (str)
        Out("\t\tPdfVersion=\"%s\"\n", str.Get());
    str.Set(Escape(engine->GetProperty(Prop_PdfFileStructure)));
    if (str)
        Out("\t\tPdfFileStructure=\"%s\"\n", str.Get());
    if (!engine->AllowsPrinting())
        Out("\t\tPrintingAllowed=\"no\"\n");
    if (!engine->AllowsCopyingText())
        Out("\t\tCopyingTextAllowed=\"no\"\n");
    if (engine->IsImageCollection())
        Out("\t\tImageCollection=\"yes\"\n");
    if (engine->PreferredLayout())
        Out("\t\tPreferredLayout=\"%d\"\n", engine->PreferredLayout());
    Out("\t/>\n");

    if (fullDump) {
        ScopedMem<WCHAR> fontlist(engine->GetProperty(Prop_FontList));
        if (fontlist) {
            WStrVec fonts;
            fonts.Split(fontlist, L"\n");
            str.Set(Escape(fonts.Join(L"\n\t\t")));
            Out("\t<FontList>\n\t\t%s\n\t</FontList>\n", str.Get());
        }
    }
}

// caller must free() the result
char *DestRectToStr(BaseEngine *engine, PageDestination *dest)
{
    if (ScopedMem<WCHAR>(dest->GetDestName())) {
        ScopedMem<char> name(Escape(dest->GetDestName()));
        return str::Format("Name=\"%s\"", name);
    }
    // as handled by LinkHandler::ScrollTo in WindowInfo.cpp
    int pageNo = dest->GetDestPageNo();
    if (pageNo <= 0 || pageNo > engine->PageCount())
        return NULL;
    RectD rect = dest->GetDestRect();
    if (rect.IsEmpty()) {
        PointD pt = engine->Transform(rect.TL(), pageNo, 1.0, 0);
        return str::Format("Point=\"%.0f %.0f\"", pt.x, pt.y);
    }
    if (rect.dx != DEST_USE_DEFAULT && rect.dy != DEST_USE_DEFAULT) {
        RectI rc = engine->Transform(rect, pageNo, 1.0, 0).Round();
        return str::Format("Rect=\"%d %d %d %d\"", rc.x, rc.y, rc.dx, rc.dy);
    }
    if (rect.y != DEST_USE_DEFAULT) {
        PointD pt = engine->Transform(rect.TL(), pageNo, 1.0, 0);
        return str::Format("Point=\"x %.0f\"", pt.y);
    }
    return NULL;
}

void DumpTocItem(BaseEngine *engine, DocTocItem *item, int level, int& idCounter)
{
    for (; item; item = item->next) {
        ScopedMem<char> title(Escape(str::Dup(item->title)));
        for (int i = 0; i < level; i++) Out("\t");
        Out("<Item Title=\"%s\"", title.Get());
        if (item->pageNo)
            Out(" Page=\"%d\"", item->pageNo);
        if (item->id != ++idCounter)
            Out(" Id=\"%d\"", item->id);
        if (item->GetLink()) {
            PageDestination *dest = item->GetLink();
            ScopedMem<char> target(Escape(dest->GetDestValue()));
            if (target)
                Out(" Target=\"%s\"", target.Get());
            if (item->pageNo != dest->GetDestPageNo())
                Out(" TargetPage=\"%d\"", dest->GetDestPageNo());
            ScopedMem<char> rectStr(DestRectToStr(engine, dest));
            if (rectStr)
                Out(" Target%s", rectStr.Get());
        }
        if (!item->child)
            Out(" />\n");
        else {
            if (item->open)
                Out(" Expanded=\"yes\"");
            Out(">\n");
            DumpTocItem(engine, item->child, level + 1, idCounter);
            for (int i = 0; i < level; i++) Out("\t");
            Out("</Item>\n");
        }
    }
}

void DumpToc(BaseEngine *engine)
{
    DocTocItem *root = engine->GetTocTree();
    if (root) {
        Out("\t<TocTree%s>\n", engine->HasTocTree() ? "" : " Expected=\"no\"");
        int idCounter = 0;
        DumpTocItem(engine, root, 2, idCounter);
        Out("\t</TocTree>\n");
    }
    else if (engine->HasTocTree())
        Out("\t<TocTree />\n");
    delete root;
}

const char *ElementTypeToStr(PageElement *el)
{
    switch (el->GetType()) {
    case Element_Link: return "Link";
    case Element_Image: return "Image";
    case Element_Comment: return "Comment";
    default: return "Unknown";
    }
}

const char *PageDestToStr(PageDestType destType)
{
#define HandleType(type) if (destType == Dest_ ## type) return #type;
#define HandleTypeDialog(type) if (destType == Dest_ ## type ## Dialog) return #type;
    // common actions
    HandleType(ScrollTo);
    HandleType(LaunchURL);
    HandleType(LaunchEmbedded);
    HandleType(LaunchFile);
    // named actions
    HandleType(NextPage);
    HandleType(PrevPage);
    HandleType(FirstPage);
    HandleType(LastPage);
    HandleTypeDialog(Find);
    HandleType(FullScreen);
    HandleType(GoBack);
    HandleType(GoForward);
    HandleTypeDialog(GoToPage);
    HandleTypeDialog(Print);
    HandleTypeDialog(SaveAs);
    HandleTypeDialog(ZoomTo);
#undef HandleType
#undef HandleTypeDialog
    CrashIf(destType != Dest_None);
    return NULL;
}

void DumpPageContent(BaseEngine *engine, int pageNo, bool fullDump)
{
    // ensure that the page is loaded
    engine->BenchLoadPage(pageNo);

    Out("\t<Page Number=\"%d\"\n", pageNo);
    if (engine->HasPageLabels()) {
        ScopedMem<char> label(Escape(engine->GetPageLabel(pageNo)));
        Out("\t\tLabel=\"%s\"\n", label.Get());
    }
    RectI bbox = engine->PageMediabox(pageNo).Round();
    Out("\t\tMediaBox=\"%d %d %d %d\"\n", bbox.x, bbox.y, bbox.dx, bbox.dy);
    RectI cbox = engine->PageContentBox(pageNo).Round();
    if (cbox != bbox)
        Out("\t\tContentBox=\"%d %d %d %d\"\n", cbox.x, cbox.y, cbox.dx, cbox.dy);
    if (!engine->HasClipOptimizations(pageNo))
        Out("\t\tHasClipOptimizations=\"no\"\n");
    Out("\t>\n");

    if (fullDump) {
        ScopedMem<char> text(Escape(engine->ExtractPageText(pageNo, L"\n")));
        if (text)
            Out("\t\t<TextContent>\n%s\t\t</TextContent>\n", text.Get());
    }

    Vec<PageElement *> *els = engine->GetElements(pageNo);
    if (els && els->Count() > 0) {
        Out("\t\t<PageElements>\n");
        for (size_t i = 0; i < els->Count(); i++) {
            RectD rect = els->At(i)->GetRect();
            Out("\t\t\t<Element Type=\"%s\"\n\t\t\t\tRect=\"%.0f %.0f %.0f %.0f\"\n",
                ElementTypeToStr(els->At(i)), rect.x, rect.y, rect.dx, rect.dy);
            PageDestination *dest = els->At(i)->AsLink();
            if (dest) {
                if (dest->GetDestType() != Dest_None)
                    Out("\t\t\t\tLinkType=\"%s\"\n", PageDestToStr(dest->GetDestType()));
                ScopedMem<char> value(Escape(dest->GetDestValue()));
                if (value)
                    Out("\t\t\t\tLinkTarget=\"%s\"\n", value.Get());
                if (dest->GetDestPageNo())
                    Out("\t\t\t\tLinkedPage=\"%d\"\n", dest->GetDestPageNo());
                ScopedMem<char> rectStr(DestRectToStr(engine, dest));
                if (rectStr)
                    Out("\t\t\t\tLinked%s\n", rectStr.Get());
            }
            ScopedMem<char> name(Escape(els->At(i)->GetValue()));
            if (name)
                Out("\t\t\t\tLabel=\"%s\"\n", name.Get());
            Out("\t\t\t/>\n");
        }
        Out("\t\t</PageElements>\n");
        DeleteVecMembers(*els);
    }
    delete els;

    Out("\t</Page>\n");
}

void DumpThumbnail(BaseEngine *engine)
{
    RectD rect = engine->Transform(engine->PageMediabox(1), 1, 1.0, 0);
    if (rect.IsEmpty()) {
        Out("\t<Thumbnail />\n");
        return;
    }

    float zoom = min(128 / (float)rect.dx, 128 / (float)rect.dy) - 0.001f;
    RectI thumb = RectD(0, 0, rect.dx * zoom, rect.dy * zoom).Round();
    rect = engine->Transform(thumb.Convert<double>(), 1, zoom, 0, true);
    RenderedBitmap *bmp = engine->RenderBitmap(1, zoom, 0, &rect);
    if (!bmp) {
        Out("\t<Thumbnail />\n");
        return;
    }

    size_t len;
    ScopedMem<unsigned char> data(tga::SerializeBitmap(bmp->GetBitmap(), &len));
    ScopedMem<char> hexData(data ? str::MemToHex(data, len) : NULL);
    if (hexData)
        Out("\t<Thumbnail>\n\t\t%s\n\t</Thumbnail>\n", hexData.Get());
    else
        Out("\t<Thumbnail />\n");

    delete bmp;
}

void DumpData(BaseEngine *engine, bool fullDump)
{
    Out(UTF8_BOM);
    Out("<?xml version=\"1.0\"?>\n");
    Out("<EngineDump>\n");
    DumpProperties(engine, fullDump);
    DumpToc(engine);
    for (int i = 1; i <= engine->PageCount(); i++)
        DumpPageContent(engine, i, fullDump);
    if (fullDump)
        DumpThumbnail(engine);
    Out("</EngineDump>\n");
}

void RenderDocument(BaseEngine *engine, const WCHAR *renderPath, bool silent=false)
{
    for (int pageNo = 1; pageNo <= engine->PageCount(); pageNo++) {
        RenderedBitmap *bmp = engine->RenderBitmap(pageNo, 1.0, 0);
        if (!bmp || silent) {
            delete bmp;
            continue;
        }
        ScopedMem<WCHAR> pageBmpPath(str::Format(renderPath, pageNo));
        if (str::EndsWithI(pageBmpPath, L".png")) {
            Bitmap gbmp(bmp->GetBitmap(), NULL);
            CLSID pngEncId = GetEncoderClsid(L"image/png");
            gbmp.Save(pageBmpPath, &pngEncId);
        }
        else if (str::EndsWithI(pageBmpPath, L".bmp")) {
            size_t bmpDataLen;
            ScopedMem<char> bmpData((char *)SerializeBitmap(bmp->GetBitmap(), &bmpDataLen));
            if (bmpData)
                file::WriteAll(pageBmpPath, bmpData, bmpDataLen);
        }
        else { // render as TGA for all other file extensions
            size_t tgaDataLen;
            ScopedMem<unsigned char> tgaData(tga::SerializeBitmap(bmp->GetBitmap(), &tgaDataLen));
            if (tgaData)
                file::WriteAll(pageBmpPath, tgaData, tgaDataLen);
        }
        delete bmp;
    }
}

class PasswordHolder : public PasswordUI {
    const WCHAR *password;
public:
    PasswordHolder(const WCHAR *password) : password(password) { }
    virtual WCHAR * GetPassword(const WCHAR *fileName, unsigned char *fileDigest,
                                unsigned char decryptionKeyOut[32], bool *saveKey) {
        return str::Dup(password);
    }
};

#define ErrOut(msg, ...) fwprintf(stderr, TEXT(msg), __VA_ARGS__)

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "C");
    DisableDataExecution();

    WStrVec argList;
    ParseCmdLine(GetCommandLine(), argList);
    if (argList.Count() < 2) {
Usage:
        ErrOut("%s <filename> [-pwd <password>][-full][-alt][-render <path-%%d.tga>]\n",
            path::GetBaseName(argList.At(0)));
        return 2;
    }

    ScopedMem<WCHAR> filePath;
    WIN32_FIND_DATA fdata;
    HANDLE hfind = FindFirstFile(argList.At(1), &fdata);
    if (INVALID_HANDLE_VALUE != hfind) {
        ScopedMem<WCHAR> dir(path::GetDir(argList.At(1)));
        filePath.Set(path::Join(dir, fdata.cFileName));
        FindClose(hfind);
    }
    else {
        // embedded documents are referred to by an invalid path
        // containing more information after a colon (e.g. "C:\file.pdf:3:0")
        filePath.Set(str::Dup(argList.At(1)));
    }

    bool fullDump = false;
    WCHAR *password = NULL;
    WCHAR *renderPath = NULL;
    bool useAlternateHandlers = false;
    bool loadOnly = false, silent = false;
    int breakAlloc = 0;

    for (size_t i = 2; i < argList.Count(); i++) {
        if (str::Eq(argList.At(i), L"-full"))
            fullDump = true;
        else if (str::Eq(argList.At(i), L"-pwd") && i + 1 < argList.Count())
            password = argList.At(++i);
        else if (str::Eq(argList.At(i), L"-render") && i + 1 < argList.Count())
            renderPath = argList.At(++i);
        else if (str::Eq(argList.At(i), L"-alt"))
            useAlternateHandlers = true;
        // -loadonly and -silent are only meant for profiling
        else if (str::Eq(argList.At(i), L"-loadonly"))
            loadOnly = true;
        else if (str::Eq(argList.At(i), L"-silent"))
            silent = true;
#ifdef DEBUG
        else if (str::Eq(argList.At(i), L"-breakalloc") && i + 1 < argList.Count())
            breakAlloc = _wtoi(argList.At(++i));
#endif
        else
            goto Usage;
    }

#ifdef DEBUG
    if (breakAlloc) {
        _CrtSetBreakAlloc(breakAlloc);
        if (!IsDebuggerPresent())
            MessageBox(NULL, L"Keep your debugger ready for the allocation breakpoint...", L"EngineDump", MB_ICONINFORMATION);
    }
#endif
    if (silent) {
        FILE *nul;
        freopen_s(&nul, "NUL", "w", stdout);
        freopen_s(&nul, "NUL", "w", stderr);
    }

    // optionally use GDI+ rendering for PDF/XPS and the original ChmEngine for CHM
    DebugGdiPlusDevice(useAlternateHandlers);
    bool useChm2Engine = !useAlternateHandlers;

    ScopedGdiPlus gdiPlus;
    DocType engineType;
    PasswordHolder pwdUI(password);
    BaseEngine *engine = EngineManager::CreateEngine(filePath, &pwdUI, &engineType, useChm2Engine);
    if (!engine) {
        ErrOut("Error: Couldn't create an engine for %s!\n", path::GetBaseName(filePath));
        return 1;
    }
    Vec<PageAnnotation> *userAnnots = LoadFileModifications(engine->FileName());
    engine->UpdateUserAnnotations(userAnnots);
    delete userAnnots;
    if (!loadOnly)
        DumpData(engine, fullDump);
    if (renderPath)
        RenderDocument(engine, renderPath, silent);
    delete engine;

#ifdef DEBUG
    // report memory leaks on stderr for engines that shouldn't leak
    if (engineType != Engine_DjVu) {
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    }
#endif

    return 0;
}
