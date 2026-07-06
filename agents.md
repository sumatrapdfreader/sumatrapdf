This is a C++ program for Windows, using mostly win32 windows API functions. The full GUI app remains Windows-only for now; we are porting **non-UI** code so it also compiles on macOS and Linux, starting with `src/base/`.

We don't use STL but our own string / helper / container functions implemented in src\base directory

Assume that Visual Studio command-line tools are available in the PATH environment variable (cl.exe, msbuild.exe etc.)

Our code is in src/ directory. External dependencies are in ext/ directory and mupdf\ directory

To build run: bun ./cmd/build.ts

This creates ./out/dbg64/SumatraPDF-dll.exe executable. Note: ./out/dbg64/SumatraPDF.exe is a different (static) build target that build.ts does NOT update, so it can be stale — always use SumatraPDF-dll.exe for testing.

To run unit tests with AI-friendly diagnostics, run `bun cmd/run-unit-tests.ts -dbg` (or `-rel` / `-asan`). It builds the 64-bit `test_util.exe`, runs it with `-for-ai`, captures output under the matching `out/<config>/unit-tests-*.txt`, and prints assertion/crash callstacks without waiting for debugger UI.

To debug run: `windbgx -Q -o -g ./out/dbg64/SumatraPDF-dll.exe`

When launching SumatraPDF.exe for ad-hoc testing, always pass the `-for-testing` cmd-line flag. It starts a new instance (won't interfere with an already running SumatraPDF), doesn't restore the previous session (only loads files given on the cmd-line) and doesn't save settings (won't overwrite the settings of the user).

After making a change to a .cpp, .c or .h file under `src/` (and before running build.ts), run clang-format on those files to reformat them in place. Do **not** clang-format third-party / vendored code (`mupdf/`, `ext/`, etc.) — keep edits there minimal and match the existing local style.

Never commit changes automatically. Always wait for explicit command to commit changes.

When committing a fix for a GitHub issue, include `(fixes #<issue-no>)` at the end of the commit message.

When committing work done with AI assistance, append the user prompt(s) that produced the change at the very end of the commit message as a single line: `prompt: ...`. If there were multiple prompts, squash them into one concise line. Record the substantive request only — omit meta-instructions such as "commit", "push", "check work", or "verify".

## Cross-platform porting (macOS / Linux)

We are making non-UI library code compile on macOS and Linux while keeping the Windows build working. **Start with `src/base/`**; other `src/` areas follow once base is portable.

### Scope

- **In scope:** platform-neutral logic and OS abstractions (files, paths, time, threading, memory, strings, etc.) under `src/base/` and later other non-UI `src/` trees.
- **Out of scope for now:** UI, Win32 windowing, menus, printing UI, installer, and anything that depends on those.

### Platform-specific source files

When a source file needs platform-specific code, keep the files in the same module directory and use a platform suffix:

| Suffix     | Used on              | Purpose                                      |
|------------|----------------------|----------------------------------------------|
| `_win`     | Windows              | Win32 and other Windows-only implementations |
| `_posix`   | macOS **and** Linux  | Code shared by both Unix-like targets        |
| `_mac`     | macOS only           | Darwin-specific code not shared with Linux   |
| `_linux`   | Linux only           | Linux-specific code not shared with macOS    |

**`_posix` is for code common to Linux and macOS** — prefer it over duplicating the same logic in `_mac` and `_linux`. Use `_mac` or `_linux` only when the two Unix platforms genuinely diverge.

Example layout:

```
src/base/File.h          # shared declaration (platform-neutral API)
src/base/File.cpp        # shared implementation, if any
src/base/File_win.cpp    # Windows implementation
src/base/File_posix.cpp  # macOS + Linux implementation
src/base/File_mac.cpp    # macOS-only pieces (when posix isn't enough)
src/base/File_linux.cpp  # Linux-only pieces (when posix isn't enough)
```

Not every file needs all four platform variants — only split when the implementation is platform-dependent. Keep portable code in the unsuffixed file (`src/base/Foo.cpp`) and move **only** the non-portable parts into the appropriate `_win` / `_posix` / `_mac` / `_linux` file.

### Guidelines

- **Preserve the public API.** Headers at the module root (`src/base/Foo.h`) should expose the same functions/types on every platform; platform differences stay in suffixed `.cpp` files.
- **No `#ifdef` sprawl in shared headers** when a platform-specific `.cpp` split is clearer. Small include-guarded typedefs or macros in a shared header are fine.
- **Prefer POSIX APIs in `_posix` files** (`open`, `read`, `stat`, `pthread`, etc.) and native APIs in `_win` files (Win32). Use `_mac` / `_linux` files for OS-specific extensions (e.g. FSEvents vs inotify).
- **Keep Windows green.** Every change must still build and pass tests on Windows (`bun ./cmd/build.ts`). Do not break the existing Windows target while adding macOS/Linux support.
- **Build system:** macOS/Linux build wiring will be added incrementally; until then, focus on making `src/base/` sources compile cleanly with a Unix toolchain (clang, `-std=c++20` or whatever the module uses).

## C/C++ #include conventions

We rely on a controlled include order rather than self-sufficient headers (this is the inverse of the common "own header first" advice). In a `.cpp`/`.c` file:

- `#include "base/Base.h"` comes **first**. It pulls in `<windows.h>` plus many common C/C++ headers and our base string / container / `ByteSlice` helpers, which most other headers assume are already available.
- Then the remaining includes (other `base/` headers, then the rest of the project headers).
- The file's **own header goes at the end** of the include list (after the headers it depends on, since headers are not self-sufficient) — only `base/Log.h` may come after it.
- `base/Log.h`, if present, is the **last** include.

Do **not** use `#pragma once` in `.h` files.

## Put explanatory comments in `.cpp`, not `.h`

Headers are re-parsed by every translation unit that includes them, so prose
comments in a `.h` cost compilation time on every include. Keep the header
declaration terse (ideally a single line) and put the explaining comment on the
**definition** in the corresponding `.cpp`. For a function declared in `Foo.h`
and defined in `Foo.cpp`, the doc comment lives above the definition in
`Foo.cpp`.

Comments that have no `.cpp` counterpart stay in the header: those documenting a
struct/class, an `enum`, a macro, a constant, or an `inline`/templated function
that is *defined* in the header (there is nowhere else to put them).

## `fmt()` is the type-safe formatter

We use our own `Str` value type (a `char*` + `int len`) for strings instead of
raw `char*` / `std::string`. The most-used formatter is `fmt()`, a macro
`#define fmt(...) strfmt::FormatTemp(__VA_ARGS__)` (StrFormat.h). It formats into
the temp arena and returns a `TempStr`, so call sites read `fmt("page %d", n)`.

`fmt()` is **type-safe**, not a raw `vsnprintf` wrapper:

- The **format string** is a plain `const char*` (almost always a string
  literal). When it's a `Str` instead — most commonly a `_TRA("...")`
  translation, which returns a `Str` — pass its `.s`, e.g.
  `fmt(_TRA("page %d").s, n)`.
- Each **variadic arg** is wrapped in a `strfmt::Arg`, which has explicit
  constructors for `Str`, `WStr`, `char`, the integer/float types, and
  `const void*`. Raw `char*` / `const char*` / `wchar_t*` constructors are
  `= delete`d. So for a `%s` you pass the `Str`/`WStr` **object**, never a
  `char*` — e.g. `fmt("%s", name)` where `name` is a `Str`, and writing
  `fmt("%s", name.s)` is a compile error, not a footgun. `%s` accepts both
  `Str` and `WStr` (a `WStr` is auto-converted to UTF-8).
- Because args carry their own `.len`, a `Str` arg need not be NUL-terminated.

For a `%s` fed a **string literal**, wrap it with `StrL(...)` (see next section)
so the length is computed at compile time: `fmt("%s", StrL("done"))`.

`logf` / `logfa` (Log.h) are macros that format via `::fmt(...)` and route the
result through `log()` / `loga()`, so they follow the same type-safe rules.

Functions that take an already-formatted `Str` (so the caller formats with
`fmt(...)`): `StrBuilder::Append`, `dbglayout`, `MaybeDelayedWarningNotification`.

Note: `logConsole` (common/log.h) is a separate, genuine `printf`-style varargs
function (`const char* fmt, ...`) — unrelated to `fmt()`.

## Make a `Str`/`WStr` from a string literal with `StrL` / `WStrL`

When constructing a `Str`/`WStr` from a **string literal**, use `StrL("...")` /
`WStrL(L"...")`, not `Str("...")` / `WStr(L"...")`. `StrL`/`WStrL` compute the
length at compile time (`sizeof(lit) - 1`), while `Str("...")` / `WStr(L"...")`
do a runtime `strlen`/`wcslen`. So e.g. `fmt("%s", StrL("done"))`, not
`fmt("%s", Str("done"))`. Use `Str(x)` / `WStr(x)` only when `x` is a runtime
`char*` / `wchar_t*` whose length isn't known at compile time.

## NUL-terminate `Str`/`WStr` before C/Win32 APIs

A `Str`/`WStr` is a `{ptr, len}` view and may be a **substring that is not
NUL-terminated** at `s[len]`. Passing its `.s` to a C runtime or Win32 API that
reads a NUL-terminated string (e.g. `CreateFileW`, `GetFileAttributesW`,
`CommandLineToArgvW`, `strlen`, `_wfopen`, `%s` in raw `printf`) can read past
the intended end.

To get a guaranteed NUL-terminated C-string, use `CStrTemp(Str)` → `char*` or
`CWStrTemp(WStr)` → `WCHAR*` (both copy into the temp arena). Prefer these over
`str::DupTemp(x).s` at the call site — the name states the intent:

    BOOL ok = GetFileAttributesEx(CWStrTemp(dir), GetFileExInfoStandard, &fi);

The encoding converters `ToWStrTemp(Str)` / `ToUtf8Temp(WStr)` already return
length-aware, NUL-terminated output — pass the **object** (`ToWStrTemp(path)`),
never `.s` (`ToWStrTemp(path.s)` re-introduces the `strlen`/over-read footgun).

Caveat: a `*Temp` result lives in the temp arena (reset each message loop), so
don't stash its pointer for later use (e.g. a `WNDCLASS::lpszClassName` kept
across calls) — keep the caller's stable string instead.

## Adding a new advanced setting

To add a new advanced setting:
- add definition in cmd/gen-settings.ts
- run "bun cmd/gen-code.ts" (or "bun cmd/gen-settings.ts") to regenerate src/Settings.h and src/Settings.cpp

## Adding a new command

To add a new command:
- add to cmd/gen-commands.ts, always at the end of the list (before the "CmdNone" command)
- run "bun cmd/gen-code.ts" (or "bun cmd/gen-commands.ts") to regenerate src/Commands.h and src/Commands.cpp
- document in docs/md/Commands.md
- add an entry to the **New commands** list at the end of the **next** section in docs/md/Version-history.md (see below)

## Adding a new cmd-line flag

To add a new cmd-line flag:
- add to cmd/gen-flags.ts
- run "bun cmd/gen-code.ts" (or "bun cmd/gen-flags.ts") to regenerate src/Flags.cpp
- implement handling in Flags.cpp
- document in docs/md/Command-line-arguments.md when appropriate
- add an entry to the **New command-line arguments** list at the end of the **next** section in docs/md/Version-history.md (see below)

## Version history (docs/md/Version-history.md)

When documenting a release (usually the **next** section at the top):

- Main bullets describe features and behavior changes in prose. Mention menus, shortcuts, and user-visible effects — not a stream of `add CmdFoo` / `add -flag` bullets.
- At the **end** of the version section, add consolidated lists (only for things **new** in that version):

  **New commands:**

  - `CmdFoo` : "Foo" — optional note (shortcut, palette-only, etc.)

  **New command-line arguments:**

  - `-foo <arg>` : brief description (include `(fixes #N)` here if relevant)

- New `-print-settings` tokens belong under **New command-line arguments** (e.g. `-print-settings` tokens: `stretch`, `center`, …).
- Command **changes** (renames, removals, new arguments on existing commands, shortcut rebinding) stay in the main bullets, not in **New commands**.
- Removed flags can be noted in the main bullets; do not list them under **New command-line arguments**.
- **In-app manual (`docs/manual.dat`):** generated by `bun cmd/gen-docs.ts` from `docs/md/` and the `docs/gen_docs.*` assets. **Do not commit `docs/manual.dat`** — it is a local build artifact (listed in `.gitignore`; use `git update-index --assume-unchanged docs/manual.dat` on a fresh checkout so regen does not dirty `git status`). After editing manual sources, run `bun cmd/gen-docs.ts` before testing in-app help locally; CI/release builds run gen-docs automatically. The app renders markdown on demand in WebView2 via `docs/gen_docs.render.js` and markdown-it. Use `bun cmd/gen-docs.ts --preview` to also emit pre-rendered HTML under `docs-www/` for offline browser preview.

## Bug reproduction / test files

We keep test and reproduction files for bugs in `C:\Users\kjk\OneDrive\!sumatra\bugs\`, named after the GitHub issue number:
- a single file is `bug-<bug-no><rest>` (e.g. `bug-534.pdf`)
- if a repro needs more than one file, use a directory `bug-<bug-no><rest>\`

When fixing a bug, look there first for an existing repro file for that issue and use it. When you create a test/repro file while working on a bug, save it there (using the naming above) after fixing the bug.

## Writing tests

Tests live in tests/ and are run with bun (e.g. `bun tests/issue-5633.ts`). Naming convention, keyed by the GitHub issue number being tested:
- the test script is `tests/issue-<number>.ts`
- if it needs a small number (one or two) of extra files, name them `tests/issue-<number>.<rest>`
- if it needs more files, or a file must live in a directory, put them in `tests/issue-<number>-data/`

Structure of each test (so they compose in tests/all.ts):
- each `tests/issue-<number>.ts` exports `export async function testit(): Promise<void>` that runs the test logic and THROWS on failure (returns normally on success). It must NOT call `process.exit` or build the app itself.
- end the file with a standalone runner so it can still be run directly:
  ```ts
  if (import.meta.main) {
    await runStandalone(testit);
  }
  ```
  `runStandalone` (from `tests/util.ts`) builds the app (unless `--no-build`), runs `testit()`, and exits 0 on pass / 1 on failure.
- shared helpers (`EXE` path, `buildApp`, `runStandalone`) live in `tests/util.ts` — use them instead of re-implementing per file.
- register every new test in `tests/all.ts` (import its `testit` and add it to the `tests` array). `bun tests/all.ts` builds once and runs them all in order, stopping at the first failure.

### Ad-hoc tests

Some checks are too slow, need large external corpora, or require network/git and should **not** run on every `tests/all.ts` invocation. Put those in `tests/ad-hoc-<name>.ts` (not `issue-<n>.ts`):

- export `async function testit()` the same way as regular tests
- end with the usual `if (import.meta.main) { await runStandalone(testit); }` standalone runner
- do **not** register them in `tests/all.ts`
- register them in `tests/before-release.ts` instead (that runner calls `all.ts` first, then each ad-hoc test)
- run ad-hoc tests directly when working on that area: `bun tests/ad-hoc-<name>.ts`
- run the full pre-release suite: `bun tests/before-release.ts`

Example: `tests/ad-hoc-exif.ts` clones/updates `../exif-py` and compares `-dump-exif` output to exif-py's `dump.txt`.

Guidelines for test scripts:
- build the app the same way cmd/build.ts does (via `buildApp`/`runStandalone` in tests/util.ts) and test the resulting out/dbg64/SumatraPDF-dll.exe
- if a needed external tool (e.g. MiKTeX) isn't installed, don't fail the test: print a clear message (with instructions to install it) and skip that part, returning normally so `tests/all.ts` continues
- a good test fails when the fix is reverted (verify this) — not just passes with the fix present
- write ad-hoc GUI automation (driving the app via window messages, screenshots) in **Bun TypeScript, not PowerShell** — bun has FFI. Put raw Win32 wrappers in `tests/winapi.ts` and higher-level actions in `tests/win-automation.ts`; extend and reuse those rather than re-declaring FFI per script. The ad-hoc scripts themselves don't need to be checked in, but the reusable helpers in those two files do.
  - `tests/winapi.ts` = raw winapi: FFI bindings + thin wrappers + constants (enum/find windows, SendMessage/PostMessage, getWindow{Text,Rect}, sendText, `captureWindowToPng` which uses PrintWindow+GDI+ so it works on occluded/background windows).
  - `tests/win-automation.ts` = high-level actions built on winapi: `launchSumatra` (passes `-for-testing`), `waitForFrame`/`findCanvas`, `clickAt`, `pressEnter/Tab/Escape`, `typeIntoInput` / `fillFormFieldAt`, `openContextMenu`/`waitForContextMenu`, `sendCommand`.
  - on this machine injected SendInput mouse/keyboard is dropped, but posting window messages cross-process works; see the project memory `env-gui-automation`.
- prefer driving the app through `-dbg-control <named-pipe>` and `cmd/control.ts` over GUI automation or adding new test-only command-line flags. Tests should pick a unique pipe name, launch `SumatraPDF-dll.exe -for-testing -dbg-control <name>`, send binary request/response commands, and quit the app through the control client.
- `-dbg-control` protocol: requests are `[u32 payloadSize][u16 command][u16 requestId][args...]`; responses are `[u32 payloadSize][u16 requestId][results...]`. Arguments/results are encoded as `[u16 type]` where `0=end`, `1=i32` plus 4 bytes, `2=bytes` plus u32 length and data, `3=utf8 string` plus u32 length, bytes, and a zero terminator, and `4=list` plus u16 element count followed by encoded elements.
- never write runtime scratch / result files directly into `tests/` — that leaves the repo dirty. Write them under `tests/tmp/` (gitignored), using `tmpPath("name")` from `tests/util.ts` (it creates the dir on demand); the OS temp dir (`os.tmpdir()`) is also fine if you clean up after
- if a binary test fixture (e.g. a .pdf) is generated from source (LaTeX, a script, etc.), commit the source alongside it (e.g. `tests/issue-<number>.tex` next to `tests/issue-<number>.pdf`) with a comment on how to regenerate it, so the fixture can be modified later

## Windows Shell Safety

The Bash tool runs under Git Bash (MSYS2), **not** cmd.exe. This causes critical issues with Windows-style commands:

- **NEVER use `2>nul`** — Bash interprets this literally and creates a file called `nul`. On Windows NTFS, `nul` is a reserved device name, making the file extremely difficult to delete (requires UAC/admin privileges). Use `2>/dev/null` instead.
- **NEVER use `rmdir /s /q`** — Bash `rmdir` does not understand cmd.exe flags. Use `rm -rf` instead.
- **NEVER use `del`** — Not available in Bash. Use `rm` instead.
- **NEVER use `dir`** — Use `ls` instead.
- **For Windows-native commands**, wrap in `cmd /c "..."` explicitly.
- In general, always use Unix-style commands and paths in the Bash tool.
