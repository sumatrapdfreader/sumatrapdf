# macOS Cocoa port

## Background

The macOS application is a small native Cocoa reader under `src/mac/`. It is not a translation of the Win32 window
hierarchy and must not emulate Win32. It should reach feature parity with the initial Linux GTK4 reader by sharing the
same portable engines, document layout, rendering policy, search, selection, persisted state, and command models.

The Cocoa source cannot include `base/Base.h` or other Sumatra headers because Apple headers define conflicting names
such as `Size`. Plain C bridge types in `SumatraMacEngine.*` keep AppKit isolated from the portable C++ implementation.

## Architecture

| Location                  | Responsibility                                                                      |
| ------------------------- | ----------------------------------------------------------------------------------- |
| `src/mac/`                | Cocoa application shell, windows, tabs, native panels, printing, and macOS services |
| `src/gui/`                | Toolkit-neutral drawing, layout, command palette, and document-view behavior        |
| `src/gui/mac/`            | Cocoa/CoreGraphics implementations of portable GUI interfaces                       |
| `src/`                    | Portable reader model, render queue, search, selection, TOC, and persisted state    |
| `cmd/helper/mac-build.ts` | macOS application build, bundle assembly, and focused tests                         |
| `packaging/mac/`          | Distribution notes and future signing/notarization inputs                           |

AppKit types must not leak into shared headers. Portable logic stays in unsuffixed source files; Cocoa and Darwin-only
code stays in `src/mac/`, `src/gui/mac/`, or `_mac` implementation files. Prefer `_posix` for code shared with Linux.

## Source-porting policy

- Reuse `ReaderModel`, `DocumentLayout`, `PageRenderService`, `TextSearch`, `TextSelection`, `TocTree`, `DocProp`,
  `FileState`, `GlobalPrefs`, and the portable command-palette model.
- Keep the Objective-C++ layer focused on native presentation and lifecycle. Extend the plain bridge instead of
  including Sumatra headers in a Cocoa translation unit.
- Preserve every format supported by the portable engine stack; PDF is not a special-case document implementation.
- Put reusable reader behavior in `src/` or `src/gui/`, not a second macOS-only model.
- Keep the Win32 application green while moving shared logic.
- Use native Cocoa conventions: application menus, sheets, `NSOutlineView`, `NSTabViewController`, `NSPrintOperation`,
  `NSPasteboard`, `NSWorkspace`, and standard keyboard equivalents.

## Implementation stages

### 1. macOS application target

Build `SumatraPDF.app` through `bun cmd/build.ts -mac`, accept command-line and Finder-opened documents, and show a
native `NSWindow`. Keep app startup and Cocoa ownership under `src/mac/`.

### 2. Shared portable reader model

Use the same `ReaderModel` and engine-selection stack as Linux. The bridge exposes page metadata and layout without
leaking C++ or AppKit types across the boundary.

### 3. Native Cocoa document viewer

Display the shared `DocumentLayout` inside an `NSScrollView` with:

- all portable engine formats;
- continuous and single-page layouts;
- fit-page, fit-width, and actual-size zoom;
- page navigation and rotation;
- native scrolling, resize handling, and Retina backing-scale updates.

### 4. Portable asynchronous rendering

Use `PageRenderService` behind the bridge, retaining its worker-local engine clone, cancellation generations,
visible/nearby priorities, stale-result rejection, and 96 MB bounded LRU cache. Deliver completion notifications on
the Cocoa main queue and never render synchronously from scrolling or drawing callbacks.

### 5. Application shell and commands

Add native document tabs, reopen-closed-tab history, file panels, fullscreen/presentation modes, toolbar state, and
command dispatch. Reuse the portable keyboard-help and command-palette models where they fit native Cocoa UI.

### 6. Reader features

Implement in the same dependency order as Linux:

1. links and cursor hit testing through `EngineBase::GetElements()`;
2. text selection through `TextSelection` and `NSPasteboard` copying;
3. find through `TextSearch`;
4. TOC through `TocTree` and `NSOutlineView`;
5. properties through `DocProp`;
6. favorites, recents, and per-document state through `FileState` and `GlobalPrefs`;
7. command palette through the shared portable collection/filter model.

Use SumatraPDF's existing settings serialization with a macOS application-support path. Do not introduce a separate
preferences schema.

### 7. macOS services

After the reader is stable, add:

- `src/base/FileWatcher_mac.cpp` behind the existing `FileWatcher` API;
- native printing through `NSPrintOperation`;
- clipboard and Finder reveal helpers;
- session restore and deterministic clean shutdown;
- document-type declarations and application metadata in the bundle.

### 8. Packaging and deferred features

Produce a versioned archive containing `SumatraPDF.app`. Keep signing, notarization, Sparkle/self-update integration,
Mac App Store sandboxing, and disk-image styling separate until distribution identity and security policy are chosen.

The following Windows features remain outside the initial native-reader scope:

- registry installer, shell preview/filter handlers, and COM/UIA integration;
- DDE, plugin embedding, and Win32 crash minidumps;
- SyncTeX integration until macOS TeX workflows are explicitly targeted;
- privileged or automatic updater/install flows.

## Verification

Every stage must keep the relevant targets green:

- Windows build: `bun cmd/build.ts -debug`
- Portable unit tests after shared changes: `bun cmd/run-unit-tests.ts -dbg`
- Linux build after shared reader changes: `bun cmd/build.ts -linux -debug`
- macOS remote build: temporary branch and
  `bun cmd/build.ts -mac-remote -branch <temporary-branch> -debug`
- macOS smoke document: `ext/a-zlib/zlib.3.pdf`

Prefer pure model/rendering tests in `test_util` or `test_engines`. Record implemented behavior and exact verification
results in `docs/mac-port-progress.md`.
