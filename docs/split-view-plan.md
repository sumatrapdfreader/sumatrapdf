# Plan: split view (issue #2810)

Status: not started. Each phase must build (`bun cmd/build.ts -dbg`) before the
next starts. Never commit; the user commits.

## Goal

Show one document in two side-by-side panes, each with its own page, scroll
position, zoom and display mode. Typical use: read text in one pane, look up a
figure or footnote in the other.

Out of scope: two different documents in one window, more than two panes,
vertical (top/bottom) split, synchronized scrolling (both panes moving
together). The design should not rule them out.

## Rejected: thangthichdua/sumatrapdf-custom@e5e90a113

That commit creates a hidden second `MainWindow`, loads the file again and
reparents its canvas into the first frame. Problems:

- the hidden window is in `gWindows`: session save, history, window count,
  quit and every `FindMainWindowBy*` see it
- two engines: double memory, annotation edits in one pane are invisible in the
  other, saving from both overwrites
- split belongs to the window, not the tab: switching tabs leaves the second
  pane on the old document
- shared `ToolbarCommon.cpp` reads a field only orig's `MainWindow` has, so ng
  doesn't compile

Ideas worth keeping: pane layout with a gap, focus decides where commands go,
"links open in the other pane", find bar over the focused pane.

## Facts that shape the design

1. Engines are ref-counted (`EngineBase::AddRef` / `Release`,
   `src/shared/EngineBase.h`). `DisplayModel` takes an engine and releases it
   in its destructor, so two `DisplayModel`s can share one engine.
2. View state lives in `DisplayModel`: zoom, rotation, display mode, scroll
   (viewport), page layout, `TextSelection`, `TextSearch`.
3. `RenderCache` keys tiles by `DisplayModel*` (`Invalidate(dm, ...)`,
   `FreeForDisplayModel`), so each pane renders and caches independently.
   Annotation edits must invalidate both.
4. `EngineBase::SetOnLayoutChanged` / `SetOnChapterLayoutProgress` hold one
   callback. A second `DisplayModel` overwrites the first's. They must become
   a list, or the tab must fan out.
5. Tab switching already swaps `win->ctrl` (`UpdateUiForCurrentTab`,
   `LoadModelIntoTab`). Most commands go through `win->ctrl` /
   `win->AsFixed()` and work on whatever it points to.
6. `win->hwndCanvas` is used ~400 times (Canvas.cpp 96, SumatraPDF.cpp 113,
   HomePage.cpp 40, RefHoverCanvas.cpp 35). Per-canvas interaction state sits
   on `MainWindow`: `canvasRc`, `mouseAction`, `dragStart`, `selectionRect`,
   `annotationBeingMoved*`, `linkOnLastButtonDown`, scrollbar state.
7. The canvas is placed through `win->canvasSlot` (an `HwndSlot`,
   `SumatraPDF.cpp` `CreateMainWindow` / `RelayoutWindow`).
8. The frame already has draggable splitters: `VirtSplitter` made by
   `NewFrameSplitter` (`sidebarSplitter`, `aiChatSplitter`), with an `onMove`
   callback (`OnSidebarSplitterMove`).
9. `TabState` (`cmd/gen-settings.ts`, session restore) is a subset of
   `FileState` (history). View fields (`PageNo`, `Zoom`, `ScrollPos`, ...)
   exist in both.

## Design

### Model: the split belongs to the tab

```
WindowTab
  ctrl          -> DisplayModel A  --+
  split         -> SplitPane*        |  share EngineBase (AddRef)
                     ctrl -> DisplayModel B --+
                     hwndCanvas
                     parked canvas state (canvasRc, mouse state, ...)
  activePane    = Main | Split
```

```
+---------------------- frame -----------------------+
| toolbar (reflects the active pane)                 |
+---------+--------------------+--+------------------+
| sidebar |  pane A (canvas)   |<>| pane B (canvas)  |
|         |  win->hwndCanvas   |  | split->hwndCanvas|
+---------+--------------------+--+------------------+
```

- `SplitPane` (new, `src/SplitPane.h/.cpp`) owns the second `DisplayModel`, its
  canvas HWND and that canvas's interaction state.
- The second `DisplayModel` is created from `tab->ctrl`'s engine (`AddRef`) and
  starts at the same page and zoom.
- `<>` is a `VirtSplitter` (fact 8) between two `HwndSlot`s. Dragging it sets
  the tab's `splitRatio` (pane A's share of the width, default 0.5, clamped
  so each pane keeps a minimum width). The ratio, not pixels, is kept, so
  resizing the window keeps the proportion.
- Only `DisplayModel` documents split: no CHM, no Markdown (webview), no Home.

### Active pane: swap, like a tab switch

`MainWindow` keeps meaning "the active pane". Focusing the other pane swaps
`win->ctrl`, `win->hwndCanvas` and the per-canvas fields with the ones parked
in `SplitPane`, then refreshes the toolbar, page box, ToC selection and title,
the same way `UpdateUiForCurrentTab` does. That leaves the ~400
`hwndCanvas` / `ctrl` call sites as they are.

The cost: the canvas window procedure must not assume its HWND is
`win->hwndCanvas`. Paint, size and scroll messages for the inactive pane must use
that pane's `DisplayModel`. Input goes like this:

- **mouse down** on the inactive pane: swap first, then handle the click
  normally
- **wheel** over the inactive pane: scroll it without activating it, like
  Windows does for unfocused windows

Rejected alternative: a `CanvasPane` struct passed everywhere
instead of `win->hwndCanvas`. It is cleaner, but touches hundreds of lines
unrelated to the feature.

### Shared engine

- engine callbacks (fact 4) become a small list in `EngineBase`; each
  `DisplayModel` adds itself in its ctor and removes itself in its dtor
- after an annotation is changed, both `DisplayModel`s on that engine are
  invalidated and both panes repainted (one helper, called where
  `gRenderCache->Invalidate` is called for annotations today)
- reload (auto-reload, `CmdReloadDocument`) recreates both models, keeps each
  pane's page and zoom
- closing the tab destroys B first, then A. The engine is freed when the last
  one releases it

### Behavior

- `CmdToggleSplitView`: opens a split with the second pane on the current page,
  or closes it. Closing keeps the active pane's view.
- internal links clicked in one pane navigate the **other** pane. External
  links and other documents are not affected.
  Back/forward history (`CmdNavigateBack`) is per `DisplayModel`, so it is per pane.
- find: searches the active pane. The find bar is placed over the active pane.
- sidebar (ToC, thumbnails) follows and drives the active pane
- presentation and fullscreen: close the split first. Single pane only.
- split state is saved per tab in the session and per document in history:
  closing and reopening a file, or restarting with `RestoreSession`, brings
  the split back with both panes' views, the ratio and the active pane
- the active pane gets a thin accent border, so it is clear where keys go

### Persisted state

A new struct in both `fileState` and `tabState` (fact 9), filled in where
pane A's view is saved today and applied after A's model is created:

```
SplitView [
    Show = true
    Ratio = 0.5          # pane A's share of the width
    ActivePane = 1       # 1 = A, 2 = B
    DisplayMode = continuous
    PageNo = 12          # same form as FileState.PageNo (bm:<bookmark> ok)
    Zoom = fit width
    Rotation = 0
    ScrollPos = 0 340
]
```

- `Show = false` is the default. Check the serializer skips an all-default
  struct, so history entries without a split don't grow
- the same struct definition is reused in `cmd/ng-gen-settings.ts`
- the `PageNo` / `Zoom` / `ScrollPos` save and restore code for pane A is
  factored into one helper taking a `DisplayModel*`, so B uses the same code
- `.ver()` with the next version; regenerate (`bun cmd/gen-code.ts`),
  settings docs follow

### ng

Ng has its own `MainWindow` and a GPUI canvas, no HWNDs. The model part
(`SplitPane` owning a second `DisplayModel`, engine callback list, active-pane
swap) should live in `src/shared/` where it doesn't touch HWNDs. The view part
is a second GPUI canvas element in ng's layout. Ng is done in its own phase,
after orig works. Until then, `CmdToggleSplitView` is disabled in ng
(command availability). No shared file may read an orig-only field: that is
the fork's compile error.

## Phases

1. **Engine callback list.** `EngineBase` keeps a list of layout and chapter
   callbacks, added and removed per `DisplayModel`. No behavior change. Unit
   test: two `DisplayModel`s on one engine both get the callback.
2. **Second `DisplayModel` on a shared engine.** Add `SplitPane` with model
   creation and destruction, without UI. Unit test: create, scroll B, check A
   is unchanged, destroy in both orders, no leak, engine released once.
3. **Second canvas and layout.** Create the canvas HWND and lay out two slots
   with a `VirtSplitter` between them in `RelayoutWindow`; dragging it sets
   `splitRatio`. Paint B from its own model. Add
   `CmdToggleSplitView` (at the end of `gen-commands.ts`; regenerate,
   `Commands.md`, Version-history **New commands**).
4. **Active-pane swap.** Implement focus, click and wheel routing,
   the toolbar, page box and ToC refresh, and the accent border.
5. **Cross-pane behavior.** Links open in the other pane. Annotation edits
   invalidate both. Reload keeps both views. Find bar placement. Leaving
   presentation and fullscreen.
6. **Persist.** `SplitView` struct in `FileState` and `TabState` (orig and
   ng generators), save and restore through the shared view helper.
7. **Tests.** Add `tests/issue-2810.ts` using `-dbg-control`:
   - toggle split, check two canvases
   - go to page N in B, check A's page is unchanged
   - add an annotation in A, check B's pixels change
   - drag the splitter, check both canvases' widths follow the ratio
   - set B's page and the ratio, quit, restart with the session, check both
     restored; reopen from history, check the same
   - close the tab, check no crash or leak (ASan build)
8. **ng.** Shared model code, GPUI second canvas, enable the command.
