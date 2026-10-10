/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/UITask.h"
#include "base/Win.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "MainWindow.h"
#include "Translations.h"
#include "SumatraDialogs.h"
#include "SumatraPDF.h"
#include "OpenFileFilters.h"

#include "SumatraLog.h"

void BuildOpenFileFilters(OpenFileFilterList& out) {
    const struct {
        Str name;
        Str filter;
        bool available;
    } fileFormats[] = {
        {Tr("PDF documents"), StrL("*.pdf;*.p7m"), true},
        {Tr("XPS documents"), StrL("*.xps;*.oxps"), true},
        {Tr("DjVu documents"), StrL("*.djvu"), true},
        {Tr("PostScript documents"), StrL("*.ps;*.eps"), IsEnginePsAvailable()},
        {Tr("DVI documents"), StrL("*.dvi"), IsEngineDviAvailable()},
        {Tr("Comic books"), StrL("*.cbz;*.cbr;*.cb7;*.cbt"), true},
        {Tr("CHM documents"), StrL("*.chm"), true},
        {Tr("SVG documents"), StrL("*.svg"), true},
        {Tr("EPUB ebooks"), StrL("*.epub"), true},
        {Tr("Microsoft Reader ebooks"), StrL("*.lit"), true},
        {Tr("Markdown documents"), StrL("*.md;*.markdown"), true},
        {Tr("Mobi documents"), StrL("*.mobi"), true},
        {Tr("FictionBook documents"), StrL("*.fb2;*.fb2z;*.zfb2;*.fb2.zip"), true},
        {Tr("PalmDoc documents"), StrL("*.pdb;*.prc"), true},
        {Tr("Images"),
         StrL("*.bmp;*.dib;*.gif;*.jpg;*.jpeg;*.jfif;*.jxr;*.hdp;*.wdp;*.png;*.tga;*.tif;*.tiff;*.webp;*.heic;*.heif;"
              "*.avif;*.jxl;*.jp2;*.j2k;*.jpx;*.jpf;*.jpm;*.j2c;*.ico"),
         true},
        {Tr("Text documents"), StrL("*.txt;*.log;*.nfo;file_id.diz;read.me;*.tcr"), true},
    };

    str::Builder allPat;
    for (const auto& ff : fileFormats) {
        if (!ff.available) {
            continue;
        }
        if (len(allPat) > 0) {
            allPat.AppendChar(';');
        }
        allPat.Append(ff.filter);
    }
    out.Add(Tr("All supported documents"), ToStr(allPat));
    for (const auto& ff : fileFormats) {
        if (ff.available && ff.name) {
            out.Add(ff.name, ff.filter);
        }
    }
    out.Add(Tr("All files"), StrL("*.*"));
}
