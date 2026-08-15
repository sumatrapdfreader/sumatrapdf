This is a C++ program for Windows, using mostly win32 windows API functions. The full Windows GUI app remains Windows-only for now; we are porting **non-UI** code so it also compiles on macOS and Linux, starting with `src/base/`. There is also an early Cocoa macOS app under `src/mac/` that can open a command-line document, render the first page through the existing engines, and display it.

We don't use STL but our own string / helper / container functions implemented in src\base directory

Assume that Visual Studio command-line tools are available in the PATH environment variable (cl.exe, msbuild.exe etc.)

Our code is in src/ directory. External dependencies are in ext/ directory

To build run: `bun cmd/build.ts -debug` (or `-release`, `-asan`, and the other modes shown by `bun cmd/build.ts -help`). Called with no options it prints usage and exits; unknown options print an error plus usage and exit unsuccessfully.

Keep `cmd/build.ts` as the single build entry point. Build-mode implementation modules live under `cmd/helper/` and are not invoked directly, except for internal delegation such as the WSL launcher.

This creates ./out/dbg64/SumatraPDF.exe executable. The static build target is SumatraPDF-static and produces ./out/<config>/SumatraPDF-static.exe.

To run a Linux build from Windows, use `bun cmd/build.ts -linux` (defaults to `-asan`) or add `-debug` / `-release`. To cross-compile the Windows exe with mingw inside WSL, use `bun cmd/build.ts -wine` (optional `-clean`, `-run`). The unified build command delegates these Windows-hosted modes to `cmd/helper/wsl-build.ts`. Both require a WSL distro named `Ubuntu` and bun in that distro; Linux deps are `sudo sh cmd/ubuntu-install-deps.sh`.

To run the macOS build on the remote Mac, use `bun cmd/build.ts -mac-remote -branch <temporary-branch> -debug` (or `-release` / `-asan`, optionally with `-clean`). It connects with `ssh kjk@macbook-pro-14`, changes to `src/sumatrapdf`, verifies that the remote checkout is clean, fetches and switches to the temporary branch, runs `cmd/build.ts -mac`, and restores the original remote checkout on success or failure. The macOS build compiles the dependency/base libraries, builds `out/mac-<config>64/test_util`, runs it with `-for-ai`, builds `test_engines`, and builds `SumatraPDF.app`.

To run unit tests with AI-friendly diagnostics, run `bun cmd/run-unit-tests.ts -dbg` (or `-rel` / `-asan`). It builds the 64-bit `test_util.exe`, runs it with `-for-ai`, captures output under the matching `out/<config>/unit-tests-*.txt`, and prints assertion/crash callstacks without waiting for debugger UI.

To debug run: `windbgx -Q -o -g ./out/dbg64/SumatraPDF.exe`

When launching SumatraPDF.exe for ad-hoc testing, always pass the `-for-testing` cmd-line flag. It starts a new instance (won't interfere with an already running SumatraPDF), doesn't restore the previous session (only loads files given on the cmd-line) and doesn't save settings (won't overwrite the settings of the user).

After making a change to a .cpp, .c or .h file under `src/` (and before running build.ts), run clang-format on those files to reformat them in place. Do **not** clang-format third-party / vendored code (`ext/`, etc.) — keep edits there minimal and match the existing local style.

For .ts / .js / .json / .md files, the equivalent is `bunx prettier --write <files>`. Settings live in `.prettierrc.json` (`printWidth` 120, `endOfLine` lf) and `.prettierignore` (vendored code, build output, and the generated `docs/md/Advanced-options-settings.md`). Format only the files you touched: most of `cmd/` and `tests/` predates the config and would produce large unrelated diffs.

Never commit changes automatically. Always wait for explicit command to commit changes.

When committing a fix for a GitHub issue, end the commit message's **first line** with `(fixes #<issue-no>)`, e.g. `fix crash on committing an empty zoom value (fixes #5909)`. That is the line GitHub shows everywhere, so the link belongs there, not buried in the body.

When committing work done with AI assistance, append the user prompt(s) that produced the change at the very end of the commit message as a single line: `prompt: ...`. If there were multiple prompts, squash them into one concise line. Record the substantive request only — omit meta-instructions such as "commit", "push", "check work", or "verify".

## Cross-platform porting (macOS / Linux)

We are making non-UI library code compile on macOS and Linux while keeping the Windows build working. **Start with `src/base/`**; other `src/` areas follow once base is portable.

### Scope

- **In scope:** platform-neutral logic and OS abstractions (files, paths, time, threading, memory, strings, etc.) under `src/base/` and later other non-UI `src/` trees; the early native macOS viewer under `src/mac/`.
- **Out of scope for now:** porting the full Windows UI, Win32 windowing, Windows menus, printing UI, installer, and anything that depends on those.

### macOS app (`src/mac/`)

`src/mac/` is an early Cocoa application, not a port of the Windows UI. Keep it small and native for now:

- Build it with `bun cmd/build.ts -mac -debug` (or `-release` / `-asan`). The app bundle is `out/mac-dbg64/SumatraPDF.app` for debug builds.
- Run it with a document path using `open out/mac-dbg64/SumatraPDF.app --args <path>`, for example `open out/mac-dbg64/SumatraPDF.app --args ./ext/a-zlib/zlib.3.pdf`. Relative paths from the repo should work; absolute paths are fine.
- The current app only opens the first command-line file, renders page 1 through the existing engine layer, displays it, and supports standard macOS Quit / `Cmd-Q`.
- Keep Objective-C / Cocoa code in `.mm` files under `src/mac/`. Do **not** include `base/Base.h` or other Sumatra headers in files that import Cocoa/AppKit: Apple headers define names such as `Size` that conflict with Sumatra types. Use a small C/C++ bridge (`SumatraMacEngine.*`) between Cocoa code and engine/base code.
- When adding mac-specific build inputs, update `MAC_APP_SOURCES` in `cmd/helper/mac-build.ts`.

### Platform-specific source files

When a source file needs platform-specific code, keep the files in the same module directory and use a platform suffix:

| Suffix   | Used on             | Purpose                                      |
| -------- | ------------------- | -------------------------------------------- |
| `_win`   | Windows             | Win32 and other Windows-only implementations |
| `_posix` | macOS **and** Linux | Code shared by both Unix-like targets        |
| `_mac`   | macOS only          | Darwin-specific code not shared with Linux   |
| `_linux` | Linux only          | Linux-specific code not shared with macOS    |

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
- **Keep Windows green.** Every change must still build and pass tests on Windows (`bun cmd/build.ts -debug`; use `bun cmd/run-unit-tests.ts -dbg` for base/test_util work). Do not break the existing Windows target while adding macOS/Linux support.
- **Keep macOS green.** `bun cmd/build.ts -mac -debug` builds the macOS dependency/base libraries, builds and runs `test_util` with `-for-ai`, builds `test_engines`, and links `SumatraPDF.app`. From Windows, use `-mac-remote` on a temporary branch for portability changes.
- **Keep Linux green.** From Windows, `bun cmd/build.ts -linux -debug` (or `-release` / `-asan`) uses the Ubuntu WSL distro. Use `bun cmd/build.ts -wine` to cross-compile the Windows exe with mingw inside WSL.
- **Make tests platform-aware.** Preserve shared behavior tests on every platform where possible. Guard Windows-only expectations (drive letters, backslash-only paths, Win32 command-line parsing, UI/printing behavior, and similar platform specifics) with `#if OS_WIN`, and add POSIX expectations when the behavior is meant to be portable.

### Remote macOS verification from Windows

When doing macOS/Linux portability changes from a Windows machine, test them on the remote Mac by building from a temporary branch:

1. Create a temporary branch locally, e.g. `git switch -c tmp/mac-port-<topic>`.
2. Commit the portability changes on that temporary branch and push it to origin, e.g. `git push -u origin tmp/mac-port-<topic>`. This temporary commit is for remote build verification; still do not make the final feature commit unless the user explicitly asks.
3. Run the remote build from Windows with `bun cmd/build.ts -mac-remote -branch tmp/mac-port-<topic> -debug`. The command aborts if the remote checkout is dirty, fetches and switches to the temporary branch, builds, runs macOS `test_util`, and restores the original remote branch or detached checkout on success or failure.

## C/C++ #include conventions

We rely on a controlled include order rather than self-sufficient headers (this is the inverse of the common "own header first" advice). In a `.cpp`/`.c` file:

- `#include "base/Base.h"` comes **first**. It pulls in `<windows.h>` plus many common C/C++ headers and our base string / container / `ByteSlice` helpers, which most other headers assume are already available.
- Then the remaining includes (other `base/` headers, then the rest of the project headers).
- The file's **own header goes at the end** of the include list (after the headers it depends on, since headers are not self-sufficient) — only `SumatraLog.h` may come after it.
- `SumatraLog.h`, if present, is the **last** include.

Do **not** use `#pragma once` in `.h` files.

## Put explanatory comments in `.cpp`, not `.h`

**Do not put prose function comments in headers.** Headers are re-parsed by every
translation unit that includes them, so comments in a `.h` cost compilation time
on every include. Keep the header declaration terse (ideally a single line) and
put the explaining comment on the **definition** in the corresponding `.cpp`.

For a function declared in `Foo.h` and defined in `Foo.cpp` (or `Foo_win.cpp` /
`Foo_posix.cpp` / etc.), the doc comment lives **only** above the definition in
the `.cpp` — not on the declaration in the `.h`.

```cpp
// Foo.h — declaration only, no prose comment
void CollectNonDefaultRegisteredExtensions(StrVec& out);

// Foo.cpp — comment on the definition
// Extensions we registered for Open With where something else is the default.
void CollectNonDefaultRegisteredExtensions(StrVec& out) {
    ...
}
```

Comments that have no `.cpp` counterpart stay in the header: those documenting a
struct/class, an `enum`, a macro, a constant, a section banner (`//--- …`), or an
`inline`/templated function that is _defined_ in the header (there is nowhere
else to put them). When in doubt for a free function or method with a `.cpp`
body: put the comment in the `.cpp`.

## `fmt()` is the type-safe formatter

We use our own `Str` value type (a `char*` + `int len`) for strings instead of
raw `char*` / `std::string`. The most-used formatter is `fmt()`, a macro
`#define fmt(...) str::FormatTemp(__VA_ARGS__)` (base/StrFormatParse.h). It formats into
the temp arena and returns a `TempStr`, so call sites read `fmt("page %d", n)`.

`fmt()` is **type-safe**, not a raw `vsnprintf` wrapper:

- The **format string** is a plain `const char*` (almost always a string
  literal). When it's a `Str` instead — most commonly a `_TRA("...")`
  translation, which returns a `Str` — pass its `.s`, e.g.
  `fmt(_TRA("page %d").s, n)`.
- Each **variadic arg** is wrapped in a `str::FmtArg`, which has explicit
  constructors for `Str`, `WStr`, `char`, the integer/float types, and
  `const void*`. Raw `char*` / `const char*` / `wchar_t*` constructors are
  `= delete`d. So for a `%s` you pass the `Str`/`WStr` **object**, never a
  `char*` — e.g. `fmt("%s", name)` where `name` is a `Str`, and writing
  `fmt("%s", name.s)` is a compile error, not a footgun. `%s` accepts both
  `Str` and `WStr` (a `WStr` is auto-converted to UTF-8).
- Because args carry their own `.len`, a `Str` arg need not be NUL-terminated.

For a `%s` fed a **string literal**, wrap it with `StrL(...)` (see next section)
so the length is computed at compile time: `fmt("%s", StrL("done"))`.

`logf` / `logfa` are macros in `base/Base.h` that format via `::fmt(...)` and
route the result through `log()` / `loga()`, so they follow the same type-safe
rules. Base only declares those two; the app implements them (`SumatraLog.cpp`,
declared in `SumatraLog.h`).

Functions that take an already-formatted `Str` (so the caller formats with
`fmt(...)`): `str::Builder::Append`, `dbglayout`, `MaybeDelayedWarningNotification`.

## Make a `Str`/`WStr` from a string literal with `StrL` / `WStrL`

When constructing a `Str`/`WStr` from a **string literal**, use `StrL("...")` /
`WStrL(L"...")`, not `Str("...")` / `WStr(L"...")`. `StrL`/`WStrL` compute the
length at compile time (`sizeof(lit) - 1`), while `Str("...")` / `WStr(L"...")`
do a runtime `strlen`/`wcslen`. So e.g. `fmt("%s", StrL("done"))`, not
`fmt("%s", Str("done"))`. Use `Str(x)` / `WStr(x)` only when `x` is a runtime
`char*` / `wchar_t*` whose length isn't known at compile time.

## Use `len(x)`, not `x.len`

`Str`, `WStr`, `Vec<T>`, `StrVec`, `StrQueue` and `str::Builder` all expose a
`.len` field, and there is a free `len()` overload for each. Prefer the free
function:

    for (int i = 0; i < len(s); i++)     // yes
    for (int i = 0; i < s.len; i++)      // no

It reads the same for every container, it works for the ones whose `.len` is not
an `int` (`str::Builder::len` is a `u32`, so `.len` drags unsigned arithmetic
into comparisons), and a type that later hides or renames the field keeps
working. Reach for `.len` only when you need to **assign** to it.

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
- run "bun cmd/gen-code.ts" (or "bun cmd/gen-settings.ts") to regenerate src/Settings.h (it also re-emits the settings docs)

## Adding a new command

To add a new command:

- add to cmd/gen-commands.ts, always at the end of the list (before the "CmdNone" command)
- run "bun cmd/gen-code.ts" (or "bun cmd/gen-commands.ts") to regenerate src/Commands.h and src/Commands.cpp
- document in docs/md/Commands.md
- add an entry to the **New commands** list at the end of the **next** section in docs/md/Version-history.md (see below)

## DocProp name maps are generated

The name↔`DocProp` `SeqStrNum` maps (`epubPropsMap`, `pdfCreatorPropsMap`, `mupdfPropsMap`, `pdfPropNames`) are generated by `cmd/gen-code.ts` between `// @gen-start docprop-*` / `// @gen-end docprop-*` markers — **don't hand-edit them**. Each `SeqStrNum` entry's number must be zigzag-LEB128 encoded (`varIntCString`); hand-writing raw bytes silently broke every lookup (title read as author, etc.). To change a mapping, edit the `docPropMaps` data in `cmd/gen-code.ts` and run `bun cmd/gen-code.ts`; it re-emits the maps with correct encoding and self-verifies first/middle/last of each. The same applies to `gVirtKeysNum` in `ShortcutParse.cpp` (`// @gen-start virt-keys-num`).

## Adding a new cmd-line flag

To add a new cmd-line flag:

- add to cmd/gen-flags.ts
- run "bun cmd/gen-code.ts" (or "bun cmd/gen-flags.ts") to regenerate src/Flags.cpp
- implement handling in Flags.cpp
- document in docs/md/Command-line-arguments.md when appropriate
- add an entry to the **New command-line arguments** list at the end of the **next** section in docs/md/Version-history.md (see below)

## Version history (docs/md/Version-history.md)

When documenting a release (usually the **next** section at the top):

- **Do not document bug fixes.** Version history is for features and behavior changes, not a changelog of defects. A plain `fix <something>` bullet does not belong there — the commit message and the GitHub issue already record it. Only mention a fix when it comes with a user-visible change worth describing on its own (a new setting, a new command, a different default), and then describe _that_, not the bug.
- Main bullets describe features and behavior changes in prose. Mention menus, shortcuts, and user-visible effects — not a stream of `add CmdFoo` / `add -flag` bullets.
- At the **end** of the version section, add consolidated lists (only for things **new** in that version):

  **New commands:**

  - `CmdFoo` : "Foo" — optional note (shortcut, palette-only, etc.)

  **New command-line arguments:**

  - `-foo <arg>` : brief description (include `(fixes #N)` here if relevant)

- New `-print-settings` tokens belong under **New command-line arguments** (e.g. `-print-settings` tokens: `stretch`, `center`, …).
- Command **changes** (renames, removals, new arguments on existing commands, shortcut rebinding) stay in the main bullets, not in **New commands**.
- Removed flags can be noted in the main bullets; do not list them under **New command-line arguments**.
- **Embedded app data (`.work/embedded.dat`):** one LzSA archive packed by `bun cmd/pack-embedded.ts` (also run from `gen-docs.ts` and the VS prebuild). Holds `translations.txt`, `marked.min.js`, `mermaid.min.js`, and the in-app manual files from `.work/docs/`. Linked as `IDR_EMBEDDED_PAK` via `src/SumatraPDF.rc`. After editing manual sources, run `bun cmd/gen-docs.ts` (or `pack-embedded.ts` if docs are already generated) before testing help/markdown mermaid locally; CI/release builds run gen-docs automatically. The app renders markdown on demand in WebView2 via `docs/gen_docs.render.js` and markdown-it. Use `bun cmd/gen-docs.ts --preview` to also emit pre-rendered HTML under `.work/www/` for offline browser preview.

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

### Which tests to run after a change

Do **not** run the full suites (`tests/run-almost-all.ts`, `tests/run-all.ts`, `tests/run-pre-release.ts`, or unrelated
unit tests) to verify a change. The release process runs them; that is what it is for.
A full run costs ~4-5 minutes and tells you almost nothing about the ten lines you just
edited. Run broader suites only when the user explicitly asks.

Instead, run only what the change can plausibly break:

- a named issue fix → `bun tests/issue-<number>.ts`
- a change to code an existing test covers → that test, found by grepping `tests/` for the
  feature, command id, or setting name involved
- base/`test_util` work → `bun cmd/run-unit-tests.ts -dbg`
- nothing covers it → say so instead of running everything as a substitute. A targeted
  manual check (launch with `-for-testing`, screenshot, probe log) is worth more than a
  green suite that never touched the code.

Never edit `tests/run-almost-all.ts` or `tests/run-all.ts` to skip tests so a run gets further. If a test fails and you
suspect it is unrelated, check it out on a clean tree (`git stash`) and run it standalone
several times - some are environment- and focus-dependent and fail intermittently
regardless of the change (`issue-1136` and `issue-2254` have both done this). One passing
run and one failing run is not evidence; compare several runs on each side.

Structure of each test (so they compose in tests/run-almost-all.ts / tests/run-all.ts):

- each `tests/issue-<number>.ts` exports `export async function testit(): Promise<void>` that runs the test logic and THROWS on failure (returns normally on success). It must NOT call `process.exit` or build the app itself.
- end the file with a standalone runner so it can still be run directly:
  ```ts
  if (import.meta.main) {
    await runStandalone(testit);
  }
  ```
  `runStandalone` (from `tests/util.ts`) builds the app (unless `--no-build`), runs `testit()`, and exits 0 on pass / 1 on failure.
- shared helpers (`EXE` path, `buildApp`, `runStandalone`) live in `tests/util.ts` — use them instead of re-implementing per file.
- register every new test in `tests/run-almost-all.ts` (import its `testit` and add it to the `tests` array). If the test cannot be made faster (print-to-PDF, LaTeX, a measured wait, high-zoom tile settle, a huge fixture, copying the exe next to restrict.ini), add it to `slowTests` in `tests/run-all.ts` instead. `bun tests/run-almost-all.ts` is the fast suite; `bun tests/run-all.ts` runs that then the slow tests, stopping at the first failure.

### Ad-hoc tests

Some checks are too slow, need large external corpora, or require network/git and should **not** run on every `tests/run-almost-all.ts` invocation. Put those in `tests/ad-hoc-<name>.ts` (not `issue-<n>.ts`):

- export `async function testit()` the same way as regular tests
- end with the usual `if (import.meta.main) { await runStandalone(testit); }` standalone runner
- do **not** register them in `tests/run-almost-all.ts` or `tests/run-all.ts`
- run ad-hoc tests directly when working on that area: `bun tests/ad-hoc-<name>.ts`
- the pre-release suite is `bun tests/run-pre-release.ts` (run-almost-all + LaTeX)

Example: `tests/ad-hoc-exif.ts` clones/updates `../exif-py` and compares `-dump-exif` output to exif-py's `dump.txt`.

Guidelines for test scripts:

- build the app the same way cmd/build.ts does (via `buildApp`/`runStandalone` in tests/util.ts) and test the resulting out/dbg64/SumatraPDF.exe
- if a needed external tool (e.g. MiKTeX) isn't installed, don't fail the test: print a clear message (with instructions to install it) and skip that part, returning normally so `tests/run-almost-all.ts` continues
- a good test fails when the fix is reverted (verify this) — not just passes with the fix present
- write ad-hoc GUI automation (driving the app via window messages, screenshots) in **Bun TypeScript, not PowerShell** — bun has FFI. Put raw Win32 wrappers in `tests/winapi.ts` and higher-level actions in `tests/win-automation.ts`; extend and reuse those rather than re-declaring FFI per script. The ad-hoc scripts themselves don't need to be checked in, but the reusable helpers in those two files do.
  - `tests/winapi.ts` = raw winapi: FFI bindings + thin wrappers + constants (enum/find windows, SendMessage/PostMessage, getWindow{Text,Rect}, sendText, `captureWindowToPng` which uses PrintWindow+GDI+ so it works on occluded/background windows).
  - `tests/win-automation.ts` = high-level actions built on winapi: `launchSumatra` (passes `-for-testing`), `waitForFrame`/`findCanvas`, `clickAt`, `pressEnter/Tab/Escape`, `typeIntoInput` / `fillFormFieldAt`, `openContextMenu`/`waitForContextMenu`, `sendCommand`.
  - on this machine injected SendInput mouse/keyboard is dropped, but posting (and sending) window messages cross-process works. There is no interactive desktop, so real-cursor probes prove nothing, and a canvas text selection can't be driven at all — check a suspicious result against a clean build before blaming your change.
- resolve command ids by name with `cmdId("CmdName")` (from `tests/util.ts`), never hardcode the numeric id. Command ids are auto-numbered in `src/Commands.h` and shift whenever commands are added or removed, so a hardcoded constant silently starts sending a _different_ command. This broke `tests/issue-5780.ts` (it sent `CmdCommandPalette` instead of `CmdOpenNextFileInFolder` after ids shifted) and had stale ids lurking in several ad-hoc tests. `tests/lint-command-ids.ts` (runs first in `tests/run-almost-all.ts`) enforces this — it fails the suite on any hardcoded `const Cmd... = <number>` or numeric `sendCommand(win, <number>)`.
- prefer driving the app through `-dbg-control <named-pipe>` and `tests/control.ts` over GUI automation or adding new test-only command-line flags. Tests should pick a unique pipe name, launch `SumatraPDF.exe -for-testing -dbg-control <name>`, send binary request/response commands, and quit the app through the control client.
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
