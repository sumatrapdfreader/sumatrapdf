# Wasm build in the browser — implementation plan

Goal: the wasm build of SumatraPDF ng should behave like a web app. It shouldn't
lose data the way a page can, offer desktop features that do nothing, or fight
the browser over keys.

Base: `ed228d347`. Findings are marked by how they were checked:

- **Reproduced**: seen happening in headless Chrome.
- **Probed**: from running the command in headless Chrome, see "How this was checked".
- **Code**: read in the code, not run.
- **Suspected**: not confirmed.

## Status

Done: phases 1-4, 5.1, 5.3, 7.1 and 7.2, checked by `tests/ad-hoc-wasm-host.ts`
and `tests/ad-hoc-wasm-commands.ts`. Where the work differs from the plan:

- 2.2: the page asks the app about every key gpui leaves with the browser
  (`sumatra_wasm_key_wanted`); the browser's keys are one list in C++
  (`IsBrowserReservedShortcut`), which also keeps them out of menus and help.
- 3.3: desktop-only settings are a list in `cmd/ng-gen-settings.ts`, not a tag
  on each field.
- 4.5: POST-VIA-BROWSER works: the form opens in a new tab from a blob URL.
  `Customize-search-translation-services.md` isn't in this repo, so the CORS
  limit of plain POST is only in the error message.
- 5.1: a document's bytes are read when it's first opened (a lazy
  `node.contents`), so opening stays synchronous. They aren't dropped when its
  last tab closes.
- 7.2: there's no notification when a font download fails.
- 5.2, search and thumbnails: done, checked by `tests/ad-hoc-wasm-slices.ts`
  (the page doesn't stall more than about 100 ms on a 2000-page PDF). Before
  this, search froze the page for good: the inline "thread" waited for its own
  handle. `TextSearch` stops between pages once a slice's time is used up
  (`sliceMs`) and `Resume()` carries on. The find and count jobs get a handle
  of their own (`NewSlicedJobHandle`), so a slice can tell its job was aborted.
  The thumbnail workers render pages until the slice is used up. Wakes for
  `uitask::Post` are coalesced (`WakeForUiTasks`): with a drain queued per
  post, one frame ran slice after slice.

Open: 5.2 for ebook layout and the PDF tools (no "working" overlay yet),
phase 6.

Already done before this plan: a second window no longer crashes (`ed228d347`). Fonts download on
any origin thanks to a CORS rule on the R2 bucket `files`. That rule exists only
in the Cloudflare dashboard, so step 7.2 adds a check for it.

## Decisions

These are recommendations. Each is cheap to change before its phase starts.

1. **Several browser tabs stay allowed.** Documents must survive that; settings
   stay last-writer-wins. A single-tab lock is simpler but breaks opening two
   documents side by side, and the site's home page writes to the same storage.
2. **Slice long work instead of adding threads.** Wasm threads need
   `SharedArrayBuffer`, so COOP/COEP headers on the page. We control those on our
   site, not on other sites hosting the wasm.
3. **Hide rather than stub.** A command with no browser meaning goes away from
   menus and the palette, rather than showing "not supported".
4. **Fix keys in `WasmShell.js`, not gpui.** `ext/gpui` is generated from
   gpui-kit-cpp, so a change there has to be made upstream and vendored. The
   Ctrl+K shim shows the page can do this itself.

## Phase 1: stop losing documents

**1.1 A tab's save deletes documents another tab added.** Reproduced. Tab B
uploaded a PDF, tab A then saved, and the file was gone from OPFS. `reconcile`
removes every OPFS entry missing from this tab's MEMFS (`WasmShell.js:209-220`).
The site's `src/ngstore.ts` writes the same directory, so an upload from the home
page while a viewer tab is open should hit this too (Code).

- Delete an OPFS entry only if this tab knew about it (`stamp` has it) and it is
  now gone from MEMFS. An entry the tab never saw belongs to someone else.
- Wrap `saveTrees` in `navigator.locks.request("sumatra-opfs", ...)`, and take the
  same lock in `ngstore.ts` writes.
- Test: the two-tab sequence above, added to `tests/ad-hoc-wasm-host.ts`.

**1.2 A failed load can wipe storage.** Code. The app starts even when
`sumatraStorageLoad` fails (`WasmShell.js:326-330`), and the next save sweeps
whatever didn't load. 1.1 mostly fixes this, because `stamp` stays empty after a
failed load. Also skip saving after a failed load and tell the user.

**1.3 Uploading a same-named file overwrites silently.** Code.
`WasmBridge_wasm.cpp` writes `/uploads/<name>` without checking. Different bytes
should get a new name (`report (1).pdf`); identical bytes should reopen the file.

**1.4 Closing the tab drops pending work.** Code. There is no `beforeunload`,
`pagehide` or `visibilitychange` handling, saves run on a 500 ms timer
(`WasmBridge_wasm.cpp:123`), and unsaved annotations get no prompt.

- Export `sumatra_wasm_flush()` to save settings and the file state, and call it on
  `visibilitychange` to hidden. OPFS writes are async and can't be relied on to
  finish during `pagehide`.
- Export `sumatra_wasm_has_unsaved()` and use it for a `beforeunload` prompt.
- Suspected: scroll position and in-place annotation saves wait for the next
  settings save. Test both after this change.

**1.5 Storage failures are invisible.** Code. A failed OPFS write only reaches the
console and the document opens anyway (`WasmBridge_wasm.cpp:45-49`). Show a
notification that the file isn't saved. Call `navigator.storage.persist()` after
the first upload so the browser is less likely to evict documents.

## Phase 2: normal browser input

**2.1 Drag and drop doesn't work.** Reproduced. Dropping `bookmarks.pdf` shows
"Error loading /bookmarks.pdf". gpui keeps only the file names
(`gpui.cpp:250562`) and the app opens them as paths (`SumatraPDF.cpp:1955`).

- In `WasmShell.js`, handle `drop` on the canvas in the capture phase, stop it
  reaching gpui, read each `File`, store it through the same path as `pickFile`
  and open it. Keep gpui's `dragenter`/`dragover` handling.
- Test: `Input.dispatchDragEvent`, as used for the reproduction.

**2.2 Most shortcuts never reach the app.** Code. `BrowserKeepsIt`
(`gpui.cpp:250777`) gives the browser every Ctrl/Cmd chord except A/C/X/Y/Z, and
all of F1-F12. So Ctrl+P prints the page and Ctrl+F opens the browser's search,
not the app's.

Extend the Ctrl+K shim into a table in `WasmShell.js` that takes these chords and
runs the app's command:

| Who gets it | Keys                                                                                                        |
| ----------- | ----------------------------------------------------------------------------------------------------------- |
| App         | Ctrl+O, S, P, F, G, D, B, 0-8, plus, minus, arrows, Home, End, Shift+S, Shift+L, Shift+H; F1-F3, F7-F9, F11 |
| Browser     | F5, Ctrl+R, F12, Ctrl+Shift+I/J/C, Ctrl+L: developers and users need these                                  |
| Browser     | Ctrl+W, N, T, Shift+N, Shift+T, Tab, PgUp, PgDn; F4: browsers don't let a page take them (to confirm)       |

- The shim needs the user's custom shortcuts too, so ask the app whether a chord
  is bound (`sumatra_wasm_key_bound`) rather than hardcoding commands.
- Text fields lose Ctrl+Left, Ctrl+Backspace and similar. Include these when a
  field has focus.
- Keyboard help (`KeyboardHelpCommon.h`) should omit keys the app doesn't get.

**2.3 Exit without a host leaves a frozen canvas.** Code
(`AppShell.cpp:910-917`). Replace the canvas with a "SumatraPDF was closed" page
and a reload link. Sites with an `exit` hook are unaffected.

**2.4 Some links skip the host's `openUrl` hook.** Probed. Search selection with
Google, Bing, Wikipedia and Scholar calls `window.open` directly while Help goes
through the hook; the direct call is likely `gp::OpenUrl` at
`SumatraPDF.cpp:4536`. Route it through `LaunchBrowser`, and check that a click
still counts as a user gesture so popup blockers allow it.

## Phase 3: remove what has no browser meaning

**3.1 Commands.** Probed: each is visible on wasm today. Replace the two `if`s in
`GetCommandVisibility` with a `gHiddenOnWasm[]` list.

| Command                                                                                  | What happens today                                                   |
| ---------------------------------------------------------------------------------------- | -------------------------------------------------------------------- |
| `CmdSumatraOnline`                                                                       | File menu; says "Sumatra Online is not in this build yet"            |
| `CmdCheckUpdate`, `CmdInstallPrereleaseUpdate`                                           | Error dialog "Couldn't download update information"                  |
| `CmdSetScreenshotHotkey`                                                                 | Dialog for a global hotkey a page can't register                     |
| `CmdSetInverseSearch`, `CmdInvokeInverseSearch`, `CmdToggleInverseSearch`                | Set opens a dialog; the others do nothing. Needs an editor to launch |
| `CmdExec`, `CmdViewWithExternalViewer`                                                   | Nothing; a page can't start programs                                 |
| `CmdListPrinters`                                                                        | Nothing                                                              |
| `CmdFixDefaultApp`, `CmdCreateShortcutToFile`                                            | Nothing                                                              |
| `CmdOpenSettingsFile`                                                                    | Nothing; there's no external editor                                  |
| `CmdDebugShowCrashMiniDumps`                                                             | "No crash reports in /settings/crashinfo"                            |
| `CmdConfigureRestrictedUse`                                                              | Lists RegistryAccess, PrinterAccess and more                         |
| `CmdToggleFilePicker`, `CmdOpenFileWithSumatraFilePicker`, `CmdOpenFileWithOSFilePicker` | Duplicates of Open; there's one picker here                          |
| `CmdShowInFolder`, `CmdCopyFilePath`                                                     | Show `/uploads`, an internal path                                    |

**3.2 Update check.** Also skip the automatic check at startup
(`UpdateCheck.cpp:82`, `SumatraPDF.cpp:7456`). It runs on every page load and a
web app is always the latest version.

**3.3 Settings.** Code. Neither settings dialog has any `OS_WASM` handling.

- Options: remove "Automatically check for updates" and the inverse search row
  (`SettingsDialog.cpp:352`, `:192`).
- Advanced Settings and the palette's settings mode: hide `externalViewers`,
  `forwardSearch`, `filePicker`, `printerUI`, `printerDefaults`, `windowState`,
  `windowPos`, `customScreenDPI`, `temporaryFile*`, `reuseInstance`,
  `explorerQuickLook`, `noCaj2pdfConverterDownload`, the AI CLI paths,
  `reloadModifiedDocuments`.
- Tag them in `cmd/ng-gen-settings.ts`, so a new desktop setting has to say whether
  it applies. Keep them in the file: settings can be shared with a desktop install.

## Phase 4: stop showing the desktop's file system

**4.1 Save As.** Probed. The dialog browses `/uploads` with a `..` button, though
the result goes to the browser as a download (`SavePathDialog.cpp:161`). On wasm,
ask only for a file name. It can fall back to `/settings` (`:216-218`, Code).

**4.2 Paths in the UI.** Probed. Properties shows `/uploads/probe.pdf`. Show the
bare name on wasm. Check the Home list and the tab tooltip as well.

**4.3 "Folder" commands.** Keep `CmdOpenNextFileInFolder`, `CmdOpenPrevFileInFolder`
and `CmdNavigateFilesInFolder`, which are useful over uploads, but rename them on
wasm: "Next Document", and "Navigate Files in Folder" becomes "Documents".

**4.4 Show Logs.** Probed: nothing happens. Download the log as a file through
`WasmDownloadFile`, which also helps with bug reports.

**4.5 Selection handlers.** Code. POST handlers need CORS from third parties
(`SelectionHandlers.cpp:254`), and `POST-VIA-BROWSER` always fails because
`LaunchFileShell` rejects MEMFS paths (`Launch_wasm.cpp:27`). Show an error naming
the reason, and document the limit in `Customize-search-translation-services.md`.
Check Translate as well: the dialog opens, but sending wasn't tested.

## Phase 5: responsiveness and memory

**5.1 Every stored file is read into memory at startup.** Code. `loadDir` reads
all of OPFS, documents included, before the app starts (`WasmShell.js:154-171`).
Startup time and memory grow with the library. This is the largest single change.

- Load `/settings` and `/fonts` at startup as now.
- List `/uploads` as names and sizes. Read a document only when it's opened, and
  drop its bytes when its last tab closes.
- `file::Exists`, file sizes and Home's thumbnails need to work without the bytes.
  Check `LoadDocument` first: opening becomes async here, which needs a design of
  its own.

**5.2 Long operations freeze the tab.** Code. `StartThread` runs inline
(`Base.cpp:505-508`), and only `RenderCache` time-slices. In order of how often
people will hit them:

1. Search (`SearchAndDDECommon.cpp:1902`, `:2101`).
2. Thumbnails in the sidebar and palette (`Sidebar.cpp:1958`, `CommandPalette.cpp:420`).
3. Ebook layout (`EngineBase.cpp:774`).
4. PDF to images, print to PDF, save selection as image (`PdfToolsCommon.cpp:592`,
   `Print_posix.cpp:331`, `PdfTools.cpp:1139`).

Slice search and thumbnails with the `RenderCachePump` approach. For the rest,
paint a "working" overlay before starting; slice them only if that proves too
slow on real files.

**5.3 Font download blocks the tab.** Code. It's a sync XHR
(`EmbeddedResources.cpp:121`), because mupdf asks for fonts on the calling thread.
Sync XHR is deprecated on the main thread. Prefetch the common fonts in the
background so most lookups hit the cache. Nothing ever evicts `/fonts`, but it's
small and "Delete cached files" exists.

## Phase 6: other browsers and devices

Everything above was checked only in headless Chrome on Windows.

- **Firefox and Safari:** run `tests/ad-hoc-wasm-host.ts` there or by hand. OPFS
  `createWritable` and `navigator.share` are the likely gaps (Suspected).
- **Touch:** gpui has no touch handlers (`grep touchstart` finds none), so phones
  and tablets rely on the browser's mouse emulation. Pinch zoom and scrolling need
  a real device. Decide whether to support them, then scope it separately.
- **Ctrl+wheel and pinch zoom:** check that they zoom the document, not the page.

## Phase 7: keep it working

**7.1 Command smoke test.** Add `tests/ad-hoc-wasm-commands.ts`, based on the probe
below, with the two hooks behind a test-only flag. It fails on an exception, a
stuck main loop, or a hidden command that became visible.

**7.2 Font CORS check.** Add a check that a font URL returns
`Access-Control-Allow-Origin` to the same test, and note the rule in
`docs/wasm-host-api.md`. Without the rule, PDFs that don't embed their fonts open
with blank pages and nothing tells the user. Consider a notification when a font
download fails.

## Order

| Step | Work                      | Size | Why here                          |
| ---- | ------------------------- | ---- | --------------------------------- |
| 1    | Phase 1 (1.1 first)       | M    | Users lose documents today        |
| 2    | Phase 3 and 7.1           | S    | Cheap; the test guards later work |
| 3    | 2.1, 2.3, 2.4             | S    | Small, visible fixes              |
| 4    | 2.2                       | M    | Needs the key table agreed        |
| 5    | Phase 4 and 7.2           | M    |                                   |
| 6    | 5.2 search and thumbnails | L    |                                   |
| 7    | 5.1                       | L    | Needs its own design              |
| 8    | Phase 6                   | ?    | Scope depends on the results      |

Every step needs `bun tests/ad-hoc-wasm-host.ts` plus the Windows and ng Linux
builds, because `src/shared` is compiled into the Windows app as well. `OS_WASM`
gates must leave other platforms unchanged.

## How this was checked

A throwaway build added two hooks: one lists each command's visibility, one runs a
command by id. Each of 267 commands ran in a freshly loaded page with storage reset
and a 4-page PDF restored, checked to be loaded first. Commands needing a selection
ran after Select All.

- No exceptions or stuck main loops. Exit stopping the app is expected.
- 85 commands showed no visible effect. Many are expected on that small PDF (First
  Page on page 1, Crop Image on a PDF), so "nothing" isn't proof that a command
  is broken. The tables above list only the ones with a reason.
- Commands ran without a user gesture, so failures of `navigator.share` and
  clipboard reads there are expected. Test Send by E-mail and paste from a real
  click.
- "Visible" means `GetCommandVisibility` allows it. A command also needs a menu
  entry or a palette row for a user to reach it.
- Only Chrome, only one PDF. No CHM, EPUB, comic or DjVu documents, and no large
  files.
