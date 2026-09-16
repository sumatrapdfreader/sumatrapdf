/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "EngineBase.h"
#include "DocController.h"
#include "PagePosition.h"

// "12" -> pageNo 12; "bm:3:5:20" -> bookmark "3:5:20"; anything else -> pageNo 1
StoredPagePos ParseStoredPagePos(Str s) {
    StoredPagePos pos;
    Str rest = s;
    if (str::TrimPrefix(rest, kBookmarkPrefix)) {
        pos.bookmark = rest;
        return pos;
    }
    int n = ParseInt(s);
    pos.pageNo = n < 1 ? 1 : n;
    return pos;
}

TempStr FormatStoredPagePosTemp(int pageNo) {
    return fmt("%d", pageNo);
}

TempStr FormatStoredBookmarkTemp(Str bookmark) {
    return fmt("%s%s", kBookmarkPrefix, bookmark);
}

// engine bookmark of the location for a chaptered doc, else the flat page number
TempStr StoredPagePosForPageTemp(DocController* ctrl, int pageNo) {
    if (!ctrl || pageNo < 1) {
        return FormatStoredPagePosTemp(1);
    }
    if (ctrl->HasChapters()) {
        Location loc = ctrl->LocationFromPageNo(pageNo);
        if (loc.IsValid()) {
            TempStr bm = ctrl->MakeBookmarkTemp(loc);
            if (bm) {
                return FormatStoredBookmarkTemp(bm);
            }
        }
    }
    return FormatStoredPagePosTemp(pageNo);
}

TempStr StoredPagePosFromCtrlTemp(DocController* ctrl) {
    if (!ctrl) {
        return FormatStoredPagePosTemp(1);
    }
    return StoredPagePosForPageTemp(ctrl, ctrl->CurrentPageNo());
}

// leading "chapter:page" from an engine bookmark ("chapter:page:pagesInChapter
// [:r<reparseIdx>]"), no engine access. Rough position hint for comparing two
// bookmarks (e.g. Favorites identity), not a substitute for LookupBookmark's
// re-pagination scaling
Location BookmarkLocationHint(Str bookmark) {
    int chapter = 0;
    int page = 0;
    Str end = str::Parse(bookmark, "%d:%d", &chapter, &page);
    if (str::IsNull(end) || chapter < 1 || page < 1) {
        return kInvalidLocation;
    }
    return {chapter, page};
}

// resolves a persisted PageNo string against ctrl; an unresolvable bookmark
// falls back to page 1
int PageNoFromStoredPagePos(DocController* ctrl, Str stored) {
    StoredPagePos pos = ParseStoredPagePos(stored);
    if (!ctrl || len(pos.bookmark) == 0 || !ctrl->HasChapters()) {
        return pos.pageNo;
    }
    Location loc = ctrl->LookupBookmark(pos.bookmark);
    if (!loc.IsValid()) {
        return 1;
    }
    return ctrl->PageNoFromLocation(loc);
}

// renders/lays out chapters in sequence until enough pages are available to
// map a legacy flat page number to a chapter-relative Location
Location LocationFromFlatPageNo(DocController* ctrl, int flatPageNo) {
    if (!ctrl || !ctrl->HasChapters()) {
        return kInvalidLocation;
    }
    int nChapters = ctrl->ChapterCount();
    if (nChapters <= 0) {
        return kInvalidLocation;
    }
    int remaining = flatPageNo < 1 ? 1 : flatPageNo;
    for (int ch = 1; ch <= nChapters; ch++) {
        int count = ctrl->ChapterPageCount(ch);
        if (remaining <= count) {
            return {ch, remaining};
        }
        remaining -= count;
    }
    int lastCount = ctrl->ChapterPageCount(nChapters);
    return {nChapters, lastCount};
}

// converts a legacy flat int page number into a "bm:..." bookmark for a
// chaptered document. Returns true if migrated.
bool MigrateStoredPagePos(DocController* ctrl, Str* pageNoStr) {
    if (!ctrl || !ctrl->HasChapters() || !pageNoStr) {
        return false;
    }
    StoredPagePos pos = ParseStoredPagePos(*pageNoStr);
    if (pos.bookmark) {
        return false;
    }
    Location loc = LocationFromFlatPageNo(ctrl, pos.pageNo);
    if (!loc.IsValid()) {
        return false;
    }
    TempStr bm = ctrl->MakeBookmarkTemp(loc);
    if (len(bm) == 0) {
        return false;
    }
    TempStr storedBm = FormatStoredBookmarkTemp(bm);
    str::ReplaceWithCopy(pageNoStr, storedBm);
    return true;
}

// migrates fs->pageNo and any fs->favorites from legacy flat page numbers to
// "bm:..." bookmarks. Returns true if any value was updated.
bool MigrateFileStatePagePos(DocController* ctrl, FileState* fs) {
    if (!ctrl || !ctrl->HasChapters() || !fs) {
        return false;
    }
    bool changed = false;
    if (MigrateStoredPagePos(ctrl, &fs->pageNo)) {
        changed = true;
    }
    if (fs->favorites) {
        for (Favorite* fav : *fs->favorites) {
            if (MigrateStoredPagePos(ctrl, &fav->pageNo)) {
                changed = true;
            }
        }
    }
    return changed;
}

// "12/62" or "3:5/20" for a chaptered bookmark; empty if pageCount is unknown
TempStr FormatFileStateProgressTemp(const FileState* fs) {
    if (!fs) {
        return {};
    }
    StoredPagePos pos = ParseStoredPagePos(fs->pageNo);
    if (len(pos.bookmark) > 0) {
        int chapter = 0;
        int page = 0;
        int chapterPages = 0;
        Str end = str::Parse(pos.bookmark, "%d:%d:%d", &chapter, &page, &chapterPages);
        if (str::IsNull(end) || chapter < 1 || page < 1) {
            return {};
        }
        if (chapterPages >= 1) {
            return fmt("%d:%d/%d", chapter, page, chapterPages);
        }
        return fmt("%d:%d", chapter, page);
    }
    if (fs->pageCount <= 0) {
        return {};
    }
    int curr = pos.pageNo < 1 ? 1 : pos.pageNo;
    return fmt("%d/%d", curr, fs->pageCount);
}
