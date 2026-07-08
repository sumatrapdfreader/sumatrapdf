# Porting SumatraPDF from Windows to macOS — Plan

Status: draft / analysis. Written 2026-07-08.

This document analyzes the structure of the Windows SumatraPDF app in `src/` and
lays out a plan for porting as much functionality as practical to macOS
(`src/mac/`). It also makes explicit **decisions** about which code becomes
portable (compiles on Windows + macOS + Linux) and which stays Windows-only.

The guiding rule matches `agents.md`: keep the full Win32 GUI working, move
**non-UI logic** to portable files using the `_win` / `_posix` / `_mac` /
`_linux` suffix convention, and grow the native Cocoa app under `src/mac/`
rather than emulating Win32 on macOS.

---

## 1. Where we are today

- **Engines are portable.** `EngineBase`, `EngineMupdf`, `EngineImages`,
  `EngineDjvuDec`, `EngineEbook`, `MobiDoc`, `EbookDoc`, etc. already compile on
  macOS and are exercised by `out/mac-*/test_engines`. Crucially,
  `EngineBase::RenderPage()` returns a **portable `Pixmap*`** (`base/Pixmap.h`,
  `BGRA8`), not a GDI bitmap.
- **`src/base/` is largely portable.** Strings, containers (`Vec`, `StrVec`,
  `Dict`), arenas, math (`Geom.h` → `Point`/`Rect`/`Size` are plain structs),
  `Color` (`COLORREF` is just `u32`), file I/O, dir iteration, crypto, threads,
  and HTTP already have `_posix` / `_mac` variants. Platform detection is clean:
  `OS_WIN` / `OS_DARWIN` / `OS_LINUX` / `OS_POSIX` macros in `base/Base.h`.
- **The Cocoa app exists but is a stub.** `src/mac/SumatraMac.mm` +
  `SumatraMacEngine.{h,cpp}` open one CLI-provided document, render page 1 to a
  `CGImage`, and show it in one `NSView`. No scrolling, navigation, selection,
  TOC, search, tabs, or settings. The C++/Cocoa boundary is bridged by
  `SumatraMacEngine` (a plain C API), because Apple headers define names like
  `Size` that collide with Sumatra types — this bridge pattern is the model we
  extend.
- **Build:** `cmd/build-mac.ts` compiles the dependency libs + `src/base`, then
  `test_util`, `test_engines`, and `SumatraPDF.app` from explicit source lists
  (`DEP_LIBS_BASE`, `TEST_*_SOURCES`, `MAC_APP_SOURCES`). Porting a file to mac
  means adding it to the relevant list here.

The engines already give us "open + render a page." **The gap is everything
between a rendered page and a real viewer**: viewport/scroll/zoom model, an
async render cache, selection/search, navigation, TOC, tabs, settings
persistence, and the native UI shell.

---

## 2. Architecture and the porting seam

The codebase has a natural layering that makes an incremental port feasible:

```
  base/            portable utils          → PORTABLE (done)
  engines          EngineBase → Pixmap     → PORTABLE (done)
  ───────────────────────────────────────────────────────────
  document model   DisplayModel, RenderCache, TextSelection,   → PORTABLE
                   TextSearch, Selection, Annotation, PdfSync,    (target of
                   DocController/DocControllerCallback            this plan)
  ───────────────────────────────────────────────────────────
  app logic        Settings, Commands, Flags, FileHistory,     → PORTABLE
                   Favorites, Translations, Theme, Accelerators,  (target)
                   EngineCreate, Ebook/HtmlFormatter
  ───────────────────────────────────────────────────────────
  UI shell         wingui/ controls, Canvas, MainWindow,       → WINDOWS-ONLY
                   Toolbar, Tabs, Menu, TOC, dialogs, WndProc     (replace w/
                   SumatraPDF.cpp message dispatch, WinMain        Cocoa)
```

**The critical existing seam is `DocController` + `DocControllerCallback`**
(`src/DocController.h`). The document model talks to the UI *only* through the
abstract `DocControllerCallback` (Repaint, UpdateScrollbars, RequestRendering,
PageNoChanged, GotoLink, …). The model never needs an `HWND`. This is exactly
the interface a Cocoa view controller will implement. Preserving and leaning on
this seam is the backbone of the whole port.

Two more facts make the seam clean:

- Engines already emit portable `Pixmap`. Windows wraps it in a GDI
  `RenderedBitmap`, and **converters already exist** in `base/Win.h`
  (`PixmapFromRenderedBitmap`, `RenderedBitmapFromPixmap`). The portable render
  cache should cache `Pixmap*`; each platform blits it natively (GDI `BitBlt` /
  CoreGraphics `CGContextDrawImage`).
- Geometry/color types are already platform-neutral, so model APIs don't leak
  Win32 types.

---

## 3. Decision: portability tiers

Every non-engine `src/` file falls into one of four tiers. This is the core
decision of the plan.

### Tier A — Portable as-is (compile on mac now, little/no change)

Pure logic, no Win32/GDI. Move into the mac build lists directly (fixing any
stray `#include` of a Win32 header, guarded by `#if OS_WIN`).

| File | Role |
|---|---|
| `DisplayModel.{cpp,h}` | viewport / scroll / zoom / page-layout model — **zero Win32** |
| `DisplayMode.{cpp,h}` | display-mode enum + string converters |
| `DocController.h` | abstract controller interface (already the seam) |
| `DocProperties.{cpp,h}` | metadata enum |
| `TextSelection.{cpp,h}` | selection model over engine text |
| `TextSearch.{cpp,h}` | search algorithm over engine text |
| `Annotation.{cpp,h}` | annotation data model |
| `PdfSync.cpp` | SyncTeX forward/inverse-search parsing |
| `EngineCreate.cpp` | file-type → engine factory |
| `HtmlFormatter.cpp`, `EbookFormatter.cpp`, `EngineEbook.cpp` | ebook layout (font metrics already behind a `#if OS_WIN` include) |
| `GlobalPrefs.cpp`, `Settings.{h,cpp}` (generated) | settings structs + (de)serialization (`COLORREF` = `u32`) |
| `Commands.{cpp,h}`, `CommandAvailability.cpp` | command IDs + availability predicates |
| `Flags.{cpp,h}` | CLI parsing (only stray field is an opaque plugin-parent handle → `void*`) |
| `FileHistory.{cpp,h}`, `FileThumbnails.cpp` | recent files + thumbnail cache logic |
| `Translations.cpp`, `TranslationLangs.cpp` | i18n (only `GetUserDefaultUILanguage()` needs a seam) |
| `SumatraConfig.cpp` | build-config flags |

Also **portable logic currently embedded in Windows UI files** that should be
*extracted* into tier-A modules (see §5):

- `wingui/Layout.{cpp,h}` — constraint layout engine, ~95% pure math.
- `wingui/UIModels.h` — `TreeModel` / `ListBoxModel` data interfaces.
- `Accelerators.cpp` — keyboard-shortcut table + parsing (needs a key-code map).
- `Theme.cpp` — theme/color definitions.
- `Menu.cpp` — the `MenuDef[]` structure + enable/disable state logic.
- `CommandPalette*.cpp` — the fuzzy-filter/ranking engine.
- `TableOfContents.cpp` — `TocItem` tree management + filtering.

### Tier B — Portable core + thin platform back-end (introduce a seam)

Logic is portable but one method or dependency touches the OS. Split into a
shared `.cpp` plus `_win` / `_mac` (or `_posix`) implementation, or replace an
`HDC`/`HWND` parameter with an abstract callback.

| File | What's coupled | Seam to introduce |
|---|---|---|
| `RenderCache.{cpp,h}` | thread-pool + cache is portable; `Paint()` takes `HDC`; debug windows use `HWND` | cache `Pixmap*`; `Paint()` → abstract draw callback / platform blit; move debug windows to `_win` |
| `Selection.{cpp,h}` | model portable; drawing uses `Gdiplus::Graphics/Pen/Brush` | separate model from draw; draw via CoreGraphics on mac |
| `ReadAloudHighlight.{cpp,h}` | build logic portable; `Paint()` takes `HDC` | draw callback like Selection |
| `base/UITask.{cpp}` | posts tasks via a hidden Win32 message window | add `UITask_mac.cpp` using `dispatch_async(dispatch_get_main_queue())` |
| `base/FileWatcher.cpp` | `ReadDirectoryChangesW` | add `FileWatcher_mac.cpp` using FSEvents/kqueue |
| `base/GdiPlus.{cpp,h}` (text measurement) | GDI text metrics used by formatter | `GdiPlus_mac.cpp` using CoreText, behind a small measurement API |
| `AppSettings.cpp` | settings logic portable; font creation via `CreateFontIndirectW`; `%APPDATA%` via `GetSpecialFolder` | font seam (NSFont/CoreText); folder seam → `~/Library/Application Support` |
| `AppTools.cpp` | path logic portable; install detection via registry + `CSIDL_*` | folder/bundle seam; registry checks become `_win`-only |
| `TextToSpeech.{cpp,h}` | interface portable; WinRT backend + `HWND` notify | `_mac` backend with `AVSpeechSynthesizer`; async callback instead of `HWND` |
| `UpdateCheck.cpp` | HTTP already abstracted (`Http_mac.cpp`); `HWND` is only a dialog parent | opaque parent handle; installer-launch seam |
| `base/Dpi.cpp` | screen scaling | `Dpi_mac.cpp` via `NSScreen.backingScaleFactor` (posix stub may already suffice) |

### Tier C — Windows-only UI, **replace** with native Cocoa (do not port)

These are Win32 windowing/controls. On mac they are re-implemented natively; the
portable model/data they consume comes from tiers A/B. Keep the Windows code in
place (optionally rename to `_win` where a shared header is introduced).

- `wingui/` control implementations: `Wnd.cpp`, `WinGui.h`, `Button`, `Edit`,
  `Checkbox`, `ListBox`, `DropDown`, `TreeView` (→ `NSOutlineView`), `TabsCtrl`
  (→ `NSTabView`), `Splitter` (→ `NSSplitView`), `Tooltip`, `Trackbar`,
  `Progress`, `Static`, `LabelWithCloseWnd`, `DialogSizer`.
- `Canvas.cpp` **window proc** (the `WndProc*` half; the pan/zoom/hit-test math
  moves to a tier-A `CanvasModel`).
- `MainWindow.{cpp,h}` → macOS `NSWindowController` + app delegate.
- `Toolbar.cpp` → `NSToolbar` (or custom). `Tabs.cpp` → native tabs.
  `FindBar.cpp` → native find bar. `Notifications.cpp` → overlay/`NSAlert`.
- `Menu.cpp` HMENU rendering half → build `NSMenu` from the extracted `MenuDef`.
- `SumatraDialogs.cpp` → `NSOpenPanel`/`NSSavePanel`/`NSPrintPanel`, native
  Properties/Settings sheets.
- `HomePage.cpp` GDI+ drawing half → native drawing (link/text-layout logic is
  tier-A).
- `wingui/HtmlWindow.cpp` (MSHTML) and `wingui/WebView.cpp`/`BrowserDocView.cpp`
  (WebView2) → `WKWebView` (needed for CHM and Markdown views).
- The `WndProc`/message-dispatch and window-lifecycle half of `SumatraPDF.cpp`
  and `SumatraStartup.cpp` (see §4).

### Tier D — Windows-only, **stays Windows-only** (stub or omit on mac)

Deep OS integration with no near-term mac need; keep entirely under `#if OS_WIN`
/ `_win` and provide a no-op or a separate mac implementation only if/when
wanted.

- `CrashHandler.cpp` (DbgHelp/minidump) — mac uses the existing
  `CrashHandlerNoOp.cpp` stub initially; a real mac crash reporter is optional
  later (signals + `backtrace()`).
- `ExternalViewers.cpp` MAPI + registry lookups — mac equivalent is
  `NSWorkspace`/LaunchServices, a separate `_mac` implementation.
- DDE + `WM_COPYDATA` single-instance IPC, plugin embedding
  (`MaybeMakePluginWindow`), `WinMain`.
- Registry: `RegistryInstaller.cpp`, `RegistryPreview.cpp`,
  `RegistrySearchFilter.cpp`; `Installer.cpp`/`Uninstaller.cpp`;
  `ifilter/`, `previewer/`, `uia/`, `TestPlugin.cpp` — Windows shell/COM
  extensions with no macOS analogue.
- `Print.cpp` (Win32 `DEVMODE`/`StartDoc`) — mac printing is a separate
  `NSPrintOperation` path later.

---

## 4. The two big files: `SumatraPDF.cpp` and `SumatraStartup.cpp`

`SumatraPDF.cpp` (~11k lines) and `SumatraStartup.cpp` (~2.5k lines) are the
crux. They mix **portable orchestration** (~60–70%) with **Win32 event dispatch
and window lifecycle** (~25–30%).

Strategy — **do not port these files wholesale**. Instead, extract the portable
orchestration into new platform-neutral units, leaving each platform a thin
adapter:

- Portable to extract: file open/load orchestration (`LoadArgs`,
  `StartLoadDocument`), session/tab **state** management (`FileState`,
  `TabState`, `SessionData`, save/restore), command **dispatch tables**
  (`Cmd*` → action), permissions/policy (`HasPermission`), display-state
  persistence, controller callback handling.
- Windows-only, keep in `_win`: `WndProcSumatraFrame` message routing,
  `CreateMainWindow`/`CreateSidebar`/`RelayoutFrame`, `WINDOWPLACEMENT`
  save/restore, menubar rebuild, `OPENFILENAME` hooks, DDE/`WM_COPYDATA`,
  `WinMain` + `RunMessageLoop`.
- macOS adapter: `NSApplicationMain` run loop, `NSWindow`/`NSView` lifecycle,
  command dispatch driven from `NSMenu`/key handlers into the shared dispatch
  table, `application:openFile:` for file opening.

Single-instance/IPC (DDE + `WM_COPYDATA`) has no portable core worth saving; mac
gets its own mechanism (`NSApplication` already coalesces documents into one
instance; a Unix-domain socket or distributed notification covers the rest).

---

## 5. New portable modules to extract

To avoid dragging Win32 into the model, extract these platform-neutral pieces
(mostly from tier-C files) into their own headers/units so both platforms share
them:

1. **`CanvasModel`** — pan/zoom/scroll math, mouse-drag velocity, hit-testing,
   wheel normalization pulled out of `Canvas.cpp`. Drives `DisplayModel`;
   consumed by both `WndProcCanvas` (win) and an `NSView` (mac).
2. **Portable render surface** — settle on `Pixmap*` as the cached render
   product in `RenderCache`; platform blit helpers (`BlitPixmap` on win exists;
   add a CoreGraphics one).
3. **Menu model** — the `MenuDef[]` + enable/disable logic from `Menu.cpp`,
   with platform builders (`HMENU` vs `NSMenu`).
4. **Key/shortcut model** — `Accelerators.cpp` table plus an abstract key enum;
   per-platform translation (`VK_*` ↔ `NSEvent` key codes).
5. **Theme/color model** — `Theme.cpp` colors as portable data; each platform
   applies native dark-mode.
6. **Layout engine** — `wingui/Layout.{cpp,h}` + `UIModels.h`, with the one
   `HWND`-DPI hook abstracted behind a DPI provider.
7. **Platform services interface** — a small header (e.g.
   `src/AppPlatform.h`) declaring the seams: special folders, locale, font
   lookup/measure, open-with, launch-installer, single-instance. Implemented in
   `AppPlatform_win.cpp` / `AppPlatform_mac.mm`.

Each extraction must **keep Windows green** (build + `run-unit-tests`) — extract
in place, re-point Windows to the new module, then add the mac build inputs.

---

## 6. Phased roadmap

Ordered to deliver a usable mac viewer early, then layer on features. Each phase
is independently shippable and keeps both platforms building.

**Phase 0 — foundation seams (base/)**
- `UITask_mac.cpp` (dispatch to main queue), `FileWatcher_mac.cpp` (FSEvents),
  `Dpi_mac.cpp`. Confirm `Pixmap`↔native blit path. These unblock everything
  above base.

**Phase 1 — portable document model**
- Add tier-A model files to `MAC_APP_SOURCES`: `DisplayModel`, `TextSelection`,
  `TextSearch`, `Annotation`, `PdfSync`, `DisplayMode`, `DocProperties`.
- Introduce the `RenderCache` seam (cache `Pixmap*`, callback-based `Paint`) and
  compile it on mac.
- **Milestone:** headless test that drives `DisplayModel` + `RenderCache`
  through `DocControllerCallback` and renders scrolled/zoomed output — proves the
  model layer is portable.

**Phase 2 — real Cocoa viewer**
- Replace the stub `SumatraMac.mm` view with an `NSView`/`NSScrollView` backed
  by `DisplayModel` + `RenderCache`, implementing `DocControllerCallback`.
- Add scrolling, zoom, continuous/facing display modes, keyboard navigation.
- Extract `CanvasModel` and use it on mac.
- **Milestone:** scroll/zoom/navigate a multi-page PDF natively.

**Phase 3 — selection, search, TOC, navigation UI**
- Port `Selection` drawing to CoreGraphics; wire `TextSelection`/`TextSearch`.
- Build a native find bar over `TextSearch`; `NSOutlineView` TOC over the
  extracted `TocItem` model; hyperlink navigation via `DocControllerCallback`.

**Phase 4 — app shell & persistence**
- Port settings load/save (tier-B folder/font seams), `FileHistory`,
  `Favorites`, `Theme`, `Accelerators`, `Commands`/dispatch table.
- Native menus (`NSMenu` from `MenuDef`), multi-tab/multi-window
  (`NSWindowController`), recent-files, session restore.
- Extract the portable orchestration from `SumatraPDF.cpp`/`SumatraStartup.cpp`.

**Phase 5 — formats & extras**
- Ebook rendering (formatter font metrics via CoreText).
- CHM + Markdown views via `WKWebView` (replaces WebView2/MSHTML).
- TextToSpeech via `AVSpeechSynthesizer`; annotations editing; printing
  (`NSPrintOperation`); external viewers via `NSWorkspace`.

**Never (tier D):** installer, registry integration, IFilter, preview handler,
UIA, DDE, plugin/embedding, Win32 crash minidumps.

---

## 7. Conventions & build

- Follow the `agents.md` suffix rules: shared decl in `Foo.h`, portable body in
  `Foo.cpp`, platform bodies in `Foo_win.cpp` / `Foo_mac.cpp` / `Foo_posix.cpp`.
  Prefer `_posix` when Linux would share the code.
- Keep Cocoa/AppKit in `.mm` under `src/mac/` and **never** include `base/Base.h`
  in a file that imports AppKit (name clashes like `Size`). Route through a C/C++
  bridge (the `SumatraMacEngine` pattern), widened as needed into the
  `AppPlatform` service interface.
- No `#ifdef` sprawl in shared headers; keep public APIs identical per platform
  and push differences into suffixed `.cpp`.
- Register every new mac input in the source lists in `cmd/build-mac.ts`
  (`DEP_LIBS_BASE` for base, `MAC_APP_SOURCES` for the app). Extend
  `test_util`/`test_engines` lists to cover newly-portable model code with
  headless tests.
- **Every change keeps Windows green** (`bun ./cmd/build.ts`, `bun
  cmd/run-unit-tests.ts -dbg`) and **keeps mac green** via the remote build
  wrapper on a temporary branch (`bun cmd/build-mac-remote.ts -branch … -debug`).

---

## 8. Risks & open questions

- **`RenderCache` refactor** is the highest-leverage and highest-risk change —
  it touches the hot rendering path on Windows. Do it behind the `Pixmap`/
  callback seam with the Windows blit unchanged, and benchmark.
- **Font/text metrics** for the ebook formatter (GDI vs CoreText) can shift line
  breaking; expect layout diffs and a measurement-abstraction shim.
- **Extracting orchestration from `SumatraPDF.cpp`** is large and easy to
  regress; do it in small, individually-verifiable slices, Windows-first.
- **CHM/Markdown** depend on a WebView; `WKWebView`'s async/message model differs
  from WebView2 — treat as its own mini-project in Phase 5.
- **Decision to confirm:** target macOS-only first (defer Linux GUI), but keep
  new seams `_posix` where trivially shared so a Linux GUI stays possible.

---

## 9. Summary of decisions

- **Portable (both platforms):** all of `base/`, engines, the document model
  (`DisplayModel`, `TextSelection`, `TextSearch`, `Annotation`, `PdfSync`,
  `DisplayMode`, `DocProperties`, `DocController`), app logic (`Settings`,
  `GlobalPrefs`, `Commands`, `CommandAvailability`, `Flags`, `FileHistory`,
  `Favorites` data, `Translations`, `Theme` data, `Accelerators` data,
  `EngineCreate`, ebook/HTML formatters), and extracted models (layout, menu,
  key, canvas, TOC-tree, command-palette filter).
- **Portable core + platform back-end (seam):** `RenderCache`, `Selection`/
  highlight drawing, `UITask`, `FileWatcher`, `Dpi`, font/text measurement,
  special-folder/locale/open-with services, `TextToSpeech`, `UpdateCheck`.
- **Windows-only, replaced by native Cocoa:** `wingui/` controls, `Canvas`
  WndProc, `MainWindow`, `Toolbar`, `Tabs`, `FindBar`, `Notifications`, HMENU
  rendering, `SumatraDialogs`, `HomePage` drawing, MSHTML/WebView2 views, and the
  message-loop/window-lifecycle halves of `SumatraPDF.cpp`/`SumatraStartup.cpp`.
- **Windows-only, stays Windows-only:** crash minidumps, DDE/IPC, plugin
  embedding, `WinMain`, registry, installer, IFilter, preview handler, UIA,
  Win32 printing.

The linchpin is the already-existing `DocController` / `DocControllerCallback`
seam plus portable `Pixmap` output: they let the entire model layer move to macOS
without touching Win32, so the native Cocoa shell only has to implement the
callback interface and draw a `Pixmap`.
