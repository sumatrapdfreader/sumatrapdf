/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// must be #included from Regress.cpp

// http://code.google.com/p/sumatrapdf/issues/detail?id=1926
static void Regress00()
{
    TCHAR *filePath = path::Join(TestFilesDir(), _T("epub\\widget-figure-gallery-20120405.epub"));
    VerifyFileExists(filePath);
    Doc doc(Doc::CreateFromFile(filePath));
    CrashAlwaysIf(doc.LoadingFailed());
    CrashAlwaysIf(Doc_Epub != doc.GetDocType());

    PoolAllocator   textAllocator;
    HtmlFormatterArgs *args = CreateFormatterArgsDoc(doc, 820, 920, &textAllocator);
    HtmlPage *pages[3];
    HtmlFormatter *formatter = CreateFormatter(doc, args);
    int page = 0;
    for (HtmlPage *pd = formatter->Next(); pd; pd = formatter->Next()) {
        pages[page++] = pd;
        if (page == dimof(pages))
            break;
    }
    delete formatter;
    delete args;
    CrashAlwaysIf(page != 3);

    args = CreateFormatterArgsDoc(doc, 820, 920, &textAllocator);
    args->reparseIdx = pages[2]->reparseIdx;
    formatter = CreateFormatter(doc, args);
    // if bug is present, this will crash in formatter->Next()
    for (HtmlPage *pd = formatter->Next(); pd; pd = formatter->Next()) {
        delete pd;
    }
    delete formatter;
    delete args;
}

