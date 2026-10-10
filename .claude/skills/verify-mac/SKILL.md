---
name: verify-mac
description: Build, launch and drive SumatraPDF ng on macOS to verify a change end-to-end (control channel, Finder open events, log capture).
---

# Verifying SumatraPDF ng changes on macOS

## Build & launch

- Build: `bun cmd/ng-build.ts -dbg` → `out/mac/dbg/SumatraPDF` and the bundle
  `out/mac/dbg/SumatraPDF.app` (same binary, cloned).
- Always launch with `-for-testing` (no session restore, doesn't save settings).
  Settings are still **read** from `~/Library/Application Support/SumatraPDF`.
- Capture logs: `-log-to-file <path>` (collects `logf` output). Write it to the
  scratchpad, not the repo.
- Sample PDF in-repo: `ext/a-zlib/zlib.3.pdf`.

```sh
out/mac/dbg/SumatraPDF -for-testing -log-to-file $LOG ext/a-zlib/zlib.3.pdf &
```

## Driving the app

Use the control channel, not GUI automation. `cmd/ng-dbg-control.ts` starts the
app with `-for-testing -dbg-control`, sends one command, prints the reply and
quits:

```sh
bun cmd/ng-dbg-control.ts ext/a-zlib/zlib.3.pdf TestCurrentTab
# path=/…/zlib.3.pdf page=1
bun cmd/ng-dbg-control.ts ext/a-zlib/zlib.3.pdf TestLayout
# page n=1 shown=1 pos=250,2,690,892 …
bun cmd/ng-dbg-control.ts ext/a-zlib/zlib.3.pdf TestInvokeCommand CmdGoToNextPage
# OK handled=1
```

- Run it with no arguments for the command list. Commands take names, never
  numeric ids.
- For several commands against one instance, import `connect` / `sendCommand` /
  `controlCommands` from `cmd/ng-dbg-control.ts` in a bun script
  (`tests/ad-hoc-mac-highlight.ts` is the model). The channel is a unix socket:
  `/tmp/<name>.sock`.
- Mouse, keyboard and window messages: `tests/mac-control.ts` (`macClick`,
  `macSendText`, `macSendMessage`).
- Synthetic PDFs are easy to hand-generate from a bun script (compute xref
  offsets from string lengths); see `readingPdf()` in
  `tests/ad-hoc-mac-highlight.ts`.

## Finder / Launch Services

Documents from Finder arrive as an Apple event, not in argv, and only reach the
bundle. Arguments after `--args` are argv.

```sh
APP=$PWD/out/mac/dbg/SumatraPDF.app
# cold launch; -n forces a new instance
open -n -a "$APP" tests/issue-1189.pdf --args -for-testing -log-to-file $LOG
# a second open goes to the running instance
open -a "$APP" tests/issue-1809.pdf
grep StartLoadDocuments $LOG
pkill -f "SumatraPDF.app/Contents/MacOS/SumatraPDF -for-testing"
```

- Check `pgrep -lf SumatraPDF` first: without `-n`, `open -a` targets whichever
  instance of the bundle is already running, including the user's own.
- Bundle sanity: `plutil -lint "$APP/Contents/Info.plist"`, `codesign -dv "$APP"`.

## Gotchas

- Screenshots don't work here: the terminal has no Screen Recording permission,
  so `screencapture -R` returns only the wallpaper and `screencapture -l <id>`
  fails with "could not create image from window". Assert on control-channel
  replies and the log instead.
- `clang-format` is not installed, so `bun cmd/ng-format.ts` does nothing; say
  so instead of claiming the C/C++ edits are formatted.
- Quit what you launch. `pkill -f` with the `-for-testing` argument in the
  pattern leaves the user's own instance alone.
- Unit tests: `bun cmd/ng-build.ts -dbg test_util -run -- -for-ai` (but
  verification = driving the app, not tests).
- Crashes or hangs: `bun cmd/ng-dbg.ts -- -for-testing <file>` runs the debug
  ASan build under lldb.
