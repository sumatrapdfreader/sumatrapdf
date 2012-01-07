/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "EngineManager.h"
#include "FileUtil.h"
#include "CmdLineParser.h"
#include "WinUtil.h"
#include "Scopes.h"

#define Out(msg, ...) printf(msg, __VA_ARGS__)

// caller must free() the result
char *Escape(TCHAR *string, bool keepString=false)
{
    ScopedMem<TCHAR> freeOnReturn;
    if (!keepString)
        freeOnReturn.Set(string);

    if (str::IsEmpty(string))
        return NULL;

    if (!str::FindChar(string, '<') && !str::FindChar(string, '&') && !str::FindChar(string, '"'))
        return str::conv::ToUtf8(string);

    str::Str<TCHAR> escaped(256);
    for (TCHAR *s = string; *s; s++) {
        switch (*s) {
        case '&': escaped.Append(_T("&amp;")); break;
        case '<': escaped.Append(_T("&lt;")); break;
        case '>': escaped.Append(_T("&gt;")); break;
        case '"': escaped.Append(_T("&quot;")); break;
        case '\'': escaped.Append(_T("&amp;")); break;
        default: escaped.Append(*s); break;
        }
    }
    return str::conv::ToUtf8(escaped.Get());
}

void DumpProperties(BaseEngine *engine)
{
    Out("\t<Properties\n");
    ScopedMem<char> str;
    str.Set(Escape((TCHAR *)engine->FileName(), true));
    Out("\t\tFilePath=\"%s\"\n", str.Get());
    str.Set(Escape(engine->GetProperty("Title")));
    if (str)
        Out("\t\tTitle=\"%s\"\n", str.Get());
    str.Set(Escape(engine->GetProperty("Subject")));
    if (str)
        Out("\t\tSubject=\"%s\"\n", str.Get());
    str.Set(Escape(engine->GetProperty("Author")));
    if (str)
        Out("\t\tAuthor=\"%s\"\n", str.Get());
    str.Set(Escape(engine->GetProperty("CreationDate")));
    if (str)
        Out("\t\tCreationDate=\"%s\"\n", str.Get());
    str.Set(Escape(engine->GetProperty("ModDate")));
    if (str)
        Out("\t\tModDate=\"%s\"\n", str.Get());
    str.Set(Escape(engine->GetProperty("Creator")));
    if (str)
        Out("\t\tCreator=\"%s\"\n", str.Get());
    str.Set(Escape(engine->GetProperty("Producer")));
    if (str)
        Out("\t\tProducer=\"%s\"\n", str.Get());
    str.Set(Escape(engine->GetProperty("PdfVersion")));
    if (str)
        Out("\t\tPdfVersion=\"%s\"\n", str.Get());
    if (!engine->IsPrintingAllowed())
        Out("\t\tPrintingAllowed=\"no\"\n");
    if (!engine->IsCopyingTextAllowed())
        Out("\t\tCopyingTextAllowed=\"no\"\n");
    if (engine->IsImageCollection())
        Out("\t\tImageCollection=\"yes\"\n");
    if (engine->PreferredLayout())
        Out("\t\tPreferredLayout=\"%d\"\n", engine->PreferredLayout());
    Out("\t/>\n");
}

// caller must free() the result
char *DestRectToStr(BaseEngine *engine, PageDestination *dest)
{
    if (ScopedMem<TCHAR>(dest->GetDestName())) {
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
        return str::Format("Point=\"%.0f,%.0f\"", pt.x, pt.y);
    }
    if (rect.dx != DEST_USE_DEFAULT && rect.dy != DEST_USE_DEFAULT) {
        RECT rc = engine->Transform(rect, pageNo, 1.0, 0).ToRECT();
        return str::Format("Rect=\"%d,%d,%d,%d\"", rc.left, rc.top, rc.right, rc.bottom);
    }
    if (rect.y != DEST_USE_DEFAULT) {
        PointD pt = engine->Transform(rect.TL(), pageNo, 1.0, 0);
        return str::Format("Point=\"x,%.0f\"", pt.y);
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

void DumpPageData(BaseEngine *engine, int pageNo, bool fullDump)
{
    // ensure that the page is loaded
    engine->BenchLoadPage(pageNo);

    Out("\t<Page Number=\"%d\"\n", pageNo);
    if (engine->HasPageLabels()) {
        ScopedMem<char> label(Escape(engine->GetPageLabel(pageNo)));
        Out("\t\tLabel=\"%s\"\n", label.Get());
    }
    if (engine->PageRotation(pageNo))
        Out("\t\tRotation=\"%d\"\n", engine->PageRotation(pageNo));
    RectD bbox = engine->PageMediabox(pageNo);
    if (!bbox.IsEmpty())
        Out("\t\tMediaBox=\"%.0f %.0f %.0f %.0f\"\n", bbox.x, bbox.y, bbox.dx, bbox.dy);
    if (engine->IsImagePage(pageNo))
        Out("\t\tImagePage=\"yes\"\n");
    Out("\t>\n");

    if (fullDump) {
        ScopedMem<char> text(Escape(engine->ExtractPageText(pageNo, _T("\n"))));
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
                if (dest->GetDestType()) {
                    ScopedMem<char> type(Escape(str::conv::FromAnsi(dest->GetDestType())));
                    Out("\t\t\t\tType=\"%s\"\n", type.Get());
                }
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
    ScopedMem<unsigned char> data(SerializeBitmap(bmp->GetBitmap(), &len));
    ScopedMem<char> hexData(data ? str::MemToHex(data, len) : NULL);
    if (hexData)
        Out("\t<Thumbnail>\n\t\t%s\n\t</Thumbnail>\n", hexData.Get());
    else
        Out("\t<Thumbnail />\n");

    delete bmp;
}

void DumpData(BaseEngine *engine, bool fullDump)
{
    Out("\xEF\xBB\xBF"); // UTF-8 BOM
    Out("<?xml version=\"1.0\"?>\n");
    Out("<EngineDump>\n");
    DumpProperties(engine);
    DumpToc(engine);
    for (int i = 1; i <= engine->PageCount(); i++)
        DumpPageData(engine, i, fullDump);
    if (fullDump)
        DumpThumbnail(engine);
    Out("</EngineDump>\n");
}

#define ErrOut(msg, ...) _ftprintf(stderr, _T(msg), __VA_ARGS__)

int main(int argc, char **argv)
{
    CmdLineParser argList(GetCommandLine());
    if (argList.Count() < 2) {
        ErrOut("Usage: %s <filename> [-full]\n", path::GetBaseName(argList.At(0)));
        return 0;
    }

    WIN32_FIND_DATA fdata;
    HANDLE hfind = FindFirstFile(argList.At(1), &fdata);
    if (INVALID_HANDLE_VALUE == hfind) {
        ErrOut("Error: %s not found!\n", path::GetBaseName(argList.At(1)));
        return 1;
    }
    FindClose(hfind);
    ScopedMem<TCHAR> filePath(path::GetDir(argList.At(1)));
    filePath.Set(path::Join(filePath, fdata.cFileName));

    ScopedGdiPlus gdiPlus;
    EngineType engineType;
    BaseEngine *engine = EngineManager::CreateEngine(filePath, NULL, &engineType);
    if (!engine) {
        ErrOut("Error: Couldn't create an engine for %s!\n", path::GetBaseName(filePath));
        return 1;
    }
    DumpData(engine, argList.Find(_T("-full")) != -1);
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
