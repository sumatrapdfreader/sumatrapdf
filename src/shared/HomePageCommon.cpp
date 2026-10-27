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

// lays the header out in a row, the brand at its start: false if the search
// box would be narrower than searchMinDx (unless `force`)
static bool PlaceHomeHeader(const HomeHeaderIn& in, int rowX, int rowDx, HomeHeaderOut& out, bool force) {
    int leftDx = out.showBrand ? in.brandDx + in.gapX : 0;
    int openDx = in.openIconDx + (out.showOpenText ? in.openTextGap + in.openTextDx : 0);
    int rightDx = in.gapX + openDx + in.gapX + in.viewIconsDx;
    int availDx = rowDx - leftDx - rightDx;
    if (availDx < in.searchMinDx && !force) {
        return false;
    }
    out.rowX = rowX;
    out.rowDx = rowDx;
    out.brandX = rowX;
    out.viewIconsX = rowX + rowDx - in.viewIconsDx;
    out.openX = out.viewIconsX - in.gapX - openDx;
    out.searchDx = std::min(std::max(availDx, in.searchMinDx / 2), in.searchMaxDx);
    // centered on the row unless a flank is in the way
    int x = rowX + ((rowDx - out.searchDx) / 2);
    x = std::min(x, out.openX - in.gapX - out.searchDx);
    out.searchX = std::max(x, rowX + leftDx);
    return true;
}

// The brand ends on the first column's right edge, the Open link and view
// icons on the last column's, the search box is centered. When the grid is too
// narrow for that (one or two columns, issue #6367), the row spans the page
// and drops the "Open..." text, then the brand, before anything overlaps.
void LayoutHomeHeader(const HomeHeaderIn& in, HomeHeaderOut& out) {
    out = {};
    int openDx = in.openIconDx + in.openTextGap + in.openTextDx;
    int leftDx = std::max(in.brandDx, in.firstColDx) + in.gapX;
    int rightDx = in.gapX + openDx + in.gapX + in.viewIconsDx;
    int flankDx = std::max(leftDx, rightDx);
    int searchDx = std::min(in.gridDx - (2 * flankDx), in.searchMaxDx);
    if (searchDx >= in.searchMinDx) {
        out.rowX = in.gridX;
        out.rowDx = in.gridDx;
        out.brandX = in.gridX + std::max(in.firstColDx - in.brandDx, 0);
        out.viewIconsX = in.gridX + in.gridDx - in.viewIconsDx;
        out.openX = out.viewIconsX - in.gapX - openDx;
        out.searchDx = searchDx;
        out.searchX = in.gridX + ((in.gridDx - searchDx) / 2);
        return;
    }
    if (PlaceHomeHeader(in, in.gridX, in.gridDx, out, false)) {
        return;
    }
    if (PlaceHomeHeader(in, in.pageX, in.pageDx, out, false)) {
        return;
    }
    out.showOpenText = false;
    if (PlaceHomeHeader(in, in.pageX, in.pageDx, out, false)) {
        return;
    }
    out.showBrand = false;
    PlaceHomeHeader(in, in.pageX, in.pageDx, out, true);
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
