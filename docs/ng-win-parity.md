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

| Difference                                                                                         | Commit       | Test                              |
| -------------------------------------------------------------------------------------------------- | ------------ | --------------------------------- |
| Page context menu rows (Select All twice, selection items, Show Thumbnails, Create From Selection) | `513a604bb5` | `context-menu-parity`             |
| Main menu: Read Aloud on home, Show Whole Pages / Read Columns whole page enabled                  | `00649d094e` | `main-menu-parity`                |
| Page info tip said 9 / 10 on the last page                                                         | `abef90d4c2` | `issue-6492`                      |
| Crash when DDE `[CmdExit]` arrives during session restore                                          | `27e4cbe219` | `restore-session-close-reentry`   |
| Convert to PDF hidden / refused for OFD                                                            | `3c9a3c8d46` | `issue-6505`                      |
| Theme = System, high contrast, `WM_SYSCOLORCHANGE` not followed at runtime                         | `976b9e4e4a` | manual (needs an OS theme switch) |
| No save / close on logoff (`WM_QUERYENDSESSION`, `WM_ENDSESSION`)                                  | `335aeeb79f` | `end-session-saves`               |
| `-quicklook` not handed to the running instance; `-plugin` / `-stress-test` could be               | `ca1a88f08f` | `quicklook-forward`               |
| DDE `[GetMousePos]` unanswered (now shared code)                                                   | `2d8a989335` | `dde-get-mouse-pos`               |
| Copy put only text on the clipboard                                                                | `62920a2ba9` | `copy-rect-selection-image`       |
| Stale cbx / open-cache files, WebView2 profiles never deleted (now shared)                         | `790411953f` | `stale-cache-cleanup`             |
| Markdown / CHM pages not reloaded for new page colors                                              | `34cbb05d6e` | `browser-doc-theme`               |
| Replace Image, Edit Image in Another App hidden (now `src/shared/ImageReplace.cpp`)                | `7ea015fa66` | `replace-image`                   |
| Changing `UseTabs` didn't convert open windows                                                     | `1c53df475a` | `use-tabs-transition`             |
| SumatraPDF Web dialog was a stub (logic now `src/shared/SumatraPDFWebCommon.cpp`)                  | `38b527d8a8` | `sumatrapdf-web-dialog`           |
| Documents loaded on the UI thread (now a Loading tab, like orig)                                   | `e76b587c3f` | `open-slow-drive`                 |
| No signatures sidebar, status bar, dialog, field marks (now `src/shared/SignatureUICommon.cpp`)    | `8a84338b44` | `sig-validation`                  |
| Copy then Paste of an annotation pasted an older clipboard image                                   | `71101165c8` | `annotation clipboard tests`      |
| Dropping a dragged tab on another window tore it off into a new one                                | `f3b3ef9486` | `tab-drag-to-window`              |
| No touch long-press text selection or drag handles                                                 | `0bbff21d97` | `touch-long-press-select`         |
| No UI Automation text for screen readers                                                           | `67b91fa2ba` | `uia-document-text`               |
| ng-only `cli markdown` missed slightly lowered subscripts                                          | `a8542f1a25` | `cli-markdown-cases`              |

The last four ng fixes needed gpui changes, vendored in `a6d7e51e2a`
(gpui-kit-cpp `d00ea240`): `WindowAtPoint`, `TouchEvent` / `El::OnTouch`
(raw touch and pen contacts on Windows) and `El::AccessibilityDocument`
(document text and selection read on demand).

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
- Screen readers: the per-page elements are Windows only. They need gpui's
  document parts (gpui-kit-cpp `2b509eab`, branch `a11y-pages`, not yet
  vendored), which only the UIA bridge exposes.
- Touch long-press is Windows only: gpui emits `TouchEvent` only there. Pen
  input still takes the mouse path in ng.
- Off Windows, SumatraPDF Web can't transfer files: the HTTP helpers it uses
  are WinHTTP only (`Http_posix.cpp` needs curl versions). Edit Image in
  Another App is Windows only.
- Async loading: command-line files, next / prev file in folder, reload and
  DDE `[Open]` still load synchronously; no network-drive copy progress on the
  Loading tab.
- Not compiled on macOS yet: gpui's `WindowAtPoint` (Cocoa) and ng's new code.
- Unverified: `WM_DISPLAYCHANGE` fullscreen re-fit, `WM_MOUSEACTIVATE` in
  presentation (depend on what GPUI handles).

## Smaller default differences

- `ThumbnailsType` defaults to full page.
- TIFF / GIF decoded by mupdf, not WIC / GDI+ (format coverage may differ).
