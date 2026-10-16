/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "PagePosition.h"
#include "Theme.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "FileHistory.h"
#include "Translations.h"
#include "AppTools.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "HomePage.h"
#include "HomePageCommon.h"

#include "SumatraLog.h"

// Home page state that orig and ng keep the same way: the tip of the day and
// promo lines, and how many files the page lists.

// the tip markup, one line each; the selected one is parsed by the tip band
StrVec gTipLines;

StrVec gPromoLines;

bool gTipsParsed = false;

bool gSelectedIsPromo = false;

int gSelectedTipIdx = -1;

void CollectTipsFromString(Str src, StrVec* out) {
    StrVec lines;
    Split(&lines, src, StrL("\n"));
    for (int i = 0; i < len(lines); i++) {
        Str line = lines[i];
        if (str::IsEmptyOrWhiteSpace(line)) {
            continue;
        }
        out->Append(line);
    }
}

// the markup of the tip currently on show, {} when there is none
Str SelectedTipLine() {
    if (!gSettings->showTips || gSelectedTipIdx < 0) {
        return {};
    }
    StrVec& v = gSelectedIsPromo ? gPromoLines : gTipLines;
    if (gSelectedTipIdx >= len(v)) {
        return {};
    }
    if (gSelectedIsPromo) {
        return v[gSelectedTipIdx];
    }
    // translated when shown, so a language change applies without re-parsing
    return str::JoinTemp(Tr("Tip:"), StrL(" "), Tr(v[gSelectedTipIdx]));
}

void PickRandomTipOrPromo() {
    bool pickPromo = (len(gPromoLines) > 0) && (rand() % 100 < 30);
    if (pickPromo) {
        gSelectedIsPromo = true;
        gSelectedTipIdx = rand() % len(gPromoLines);
    } else if (len(gTipLines) > 0) {
        gSelectedIsPromo = false;
        gSelectedTipIdx = rand() % len(gTipLines);
    }
}

void PickAnotherRandomTip() {
    bool prevIsPromo = gSelectedIsPromo;
    int prev = gSelectedTipIdx;
    // keep picking until we get a different one
    int maxIter = 100;
    while (maxIter-- > 0) {
        PickRandomTipOrPromo();
        if (gSelectedIsPromo != prevIsPromo || gSelectedTipIdx != prev) {
            return;
        }
    }
}

TempStr TrimGitTemp(Str s) {
    if (gitCommidId && str::EndsWith(s, gitCommidId)) {
        int sLen = len(s);
        int gitLen = len(gitCommidId);
        return str::DupTemp(Str(s.s, sLen - gitLen - 7));
    }
    return s;
}

// Home-list entries with a path (same set as thumbnails when search is empty).
int CountHomePageFiles() {
    Vec<FileState*> all;
    if (gSettings && gSettings->homePageSortByFrequentlyRead) {
        FileHistoryGetFrequencyOrder(all);
    } else {
        FileHistoryGetRecentlyOpenedOrder(all);
    }
    int n = 0;
    for (FileState* fs : all) {
        if (fs && len(fs->filePath) > 0) {
            n++;
        }
    }
    return n;
}

// "Page 24 of 47" while a document is being read, otherwise its page count;
// a chapter position ("3:12") is shown as stored
TempStr HomeThumbnailMetaTemp(FileState* fs, float* progressOut) {
    *progressOut = 0;
    bool showProgress = gSettings && gSettings->showHomePageReadingProgress;
    StoredPagePos pos = ParseStoredPagePos(fs->pageNo);
    if (showProgress && len(pos.bookmark) > 0) {
        return FormatFileStateProgressTemp(fs);
    }
    if (fs->pageCount <= 0) {
        return {};
    }
    if (showProgress && pos.pageNo > 1 && pos.pageNo <= fs->pageCount) {
        *progressOut = (float)pos.pageNo / (float)fs->pageCount;
        return fmt(Tr("Page %d of %d").s, pos.pageNo, fs->pageCount);
    }
    if (fs->pageCount == 1) {
        return str::DupTemp(Tr("1 page"));
    }
    return fmt(Tr("%d pages").s, fs->pageCount);
}

// one tip per line; cmd/trans-dl.ts extracts each line for translation
Str sumatraTips = StrL(R"tips(You can [customize scrollbar](CmdChangeScrollbar).
You can [customize keyboard shortcuts](Help/Customize-keyboard-shortcuts).
You can [customize toolbar](Help/Customize-toolbar).
Press (Kbd/(Key/CmdCommandPalette)) to open [command palette](CmdCommandPalette).
You can [extract text from PDF file](Help/Tool-x-extract-text-from-pdf).
You can [toggle menu bar](CmdToggleMenuBar) with (Kbd/(Key/CmdToggleMenuBar)).
You can [toggle toolbar](CmdToggleToolbar) with (Kbd/(Key/CmdToggleToolbar)).
You can [edit PDF annotations](Help/Editing-annotations).
You can enable [citation preview on hover](Help/Citation-hover-preview).
You can [have documents read aloud](Help/Read-Aloud).
You can [sign a PDF](Help/Sign-a-PDF).
You can [fill PDF forms](Help/Fill-PDF-forms).
You can [merge PDFs](Help/Merge-PDFs) and [reorder pages](Help/Reorder-PDF-pages).
You can [split a PDF](Help/Split-a-PDF).
You can [redact a PDF](Help/Redact-a-PDF).
You can [present a PDF](Help/Present-a-PDF) full screen.
You can [use SumatraPDF with LaTeX](Help/LaTeX-integration) for forward and inverse search.
You can [read comics and manga](Help/Comics-and-manga) right to left.
You can [bookmark pages as favorites](Help/Managing-favorites).
You can [chat with AI about a document](Help/AI-Chat-with-document).
You can [customize theme colors](Help/Customize-theme-colors).
You can [save a page region as an image](Help/Save-page-region-as-image).
You can [print selected pages](Help/Print-selected-pages).
Press (Kbd/@) to quickly switch between tabs.
Press (Kbd/#) to navigate file history.
Press (Kbd/$) to navigate via favorites.
Press (Kbd/%) to navigate via table of contents.
Press (Kbd/&) to navigate via thumbnails.
Press (Kbd/(Key/CmdToggleAutomaticallyScroll)) to [scroll automatically](Help/Scrolling-and-zooming#scroll-automatically).
You can keep your place with the [reading bar](Help/Scrolling-and-zooming#keep-your-place-with-a-reading-bar).
Press (Kbd/=) to [change a setting](CmdQuickEditSettings) without opening Advanced Settings.
You can [browse files in the current folder](CmdNavigateFilesInFolder). [Learn more](Help/Scrolling-and-zooming#browse-files-in-the-folder)
You can overlay a [page grid](Help/Scrolling-and-zooming#overlay-a-page-grid) for measurement and calligraphy.
You can [zoom to fit the page content](Help/Scrolling-and-zooming#fit-the-page-content).
You can [paste an image from the clipboard](Help/Add-an-image-to-a-PDF#insert-an-image-from-the-clipboard).
You can [translate selected text](Help/Customize-search-translation-services#search-or-translate-the-selection).
)tips");
