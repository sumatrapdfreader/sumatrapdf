# Linux GTK4 port progress

This file tracks implementation of [linux-port.md](linux-port.md). Each completed stage is committed separately
after its relevant Windows, Linux, and portable checks pass.

## Status

| Stage | Status   | Summary                                      |
| ----- | -------- | -------------------------------------------- |
| 1     | Complete | GTK4 application executable and file opening |
| 2     | Complete | Shared portable reader model                 |
| 3     | Complete | Embeddable canvas and document viewer        |
| 4     | Complete | Portable asynchronous rendering              |
| 5     | Complete | Application shell, tabs, and commands        |
| 6     | Complete | Reader features                              |
| 7     | Complete | Linux desktop services                       |
| 8     | Complete | Portable packaging; distro formats deferred  |

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

### Recent files and per-document state

Completed 2026-08-16.

- Added a Linux preferences bridge that uses the existing `GlobalPrefs` / `FileState` parser and serializer, stored
  at `$XDG_CONFIG_HOME/sumatrapdf/SumatraPDF-settings.txt` or the equivalent `$HOME/.config` path.
- Added a native Recent Files submenu backed by the ten most recent non-missing document states.
- Persisted and restored each document's page, continuous/single-page layout, zoom, and rotation by keeping its
  in-memory file state current as the view changes and when a tab is closed.
- Deferred an initial restored page jump until GTK has allocated the canvas, avoiding a zero-sized first layout
  that could otherwise reset the document to page 1.

Verification:

- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,736 assertions and all Linux targets linked.
- An isolated two-run WSLg smoke opened `ext/a-zlib/zlib.3.pdf`, navigated to page 2, quit, and reopened it from the
  same XDG configuration. The second run retained page 2 without another navigation command.
- A restore smoke also retained an explicitly seeded single-page layout, 100% zoom, and 90-degree rotation without
  overwriting partially restored state through view-change callbacks.
- `bun cmd/build.ts -linux`: the ASan build passed; Linux `test_util` passed 102,736 assertions and all Linux targets
  linked.
- The isolated ASan WSLg smoke saved page 2 and exited with status 0 under `detect_leaks=0`; it reported no
  memory-safety errors, and WSLg only emitted its existing non-fatal Mesa renderer warnings.

### Favorites

Completed 2026-08-16.

- Reused the existing serialized `FileState::favorites` data rather than introducing a Linux-only bookmark format.
- Added native actions for adding and removing the current page, with the existing `Ctrl+B` shortcut for adding a
  favorite.
- Added a native Favorites sidebar shared with the table-of-contents pane. It lists favorites across documents,
  persists its visibility, switches to an already-open tab when possible, and otherwise opens the target document.
- Kept temporary search marks out of the Linux favorites list and made Linux path matching case-sensitive.

Verification:

- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,570 assertions and all Linux targets linked.
- An isolated WSLg action smoke added page 2 of `ext/a-zlib/zlib.3.pdf`, displayed the Favorites sidebar, moved to
  page 1, activated favorite 0, returned to page 2, and exited with status 0. The saved settings contained the page
  2 favorite and sidebar visibility; a second run removed the favorite successfully.
- `bun cmd/build.ts -linux`: the ASan build passed; Linux `test_util` passed 102,976 assertions and all Linux targets
  linked.
- The same isolated ASan WSLg action sequence exited with status 0 under `detect_leaks=0`, saved the page 2 favorite,
  and reported no memory-safety errors. WSLg only emitted its existing non-fatal Mesa renderer warnings.

### Command palette

Completed 2026-08-16, completing Stage 6.

- Extracted word splitting and case-insensitive multi-word matching from the Win32 drawing implementation into
  portable `FilterUtil` code, retaining the same filtering behavior in the Windows palette and related views.
- Added a portable command-palette model that collects generated command descriptions, sorts them, filters them,
  and preserves command IDs independently of a native UI toolkit.
- Added a modal GTK command palette with live search, mouse activation, arrow-key selection, Enter execution,
  Escape dismissal, and the existing `Ctrl+K` shortcut. Its supported commands route through the normal Linux
  command dispatcher.
- Added portable unit coverage for collection, ordering, case-insensitive multi-word filtering, and empty results.

Verification:

- `bun cmd/build.ts -debug`: passed with no warnings after regenerating the Visual Studio projects for the new
  portable sources.
- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,580 assertions and all Linux targets linked.
- `bun cmd/build.ts -linux`: the ASan build passed; Linux `test_util` passed 102,408 assertions and all Linux targets
  linked.
- `bun cmd/build.ts -mac-remote -branch tmp/mac-port-command-palette-20260816 -debug`: passed after bringing the
  macOS test manifests up to date; macOS `test_util` passed 102,656 assertions, `test_engines` linked, and
  `SumatraPDF.app` linked.
- Under WSLg, both debug and ASan applications opened the GTK command palette over
  `ext/a-zlib/zlib.3.pdf` and quit through application actions with status 0. ASan used `detect_leaks=0`, reported no
  memory-safety errors, and WSLg only emitted its existing non-fatal Mesa renderer warnings.

## Stage 7: Linux desktop services

### Automatic document reload

Completed 2026-08-16.

- Added `src/base/FileWatcher_linux.cpp`, implementing the existing `FileWatcher` API with one `inotify` descriptor,
  shared directory watches, an `eventfd` shutdown wakeup, synchronized subscribe/unsubscribe operations, ignored-file
  support, and deterministic worker-thread shutdown.
- Added a Linux-only unit test that watches a temporary file, rewrites it, observes the asynchronous notification,
  unsubscribes, and shuts the service down.
- Subscribed every successfully opened Linux tab to its document path. Notifications are coalesced and posted back to
  the GTK main thread; reload preserves the current page, zoom, rotation, and continuous/single-page mode.
- Made pending reload tasks retain their tab shell safely across tab/window teardown, and drain posted callbacks during
  application shutdown.

Verification:

- `bun cmd/build.ts -debug`: passed with 0 warnings and 0 errors after regenerating the Visual Studio projects.
- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,813 assertions, including the new real
  `inotify` test, and all Linux targets linked.
- `bun cmd/build.ts -linux`: the ASan build passed; Linux `test_util` passed 102,790 assertions and all Linux targets
  linked.
- Debug and ASan WSLg smokes opened a copied PDF, atomically replaced the watched file, and quit cleanly with status 0.
  ASan used `detect_leaks=0` and reported no memory-safety errors; only the existing WSLg Mesa warnings appeared.

### Native printing

Completed 2026-08-16.

- Added a GTK `GtkPrintOperation` implementation in `src/linux/LinuxPrint.cpp`, exposed through the File menu,
  `Ctrl+P`, and the command palette.
- Added narrow portable print accessors to `ReaderModel` and `DocumentView`. Page rasterization uses the engine's
  `RenderTarget::Print` path while native dialog, settings, page setup, and Cairo presentation remain Linux-only.
- Fit and center each selected page inside the printer's imageable area, preserve the reader's current rotation, and
  cap rasterization at 300 DPI to bound per-page memory use.
- Retain the native print settings and page setup between print jobs and release them during clean application shutdown.

Verification:

- `bun cmd/build.ts -debug`: passed with 0 warnings and 0 errors.
- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,795 assertions and all Linux targets,
  including the 46-source GTK application, linked.
- `bun cmd/build.ts -linux`: the ASan build passed; Linux `test_util` passed 102,775 assertions and all Linux targets
  linked.
- `bun cmd/build.ts -mac-remote -branch tmp/mac-port-linux-print-20260816 -debug`: passed; macOS `test_util` passed
  102,793 assertions, `test_engines` linked, and `SumatraPDF.app` linked with the shared reader change.
- The WSL environment has no configured physical printer, so verification covers native print API compilation and
  linkage rather than submitting a real printer job.

### Clipboard and file-manager integration

Completed 2026-08-16.

- Consolidated Linux clipboard writes in a small desktop-services module and reused it for selected text and file
  paths.
- Added Copy File Path to the Edit menu, application actions, and the command palette.
- Added Show in Folder to the File menu, application actions, and the command palette. It first uses the standard
  `org.freedesktop.FileManager1.ShowItems` D-Bus interface so file managers can select the document, then falls back
  to opening the containing directory through the desktop's default URI handler.

Verification:

- `bun cmd/build.ts -debug`: passed with 0 warnings and 0 errors.
- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,757 assertions and all Linux targets,
  including the 47-source GTK application, linked.
- `bun cmd/build.ts -linux`: the ASan build passed; Linux `test_util` passed 102,888 assertions and all Linux targets
  linked.
- An isolated WSLg smoke opened `ext/a-zlib/zlib.3.pdf`, invoked the Copy File Path application action, and quit
  cleanly with status 0. WSLg only emitted its existing non-fatal Mesa renderer warnings.

### XDG default PDF reader

Completed 2026-08-16.

- Added a minimal `org.sumatrapdf.SumatraPDF.desktop` application entry with PDF MIME support under
  `packaging/linux/`.
- Added an explicit Make Default PDF Reader menu action. It locates the installed desktop entry through GIO, updates
  the user's MIME association without a shell command, verifies the selected handler, and reports success or a useful
  installation/error message in a native modal window.
- Kept registration user-initiated; the application does not change associations or prompt during startup.

Verification:

- `bun cmd/build.ts -debug`: passed with 0 warnings and 0 errors.
- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,550 assertions and all Linux targets linked.
- `bun cmd/build.ts -linux`: the ASan build passed; Linux `test_util` passed 102,729 assertions and all Linux targets
  linked.
- An isolated WSLg smoke installed only the new desktop entry under a temporary `XDG_DATA_HOME`, invoked Make Default
  PDF Reader, and confirmed with a fresh `gio mime application/pdf` query that
  `org.sumatrapdf.SumatraPDF.desktop` was the selected handler. The application then quit cleanly.
- `desktop-file-validate` is not installed in the WSL image; successful GIO discovery and registration provided the
  functional desktop-entry validation for this step.

### Session restore and clean shutdown

Completed 2026-08-16.

- Reused the existing serialized `SessionData` and `TabState` structures to store the open Linux tabs, their notebook
  order, selected tab, and independent page/layout/zoom/rotation state.
- Restored the saved session only for a bare initial launch. Explicit document arguments start with exactly those
  documents, and missing saved files are skipped instead of creating stale error tabs.
- Captured the live session before GTK dismantles the notebook on window close, explicit Quit, or application
  shutdown. Existing tab, render-service, and file-watcher teardown remains deterministic afterward.
- Honored the existing `RememberOpenedFiles` and `RestoreSession` preferences instead of adding Linux-only settings.

Verification:

- `bun cmd/build.ts -debug`: passed with 0 warnings and 0 errors.
- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,641 assertions and all Linux targets linked.
- `bun cmd/build.ts -linux`: the ASan build passed; Linux `test_util` passed 102,822 assertions and all Linux targets
  linked.
- An isolated two-run WSLg smoke opened two PDFs, selected page 2 in the second tab, quit, and relaunched without
  arguments. Both tabs and the selected second tab were restored; after switching to the first tab and selecting page
  1, clean shutdown persisted `TabIndex = 1` and independent page 1/page 2 tab states.
- The same two-run smoke passed under ASan with `detect_leaks=0` for the documented GTK/Pango process caches and no
  AddressSanitizer errors. WSLg only emitted its existing non-fatal Mesa renderer warnings.

### Desktop and AppStream resources

Completed 2026-08-16, completing Stage 7.

- Expanded the freedesktop desktop entry with a generic name and searchable keywords while retaining the application
  ID, command line, icon name, and PDF MIME association used by the default-reader integration.
- Added AppStream metadata describing the native Linux reader, its desktop entry, executable, supported media type,
  project links, license, and content rating.
- Made every Linux build validate the identifiers and essential integration fields, then stage the desktop entry,
  AppStream metadata, and existing scalable SumatraPDF icon under the output directory's standard `share/` hierarchy.

Verification:

- `appstreamcli validate --no-net packaging/linux/org.sumatrapdf.SumatraPDF.metainfo.xml`: passed. Pedantic mode only
  noted the established mixed-case application ID and the intentionally absent release history.
- `bun cmd/build.ts -debug`: passed with 0 warnings and 0 errors.
- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,840 assertions, all Linux targets linked, and
  all three desktop resources were staged.
- `bun cmd/build.ts -linux`: the ASan build passed and staged the same resources; an explicit test run with
  `detect_leaks=0` passed 102,754 assertions.
- Byte-for-byte comparisons confirmed that both debug and ASan staged resources match their source files, and the
  staged AppStream file passed `appstreamcli validate --no-net`.

## Stage 8: Packaging and deferred features

Completed 2026-08-16 for the initial portable-reader scope.

- Made every Linux configuration produce a versioned, architecture-specific `.tar.gz` archive after all build and
  test targets pass. Debug and ASan names retain their configuration suffix; the release artifact is
  `out/linux-rel64/SumatraPDF-3.7-linux-x64.tar.gz`.
- Packaged the application under `bin/`, desktop/AppStream/icon resources under the standard `share/` hierarchy, and
  included the project license and Linux-specific run/install instructions.
- Assembled archives on the native Linux temporary filesystem so files extracted from Windows-hosted WSL builds have
  normal `0755` executable/directory and `0644` data-file permissions.
- Normalized archive entry ordering, timestamps, ownership, and group metadata for reproducible packaging inputs.
- Kept GTK and other platform libraries as system dependencies and documented that boundary. Debian, RPM, and Flatpak
  packages remain deliberately deferred until the application and its supported-format policy stabilize, as planned.
- The resident/autostart mode, privileged self-updater, automatic OCR/translation installation, and forced renderer
  selection remain outside the initial reader-port scope.

Verification:

- `bun cmd/build.ts -debug`: passed with 0 warnings and 0 errors.
- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,902 assertions and produced the debug archive.
- `bun cmd/build.ts -linux -release`: passed; an explicit release `test_util` run passed 102,674 assertions and produced
  the unsuffixed release archive.
- `bun cmd/build.ts -linux`: the ASan build passed; an explicit run with `detect_leaks=0` passed 102,758 assertions and
  produced the ASan archive.
- Extracted the release archive into a fresh WSL temporary directory, checked its file modes and packaged binary byte
  for byte, validated its AppStream metadata, and opened `ext/a-zlib/zlib.3.pdf` from the packaged executable under
  WSLg until the expected timeout. Only the existing non-fatal Mesa renderer warnings appeared.
