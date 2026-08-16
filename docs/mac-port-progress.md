# macOS Cocoa port progress

This file tracks implementation of [mac-port-plan.md](mac-port-plan.md). Remote verification uses temporary branches;
those commits are verification artifacts rather than authorization for final feature commits.

## Status

| Stage | Status      | Summary                                           |
| ----- | ----------- | ------------------------------------------------- |
| 1     | Complete    | Cocoa application bundle and file opening         |
| 2     | Complete    | Shared portable reader model                      |
| 3     | Complete    | Scrollable multi-page Cocoa document viewer       |
| 4     | Complete    | Portable asynchronous rendering and bounded cache |
| 5     | In progress | Native shell, toolbar, menus, and commands        |
| 6     | In progress | Find, TOC, and properties are exposed in Cocoa    |
| 7     | In progress | Finder integration and clean shutdown are present |
| 8     | Not started | Versioned application archive                     |

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
- Visible pages render through the portable asynchronous `PageRenderService`. Its bounded cache replaces the old
  unbounded Cocoa image cache, and nearby pages are prefetched at lower priorities.
- Shared `TextSearch` drives Find, Find Next, and Find Previous, including wrapped search and highlighted result
  rectangles. Shared engine TOC and property models drive native Table of Contents and Document Properties dialogs.
- The application still owns a single document, and several menu entries remain disabled placeholders. Text
  selection, links, favorites/history, and the command palette are not exposed in the Cocoa reader yet.

## Verification

- Windows debug build: passed on 2026-08-16.
- Linux/WSL debug build: passed on 2026-08-16, including 102,889 `test_util` assertions.
- Remote macOS debug build: passed on 2026-08-16, including 102,892 `test_util` assertions, 23 portable compile checks,
  `test_engines`, and the 35-source `SumatraPDF.app` link.

The asynchronous-rendering baseline was verified from temporary branch `tmp/mac-port-parity-20260816`. Reader-feature
verification continues from the same temporary branch as each slice lands.
