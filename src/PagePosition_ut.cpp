/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/UtAssert.h"

#include "ChapterTable.h"
#include "Settings.h"
#include "DocController.h"
#include "PagePosition.h"

struct MockDocControllerCallback : DocControllerCallback {
    void PageNoChanged(DocController*, int) override {}
    void ZoomChanged(DocController*, float) override {}
    void GotoLink(IPageDestination*) override {}
    void Repaint() override {}
    void UpdateScrollbars(DisplayModel*, Size) override {}
    void RequestRendering(DisplayModel*, int) override {}
    void RequestPredictiveRendering(DisplayModel*, int, const int*, int) override {}
    void CleanUp(DisplayModel*) override {}
    void RenderThumbnail(DisplayModel*, Size, const OnBitmapRendered*) override {}
    void FocusFrame(bool) override {}
    void SaveDownload(Str, Str) override {}
    void FindResultReceived(int, int, int) override {}
    void FindAllResultReceived(Str) override {}
    void TocChanged(DocController*) override {}
    void PagesRenumbered(DisplayModel*) override {}
};

struct MockChapterDocCtrl : DocController {
    int laidOut[3]{0, 0, 0};
    int chapterSizes[3]{5, 10, 8};

    MockChapterDocCtrl(DocControllerCallback* cb) : DocController(cb) {}

    Str GetFilePath() const override { return {}; }
    Str GetDefaultFileExt() const override { return {}; }
    int PageCount() const override { return 23; }
    TempStr GetPropertyTemp(DocProp) override { return {}; }
    int CurrentPageNo() const override { return 1; }
    void GoToPage(int, bool) override {}
    bool CanNavigate(int) const override { return false; }
    void Navigate(int) override {}
    void SetDisplayMode(DisplayMode, bool) override {}
    DisplayMode GetDisplayMode() const override { return DisplayMode::Automatic; }
    void SetInPresentation(bool) override {}
    void SetZoomVirtual(float, Point*) override {}
    float GetZoomVirtual(bool) const override { return 100.f; }
    float GetNextZoomStep(float) const override { return 100.f; }
    void SetViewPortSize(Size) override {}
    TocTree* GetToc() override { return nullptr; }
    void ScrollTo(int, RectF, float) override {}
    IPageDestination* GetNamedDest(Str) override { return nullptr; }
    void GetDisplayState(FileState*) override {}
    void CreateThumbnail(Size, const OnBitmapRendered*) override {}

    int ChapterCount() override { return 3; }
    int ChapterPageCount(int chapter) override {
        if (chapter < 1 || chapter > 3) {
            return 0;
        }
        laidOut[chapter - 1]++;
        return chapterSizes[chapter - 1];
    }
    TempStr MakeBookmarkTemp(Location loc) override {
        return fmt("%d:%d:%d", loc.chapter, loc.page, ChapterPageCount(loc.chapter));
    }
};

void PagePosition_UnitTests() {
    // plain int
    {
        StoredPagePos pos = ParseStoredPagePos(StrL("12"));
        utassert(pos.pageNo == 12);
        utassert(len(pos.bookmark) == 0);
    }

    // bookmark
    {
        StoredPagePos pos = ParseStoredPagePos(StrL("bm:3:5:20"));
        utassert(len(pos.bookmark) > 0);
        utassert(str::Eq(pos.bookmark, StrL("3:5:20")));
    }

    // garbage falls back to page 1
    {
        StoredPagePos pos = ParseStoredPagePos(StrL("not a number"));
        utassert(pos.pageNo == 1);
        utassert(len(pos.bookmark) == 0);

        pos = ParseStoredPagePos(StrL(""));
        utassert(pos.pageNo == 1);
        utassert(len(pos.bookmark) == 0);

        pos = ParseStoredPagePos(StrL("0"));
        utassert(pos.pageNo == 1);

        pos = ParseStoredPagePos(StrL("-5"));
        utassert(pos.pageNo == 1);
    }

    // formatting round trips
    {
        utassert(str::Eq(FormatStoredPagePosTemp(12), StrL("12")));
        utassert(str::Eq(FormatStoredBookmarkTemp(StrL("3:5:20")), StrL("bm:3:5:20")));
    }

    // BookmarkLocationHint
    {
        Location loc = BookmarkLocationHint(StrL("3:5:20"));
        utassert(loc.chapter == 3 && loc.page == 5);

        loc = BookmarkLocationHint(StrL("3:5:20:r10452"));
        utassert(loc.chapter == 3 && loc.page == 5);

        loc = BookmarkLocationHint(StrL("invalid"));
        utassert(!loc.IsValid());

        loc = BookmarkLocationHint(StrL(""));
        utassert(!loc.IsValid());
    }

    // migration tests
    {
        MockDocControllerCallback cb;
        MockChapterDocCtrl ctrl(&cb);

        // LocationFromFlatPageNo renders only as many chapters as needed
        Location loc1 = LocationFromFlatPageNo(&ctrl, 3);
        utassert(loc1.chapter == 1 && loc1.page == 3);
        utassert(ctrl.laidOut[0] > 0);
        utassert(ctrl.laidOut[1] == 0);
        utassert(ctrl.laidOut[2] == 0);

        Location loc2 = LocationFromFlatPageNo(&ctrl, 12); // 5 in ch1 + 7 in ch2
        utassert(loc2.chapter == 2 && loc2.page == 7);
        utassert(ctrl.laidOut[1] > 0);
        utassert(ctrl.laidOut[2] == 0); // ch3 still not touched!

        Location loc3 = LocationFromFlatPageNo(&ctrl, 50); // beyond end, clamps to last
        utassert(loc3.chapter == 3 && loc3.page == 8);
        utassert(ctrl.laidOut[2] > 0);

        // MigrateStoredPagePos
        Str pageNo = str::Dup(StrL("12"));
        bool migrated = MigrateStoredPagePos(&ctrl, &pageNo);
        utassert(migrated);
        utassert(str::Eq(pageNo, StrL("bm:2:7:10")));

        // already a bookmark -> no migration
        migrated = MigrateStoredPagePos(&ctrl, &pageNo);
        utassert(!migrated);
        utassert(str::Eq(pageNo, StrL("bm:2:7:10")));
        str::Free(pageNo);

        // MigrateFileStatePagePos
        FileState fs{};
        fs.pageNo = str::Dup(StrL("12"));
        Vec<Favorite*> favs;
        Favorite fav1{};
        fav1.pageNo = str::Dup(StrL("3"));
        Favorite fav2{};
        fav2.pageNo = str::Dup(StrL("bm:2:1:10"));
        VecAppend(favs, &fav1);
        VecAppend(favs, &fav2);
        fs.favorites = &favs;

        migrated = MigrateFileStatePagePos(&ctrl, &fs);
        utassert(migrated);
        utassert(str::Eq(fs.pageNo, StrL("bm:2:7:10")));
        utassert(str::Eq(fav1.pageNo, StrL("bm:1:3:5")));
        utassert(str::Eq(fav2.pageNo, StrL("bm:2:1:10")));

        // running again does nothing
        migrated = MigrateFileStatePagePos(&ctrl, &fs);
        utassert(!migrated);

        str::Free(fs.pageNo);
        str::Free(fav1.pageNo);
        str::Free(fav2.pageNo);
    }
}
