# macOS Cocoa port progress

This file tracks implementation of [mac-port-plan.md](mac-port-plan.md). Remote verification uses temporary branches;
those commits are verification artifacts rather than authorization for final feature commits.

## Status

| Stage | Status      | Summary                                           |
| ----- | ----------- | ------------------------------------------------- |
| 1     | Complete    | Cocoa application bundle and file opening         |
| 2     | Complete    | Shared portable reader model                      |
| 3     | Complete    | Scrollable multi-page Cocoa document viewer       |
| 4     | Not started | Portable asynchronous rendering                   |
| 5     | In progress | Native shell, toolbar, menus, and commands        |
| 6     | Not started | Reader features                                   |
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
- Rendering remains synchronous on the main thread and cached without a fixed memory bound. The application still
  owns a single document, and several menu entries are disabled placeholders.
- `TextSelection.cpp` and `TextSearch.cpp` compile in the macOS application and engine tests, but are not exposed in
  the Cocoa reader yet.

Previously verified by the macOS build as the relevant functionality was introduced; the next implementation slice
will establish a fresh remote-build baseline against this reconciled plan.
