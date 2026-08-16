# Linux GTK4 port

## Background

The Linux work in [casimir-engineering/shenzhen-pdf](https://github.com/casimir-engineering/shenzhen-pdf)
is a useful behavior and packaging reference, but it cannot be merged wholesale. That fork diverged at
`e69876905` and added its application under `portable/linux/gtk4`: about 21,000 lines of production C plus a
separate direct-MuPDF C API, independent layout, rendering, search, selection, settings, and annotation stacks.

SumatraPDF already has portable C++ engines and an expanding toolkit-neutral UI layer. The Linux application
must use those rather than create a second document implementation.

## Architecture

| Location                    | Responsibility                                                                       |
| --------------------------- | ------------------------------------------------------------------------------------ |
| `src/linux/`                | GTK application shell, windows, tabs, native dialogs, printing, and desktop services |
| `src/gui/`                  | Toolkit-neutral canvases, input, drawing, and reusable document-view UI              |
| `src/gui/gtk4/`             | GTK4 implementations of portable GUI interfaces                                      |
| `src/`                      | Portable reader model, rendering queue, search, selection, and persisted state       |
| `cmd/helper/linux-build.ts` | Linux application build and focused tests                                            |
| `packaging/linux/`          | Desktop files, AppStream metadata, icons, and distro packaging                       |

GTK types must not leak from `src/linux/` into shared headers. Platform-neutral code stays in unsuffixed files;
Linux and GTK-specific code stays in `src/linux/` or `src/gui/gtk4/`.

## Source-porting policy

- Do not import the fork's `portable/core/shenzhen_pdf_core.*`.
- Use `EngineBase`, `DocumentLayout`, `TextSearch`, `TextSelection`, `TocTree`, and `DocProp`.
- Rewrite the GTK shell in the project's C++ style without STL.
- Adapt proven behavior such as zoom anchoring, cancellation generations, bounded render caches, state write
  coalescing, and work-area clamping.
- Keep direct copies attributable to their original fork commit.
- Keep GTK-native shell widgets in `src/linux`; put reusable canvas-style UI behind interfaces in `src/gui`.
- Start with GTK4 alone. Consider a required libadwaita dependency only after the basic shell is working and its
  tab or adaptive-window features provide a clear benefit.

## Implementation stages

### 1. Linux application target

Add a real GTK4 application target alongside the existing keyboard-help executable.

Initial files:

- `src/linux/SumatraLinux.cpp`
- `src/linux/LinuxApp.cpp`
- `src/linux/LinuxWindow.cpp`
- `src/linux/LinuxTab.cpp`
- `src/linux/LinuxDialogs.cpp`

The first target opens an empty `GtkApplicationWindow`, accepts document paths through `GtkApplication`, and is
built as `out/linux-<config>64/SumatraPDF` by `bun cmd/build.ts -linux`.

### 2. Shared portable reader model

Move the portable engine and layout work currently hidden behind `SumatraMacEngine.cpp` into a shared C++ model.
It owns an `EngineBase`, page geometry, `DocumentLayout`, current page, display mode, zoom, rotation, and initial
synchronous rendering. Keep the Objective-C bridge thin so Cocoa files still do not include conflicting Sumatra
headers.

The model must retain all currently supported engine types rather than becoming PDF-only.

### 3. Embeddable portable canvas

Add `PlatformCanvas` under `src/gui`, with a GTK4 implementation under `src/gui/gtk4`. Unlike `PlatformWindow`, a
canvas is a child surface that can be embedded beside a native toolbar, tabs, and sidebars.

Build the portable document canvas on `PlatformCanvas`, `Gfx`, the reader model, and `DocumentLayout`. Extend input
events incrementally with scrolling, named keys, dragging, and focus as required.

The first viewer milestone supports:

- opening every format supported by the portable engines;
- continuous and single-page layouts;
- fit-page, fit-width, and actual-size zoom;
- page navigation and rotation;
- wheel scrolling and cursor-anchored zoom.

### 4. Portable asynchronous rendering

Either split the portable scheduling and cache logic from `RenderCache.cpp` or add a smaller shared page-render
service using `EngineBase::Clone`, `AbortCookie`, base threading primitives, and `PlatformPostTask`.

Preserve these policies from the reference port:

- visible, nearby, and background priorities;
- cancellation generations and stale-result rejection;
- worker-local engine clones;
- bounded memory, initially 96 MB;
- main-thread completion delivery.

Add pure unit tests for request replacement, LRU eviction, zoom anchoring, cancellation, and shutdown.

### 5. Application shell and commands

Add the native menu/header bar, tabs, reopen-closed-tab history, file dialogs, fullscreen, presentation mode, and
command dispatch using `Commands.cpp`. Continue using the portable `KeyboardHelp.cpp` dialog.

The fork's `spdf_app`, `spdf_window`, `spdf_tab`, and `spdf_shortcuts` modules are behavior checklists, not
source-level merge candidates.

### 6. Reader features

Implement in dependency order:

1. links and cursor hit testing through `EngineBase::GetElements()`;
2. text selection through `TextSelection`;
3. find through `TextSearch`;
4. TOC through `TocTree`;
5. properties through `DocProp`;
6. favorites, recents, and per-document state through existing `FileState` and `GlobalPrefs`;
7. command palette by separating portable filtering and collection from Windows presentation.

Port SumatraPDF's settings serialization and POSIX configuration-path handling. Do not adopt the fork's separate
JSON schema.

### 7. Linux services

After the core viewer is stable, add:

- `src/base/FileWatcher_linux.cpp`, preferably using inotify behind the existing `FileWatcher` API;
- `src/linux/LinuxPrint.cpp` using `GtkPrintOperation`;
- clipboard and show-in-folder helpers;
- XDG default-reader registration;
- session restore and clean shutdown;
- desktop and AppStream resources.

### 8. Packaging and deferred features

Start with a portable tarball and desktop/AppStream files. Add deb, RPM, and Flatpak packaging separately after the
application stabilizes.

The following reference-port features require separate product and security decisions and are not part of the
initial reader port:

- resident/autostart mode;
- the minisign and `pkexec` self-updater;
- automatic OCR package installation;
- automatic Argos Translate installation;
- forced GSK renderer selection.

## Verification

Every stage must keep the relevant targets green:

- Windows build: `bun cmd/build.ts -debug`
- Portable unit tests: `bun cmd/run-unit-tests.ts -dbg`
- Linux build: `bun cmd/build.ts -linux -debug`
- macOS after shared model or GUI changes: temporary branch and `bun cmd/build.ts -mac-remote -branch <branch> -debug`
- Linux smoke document: `ext/a-zlib/zlib.3.pdf`

Prefer pure model, layout, and rendering tests in `test_util` or `test_engines` over display-dependent GTK tests.
Record completed work and verification results in `docs/linux-port-progress.md`.
