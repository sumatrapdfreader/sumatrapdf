/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "BaseEngine.h"
#include "ChmEngine.h"
#include "CmdLineParser.h"
#include "Doc.h"
#include "FileUtil.h"
#include "PdfEngine.h"
#include "TgaReader.h"
#include "WinUtil.h"

#define Out(msg, ...) printf(msg, __VA_ARGS__)

// caller must free() the result
char *Escape(WCHAR *string, bool keepString=false)
{
    ScopedMem<WCHAR> freeOnReturn;
    if (!keepString)
        freeOnReturn.Set(string);

    if (str::IsEmpty(string))
        return NULL;

    if (!str::FindChar(string, '<') && !str::FindChar(string, '&') && !str::FindChar(string, '"'))
        return str::conv::ToUtf8(string);

    str::Str<WCHAR> escaped(256);
    for (WCHAR *s = string; *s; s++) {
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

void DumpProperties(BaseEngine *engine)
{
    Out("\t<Properties\n");
    ScopedMem<char> str;
    str.Set(Escape((WCHAR *)engine->FileName(), true));
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
        Out("\t\tProducer=\"%s\"\n", str.Get());
    str.Set(Escape(engine->GetProperty(Prop_PdfVersion)));
    if (str)
        Out("\t\tPdfVersion=\"%s\"\n", str.Get());
    str.Set(Escape(engine->GetProperty(Prop_PdfFileStructure)));
    if (str)
        Out("\t\tPdfFileStructure=\"%s\"\n", str.Get());
    if (!engine->IsPrintingAllowed())
        Out("\t\tPrintingAllowed=\"no\"\n");
    if (!engine->IsCopyingTextAllowed())
        Out("\t\tCopyingTextAllowed=\"no\"\n");
    if (engine->IsImageCollection())
        Out("\t\tImageCollection=\"yes\"\n");
    if (engine->PreferredLayout())
        Out("\t\tPreferredLayout=\"%d\"\n", engine->PreferredLayout());
    Out("\t/>\n");

    ScopedMem<WCHAR> fontlist(engine->GetProperty(Prop_FontList));
    if (fontlist) {
        WStrVec fonts;
        fonts.Split(fontlist, L"\n");
        str.Set(Escape(fonts.Join(L"\n\t\t")));
        Out("\t<FontList>\n\t\t%s\n\t</FontList>\n", str.Get());
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
        ScopedMem<char> title(Escape(item->title, true));
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
            Out("\t\t\t<Element\n\t\t\t\tRect=\"%.0f %.0f %.0f %.0f\"\n", rect.x, rect.y, rect.dx, rect.dy);
            PageDestination *dest = els->At(i)->AsLink();
            if (dest) {
                if (dest->GetDestType() != Dest_None)
                    Out("\t\t\t\tType=\"%s\"\n", PageDestToStr(dest->GetDestType()));
                ScopedMem<char> value(Escape(dest->GetDestValue()));
                if (value)
                    Out("\t\t\t\tTarget=\"%s\"\n", value.Get());
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
    DumpProperties(engine);
    DumpToc(engine);
    for (int i = 1; i <= engine->PageCount(); i++)
        DumpPageContent(engine, i, fullDump);
    if (fullDump)
        DumpThumbnail(engine);
    Out("</EngineDump>\n");
}

void RenderDocument(BaseEngine *engine, const WCHAR *renderPath)
{
    for (int pageNo = 1; pageNo <= engine->PageCount(); pageNo++) {
        RenderedBitmap *bmp = engine->RenderBitmap(pageNo, 1.0, 0);
        size_t len = 0;
        ScopedMem<unsigned char> data;
        if (bmp && str::EndsWithI(renderPath, L".bmp"))
            data.Set(SerializeBitmap(bmp->GetBitmap(), &len));
        else if (bmp)
            data.Set(tga::SerializeBitmap(bmp->GetBitmap(), &len));
        ScopedMem<WCHAR> pageBmpPath(str::Format(renderPath, pageNo));
        file::WriteAll(pageBmpPath, data, len);
        delete bmp;
    }
}

class PasswordHolder : public PasswordUI {
    const WCHAR *password;
public:
    PasswordHolder(const WCHAR *password) : password(password) { }
    virtual WCHAR * GetPassword(const WCHAR *fileName, unsigned char *fileDigest,
                                unsigned char decryptionKeyOut[32], bool *saveKey) {
        return password ? str::Dup(password) : NULL;
    }
};

#define ErrOut(msg, ...) fwprintf(stderr, TEXT(msg), __VA_ARGS__)

int main(int argc, char **argv)
{
#ifdef DEBUG
    // _CrtSetBreakAlloc(527);
#endif

    setlocale(LC_ALL, "C");
    DisableDataExecution();

    WStrVec argList;
    ParseCmdLine(GetCommandLine(), argList);
    if (argList.Count() < 2) {
Usage:
        ErrOut("%s <filename> [-pwd <password>][-full][-alt][-render <path-%%d.tga>]\n",
            path::GetBaseName(argList.At(0)));
        return 0;
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
    bool loadOnly = false;

    for (size_t i = 2; i < argList.Count(); i++) {
        if (str::Eq(argList.At(i), L"-full"))
            fullDump = true;
        else if (str::Eq(argList.At(i), L"-pwd") && i + 1 < argList.Count())
            password = argList.At(++i);
        else if (str::Eq(argList.At(i), L"-render") && i + 1 < argList.Count())
            renderPath = argList.At(++i);
        else if (str::Eq(argList.At(i), L"-alt"))
            useAlternateHandlers = true;
        else if (str::Eq(argList.At(i), L"-loadonly"))
            loadOnly = true;
        else
            goto Usage;
    }

    // optionally use GDI+ rendering for PDF/XPS and the original ChmEngine for CHM
    DebugGdiPlusDevice(useAlternateHandlers);
    DebugAlternateChmEngine(!useAlternateHandlers);

    ScopedGdiPlus gdiPlus;
    DocType engineType;
    PasswordHolder pwdUI(password);
    BaseEngine *engine = EngineManager::CreateEngine(true, filePath, &pwdUI, &engineType);
    if (!engine) {
        ErrOut("Error: Couldn't create an engine for %s!\n", path::GetBaseName(filePath));
        return 1;
    }
    if (!loadOnly)
        DumpData(engine, fullDump);
    if (renderPath)
        RenderDocument(engine, renderPath);
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
