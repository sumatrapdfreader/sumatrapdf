# ng Windows vs orig: functional differences

Found 2026-11-21 by running both debug builds side by side, then fixed in
ng. Look-and-feel differences (native vs GPUI) are out of scope.

What was compared:

- commands, settings structs, accelerators: identical (shared sources)
- main menu tree and enabled state, 8 document types (home, PDF, EPUB, CBZ,
  DjVu, CHM, PNG, MD): orig read from its live popups, ng via `TestMainMenu`
- `GetCommandVisibility` for every command × menu/palette/toolbar, same 8 docs
- page context menu at canvas center (PDF, EPUB, CBZ, PNG, CHM)
- full test suite with `-ng` (`run-github-ci.ts -ng`): 20 of 603 failed at
  first, 9 of 615 after the fixes below, 0 after the test fixes
- code audit of ng for stubbed or never-ported orig behavior

## Fixed

| Difference                                                                                           | Commit                    | Test                              |
| ---------------------------------------------------------------------------------------------------- | ------------------------- | --------------------------------- |
| Page context menu rows (Select All twice, selection items, Show Thumbnails, Create From Selection)   | `513a604bb5`              | `context-menu-parity`             |
| Main menu: Read Aloud on home, Show Whole Pages / Read Columns whole page enabled                    | `00649d094e`              | `main-menu-parity`                |
| Page info tip said 9 / 10 on the last page                                                           | `abef90d4c2`              | `issue-6492`                      |
| Crash when DDE `[CmdExit]` arrives during session restore                                            | `27e4cbe219`              | `restore-session-close-reentry`   |
| Convert to PDF hidden / refused for OFD                                                              | `3c9a3c8d46`              | `issue-6505`                      |
| Theme = System, high contrast, `WM_SYSCOLORCHANGE` not followed at runtime                           | `976b9e4e4a`              | manual (needs an OS theme switch) |
| No save / close on logoff (`WM_QUERYENDSESSION`, `WM_ENDSESSION`)                                    | `335aeeb79f`              | `end-session-saves`               |
| `-quicklook` not handed to the running instance; `-plugin` / `-stress-test` could be                 | `ca1a88f08f`              | `quicklook-forward`               |
| DDE `[GetMousePos]` unanswered (now shared code)                                                     | `2d8a989335`              | `dde-get-mouse-pos`               |
| Copy put only text on the clipboard                                                                  | `62920a2ba9`              | `copy-rect-selection-image`       |
| Stale cbx / open-cache files, WebView2 profiles never deleted (now shared)                           | `790411953f`              | `stale-cache-cleanup`             |
| Markdown / CHM pages not reloaded for new page colors                                                | `34cbb05d6e`              | `browser-doc-theme`               |
| Replace Image, Edit Image in Another App hidden (now `src/shared/ImageReplace.cpp`)                  | `7ea015fa66`              | `replace-image`                   |
| Changing `UseTabs` didn't convert open windows                                                       | `1c53df475a`              | `use-tabs-transition`             |
| SumatraPDF Web dialog was a stub (logic now `src/shared/SumatraPDFWebCommon.cpp`)                    | `38b527d8a8`              | `sumatrapdf-web-dialog`           |
| Documents loaded on the UI thread (now a Loading tab, like orig)                                     | `e76b587c3f`              | `open-slow-drive`                 |
| No signatures sidebar, status bar, dialog, field marks (now `src/shared/SignatureUICommon.cpp`)      | `8a84338b44`              | `sig-validation`                  |
| Copy then Paste of an annotation pasted an older clipboard image                                     | `71101165c8`              | `annotation clipboard tests`      |
| Dropping a dragged tab on another window tore it off into a new one                                  | `f3b3ef9486`              | `tab-drag-to-window`              |
| No touch long-press text selection or drag handles                                                   | `0bbff21d97`              | `touch-long-press-select`         |
| No UI Automation text for screen readers                                                             | `67b91fa2ba`              | `uia-document-text`               |
| Screen readers saw no page elements or Start Page                                                    | `f1e3b79150`              | `uia-document-text`               |
| Pen input took the mouse path: no pen drag selection, barrel drag was a right click, eraser drew ink | `756d278035`              | `pen-input`                       |
| Next / prev file in folder loaded on the UI thread                                                   | `9208051ae8`              | `open-slow-drive`                 |
| No copy progress on the Loading tab for a comic on a network drive                                   | `39281021c1`              | `open-slow-drive`                 |
| No elapsed time on the Loading message ("Loading x ... 3s")                                          | `a593313927`              | `open-slow-drive`                 |
| Fullscreen / presentation kept the old size after a display change                                   | `e40f947d91`              | `fullscreen-display-change`       |
| The click activating a presentation also turned the page                                             | `4b964859e4`              | `presentation-activating-click`   |
| An RDP connect changing the DPI without WM_DPICHANGED (#4581) kept the old scale                     | (this commit)             | `dpi-display-change`              |
| Edit Image in Another App, Replace Image Windows only                                                | `c7b93dcfea` `e09072172a` | `replace-image` (Windows)         |
| SumatraPDF Web couldn't transfer files off Windows (now libcurl on posix)                            | `7e3b6bcad2` `dacf000946` | `ad-hoc-http-transfer`            |
| ng-only `cli markdown` missed slightly lowered subscripts                                            | `a8542f1a25`              | `cli-markdown-cases`              |

Fixes that needed gpui changes: tab drop, touch long-press, screen readers
(`a6d7e51e2a`, gpui-kit-cpp `d00ea240`: `WindowAtPoint`, `TouchEvent` /
`El::OnTouch`, `El::AccessibilityDocument`) and page elements, pen input
(`7b32a3964e`, gpui-kit-cpp `e8eed752`: accessibility document parts,
`TouchEvent::eraser`, kept pen contacts no longer become mouse input).

Failures that were the test, not ng (fixed in the tests): `issue-3585`
`69a7b6f519`, `tab-menu-close` `21fcb9d5a9`, `pdf-script` `fa12c508fc`,
`cli-pdf-tools` `f5b40637d2`, `cli-markdown-*` `fda5c311a2`, `issue-6080`
`4b37137a85`, `trim-empty-margins-restore` `efba7ef431`, `issue-6346`
`a1a7e8a213` (its probe, in shared code), `free-text-edit-matches-render`
`cdf403f737`, `issue-6241` `f01009c3c8`, `issue-5850` / `issue-6028` skip on ng
`153c7202d1`, `toolbar-hover-dropdown` `0fea76adb6` `84b387eeaa`.

## Not differences after investigation

- Favorites menu enabled state: orig's `MenuUpdateStateForWindow` re-enables
  what the shared `RebuildFavMenu` disabled. ng's state is the intended one.
- `issue-6561`: failed only right after a failing `sig-validation`.
- `home-tip-double-click`, `sidebar-thumbnails` repaint: in a locked session
  new GPUI frames don't reach PrintWindow captures. The log shows ng renders
  the new tip.

## Remaining, by design or not ported

- `Scrollbars = windows` reads as smart: ng dropped the mode (`6e9de392d8`).
- Explorer preview / thumbnail handlers and search filter: not ported (out of
  scope for now); `-with-filter/-with-preview/-with-thumbnails` ignored.
- Installer `-help` text, store-installer and installer-by-exe-name modes: ng's
  Windows build is for testing, not distributed.
- `-html-backend` / IE fallback: ng renders CHM / markdown with WebView2 only.
- Debug-only flags `-unit-tests`, `-test-render-page`, `-test-plugin` (ng has
  `test_util`).
- Screen readers off Windows: the page elements come from gpui's document
  parts, which only the UIA bridge exposes (not AT-SPI or AppKit yet).
- Touch long-press and pen handling are Windows only: gpui emits `TouchEvent`
  only there. Orig's pen barrel button is a left drag, so neither app has a pen
  context menu.
- Command-line files, reload and DDE `[Open]` load on the UI thread in both apps.
- SumatraPDF Web on Wasm: no browser-fetch path, transfers report failure.
- Not compiled on macOS yet: gpui's `WindowAtPoint` (Cocoa) and ng's new code,
  including the libcurl transfers and Edit Image's `open` launch. Linux builds
  under WSL; its transfers are tested in `test_util`, not through the dialog.

## Smaller default differences

- `ThumbnailsType` defaults to full page.
- TIFF / GIF decoded by mupdf, not WIC / GDI+ (format coverage may differ).
