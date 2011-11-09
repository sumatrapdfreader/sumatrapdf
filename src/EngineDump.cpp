/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "EngineManager.h"
#include "FileUtil.h"
#include "CmdLineParser.h"
#include "WinUtil.h"

#define Out(msg, ...) _tprintf(_T(msg), __VA_ARGS__)
#define ErrOut(msg, ...) _ftprintf(stderr, _T(msg), __VA_ARGS__)

TCHAR *Escape(TCHAR *string)
{
    if (str::IsEmpty(string)) {
        free(string);
        return NULL;
    }

    if (!str::FindChar(string, '<') && !str::FindChar(string, '&') && !str::FindChar(string, '"'))
        return string;

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
    free(string);
    return escaped.StealData();
}

void DumpProperties(BaseEngine *engine)
{
    Out("\t<Properties\n");
    ScopedMem<TCHAR> str;
    str.Set(Escape(str::Dup(engine->FileName())));
    Out("\t\tFilePath=\"%s\"\n", str);
    str.Set(Escape(engine->GetProperty("Title")));
    if (str)
        Out("\t\tTitle=\"%s\"\n", str);
    str.Set(Escape(engine->GetProperty("Subject")));
    if (str)
        Out("\t\tSubject=\"%s\"\n", str);
    str.Set(Escape(engine->GetProperty("Author")));
    if (str)
        Out("\t\tAuthor=\"%s\"\n", str);
    str.Set(Escape(engine->GetProperty("CreationDate")));
    if (str)
        Out("\t\tCreationDate=\"%s\"\n", str);
    str.Set(Escape(engine->GetProperty("ModDate")));
    if (str)
        Out("\t\tModDate=\"%s\"\n", str);
    str.Set(Escape(engine->GetProperty("Creator")));
    if (str)
        Out("\t\tCreator=\"%s\"\n", str);
    str.Set(Escape(engine->GetProperty("Producer")));
    if (str)
        Out("\t\tProducer=\"%s\"\n", str);
    str.Set(Escape(engine->GetProperty("PdfVersion")));
    if (str)
        Out("\t\tPdfVersion=\"%s\"\n", str);
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

void DumpTocItem(DocTocItem *item, int level)
{
    for (; item; item = item->next) {
        ScopedMem<TCHAR> title(Escape(str::Dup(item->title)));
        for (int i = 0; i < level; i++) Out("\t");
        Out("<Item Title=\"%s\"", title);
        if (item->pageNo)
            Out(" Page=\"%d\"", item->pageNo);
        if (!item->child)
            Out(" />\n");
        else {
            Out(">\n");
            DumpTocItem(item->child, level + 1);
            for (int i = 0; i < level; i++) Out("\t");
            Out("</Item>\n");
        }
    }
}

void DumpToc(BaseEngine *engine)
{
    DocTocItem *root = engine->GetTocTree();
    if (root) {
        Out("\t<TocTree%s>\n", engine->HasTocTree() ? _T("") : _T(" Expected=\"no\""));
        DumpTocItem(root, 2);
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
        ScopedMem<TCHAR> label(Escape(engine->GetPageLabel(pageNo)));
        Out("\t\tLabel=\"%s\"\n", label);
    }
    if (engine->PageRotation(pageNo))
        Out("\t\tRotation=\"%d\"\n", engine->PageRotation(pageNo));
    RectD bbox = engine->PageMediabox(pageNo);
    Out("\t\tMediaBox=\"%.0f %.0f %.0f %.0f\"\n", bbox.x, bbox.y, bbox.dx, bbox.dy);
    if (engine->IsImagePage(pageNo))
        Out("\t\tImagePage=\"yes\"\n");
    Out("\t>\n");

    if (fullDump) {
        ScopedMem<TCHAR> text(Escape(engine->ExtractPageText(pageNo, _T("\n"))));
        if (text)
            Out("\t\t<TextContent>\n%s\t\t</TextContent>\n", text);
    }

    Vec<PageElement *> *els = engine->GetElements(pageNo);
    if (els && els->Count() > 0) {
        Out("\t\t<PageElements>\n");
        for (size_t i = 0; i < els->Count(); i++) {
            RectD rect = els->At(i)->GetRect();
            Out("\t\t\t<Element\n\t\t\t\tRect=\"%.0f %.0f %.0f %.0f\"\n", rect.x, rect.y, rect.dx, rect.dy);
            PageDestination *dest = els->At(i)->AsLink();
            if (dest) {
                if (dest->GetType()) {
                    ScopedMem<TCHAR> type(Escape(str::conv::FromAnsi(dest->GetType())));
                    Out("\t\t\t\tType=\"%s\"\n", type);
                }
                if (dest->GetDestPageNo())
                    Out("\t\t\t\tLinkedPage=\"%d\"\n", dest->GetDestPageNo());
                ScopedMem<TCHAR> value(Escape(dest->GetDestValue()));
                if (value)
                    Out("\t\t\t\tTarget=\"%s\"\n", value);
            }
            ScopedMem<TCHAR> name(Escape(els->At(i)->GetValue()));
            if (name)
                Out("\t\t\t\tLabel=\"%s\"\n", name);
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

    float zoom = min(128 / (float)rect.dx, 128 / (float)rect.dy);
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
    ScopedMem<TCHAR> hexDataT(hexData ? str::conv::FromAnsi(hexData) : NULL);
    if (hexDataT)
        Out("\t<Thumbnail>\n\t\t%s\n\t</Thumbnail>\n", hexDataT);
    else
        Out("\t<Thumbnail />\n");

    delete bmp;
}

void DumpData(BaseEngine *engine, bool fullDump)
{
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

    BaseEngine *engine = EngineManager::CreateEngine(filePath);
    if (!engine) {
        ErrOut("Error: Couldn't create an engine for %s!\n", path::GetBaseName(filePath));
        return 1;
    }
    DumpData(engine, argList.Find(_T("-full")) != -1);
    delete engine;

    return 0;
}
