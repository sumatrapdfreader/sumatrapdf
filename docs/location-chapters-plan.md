# Plan: chapter-aware page locations (`Location`)

Status: implemented (phases 1-7). Each phase must build (`bun cmd/build.ts -debug`) before
the next starts. Never commit; the user commits.

## Why

mupdf addresses pages as `fz_location {chapter, page}`. SumatraPDF predates that
and uses one flat 1-based `pageNo` everywhere. For a multi-chapter ebook this
forces laying out every chapter before the first page shows:

- `EngineMupdf::FinishLoading` calls `fz_count_pages` (lays out every EPUB
  chapter) and `FinishNonPDFLoading` then `fz_load_page`s every page.
- `EngineMobi::FinishLoading` runs `HtmlFormatter::FormatAllPages` over the
  whole book. `C:\Users\kjk\OneDrive\!sumatra\1000.mobi` (11.9 MB of HTML,
  886 `<mbp:pagebreak>` chapters, 3152 pages) takes **38 s** to open in the
  release build (`LoadDocument: 38069 ms`). Target: well under 1 s.

## Facts that shape the design (verified, do not re-derive)

1. `.mobi` never reaches mupdf. `EngineCreate.cpp` routes it to
   `CreateEngineMobiFromFile` (our `EngineEbook.cpp` + `MobiDoc`). mupdf's
   `mobi.c` lacks HUFF/CDIC decompression and flattens a book into a single
   HTML chapter, so routing mobi through mupdf is not an option. Both engines
   need chapters: **EPUB in `EngineMupdf`, MOBI in `EngineEbook`.**
2. mupdf's `fz_bookmark` is an in-memory `fz_html_flow*` pointer
   (`html-outline.c: fz_make_html_bookmark`). It cannot be persisted. The
   persisted "bookmark" is therefore ours: `chapter:page:pagesInChapter`
   (proportional restore when the chapter re-paginates), and for `EngineEbook`
   the exact `reparseIdx` byte offset that `FileState.ReparseIdx` was designed
   for. `fz_make_bookmark`/`fz_lookup_bookmark` are not used.
3. In mupdf only EPUB has >1 chapter (`epub_count_chapters` = spine length).
   fb2/html/txt/md are `htdoc` single-chapter; XPS/PDF single-chapter. EPUB
   chapter layout is already lazy inside mupdf (`count_chapter_pages` lays out
   one chapter on demand); what forces the full layout is *our* use of
   `fz_count_pages` / `fz_load_page` / `fz_load_outline`.
4. `fz_load_outline` resolves every outline node with `fz_resolve_link`
   (`outline.c:118`), which lays out every chapter a fragment link points into.
   Use `fz_new_outline_iterator` and keep the URI; resolve on click.
5. Our page numbers are 1-based. `Location` is therefore 1-based in both
   fields; `fz_location` is `{loc.chapter - 1, loc.page - 1}`.
6. `FileState.ReparseIdx` (`cmd/gen-settings.ts:997`) is unused today.

## Design

### `Location`

```cpp
// src/EngineBase.h
struct Location {
    int chapter = 0; // 1-based, 0 = invalid
    int page = 0;    // 1-based within chapter, 0 = invalid
    bool IsValid() const { return chapter >= 1 && page >= 1; }
    bool operator==(const Location&) const; bool operator!=(const Location&) const;
};
inline Location LocFromPageNo(int pageNo) { return {1, pageNo}; } // single-chapter docs
```

### Flat `pageNo` stays the view index; chapters may be *unlaid*

The app keeps using a flat 1-based `pageNo` (DisplayModel, RenderCache, text
selection, etc.). What changes: a chapter that has not been laid out counts as
**1 placeholder page** in the flat numbering. When a chapter gets laid out (on
first render / text extraction / explicit navigation into it) its real page
count replaces the placeholder and every later `pageNo` shifts. So:

- The engine owns a **chapter table** (`ChapterTable`, see below): per chapter
  `{pageCount, laidOut}`, prefix sums, and a `generation` counter bumped on
  every count change. `EngineBase::pageCount` is always the table's total.
- State that must survive a shift holds a `Location`, not a `pageNo`:
  `ScrollState`, nav history, pending scroll, RenderCache entries, TOC/link
  destinations. `DisplayModel` re-syncs (`SyncWithEngineLayout`) whenever the
  engine generation differs from its own, rebuilding `pagesInfo` and restoring
  the position from `Location`.
- Placeholder = 1 page keeps things deterministic (no estimates). All pages of
  a reflowable doc share one mediabox, so placeholders have a known size and
  never force layout just to be measured.

```
  chapter:      1       2      3     4 ... 886
  laid out:    yes      no    yes    no
  pages:      1 2 3 4   [1]   1 2   [1]        <- [1] = placeholder
  pageNo:     1 2 3 4    5    6 7    8
  after laying out chapter 2 (3 pages): pageNo 6,7 -> 8,9 ; generation++
```

### Engine-side contract (`EngineBase`)

Non-virtual, backed by `ChapterTable chapters` (a member; single-chapter engines
never touch it and behave exactly as today):

```cpp
int  ChapterCount();                      // 1 when no chapters
bool HasChapters();                       // ChapterCount() > 1
int  ChapterPageCount(int chapter);       // lays out that chapter (real count)
bool IsChapterLaidOut(int chapter);
Location LocationFromPageNo(int pageNo);  // table lookup, never lays out
int  PageNoFromLocation(Location loc);    // lays out loc.chapter first, clamps loc.page
Location NextLocation(Location), PrevLocation(Location); // cross chapter edges; lays out target chapter
Location FirstLocation(), LastLocation(); // last lays out the last chapter
Location ClampLocation(Location);         // lays out loc.chapter, clamps
int  LayoutGeneration();                  // atomic read
void EnsureAllChaptersLaidOut(ProgressUpdateUI* = nullptr); // print / dump / full search
virtual TempStr MakeBookmarkTemp(Location);   // default: "%d:%d:%d" chapter:page:ChapterPageCount
virtual Location LookupBookmark(Str);         // default: parse; scale page by newCount/oldCount
virtual Location ResolveDest(IPageDestination*); // default {1, dest->pageNo}; caches into dest
```

One virtual for engines with chapters:

```cpp
virtual int LayOutChapter(int chapter); // returns real page count; engine calls chapters.SetPageCount()
```

`ChapterTable` (new `src/ChapterTable.h/.cpp`, unit-tested in `test_util`):
`Init(nChapters)`, `SetPageCount(chapter, n)` (bumps generation when changed),
`TotalPages()`, `LocationFromPageNo`, `PageNoFromLocation` (pure, no layout),
`IsLaidOut`, `Generation()`. Thread-safe (mutex; generation atomic). Engines
call `SetPageCount` from whatever thread lays out (render thread included).

Per-page virtuals (`PageMediabox(int pageNo)`, `RenderPage`, ...) keep `int
pageNo`; engines convert with `LocationFromPageNo` at entry. Page-keyed engine
storage (`EngineMupdf::pages`, `EngineEbook::pages`, `EngineBase::pagesText`)
becomes per-chapter (`Vec<Vec<T*>>`, chapter-major) so a shift never
re-associates cached data with the wrong page.

Structs carrying a page: `IPageDestination`, `TocItem`, `IPageElement`,
`RenderPageArgs` gain `Location loc`. `pageNo` stays as the cached flat value
(`-1` = unresolved). `TocItem::pageNo == -1` with a valid `dest` means
"resolve on click".

### Lazy resolution of destinations

- `EngineMupdf`: TOC built from `fz_new_outline_iterator` (no `fz_load_outline`).
  For chaptered docs a TOC item stores only the URI and gets `loc = {chapter,0}`
  by resolving the URI *without its fragment* (`epub_resolve_link` returns the
  spine index without layout when there is no `#`). `ResolveDest` does the full
  `fz_resolve_link_dest` (lays out one chapter) and caches `loc`/`pageNo`.
- `EngineEbook` (mobi): TOC and links are `filepos` byte offsets. Chapter =
  binary search of the offset in the chapter-start table (no formatting);
  page = format that chapter, then the existing reparseIdx scan.
- `LinkHandler::ScrollTo(IPageDestination*)` and `GoToTocLink` call
  `ctrl->ResolveDest(dest)` and navigate by `Location`.

### `DisplayModel`

- `PageCount()` returns a snapshot taken by `BuildPagesInfo`, not the live
  engine count. `PageInfo` gains `Location loc`.
- `SyncWithEngineLayout()`: if `engine->LayoutGeneration() != layoutGeneration`
  → remember `CurrentLocation()` + scroll offset, rebuild `pagesInfo`, `Relayout`,
  restore via `GoToLocation`, `renderCache->RekeyForLayoutChange(this)`,
  `cb->PagesRenumbered(this)`. Called at the top of `RepaintDisplay`,
  `RecalcVisibleParts`, `GoToPage`, `CurrentPageNo` and from the render-done
  callback path (`RenderCache` → UI thread).
- `Location CurrentLocation()`, `GoToLocation(Location, bool addNavPoint)`
  (= `PageNoFromLocation` → sync → `GoToPage`).
- `GoToNextPage`/`GoToPrevPage`/`GoToLastPage` for chaptered docs go through
  `NextLocation`/`PrevLocation`/`LastLocation` so "prev" from chapter 6 page 1
  lands on the *last* page of chapter 5 even if chapter 5 was never laid out.
  Single-chapter docs keep the current row-based code path untouched.
- `ScrollState` gains `Location loc`; `GetScrollState` fills it,
  `SetScrollState` prefers `loc` when valid (nav history survives shifts).
- `RenderCache`: `BitmapCacheEntry` gains `Location loc` (from
  `PageInfo::loc`); `RekeyForLayoutChange(dm)` recomputes `pageNo` from `loc`
  and drops entries whose `loc` no longer maps. `RenderPageArgs::loc` is what
  `EngineMupdf`/`EngineEbook` actually render (they ignore `pageNo` when `loc`
  is valid) so a shift between request and render cannot draw the wrong page.
- `DocControllerCallback::PagesRenumbered(DisplayModel*)`: SumatraPDF.cpp
  clears selection, aborts an in-progress find, refreshes toolbar / tab /
  page-info / TOC selection, repaints.

### Persistence (`FileState` / `TabState`)

`PageNo` changes from `Int` to `Str` in `cmd/gen-settings.ts` (both structs;
`Favorite.PageNo` stays `Int`). Value grammar, parsed by one helper
(`src/PagePosition.h/.cpp`: `ParseStoredPage(Str) -> {int pageNo; Str bookmark}`,
`FormatStoredPageTemp(...)`):

- `"12"` – flat page number (legacy files and every single-chapter document)
- `"bm:" + engine bookmark` – chaptered documents, e.g. `bm:37:2:5`
  (EngineMupdf: chapter:page:pagesInChapter) or `bm:37:2:5:r1834211`
  (EngineEbook adds the exact reparseIdx). Prefix `kBookmarkPrefix = "bm:"`.

Old builds parse `bm:...` as int 0 → open at page 1 (acceptable). New builds
read old integers unchanged. `FileState.ReparseIdx` stays declared (compat) but
is no longer written. `SetTabState` / `LoadDocIntoCurrentTab` / `NewTabState`
/ `CloneTabState` / `GoToFavorite`'s FileState hack / `RememberSessionState`
all go through the helper. `-page N`, DDE `GotoPage`, `-dbg-control` keep
flat page numbers (`LocationFromPageNo`).

### UI, only when `ctrl->HasChapters()`

- Toolbar: `Page:` → `[chapter edit] / 886  [page edit] / 4`. Second `Edit`
  created by `ToolbarCreatePageEdit` (chapter variant), two `VirtText` totals,
  hit-test / show-hide / theming / teardown mirror the existing page edit.
  Enter in either edit → `GoToLocation({chapterEdit, pageEdit})`.
  `PageNoChanged` fills both edits from `CurrentLocation()`.
- Go To Page dialog: two edits (`Chapter (of 886)`, `Page (of 4)`), `OnOk`
  builds a `Location`, `ClampLocation`, `GoToLocation`.
- Page-info notification and tab suffix: `Ch 37 / 886, page 2 / 5`.
- TOC: click resolves lazily (above); `UpdateTocSelection` matches by
  `loc.chapter` when `pageNo == -1`.
- Print, `EngineDump`, full-document text search, PDF export, stress test:
  call `EnsureAllChaptersLaidOut()` first (this is today's open cost, now
  paid only when the user asks for a whole-document operation).

### Out of scope (explicitly)

Favorites keep flat page numbers. Annotations / forms are PDF-only (no
chapters). `EngineEpub`/`EngineFb2` fallbacks in `EngineEbook.cpp` stay
single-chapter. No mupdf patch is needed (no `ext/patches` change).

## Phases

Order: 1 → 4 → 2 → 3 → 5 → 6 → 7 (view-side plumbing before any engine
reports chapters, so a PDF-only tree stays correct at every step).

Each phase: implement, `clang-format` touched `src/` files, build debug,
fix warnings, run the named checks. Report what was verified and what was not.

### Phase 1 – `Location` + `ChapterTable` + `EngineBase` contract
Files: `src/EngineBase.h/.cpp`, new `src/ChapterTable.h/.cpp` (+ premake
file list `premake5.files.lua`), `src/AppUnitTests.cpp` or a `_ut.cpp` next to
it for `ChapterTable` tests (`bun cmd/run-unit-tests.ts -dbg`).
Adds `Location`, `ChapterTable`, the non-virtual chapter API, `LayOutChapter`,
bookmark defaults, `ResolveDest`, `loc` fields on `IPageDestination`,
`TocItem`, `IPageElement`, `RenderPageArgs` (default `{}`; `NewSimpleDest` /
`AllocTocItem` set `loc = LocFromPageNo(pageNo)` when `pageNo >= 1`).
`pagesText`/`pagesTextState` become chapter-major and are (re)allocated per
chapter on first use. No behavior change for existing engines.

### Phase 2 – `EngineMupdf` lazy EPUB chapters
- `FinishLoading`: `chapters.Init(fz_count_chapters)`; count chapter 1 only
  (`fz_count_chapter_pages(ctx, doc, 0)`); no `fz_count_pages` when
  `ChapterCount() > 1`. `LayOutChapter(ch)` = `fz_count_chapter_pages` under
  `docLock` + `chapters.SetPageCount`.
- `FinishNonPDFLoading`: for `isReflowable` fill mediaboxes from the layout
  size (`ebookLayoutW/H` in points; verify against page 1's `fz_bound_page`)
  instead of loading every page. Non-reflow (XPS) keeps the loop.
- `pages` → chapter-major; `GetFzPageInfoLocked` takes a `Location`, loads
  with `fz_load_chapter_page`, then refreshes the chapter count (a placeholder
  chapter becomes real here) and grows that chapter's `FzPageInfo` vector.
  Every `pages[pageNo - 1]` / `pi->pageNo` use goes through helpers.
- TOC via outline iterator; `ResolveDest` for `PageDestinationMupdf`;
  `HandleLinkMupdf` returns a `Location`. Heading-TOC background scan and
  page-label scan skipped when `HasChapters()`.
- `ApplyReflowThemeCss`: drop the "keeping pageCount" hack; re-`Init` the
  table (all chapters unlaid), bump generation; DisplayModel restores position.
- `src/libsumatrapdf.def`: add `fz_count_chapter_pages`,
  `fz_location_from_page_number` (if used).
- Check: open `C:\Users\kjk\OneDrive\!sumatra\Dune - Frank Herbert.epub` with
  `-log-to-file`, confirm `LoadDocument` ms drops and only chapter 1 is laid
  out at open (log the chapter count / laid-out count); page through with
  `CmdGoToNextPage` via `-dbg-control` and confirm no crash and page count grows.

### Phase 3 – `EngineEbook` lazy MOBI chapters
- `EngineEbook` gains chapter starts: scan `doc->GetHtmlData()` once for
  `<mbp:pagebreak` (case-insensitive) → `Vec<int> chapterStart` (chapter 1
  starts at 0). Only `EngineMobi` enables it; when no marker is found the
  document stays single-chapter and formats as today.
- `pages` → chapter-major `Vec<Vec<HtmlPage*>*>`. `LayOutChapter(ch)` formats
  `Str(html.s + start, end - start)` with `MobiFormatter`, adds `start` to every
  `HtmlPage::reparseIdx` and anchor offset, extracts that chapter's anchors
  (`ExtractPageAnchors` becomes per-chapter), and calls
  `chapters.SetPageCount`. Guard with `pagesAccess`. Check how
  `HtmlFormatter` treats a truncated slice (unclosed tags) and that
  `GumboHtmlParser` positions stay slice-relative.
- `GetNamedDest(filepos)`: chapter by binary search over `chapterStart`, lay
  it out, then the existing page scan within that chapter. `ResolveDest` for
  ebook TOC dests (`pageNo == -1`, `name` = filepos).
- `MakeBookmarkTemp` appends `:r<reparseIdx of loc's page>`; `LookupBookmark`
  uses it when present (chapter from the table, page by reparseIdx scan).
- Check: `1000.mobi` `LoadDocument` well under 1 s in a release build
  (`bun cmd/build.ts -release`, `-log-to-file`), first page renders,
  next/prev across a chapter edge, TOC click into a far chapter.

### Phase 4 – `DisplayModel` / `RenderCache` / `DocController`
As in the design: `PageInfo::loc`, snapshot `PageCount`, `SyncWithEngineLayout`,
`CurrentLocation`/`GoToLocation`, chapter-aware next/prev/last, `ScrollState::loc`,
`RenderCache` re-keying + `RenderPageArgs::loc`, `PagesRenumbered` callback,
`DocController` additions (`HasChapters`, `CurrentLocation`, `GoToLocation`,
`ResolveDest`, `ChapterCount`, `ChapterPageCount`; ChmModel/MarkdownModel use
the single-chapter defaults). `LinkHandler` navigates by `Location`.
Check: PDF behavior unchanged (`bun tests/issue-5949.ts` or whichever tests
grep as page-navigation related), EPUB/MOBI continuous mode scrolls through
chapter edges without position jumps.

### Phase 5 – persistence
`cmd/gen-settings.ts` (`PageNo` → `Str` in `FileState` and `TabState`, doc
strings updated), `bun cmd/gen-code.ts`, `src/PagePosition.h/.cpp`, all
read/write sites listed in the design, `docs/md/Advanced-options-settings.md`
regenerated. Check: a settings file with `PageNo = 12` still opens a PDF at
page 12; closing an EPUB at chapter 37 and reopening lands in chapter 37; an
old-style int for an EPUB still opens at that flat page.

### Phase 6 – UI for chaptered documents
Toolbar, Go To Page dialog, page-info notification, tab suffix, TOC selection,
`EnsureAllChaptersLaidOut` at print / dump / full search / PDF export /
stress. Check with `-dbg-control` + screenshot (see `verify` skill).

### Phase 7 – tests + docs
- `tests/ad-hoc-chapters.ts`: opens `1000.mobi` and an EPUB, asserts load time
  and chapter navigation through `-dbg-control` (add `ChapterInfo` /
  `GoToLocation` control commands if needed).
- `tests/issue-*.ts`-style regression for settings backward compat.
- `docs/md/Version-history.md` entry (behavior change: chapter/page toolbar for
  multi-chapter ebooks, much faster open), `docs/md/Commands.md` only if a
  command was added.

## Outcome

- `1000.mobi` (892 chapters): `LoadDocument` 97.5 ms in a release build and
  ~140 ms in a debug build (was 38069 ms in release before this work) — under the "well under 1 s" target
  with headroom to spare, in the slower build config.
- `Dune - Frank Herbert.epub`: 62 chapters; toolbar reads `Chapter: [n] / 62`,
  `Page: [n] / M`.
- Two bugs found by `tests/ad-hoc-chapters.ts` and fixed in Phase 7:
  - `EngineMobi::LayOutChapter` asserted (`ReportIf`) when the UI thread and
    the render thread raced to lay out the same not-yet-laid-out chapter.
    Made idempotent: whichever caller wins keeps its result, the loser's
    redundant format is discarded.
  - `DisplayModel::GoToNextPage`/`GoToPrevPage`/`GoToLastPage` only used the
    chapter-aware `Location` crossing in non-continuous display modes.
    Continuous mode (EPUB's default) fell through to the flat row-based path,
    so `CmdGoToPrevPage` crossing into an unlaid previous chapter landed on
    its 1-page placeholder instead of its real last page. Fixed by dropping
    the non-continuous restriction for chaptered docs.
- `tests/tmp/toolbar-epub.png`/`toolbar-pdf.png`: confirmed the Bookmarks
  sidebar showing empty-title dots under "Book One - DUNE" is pre-existing
  (reproduces on the pre-Phase-1 baseline via `git stash`), not caused by
  this work; left unfixed.
