# Deep modules plan

Goal: narrower interfaces, fewer pass-throughs, invariants owned by one
function. No behavior change unless noted.

## Rules for every phase

- Files under `src/shared/` and `src/base/` are built by both apps (see
  `cmd/helper/ng-shared.ts`). A phase touching them must build both:
  `bun cmd/build.ts -dbg` and `bun cmd/ng-build.ts -dbg`. Update `src/ng/`
  callers too.
- App-only phases: `bun cmd/build.ts -dbg`.
- clang-format every touched `.cpp`/`.h` under `src/` (`bun cmd/ng-format.ts`
  formats ng).
- Follow AGENTS.md: enums instead of bool params, `len(x)`, no prose in
  headers, minimal diff, no unrelated edits.
- Re-verify each claim (line numbers drift); skip a site when it differs
  from the pattern and say so.
- Never commit.

## Phase 1: engine annotation / undo API (shared)

1a. `src/shared/EngineCreate.cpp:582-647`: the `EngineX` wrappers only check
`kind == kindEngineMupdf` then call `EngineMupdfX`. Make the `EngineMupdfX`
functions tolerate a null / non-Mupdf engine (most already do via
`AsEngineMupdf()`), keep one public name per operation (`EngineX`), delete
the duplicates from `EngineAll.h`. Candidates: GetAnnotations,
HasUnsavedAnnotations, HasRedactMarks, HasRedactMarksNoWait,
HasUserRedactMarks, GetAnnotationAtPos, GetWidgetAtPos.
`EngineGetAnnotations` returns bool (false for non-Mupdf): keep that.

1b. Undo/redo (`EngineAll.h:122-127`, `EngineMupdf.cpp:~11404-11715`): six
functions, callers pick with `redo ? A : B`. Replace with:

```cpp
enum class UndoDir { Undo, Redo };
enum class EngineWait { Wait, NoWait };
bool EngineCanUndoRedo(EngineBase*, UndoDir, EngineWait);
bool EngineUndoRedo(EngineBase*, UndoDir, Vec<Annotation*>& removedOut);
```

Callers: `SumatraPDFCommon.cpp:~2681,2696`, `MainWindowCommon.cpp:~519`,
`SumatraControl.cpp:~517`, ng.

Verify: both builds; `bun cmd/run-unit-tests.ts -dbg`; grep `tests/` for
undo / annotation tests and run one (e.g. a test mentioning `CmdUndo`).

## Phase 2: `AsBrowser()` (shared)

`ChmModel` and `MarkdownModel` derive from `BrowserDocController`
(`DocController.h:196`), which has every method callers use. ~13 sites in
src (SearchAndDDE.cpp, Print.cpp, Selection.cpp, WindowTab.cpp,
MainWindowCommon.cpp, SumatraPDF.cpp x9) plus ~26 in src/ng do
`if (AsChm()) AsChm()->X(); else if (AsMarkdown()) AsMarkdown()->X();`.

- `virtual BrowserDocController* AsBrowser() { return nullptr; }` on
  DocController, overridden in BrowserDocController.
- `MainWindow::AsBrowser()` / `WindowTab::AsBrowser()` next to their
  `AsChm()`.
- Convert the paired sites only; sites that need Chm-specific behavior stay.
- `IsBrowserDocController(c)` → `c && c->AsBrowser()`.

Verify: both builds; open a .chm and a .md with `-for-testing` (look for
fixtures under `tests/`), check they render.

## Phase 3: EngineBase defaults and converted-PDF engines (shared)

3a. In `EngineBase.h`, give defaults to pure virtuals implemented identically:
`HasClipOptimizations` → false (DjVu, Ebook, Images), `BenchLoadPage` →
true (DjVu, Ebook). Delete the identical overrides. Leave `GetElementAtPos`
alone (DjVu and Ebook iterate in different orders). `GetFileData` /
`SaveFileAs` only if the three copies (DjVu/Ebook/Images) are truly
identical and the data member unifies cleanly; otherwise skip.

3b. `EngineCaj.cpp:~452-476` and `EngineDvi.cpp:~559-583` duplicate
`Clone`, `SaveFileAs` and the `cachedPdf` load tail. Hoist into
`EnginePdfProxy` (`EngineBase.h:~669`) or a small derived base. Leave
`EnginePs` if its data-based variant doesn't fit.

Verify: both builds; unit tests; open a .djvu and an image with
`-for-testing`.

## Phase 4: app-layer helpers (Windows app only)

4a. Replace hand-written `if (GetCapture() == win->hwndCanvas)
ReleaseCapture();` with existing `CanvasSetCapture(win, false)` (Canvas.cpp
x9, SumatraPDF.cpp:~4910, AnnotPlacement.cpp:~160,175).

4b. `SetSidebarVisibility(win, x, gSettings->showFavorites[, rf])` at ~13
sites (SidebarPanel.cpp, SumatraPDF.cpp) → new
`void SetTocVisibility(MainWindow*, bool tocVisible, SidebarResizeFrame rf = SidebarResizeFrame::Keep)`
in SumatraPDF.h. Don't touch shared/StressTesting.cpp or ng.

4c. `static void RedrawAllWindows()` for the 5 copies of
`for (auto& w : gWindows) w->RedrawAll(true);` in ExecuteCmd.

4d. `static void SetTabAnnotsHidden(MainWindow*, WindowTab*, bool hide)`
for CmdToggleShowAnnotations / CmdShowAnnotations / CmdHideAnnotations
(keeps tab and engine `hideAnnotations` in sync, rerenders on change).
Use an enum if a bool param reads ambiguously.

4e. Notifications.cpp:~813-876: four HWND wrappers copy the same body; route
through one static helper (keep the `<=0` vs `<0` timeout difference). Replace
the 4 hand-built `NotificationCreateArgs` in SumatraPDF.cpp
(~4387, 4879, 9322, 9358) with the wrappers when equivalent.

4f. TransitionFromTabs / TransitionToTabs (SumatraPDF.cpp ~9532-9619): the
three identical chrome sequences → `static void ApplyTitlebarChrome(MainWindow*, ...)`.
Leave the fourth (different) site.

Verify: `bun cmd/build.ts -dbg`; run tests found by grepping `tests/` for
`ShowAnnotations`, `showFavorites`/`Toc`, `TabsInTitlebar`.

## Phase 5: base/gui cleanups

5a. `WinGui.h:114-118`: five `gfxDoubleBuffer*` fields → one
`GfxDoubleBuffer gfxDoubleBuffer;` member; collapse the copy-in/copy-out in
`Gfx.cpp:~410-487`. Move the POD struct where include order allows.

5b. `base/Win.h`: delete 0-use exports (`ToBool` dup of `tobool`,
`HdcExTextOut`, `LbInsertString`, `LvSetItemText`, `RenderedBitmap::Blit`
if unused), make internal-only ones static (`DeleteObjectSafe`,
`HwndSetWindowExStyle`, `WriteRegNone`, `IsKeyPressed`). Grep src/ng too.

5c. `HwndResizeClientSize` vs `ResizeHwndToClientArea(..., bool hasMenu)`
(always false): one function, keep the unchanged-size early return.

5d. `MessageBoxWarningSimple(HWND, WStr, WStr)`: both callers convert from
Str; replace with `MsgBoxWarning(HWND, Str msg, Str title)` built on
`MsgBox`.

5e. Replace `DoLayout(HwndClientRect(hwnd).Size())` with the existing
no-arg `DoLayout()` where it is identical (`WindowBase.cpp:~291`).

Verify: both builds (Win.* is shared); unit tests.

## Phase 6: tool-dialog creation boilerplate

~20 dialogs repeat: fill `CreateCustomArgs` (owner, title, `WS_POPUPWINDOW |
WS_CAPTION`, font, isRtl, bgColor, app icon), `CreateCustom`, `SetColors`,
then `LayoutAndSizeToContent` + `DoLayout` + `HwndCenterDialog`.

- `bool CreateToolDialog(WindowBase*, HWND owner, Str title, PlatformFont*, Color txt, Color bg)`
  in `SumatraDialogs.h/.cpp`.
- `void SizeToLayout(WindowBase*, ILayout*, int minDx, int minDy, HWND centerOn)`
  (or a WindowBase method) for the size/layout/center tail.

Convert only dialogs matching the pattern exactly.

Verify: `bun cmd/build.ts -dbg`; open 3-4 converted dialogs with
`-for-testing` and screenshot (GoTo page, Custom zoom, Add favorite).

## Not doing

- Engine category enum (image/ebook predicates differ subtly per site).
- `AutoMemDC` across ~40 sites. Separate note: `Canvas.cpp:~3186` selects
  `gBitmapReloadingCue` into a DC and deletes the DC without restoring.
- `win->ctrl` / `AsFixed()` cleanup (hundreds of sites).
