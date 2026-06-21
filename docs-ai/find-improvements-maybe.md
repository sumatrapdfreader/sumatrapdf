# Find bar — possible future improvements

Context: the Chrome-style floating find bar lives in `src/FindBar.cpp` /
`src/FindBar.h`. The "n / m" match counter and its background counting thread
live in `src/SearchAndDDE.cpp`. These two items came out of a code review and
were consciously deferred (low severity / refactor-only). Each is self-contained.

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
