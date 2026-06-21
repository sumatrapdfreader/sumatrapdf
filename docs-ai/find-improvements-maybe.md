# Find UI — possible future improvements

Context: there are now **two** find UIs that share a backend:

- the compact Chrome-style overlay bar — `src/FindBar.cpp` / `src/FindBar.h`
  (`FindBarWnd`).
- the floating, movable/resizable find **window** with a results list —
  `src/FindWindow.cpp` / `src/FindWindow.h` (`FindWindowWnd`). It's enabled by the
  `SearchUIFloating` advanced setting and toggled by the pin/dock buttons.

Which one is active is chosen by `gGlobalPrefs->searchUIFloating`;
`ShowFindBar`/`HideFindBar`/`FindBarSetStatus`/`FindBarSetMatchCaseChecked` in
`src/FindBar.cpp` dispatch to whichever UI applies. The shared backend (the find
thread, the coalescing count thread that builds `n / m` + the per-match snippet
list `MainWindow::findMatches`, and `GoToFindMatch`) lives in
`src/SearchAndDDE.cpp`.

Items 1–2 came out of the first review (compact bar). Items 3+ came out of the
review of the floating-window feature (issue #4092) and were consciously
deferred (low severity, perf-only, or refactor-only). Each is self-contained.

---

## 1. Harden the match-count cache key against engine-pointer reuse

### Problem
The "n / m" counter caches all match positions for the current search term so
prev/next is instant. The cache is keyed partly on the document's engine pointer:

- `MainWindow::findCountEngine` (a `void*`, declared in `src/MainWindow.h`) stores
  `(void*)dm->GetEngine()` for the document the cache was built against. It is
  *compared, never dereferenced* (see the comment on the field).
- `UpdateMatchCount()` in `src/SearchAndDDE.cpp` decides "cache hit" with:
  ```cpp
  bool cacheHit = win->findCountValid && win->findCountText &&
                  str::Eq(win->findCountText, text) &&
                  win->findCountMatchCase == win->findMatchCase &&
                  win->findCountEngine == engine;   // <-- raw pointer compare
  ```

This correctly handles the common case (tab switch → different engine pointer →
cache miss → rescan). The hole: if the user reloads/replaces the document, the
old `EngineBase` is freed and a **new engine can be allocated at the same
address**. Then `findCountEngine == engine` is true again, the term + match-case
match, and the cache "hits" — showing `n / m` computed for the *old* document's
pages (wrong, possibly out-of-range indices) instead of rescanning.

Severity is low: requires a reload that reuses the exact address AND the same
search term + match-case still in the box. But it is a real stale-data bug.

### Fix (sketch)
Add a monotonically increasing "document generation" counter that bumps every
time a document is (re)loaded into a `MainWindow`/tab, and key the cache on that
instead of (or in addition to) the engine pointer.

- Add e.g. `int docEpoch` to the per-tab/DisplayModel state, bumped on load, OR a
  per-`MainWindow` `int findCountDocGen` bumped wherever the doc is swapped.
- Replace `findCountEngine` comparison with `findCountDocGen == <current gen>`.
- Invalidate the cache (`win->findCountValid = false`) on document load/close.
  The natural hook is wherever the old inline find UI used to be reset on load;
  `LoadDocIntoWindow` / `UpdateFindbox` in `src/SumatraPDF.cpp` /
  `src/Toolbar.cpp` are candidates — find where `dm->textSearch` / find state is
  reset on a new doc and bump the generation there.

Simplest correct version: in the doc-load path call a new
`InvalidateFindCount(win)` (set `findCountValid = false`, free `findCountText`).
Then the cache can never survive a reload and the pointer compare is no longer
load-bearing.

### Files
- `src/MainWindow.h` — cache fields (`findCount*`).
- `src/SearchAndDDE.cpp` — `UpdateMatchCount`, `ShowMatchCount`, `StartFindCount`,
  `CountEndTask`.
- doc-load path in `src/SumatraPDF.cpp` (search for where `textSearch` /
  selection is reset on load).

---

## 2. Replace FindBar's hand-rolled layout with the wingui layout engine

### Problem
`FindBarWnd::Layout()` in `src/FindBar.cpp` positions its three children — the
`Edit` (search box), the status `Static` ("n / m"), and the embedded Win32
toolbar (prev/next/match-case/close) — with manual `MoveWindow` + a running `x`
cursor and hand-computed DPI gaps and vertical centering:

```cpp
int p = DpiScale(hwnd, 6);
int gap = DpiScale(hwnd, 4);
...
MoveWindow(edit->hwnd, x, (barDy - editDy) / 2, editDx, editDy, TRUE);
x += editDx + gap;
MoveWindow(status->hwnd, ...);
x += statusDx + gap;
MoveWindow(hwndBtns, ...);
SetWindowPos(hwnd, ... barDx, barDy ...);
```

This duplicates the `HBox`/`VBox` + `LayoutAndSizeToContent` machinery in
`src/wingui/Layout.h` that `src/CommandPalette.cpp` already uses for the same
shape (an `Edit` plus a horizontal row of controls). The manual version re-derives
spacing/centering/RTL by hand, so any change (spacing, adding a control, RTL,
high-DPI) means re-doing pixel math and re-testing.

This is a refactor, not a bug — the current layout works and was visually
verified (incl. the `SS_CENTERIMAGE` fix that vertically centers the status).
Deferred because reworking it risks regressions for no user-visible gain.

### Fix (sketch)
Model on `CommandPaletteWnd::Create` in `src/CommandPalette.cpp` (look for
`new HBox()`, `AddChild`, `Padding`, and `LayoutAndSizeToContent`):

- Build an `HBox` with `alignCross = CrossAxisAlign::CrossCenter` containing the
  `Edit`, the status `Static`, and the buttons.
- Wrap children in `Padding`/`Insets` for gaps instead of manual `+= gap`.
- The catch: the buttons are a raw Win32 `TOOLBARCLASSNAME` HWND, not a wingui
  `Wnd`, so it can't be added to an `HBox` directly. Options:
  1. Wrap the toolbar HWND in a thin `ILayout`/`Wnd` adapter that reports the
     toolbar's `TB_GETMAXSIZE` as its ideal size and `MoveWindow`s on `SetBounds`.
  2. Replace the embedded toolbar with individual wingui controls (e.g. bitmap
     `Button`s or owner-drawn statics) so everything is a `Wnd` — but that loses
     the toolbar's free tooltip + checked-state (`BTNS_CHECK`) handling, which is
     currently why a real toolbar is used (see tooltips via `TTN_GETDISPINFOW` in
     `FindBarWnd::OnNotify` and match-case `TB_CHECKBUTTON` in
     `FindBarSetMatchCaseChecked`).
- Option 1 (adapter) is the lower-risk path: keep the toolbar, just let the
  layout engine place it.

If/when this is done, also revisit the manual RTL handling (`WS_EX_LAYOUTRTL` on
the popup + the button toolbar, set in `FindBarWnd::Create`) — the layout engine
mirrors automatically, so the explicit exStyle may become unnecessary.

### Files
- `src/FindBar.cpp` — `FindBarWnd::Create`, `FindBarWnd::Layout`.
- `src/wingui/Layout.h` — `HBox`, `VBox`, `Padding`, `LayoutAndSizeToContent`.
- `src/CommandPalette.cpp` — reference implementation of the same shape.

---

## 3. Share a find-UI "core" between the compact bar and the floating window

### Problem
`FindWindowWnd` (`src/FindWindow.cpp`) duplicates almost all of `FindBarWnd`
(`src/FindBar.cpp`): the `Edit` + status `Static` + a flat
`TOOLBARCLASSNAMEW` toolbar with the same prev/next/match-case buttons, the same
`SetWindowTheme(hwnd, L"", L"")` + `NM_CUSTOMDRAW` dark-background workaround, the
same `BuildStdToolbarImageList` + theme-rebuild logic, the same tooltip table +
`TTN_GETDISPINFOW` `OnNotify`, and the same `OnTextChanged -> OnFindBarTextChanged`
dispatch. The button/command tables and the two tooltip lookup functions
(`FindBarButtonTooltip` / `FindWindowButtonTooltip`) are three-quarters identical.

Every find-UI change (new button, icon, RTL/theme fix, the toolbar dark-bg hack)
now has to be made twice and kept byte-identical, and will silently drift.

### Fix (sketch)
Extract a `FindUiCore` that owns the edit + status + toolbar + the
`OnFindBarTextChanged` dispatch + tooltip table + the `NM_CUSTOMDRAW`/theme/
imagelist plumbing, and have both `FindBarWnd` (docked chrome) and
`FindWindowWnd` (popup caption + results list) embed it and only add their own
chrome. The toolbar differs by exactly one button (compact has Close at index 4;
floating has the dock button) — parameterize the button table.

This is the deepest of the deferred items and touches the most code; do it before
adding any more find-UI features, not after. The duplicated `NM_CUSTOMDRAW`
toolbar-bg fill is itself a near-copy of the main toolbar's custom draw in
`src/Toolbar.cpp` (line ~507) — a shared "theme a flat toolbar's background"
helper could serve all three.

### Files
- `src/FindBar.cpp`, `src/FindWindow.cpp` — the two near-duplicate window classes.
- `src/Toolbar.cpp` — the canonical toolbar custom-draw to factor against.

---

## 4. Results list: stop rebuilding the whole ListBox every keystroke

### Problem
On every completed count (i.e. roughly every keystroke), `CountEndTask` ->
`FindWindowRefreshResults` -> `FindWindowWnd::RefreshResults` calls
`FillWithItems` (`src/wingui/UIModels.cpp`), which does `ListBox_ResetContent`
then one `LB_ADDSTRING` **with a UTF-8 -> WStr conversion per item**. For a common
term on a big document that's up to `kMaxFindResults` (5000) window messages +
5000 string conversions, tearing down and rebuilding the entire listbox just to
repaint ~25 visible rows. `DrawResultItem` already pulls text straight from
`win->findMatches[i]` by index, so the listbox's own string copies are pure waste.

### Fix (sketch)
Make the results listbox owner-data: create it with `LBS_NODATA` and, on refresh,
just send `LB_SETCOUNT` with `win->findMatches.size()` instead of `FillWithItems`.
The owner-draw path (`FindWindowWnd::DrawResultItem`) is already index-based, so no
per-item strings are needed. Check `src/wingui/ListBox.cpp` for whether the wingui
`ListBox` wrapper supports `LBS_NODATA` (the model API assumes string items) — may
need a small "virtual list" mode on the wrapper, or bypass the wrapper for the
count.

### Files
- `src/FindWindow.cpp` — `RefreshResults`, the `ListBox::CreateArgs` in `Create`,
  `DrawResultItem`.
- `src/wingui/ListBox.cpp`, `src/wingui/UIModels.cpp` — `FillWithItems`, listbox
  styles, model API.

---

## 5. Build result snippets lazily instead of all 5000 up front

### Problem
`CountThread` (`src/SearchAndDDE.cpp`) calls `BuildSnippet` for every match up to
`kMaxFindResults` on every count, and the count runs on every keystroke.
`BuildSnippet` does `engine->GetTextForPage` (cheap — cached) + `str::Dup` +
`NormalizeWSInPlace` + `ToUtf8Temp` + `str::FormatTemp` + `str::Dup` (two heap
allocs + a normalize + a utf8 pass) **per match**. Only ~25 snippets are ever on
screen; the rest are allocated, then freed on the next keystroke, unseen.

### Fix (sketch)
Defer snippet construction to render time. Keep the count building the cheap
`positions` + the match coordinates (`startPage/startGlyph/endPage/endGlyph`) for
all matches, but build the `snippet` string lazily in `DrawResultItem` (or
`FindResultsModel::Item`) for the rows actually painted, caching it back into the
`FindMatch` (or a side cache keyed by index) so scrolling doesn't rebuild. Note
`GetTextForPage` is engine-thread-affine-ish; building snippets on the UI thread
at paint time is fine since the page text is cached by then, but confirm against
the concurrency rules (see `GoToFindMatch` / `AbortFinding` serialization).
Alternative/simpler: only rebuild snippets when the search term actually changed,
not on every count completion.

### Files
- `src/SearchAndDDE.cpp` — `CountThread`, `BuildSnippet`, `FindMatch` install in
  `CountEndTask`, `kMaxFindResults`.
- `src/MainWindow.h` — `FindMatch` (would gain a "snippet not yet built" state).
- `src/FindWindow.cpp` — `DrawResultItem` / `FindResultsModel::Item`.

---

## 6. Dispatch find status/match-case by actual visibility, not the preference

### Problem
`HideFindBar` decides which UI to act on by **actual visibility**
(`IsFindWindowVisible(win)`), but `FindBarSetStatus` and
`FindBarSetMatchCaseChecked` (`src/FindBar.cpp`) decide by the **persisted
preference** (`gGlobalPrefs->searchUIFloating`). If the two ever disagree —
preference says floating but the compact bar is the one actually showing (or vice
versa) — a count completion routes the `n / m` status and the Match-Case checkbox
update to the hidden control, leaving the visible UI stale.

In normal flow `ToggleFloatingFindUI` keeps the preference and the visible UI in
sync, so this is currently latent, but it's a fragile coupling.

### Fix (sketch)
Make the status/match-case setters key off which UI is actually visible, mirroring
`HideFindBar`: `if (IsFindWindowVisible(win)) { FindWindowSet...; return; }` then
fall through to the compact bar. A tiny helper like
`static bool UseFloatingFindUI(win) { return IsFindWindowVisible(win); }` used by
all four dispatchers removes the preference-vs-visibility split.

### Files
- `src/FindBar.cpp` — `FindBarSetStatus`, `FindBarSetMatchCaseChecked`,
  `ShowFindBar`, `HideFindBar` (unify the dispatch predicate).

---

## 7. Binary-search the match lists instead of linear scans

### Problem
`FindWindowWnd::CurrentMatchIndex` and `FindWindowWnd::FirstMatchFromCurrentPage`
(`src/FindWindow.cpp`) do an O(n) linear scan of `win->findMatches` on every
selection move (each arrow key, Next/Prev, and each `RefreshResults`). The matches
are already in page order (the count appends them in `FindNext` order, sorted by
`(page, glyph)`).

### Fix (sketch)
- `FirstMatchFromCurrentPage`: `std::lower_bound` on `startPage >= curPage`.
- `CurrentMatchIndex`: matches are sorted by `(startPage, startGlyph)`, so a binary
  search works; simpler still, remember the index navigated to in `GoToFindMatch`/
  `OnResultSelected` and validate it instead of rescanning.

Low impact (n <= 5000, only on user input), pure cleanup.

### Files
- `src/FindWindow.cpp` — `CurrentMatchIndex`, `FirstMatchFromCurrentPage`.

---

## 8. Remove the vestigial `AbortFinding(win, hideMessage)` parameter + dead Kind

### Problem
The "Searching n of m..." find-progress notification was removed (the find UI's
own `n / m` counter is the feedback now). `ShowUI` no longer creates it, so the
`kNotifFindProgress` `Kind` (`src/SearchAndDDE.cpp` line ~46) and the
`RemoveNotificationsForGroup(..., kNotifFindProgress)` in `AbortFinding` are dead
(the `HideUI` copy was already removed). With that gone, `AbortFinding`'s
`hideMessage` bool parameter no longer does anything.

Left in place only because removing the parameter means editing all ~14 call
sites (`src/FindBar.cpp`, `src/FindWindow.cpp`, `src/SearchAndDDE.cpp`,
`src/SumatraPDF.cpp`) plus the declaration in `src/SearchAndDDE.h`.

### Fix (sketch)
Drop the `hideMessage` parameter from `AbortFinding`, remove the dead
`RemoveNotificationsForGroup` block, and remove the `kNotifFindProgress` `Kind`
declaration. Mechanical: `AbortFinding(win, true/false)` -> `AbortFinding(win)` at
every call site. Verify the one return-value user, `OnFrameKeyEsc` in
`src/SumatraPDF.cpp` (Esc consumed iff a find thread was aborted) — behavior is
unchanged because the notification path already always returned false.

### Files
- `src/SearchAndDDE.cpp` / `src/SearchAndDDE.h` — `AbortFinding`, `kNotifFindProgress`.
- ~14 call sites across `src/FindBar.cpp`, `src/FindWindow.cpp`,
  `src/SearchAndDDE.cpp`, `src/SumatraPDF.cpp`.

---

## 9. (Optional) restore a way to cancel a long explicit find mid-scan

### Problem
With the progress notification gone, an explicit Find Next/Prev (or DDE
`[Search]`, or a Match-Case re-run) on a huge document scans to completion with no
on-screen cancel affordance. Esc still cancels (`OnFrameKeyEsc` -> `AbortFinding`
sets `findCancelled`), and the Find toolbar buttons are disabled during the scan,
so this is minor — but there's no longer a visible "searching… (cancel)" control.

### Fix (sketch)
If it ever feels slow in practice, show a lightweight, *cancelable* status only
when an explicit find runs longer than e.g. 300 ms (a timer-armed notification, or
a spinner in the find UI's status area), rather than the old always-on
"Searching n of m..." flash. Most of the time the find is instant and nothing
should appear. Keep find-as-you-type silent (it self-cancels and defers to the
count thread; see `UpdateFindStatus`'s `showProgress` check).

### Files
- `src/SearchAndDDE.cpp` — `ShowUI` / `HideUI` / `UpdateFindStatus` (the
  `showProgress` path), `FindThreadData`.
