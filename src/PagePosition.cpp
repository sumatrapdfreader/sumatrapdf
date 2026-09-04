/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/UIModels.h"
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

// engine bookmark of the current location for a chaptered doc, else the
// flat current page number
TempStr StoredPagePosFromCtrlTemp(DocController* ctrl) {
    if (!ctrl) {
        return FormatStoredPagePosTemp(1);
    }
    if (ctrl->HasChapters()) {
        TempStr bm = ctrl->MakeBookmarkTemp(ctrl->CurrentLocation());
        if (bm) {
            return FormatStoredBookmarkTemp(bm);
        }
    }
    return FormatStoredPagePosTemp(ctrl->CurrentPageNo());
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
