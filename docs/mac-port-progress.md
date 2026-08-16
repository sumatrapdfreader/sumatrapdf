# macOS Cocoa port progress

This file tracks implementation of [mac-port-plan.md](mac-port-plan.md). Remote verification uses temporary branches;
those commits are verification artifacts rather than authorization for final feature commits.

## Status

| Stage | Status      | Summary                                            |
| ----- | ----------- | -------------------------------------------------- |
| 1     | Complete    | Cocoa application bundle and file opening          |
| 2     | Complete    | Shared portable reader model                       |
| 3     | Complete    | Scrollable multi-page Cocoa document viewer        |
| 4     | Complete    | Portable asynchronous rendering and bounded cache  |
| 5     | Complete    | Native tabs, shell, toolbar, menus, and commands   |
| 6     | In progress | Core portable reader interactions are exposed      |
| 7     | In progress | Reload, print, Finder, session, bundle integration |
| 8     | Complete    | Versioned portable application archive             |

## Existing baseline

Reconciled 2026-08-16.

- `SumatraPDF.app` is built by the unified macOS build and accepts both command-line paths and Finder open events.
- `SumatraMacEngine` uses the shared `ReaderModel`, preserving the portable engine set rather than calling MuPDF
  directly.
- The Cocoa viewer uses shared `DocumentLayout` for continuous and single-page modes, renders all visible pages,
  supports scrolling, fit-page/fit-width/actual-size zoom, page navigation, rotation, resize relayout, and Retina
  backing-scale changes.
- A native configurable toolbar, application menus, open/go-to panels, fullscreen, Show in Finder, and the portable
  keyboard-help dialog are present.
- A native toolbar tab selector keeps independent reader/render-service state per document, restores page/zoom/layout
  and scroll position when switching, switches to already-open files, supports next/previous tab commands, and keeps a
  bounded reopen-closed stack.
- Visible pages render through the portable asynchronous `PageRenderService`. Its bounded cache replaces the old
  unbounded Cocoa image cache, and nearby pages are prefetched at lower priorities.
- Shared `TextSearch` drives Find, Find Next, and Find Previous, including wrapped search and highlighted result
  rectangles. Shared engine TOC and property models drive native Table of Contents and Document Properties dialogs.
- Shared `TextSelection` drives mouse selection, Select All, and UTF-8 clipboard copy. Engine page-element hit testing
  drives internal-page, URL, and file links.
- The Cocoa command chooser filters the supported native action set through shared `CommandPaletteModel`; unsupported
  platform commands are not offered.
- Per-file view state, recent documents, favorites, and bare-launch session restore use the existing
  `GlobalPrefs`/`FileState` serialization in Application Support, keeping settings compatible with the portable
  preference model.
- The application still owns a single document, and several menu entries remain disabled placeholders. Text
  selection refinements remain incomplete.
- The application bundle advertises its supported document extensions to Finder. Every successful macOS build emits
  a versioned `.tar.gz` containing `SumatraPDF.app`, the license, and macOS installation notes; signing and
  notarization remain deferred release-infrastructure work.
- Native `NSPrintOperation` prints the document through the shared engine render path with Cocoa page ranges and the
  current rotation.
- A native vnode dispatch source reloads the active document after writes or atomic replacements while preserving its
  saved view state.

## Verification

- Windows debug build: passed on 2026-08-16.
- Linux/WSL debug build: passed on 2026-08-16, including 102,889 `test_util` assertions.
- Remote macOS debug build: passed on 2026-08-16, including 102,892 `test_util` assertions, 23 portable compile checks,
  `test_engines`, and the 35-source `SumatraPDF.app` link.

The asynchronous-rendering baseline was verified from temporary branch `tmp/mac-port-parity-20260816`. Reader-feature
verification continues from the same temporary branch as each slice lands.
