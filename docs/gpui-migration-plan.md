# Moving ng's platform code into gpui-kit-cpp

ng keeps OS code that a UI framework should own. Each item moves it into
`../gpui-kit-cpp` (Rust gpui is the spec; record departures in its
`port-status.md`), publishes the dist and switches ng to it.

## Process per item

1. gpui-kit-cpp: implement per its AGENTS.md (suffix files, no OS `#if` in
   `.cpp`, base types, tests in `tests/`). `bun cmd/format.ts <paths>`,
   `bun cmd/test.ts -dbg`, `bun cmd/mac-build.ts -dbg tests` (remote Mac) and
   run `~/src/gpui-cpp/out/mac/dbg/tests` there when Mac code changed.
2. Local dist: `bun cmd/update-dist.ts -no-publish`; copy
   `.work/gpui-kit-cpp-dist/gpui.{h,cpp}` into `ext/gpui/`.
3. ng: replace its code, build (`bun cmd/ng-build.ts -dbg`), run the targeted
   tests. Mac-only ng code: build on the Mac from a scratch clone (bundle +
   diff), never in `~/src/sumatrapdf` there.
4. Commit + push gpui-kit-cpp, `bun cmd/update-dist.ts`,
   `bun cmd/ng-update-gpui.ts`, rebuild, retest, commit ng.

## Working notes

- Bash heredocs drop backslashes: write edit scripts or C++ with the Write
  tool. Python on Windows: `open(p, newline='', encoding='utf-8')`.
- `El` has a size budget (`static_assert` after `struct El`); put rarely set
  fields in `ElLifecycle`. Listener handlers take `const E*`; `const_cast` to
  clear `propagate`.
- gpui builds: Windows `bun cmd/test.ts -dbg`; Mac `bun cmd/mac-build.ts -dbg
  tests` (force-pushes scratch branch `mac-build-tmp`; allowed), then
  `ssh kjk@macbook-pro-14 'cd ~/src/gpui-cpp && ./out/mac/dbg/tests'`; Linux
  `wsl -e bash -lc 'cd /mnt/c/Users/kjk/src/gpui-kit-cpp && ~/.bun/bin/bun
  cmd/build.ts -dbg tests && ./out/linux/dbg/tests'`; wasm
  `EMSDK=C:/Users/kjk/src/sumatrapdf/.work/emsdk bun cmd/build.ts -rel -wasm
  hello_world`; Android `bun cmd/mobile-build.ts -android -rel`; iOS
  `bun cmd/mac-build.ts -ios -rel`.
- ng builds: `bun cmd/ng-build.ts -dbg` (Windows), `-wasm -dbg`, `-linux -dbg`
  (WSL). Mac: `git bundle create b <mac HEAD>..HEAD`, `git diff HEAD
  --binary`, scp both, `git clone --shared ~/src/sumatrapdf /tmp/x`, fetch
  bundle, apply diff, `~/.bun/bin/bun cmd/ng-build.ts -dbg`; delete /tmp/x
  after. Tests: `bun tests/<name>.ts -ng --no-build`; on the Mac
  `-exe out/mac/dbg/SumatraPDF`.
- A locked session makes window captures blank: pixel tests fail with
  nothing on screen. Not a regression.
- Agents never commit, push or run `update-dist.ts` without `-no-publish`.

## Items

All nine are done (2026-11-20); gpui-kit-cpp commits d4afa4d3 to 97465f44.

| # | ng code | gpui API (Rust name) | Notes |
|---|---|---|---|
| 1 | `AppShellRenderScale` (`NativeWindow_mac.mm`, `NativeWindow_wasm.cpp`) | `WindowScaleFactor` (`window.scale_factor()`) | per `window_*.cpp` |
| 2 | `SystemLocale_mac.mm`, stub in `src/shared/Translations.cpp` | `PlatPreferredLocalesTemp` (sys-locale `get_locales`) | all `locale_*.cpp` |
| 3 | `TestCaptureWindow` capture (`NativeWindow_mac.mm`) | `WindowCapturePng` | diagnostic, no Rust twin |
| 4 | `NativeCursors_win.cpp` | cursor from an image | no Rust twin; Windows + macOS |
| 5 | `NativeMsgBox_win.cpp` | `WindowPrompt` (`window.prompt`) | TaskDialog / NSAlert |
| 6 | `NativeFileDlg_win.cpp` | `PromptForNewPathTemp` (`prompt_for_new_path`) + filters | Windows first |
| 7 | `TouchGestures_win.cpp` | `PinchEvent` on Windows + gesture events | WM_GESTURE |
| 8 | `OleDragDrop_win.cpp` | drag out of the window, URL drops | no Rust twin |
| 9 | `ToolWindowPlat.h`, `ToolWindow_mac.mm`, `ToolWindow_linux.cpp`, HWND code in `ToolWindow.cpp`, `AppShellMakeKeyWindow` | `WindowOptions` (kind, min size, bounds), `bounds()`, `activate_window()`, `is_window_active()`, display work area, owner/modal | largest |

Stays in ng (app logic, not UI framework): `AppTools_posix.cpp`,
`Print_posix.cpp`, `ReadAloud_*`, `WasmBridge_wasm.cpp`, the open-documents
queue in `NativeWindow_mac.mm`.
