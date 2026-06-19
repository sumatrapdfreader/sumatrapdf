This is a C++ program for Windows, using mostly win32 windows API functions

We don't use STL but our own string / helper / container functions implemented in src\utils directory

Assume that Visual Studio command-line tools are available in the PATH environment variable (cl.exe, msbuild.exe etc.)

Our code is in src/ directory. External dependencies are in ext/ directory and mupdf\ directory

To build run: bun ./cmd/build.ts

This creates ./out/dbg64/SumatraPDF-dll.exe executable. Note: ./out/dbg64/SumatraPDF.exe is a different (static) build target that build.ts does NOT update, so it can be stale — always use SumatraPDF-dll.exe for testing.

To debug run: `windbgx -Q -o -g ./out/dbg64/SumatraPDF-dll.exe`

When launching SumatraPDF.exe for ad-hoc testing, always pass the `-for-testing` cmd-line flag. It starts a new instance (won't interfere with an already running SumatraPDF), doesn't restore the previous session (only loads files given on the cmd-line) and doesn't save settings (won't overwrite the settings of the user).

After making a change to a .cpp, .c or .h file under `src/` (and before running build.ts), run clang-format on those files to reformat them in place. Do **not** clang-format third-party / vendored code (`mupdf/`, `ext/`, etc.) — keep edits there minimal and match the existing local style.

Never commit changes automatically. Always wait for explicit command to commit changes.

When committing a fix for a GitHub issue, include `(fixes #<issue-no>)` at the end of the commit message.

## Adding a new advanced setting

To add a new advanced setting:
- add definition in cmd/gen-settings.ts
- run "bun cmd/gen-settings.ts" to regenerate src/Settings.h and src/Settings.cpp

## Adding a new command

To add a new command:
- add to cmd/gen-commands.ts, always at the end of the list (before the "CmdNone" command)
- run "bun cmd/gen-commands.ts" to regenerate src/Commands.h and src/Commands.cpp
- document in docs/md/Commands.md
- add an entry to the **New commands** list at the end of the **next** section in docs/md/Version-history.md (see below)

## Adding a new cmd-line flag

To add a new cmd-line flag:
- add to cmd/gen-flags.ts
- run "bun cmd/gen-flags.ts" to regenerate src/Flags.h and src/Flags.cpp
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
- After editing, run `bun cmd/gen-docs.ts` to refresh the HTML docs.

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
- bun has FFI; if you need to call Windows APIs, put reusable wrappers in tests/winapi.ts
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
