# ng Windows vs orig: functional differences

Found 2026-11-21 by running both debug builds side by side, then fixed in
ng. Look-and-feel differences (native vs GPUI) are out of scope.

What was compared:

- commands, settings structs, accelerators: identical (shared sources)
- main menu tree and enabled state, 8 document types (home, PDF, EPUB, CBZ,
  DjVu, CHM, PNG, MD): orig read from its live popups, ng via `TestMainMenu`
- `GetCommandVisibility` for every command × menu/palette/toolbar, same 8 docs
- page context menu at canvas center (PDF, EPUB, CBZ, PNG, CHM)
- full test suite with `-ng` (`run-github-ci.ts -ng`): 20 of 603 failed;
  re-run against orig and twice more against ng to drop env/flaky ones
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
| SumatraPDF Web dialog was a stub (logic now `src/shared/SumatraPDFWebCommon.cpp`)                  | `527e941f5f` | `sumatrapdf-web-dialog`           |
| Documents loaded on the UI thread (now a Loading tab, like orig)                                   | `8ab33131ab` | `open-slow-drive`                 |
| No signatures sidebar, status bar, dialog, field marks (now `src/shared/SignatureUICommon.cpp`)    | `c6785b376a` | `sig-validation`                  |
| ng-only `cli markdown` missed slightly lowered subscripts                                          | `a8542f1a25` | `cli-markdown-cases`              |

Failures that were the test, not ng (fixed in the tests): `issue-3585`
`69a7b6f519`, `tab-menu-close` `21fcb9d5a9`, `pdf-script` `fa12c508fc`,
`cli-pdf-tools` `f5b40637d2`, `cli-markdown-*` `fda5c311a2`.

## Not differences after investigation

- Favorites menu enabled state: orig's `MenuUpdateStateForWindow` re-enables
  what the shared `RebuildFavMenu` disabled. ng's state is the intended one.
- `issue-6241`: both apps cap Fit Visible at 800% in a large window; the small
  Fit Width gap is the overlay scrollbar taking no width.
- `issue-6561`: failed only right after a failing `sig-validation`; passes now.
- `home-tip-double-click`, `sidebar-thumbnails` repaint: in a locked session
  new GPUI frames don't reach PrintWindow captures. The log shows ng renders
  the new tip.

## Remaining, by design or blocked

- `Scrollbars = windows` maps to smart (`issue-5850`, `issue-6028`): ng uses
  no native toolkit UI.
- Explorer preview / thumbnail handlers and search filter: not ported (out of
  scope for now); `-with-filter/-with-preview/-with-thumbnails` ignored.
- Installer `-help` text, store-installer and installer-by-exe-name modes: ng's
  Windows build is for testing, not distributed.
- `-html-backend` / IE fallback: ng renders CHM / markdown with WebView2 only.
- Debug-only flags `-unit-tests`, `-test-render-page`, `-test-plugin` (ng has
  `test_util`).
- GPUI gaps (need `../gpui-kit-cpp` changes):
  - touch long-press selection and drag handles: gpui's Windows layer never
    emits `LongPressEvent` and exposes no `WM_POINTER*`
  - dropping a tab on another window's tab strip (mouse capture stays with the
    source window)
  - UI Automation provider for document text (screen readers)
- Off Windows, SumatraPDF Web can't transfer files: the HTTP helpers it uses
  are WinHTTP only (`Http_posix.cpp` needs curl versions).
- Async loading: command-line files, next / prev file in folder, reload and
  DDE `[Open]` still load synchronously; no network-drive copy progress on the
  Loading tab.
- Unverified: `WM_DISPLAYCHANGE` fullscreen re-fit, `WM_MOUSEACTIVATE` in
  presentation, pen `WM_POINTER` (depend on what GPUI handles).

## Smaller default differences

- `ThumbnailsType` defaults to full page.
- TIFF / GIF decoded by mupdf, not WIC / GDI+ (format coverage may differ).
