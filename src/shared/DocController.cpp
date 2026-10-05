/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/UIModels.h"
#include "EngineBase.h"
#include "DocController.h"

Location DocController::CurrentLocation() {
    return LocFromPageNo(CurrentPageNo());
}

void DocController::GoToLocation(Location loc, bool addNavPoint) {
    GoToPage(loc.page, addNavPoint);
}

Location DocController::LocationFromPageNo(int pageNo) {
    return LocFromPageNo(pageNo);
}

int DocController::PageNoFromLocation(Location loc) {
    return loc.page;
}

// default: dest->loc if valid else LocFromPageNo(dest->pageNo)
Location DocController::ResolveDest(IPageDestination* dest) {
    if (!dest) {
        return kInvalidLocation;
    }
    if (dest->loc.IsValid()) {
        return dest->loc;
    }
    return LocFromPageNo(dest->pageNo);
}

// default: no chapters, no engine bookmark
TempStr DocController::MakeBookmarkTemp(__unused Location loc) {
    return {};
}

Location DocController::LookupBookmark(__unused Str s) {
    return kInvalidLocation;
}

// default: single-chapter, clamp page into [1, PageCount()]
Location DocController::ClampLocation(Location loc) {
    int n = PageCount();
    int p = limitValue(loc.page, 1, n);
    return {1, p};
}
