# Linux GTK4 port progress

This file tracks implementation of [linux-port.md](linux-port.md). Each completed stage is committed separately
after its relevant Windows, Linux, and portable checks pass.

## Status

| Stage | Status      | Summary                                      |
| ----- | ----------- | -------------------------------------------- |
| 1     | Complete    | GTK4 application executable and file opening |
| 2     | Complete    | Shared portable reader model                 |
| 3     | Complete    | Embeddable canvas and document viewer        |
| 4     | Complete    | Portable asynchronous rendering              |
| 5     | Complete    | Application shell, tabs, and commands        |
| 6     | In progress | Reader features                              |
| 7     | Not started | Linux desktop services                       |
| 8     | Not started | Packaging and deferred features              |

## Stage 1: Linux application target

Completed 2026-08-16.

- Added `src/linux/SumatraLinux.cpp`, `LinuxApp.*`, and `LinuxWindow.*`.
- Added a `GtkApplication` with activation and command-line file-open handling.
- Added a native application window that displays the requested local path.
- Extended `cmd/helper/linux-build.ts` to build `out/linux-<config>64/SumatraPDF` while retaining the GTK4
  keyboard-help executable and existing test targets.

Verification:

- `bun cmd/build.ts -debug`: passed with no warnings.
- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,782 assertions and the application,
  keyboard-help viewer, and `test_engines` linked.
- `timeout 3s ./out/linux-dbg64/SumatraPDF ext/a-zlib/zlib.3.pdf` under WSLg: application stayed alive until the
  expected timeout; Mesa emitted non-fatal renderer warnings.

## Stage 2: Shared portable reader model

Completed 2026-08-16.

- Added `ReaderModel`, which owns engine selection and lifetime and exposes page metadata, shared document layout,
  and page rendering without depending on a native UI toolkit.
- Replaced the duplicate engine-selection and layout logic in the Cocoa bridge with `ReaderModel`.
- Connected the Linux window to the real engine stack so opening a supported document validates it and displays its
  page count.
- Shared the portable engine source and link manifests between the Linux application and `test_engines`, and added
  `ReaderModel` to the Windows and macOS application build inputs.

Verification:

- `bun cmd/build.ts -debug`: passed with no warnings.
- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,859 assertions and the application,
  keyboard-help viewer, and `test_engines` linked.
- `timeout 3s ./out/linux-dbg64/SumatraPDF ext/a-zlib/zlib.3.pdf` under WSLg: the document-backed application stayed
  alive until the expected timeout; Mesa emitted non-fatal renderer warnings.

## Stage 3: Embeddable canvas and document viewer

Completed 2026-08-16.

- Added the toolkit-neutral `PlatformCanvas` interface for an embeddable drawing surface, pointer input, scrolling,
  named keys, focus, invalidation, and cursor changes.
- Added the GTK4 canvas implementation in `src/gui/gtk4/PlatformCanvasGtk.cpp` using `GtkDrawingArea`, Cairo-backed
  `Gfx`, and GTK event controllers.
- Added a portable `DocumentView` that combines `ReaderModel`, `DocumentLayout`, and `Gfx` to synchronously render
  visible pages. Its temporary cache retains visible pages only; Stage 4 replaces this with the bounded asynchronous
  cache.
- Replaced the Linux path label with the real document canvas. It supports continuous and single-page modes, page
  navigation, fit-page, fit-width and actual-size zoom, rotation, wheel and drag scrolling, and cursor-anchored
  Ctrl-wheel zoom.
- Added initial keyboard controls: `Page Up`/`Page Down`, `Home`/`End`, arrows, `+`/`-`, `Ctrl-0`, `C` for layout,
  `F`/`W`/`1` for zoom modes, and `R`/`Shift-R` for rotation.

Verification:

- `bun cmd/build.ts -debug`: passed with no warnings.
- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 103,142 assertions and all Linux targets linked.
- `timeout 5s ./out/linux-dbg64/SumatraPDF ext/a-zlib/zlib.3.pdf` under WSLg: the rendered-document window stayed
  alive until the expected timeout; Mesa emitted non-fatal renderer warnings.

## Stage 4: Portable asynchronous rendering

Completed 2026-08-16.

- Added a portable page-render policy with visible, nearby, and background priorities, same-page request
  replacement, generation filtering, and LRU eviction selection.
- Added `PageRenderService`, which renders away from the UI thread through a worker-local `EngineBase::Clone`, uses
  `AbortCookie` during cancellation and shutdown, rejects results from stale generations, and posts completion to
  the native main thread through `PlatformPostTask`.
- Bounded the rendered-page cache to 96 MB. Oversized pages are not cached, and older entries are evicted by recent
  use before a new entry can take the cache over budget.
- Connected `DocumentView` to the service. Paints request missing visible pages first and prefetch nearby and
  background pages; zoom, rotation, fit-mode resize, layout changes, and single-page navigation cancel obsolete
  work.
- Added a standard `app.quit` action and `Ctrl-Q` accelerator so orderly Linux shutdown can cancel and join the
  rendering worker.
- Added portable unit coverage for priority order, request replacement, stale-generation removal, and LRU eviction.

Verification:

- `bun cmd/build.ts -debug`: passed with no warnings.
- `bun cmd/run-unit-tests.ts -dbg`: passed, including the new render-policy tests.
- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,514 assertions and all Linux targets linked.
- `bun cmd/build.ts -linux`: the ASan build passed; Linux `test_util` passed 102,648 assertions.
- Both debug and ASan applications opened `ext/a-zlib/zlib.3.pdf`, rendered for several seconds, accepted
  `gapplication action org.sumatrapdf.SumatraPDF quit`, joined the render worker, and exited with status 0. ASan
  reported no error; WSLg only emitted its existing non-fatal Mesa renderer warnings.

## Stage 5: Application shell and commands

Completed 2026-08-16.

### Native reader controls

Completed 2026-08-16.

- Added a GTK header and reader toolbar with file open, previous/next page, page status, fit-page, fit-width,
  actual-size, rotation, continuous-layout, and fullscreen controls.
- Routed toolbar and window shortcuts through the generated `Cmd*` identifiers instead of adding a separate Linux
  command vocabulary.
- Added a GTK native file chooser and concise document/error window titles.
- Added a portable `DocumentView::onStateChanged` callback so native shells can keep page and mode controls current
  after keyboard, mouse, or toolbar navigation without introducing GTK dependencies in `src/gui`.

Verification:

- `bun cmd/build.ts -debug`: passed with no warnings.
- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,679 assertions and all Linux targets linked.
- The GTK application opened and rendered `ext/a-zlib/zlib.3.pdf` under WSLg with the native shell visible, accepted
  the `app.quit` action after five seconds, and exited with status 0. WSLg only emitted its existing non-fatal Mesa
  renderer warnings.

### Document tabs

Completed 2026-08-16.

- Added a Linux tab owner around each portable `DocumentView`; every tab has independent layout, navigation state,
  engine, asynchronous worker, and render cache.
- Added scrollable, reorderable GTK tabs with close buttons, active-tab title/control synchronization, and an empty
  window state when the last tab closes.
- Open all files delivered in one `GtkApplication::open` request instead of ignoring all but the first.
- Added next/previous tab commands, `Ctrl+Tab` and `Ctrl+Page Up/Down` switching, `Ctrl+W` close, and a bounded
  ten-document reopen history exposed through `Ctrl+Shift+T` and the toolbar.
- Exposed close, reopen, next, and previous tab application actions so the native lifecycle can be smoke-tested
  without synthetic pointer or keyboard input.

Verification:

- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,787 assertions and all Linux targets linked.
- `bun cmd/build.ts -linux`: the ASan build passed; Linux `test_util` passed 102,840 assertions.
- Under WSLg, the ASan app opened two document tabs, switched forward and backward, closed a tab, reopened it, and
  quit through application actions. Both per-tab render workers shut down cleanly, the process exited with status 0,
  and ASan reported no errors. WSLg only emitted its existing non-fatal Mesa renderer warnings.

### Embedded MuPDF fonts

Completed 2026-08-16.

- Fixed the Linux x64 font-object contract by compiling MuPDF with `HAVE_OBJCOPY` and exporting the resource-path
  `_start` and `_end` symbols its font table expects.
- Removed the invalid fallback that treated GNU `objcopy`'s absolute `_size` symbols as addressable integer objects.
  PDFs that use MuPDF's embedded Base-14 fonts now render instead of dereferencing the encoded size as a pointer.
- Kept the generated-C font path used by macOS and Linux arm64 unchanged.

Verification:

- `bun cmd/build.ts -linux -debug -clean`: passed; Linux `test_util` passed 102,701 assertions and all targets linked.
- `bun cmd/build.ts -linux`: the rebuilt ASan target passed; Linux `test_util` passed 102,688 assertions.
- The debug and ASan applications rendered `tests/combining-mark-first.pdf` and `tests/issue-1189.pdf`, which both
  reproduced the invalid embedded-font read before the fix. The two-tab switch, close, reopen, and quit lifecycle
  exited with status 0, and ASan reported no errors.

### Native menu, presentation, and keyboard help

Completed 2026-08-16.

- Added a native application menu and menu button for document/tab lifecycle, fullscreen, presentation, keyboard
  help, and quit actions, with GTK accelerators backed by the generated `Cmd*` identifiers.
- Added presentation mode. It temporarily hides the toolbar and tab strip, selects single-page fit-page layout,
  enters fullscreen, and restores the previous layout, zoom, and fullscreen state on exit.
- Integrated the portable `KeyboardHelp.cpp` dialog into the real Linux application instead of keeping it only as
  a standalone GTK4 build target.
- Registered the application menu during GTK startup so it is available before the first window is activated and
  satisfies GTK's application-registration lifecycle.

Verification:

- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,734 assertions and all Linux targets linked.
- `bun cmd/build.ts -linux`: the ASan build passed; Linux `test_util` passed 102,583 assertions.
- Under WSLg, both debug and ASan applications entered and exited presentation and fullscreen modes, opened and
  closed keyboard help, and quit through application actions with status 0. ASan reported no errors; WSLg only
  emitted its existing non-fatal Mesa renderer warnings.

## Stage 6: Reader features

In progress.

### Document links

Completed 2026-08-16.

- Added page-coordinate link hit testing to the portable `DocumentView`, including zoom and rotation transforms,
  hand-cursor feedback, and click-versus-pan handling.
- Added a portable `ILinkHandler` implementation for internal pages, named destinations, destination positions and
  zooms, relative files, HTTP/HTTPS URLs, mail links, and engine-specific MuPDF and DjVu destinations.
- Kept desktop actions outside the portable view: the Linux shell launches permitted external URIs through GIO and
  opens local or document-relative targets in a new application tab.
- Added `test_engines <path> -list-links` as a focused engine probe that prints link targets and page rectangles.

Verification:

- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,820 assertions and all Linux targets linked.
- `out/linux-dbg64/test_engines ext/a-zlib/zlib.3.pdf -list-links`: found all 13 URL and mail links across the two
  pages.
- `bun cmd/build.ts -linux`: the ASan build passed; Linux `test_util` passed 102,833 assertions, and the ASan link
  probe also found 13 links.
- Both debug and ASan applications opened and rendered the link-bearing zlib manual, then shut down through the
  application action with status 0. ASan reported no errors; WSLg only emitted its existing non-fatal Mesa renderer
  warnings.

### Text selection and clipboard

Completed 2026-08-16.

- Integrated the existing portable `TextSelection` engine into each `DocumentView` and kept its lifetime tied to
  the tab's reader engine.
- Added glyph hit testing and drag selection across pages while retaining canvas panning when a drag starts outside
  text and giving document links priority over text beneath them.
- Painted selection rectangles through portable `Gfx`, with page-to-screen transforms that honor per-page zoom and
  document rotation.
- Added generated-command routing for Select All and Copy, GTK clipboard export, `Ctrl+A` / `Ctrl+C` accelerators,
  and Escape-to-clear behavior.
- Added `test_engines <path> -select-all-text` as a focused probe for portable selection extraction and geometry.

Verification:

- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,815 assertions and all Linux targets linked.
- The debug selection probe selected 4,284 UTF-8 bytes from `ext/a-zlib/zlib.3.pdf` into 74 highlight rectangles.
- `bun cmd/build.ts -linux`: the ASan build passed; Linux `test_util` passed 102,692 assertions.
- Under WSLg, both debug and ASan applications ran Select All, Copy, and Quit through application actions with
  status 0. The ASan engine probe reproduced the same 4,284-byte, 74-rectangle selection and reported no errors;
  WSLg only emitted its existing non-fatal Mesa renderer warnings.

### Find

Completed 2026-08-16.

- Integrated the existing portable `TextSearch` state machine into each document view and reused text-selection
  geometry to highlight the current match.
- Added forward and backward navigation with document-boundary wrapping, current-match page navigation, and stale
  highlight clearing when a term has no matches.
- Added a native GTK search bar with entry activation, Previous, Next, status, and Close controls, plus `Ctrl+F`,
  `F3`, `Shift+F3`, and Escape handling through generated command identifiers.
- Added a parameterized application search action for deterministic integration checks and
  `test_engines <path> -find-text <term>` as a focused portable search probe.

Verification:

- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,854 assertions and all Linux targets linked.
- The debug search probe found 27 matches for `zlib` in `ext/a-zlib/zlib.3.pdf`.
- `bun cmd/build.ts -linux`: the ASan build passed; Linux `test_util` passed 102,796 assertions, and the ASan search
  probe found the same 27 matches.
- Under WSLg, both debug and ASan applications searched for `zlib`, navigated to the next and previous matches, and
  quit through application actions with status 0. ASan reported no errors; WSLg only emitted its existing non-fatal
  Mesa renderer warnings.

### Table of contents

Completed 2026-08-16.

- Added a small toolkit-neutral TOC surface to `DocumentView` that flattens the existing `TocTree` into stable
  title/depth/index accessors while keeping engine-owned destinations and navigation inside the portable view.
- Added a native GTK bookmarks sidebar with hierarchy indentation, scrolling, tooltips, row activation, menu and
  `F12` toggle actions, per-tab rebuilding, and presentation-mode hiding/restoration.
- Routed TOC destinations through the same portable `ILinkHandler` used by document links so internal page,
  coordinate, URL, and file targets retain engine-specific handling.
- Added `test_engines <path> -list-toc` as a focused engine and hierarchy probe, plus a parameterized GTK action for
  deterministic navigation checks.

Verification:

- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,903 assertions and all Linux targets linked.
- The debug and ASan TOC probes found the same 178 nested entries in
  `bug-1352-merged_manuals-1.4.2.pdf`.
- `bun cmd/build.ts -linux`: the ASan build passed; Linux `test_util` passed 102,744 assertions.
- Under WSLg, the debug and ASan applications displayed the bookmarks sidebar, activated entry 10, closed the tab,
  and quit through application actions. There were no sanitizer memory-access errors; LeakSanitizer reports the
  known GTK/Pango/fontconfig process-lifetime caches after the outline creates many labels, and WSLg emitted its
  existing non-fatal Mesa renderer warnings.

### Document properties

Completed 2026-08-16.

- Added lazy property loading to `DocumentView` through the existing `EngineBase::GetProperties` and `DocProp`
  APIs. Values are copied into view-owned storage on first use, so temporary engine strings remain valid without
  adding metadata work to document startup.
- Added a native modal GTK properties window with a scrollable name/value grid, selectable wrapped values, a menu
  action, and the existing `Ctrl+D` shortcut.
- Added `test_engines <path> -list-properties` as a focused portable metadata probe.

Verification:

- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,796 assertions and all Linux targets linked.
- The debug and ASan property probes returned the same six properties for `ext/a-zlib/zlib.3.pdf`.
- `bun cmd/build.ts -linux`: the ASan build passed; Linux `test_util` passed 102,703 assertions.
- Under WSLg, both the debug and ASan applications opened the properties window and quit through application
  actions with status 0. The ASan UI smoke used `detect_leaks=0` for the previously documented GTK/Pango process
  caches and reported no memory-safety errors; WSLg only emitted its existing non-fatal Mesa renderer warnings.
