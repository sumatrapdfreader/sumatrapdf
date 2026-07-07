/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// must be #included from Regress.cpp

// test that a given epub file loads correctly. crash otherwise
static void RegressTestEpubLoading(Str fileName) {
    TempStr filePath = path::JoinTemp(TestFilesDir(), fileName);
    VerifyFileExists(filePath);
    FileType kind = GuessFileType(fileName, true);
    ReportIf(!EpubDoc::IsSupportedFileType(kind));
    EpubDoc* doc = EpubDoc::CreateFromFile(filePath);
    ReportIf(!doc);
    delete doc;
}

// https://code.google.com/archive/p/sumatrapdf/issues/2102
static void Regress02() {
    RegressTestEpubLoading("epub\\sumatra-crash-nov-23-2012.epub");
}

// https://code.google.com/archive/p/sumatrapdf/issues/2091
static void Regress01() {
    RegressTestEpubLoading("epub\\sumatra-crash-nov-12-2012.epub");
}

// https://code.google.com/archive/p/sumatrapdf/issues/1926
static void Regress00() {
    TempStr filePath = path::JoinTemp(TestFilesDir(), StrL("epub\\widget-figure-gallery-20120405.epub"));
    VerifyFileExists(filePath);
    FileType kind = GuessFileType(filePath, true);
    ReportIf(!EpubDoc::IsSupportedFileType(kind));
    EpubDoc* doc = EpubDoc::CreateFromFile(filePath);
    ReportIf(!doc);

    Arena* textAllocator = ArenaNew();
    HtmlFormatterArgs* args = CreateFormatterDefaultArgs(820, 920, textAllocator);
    if (!args) {
        return;
    }
    args->htmlStr = doc->GetHtmlData();
    HtmlPage* pages[3];
    HtmlFormatter* formatter = new EpubFormatter(args, doc);
    int page = 0;
    for (HtmlPage* pd = formatter->Next(); pd; pd = formatter->Next()) {
        pages[page++] = pd;
        if (page == dimof(pages)) break;
    }
    delete formatter;
    delete args;
    ReportIf(page != 3);

    args = CreateFormatterDefaultArgs(820, 920, textAllocator);
    args->htmlStr = doc->GetHtmlData();
    args->reparseIdx = pages[2]->reparseIdx;
    formatter = new EpubFormatter(args, doc);
    // if bug is present, this will crash in formatter->Next()
    for (HtmlPage* pd = formatter->Next(); pd; pd = formatter->Next()) {
        delete pd;
    }
    delete formatter;
    delete args;
    delete doc;
    ArenaDelete(textAllocator);
}
