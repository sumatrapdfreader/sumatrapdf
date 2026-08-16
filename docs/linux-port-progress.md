# Linux GTK4 port progress

This file tracks implementation of [linux-port.md](linux-port.md). Each completed stage is committed separately
after its relevant Windows, Linux, and portable checks pass.

## Status

| Stage | Status      | Summary                                      |
| ----- | ----------- | -------------------------------------------- |
| 1     | Complete    | GTK4 application executable and file opening |
| 2     | Complete    | Shared portable reader model                 |
| 3     | Complete    | Embeddable canvas and document viewer        |
| 4     | Not started | Portable asynchronous rendering              |
| 5     | Not started | Application shell, tabs, and commands        |
| 6     | Not started | Reader features                              |
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
