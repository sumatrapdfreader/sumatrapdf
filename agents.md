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

## Adding a new advanced setting

To add a new advanced setting:
- add definition in cmd/gen-settings.ts
- run "bun cmd/gen-settings.ts" to regenerate src/Settings.h and src/Settings.cpp

## Adding a new command

To add a new command:
- add to cmd/gen-commands.ts, always at the end of the list (before the "CmdNone" command)
- run "bun cmd/gen-commands.ts" to regenerate src/Commands.h and src/Commands.cpp
- document in docs/md/Commands.md
- document in docs/md/Version-history.md in **next** section

## Adding a new cmd-line flag

To add a new cmd-line flag:
- add to cmd/gen-flags.ts
- run "bun cmd/gen-flags.ts" to regenerate src/Flags.h and src/Flags.cpp
- implement handling in Flags.cpp
- document in docs/md/Version-history.md in **next** section

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

Guidelines for test scripts:
- build the app the same way cmd/build.ts does (via `buildApp`/`runStandalone` in tests/util.ts) and test the resulting out/dbg64/SumatraPDF-dll.exe
- if a needed external tool (e.g. MiKTeX) isn't installed, throw with a clear error message and instructions to install it
- a good test fails when the fix is reverted (verify this) — not just passes with the fix present
- bun has FFI; if you need to call Windows APIs, put reusable wrappers in tests/winapi.ts
- prefer driving the app via cmd-line flags that write a machine-readable result (see `-test-synctex`) over GUI automation
- put runtime scratch output in a gitignored subdir or the OS temp dir so it isn't committed
- if a binary test fixture (e.g. a .pdf) is generated from source (LaTeX, a script, etc.), commit the source alongside it (e.g. `tests/issue-<number>.tex` next to `tests/issue-<number>.pdf`) with a comment on how to regenerate it, so the fixture can be modified later

## Windows Shell Safety

The Bash tool runs under Git Bash (MSYS2), **not** cmd.exe. This causes critical issues with Windows-style commands:

- **NEVER use `2>nul`** — Bash interprets this literally and creates a file called `nul`. On Windows NTFS, `nul` is a reserved device name, making the file extremely difficult to delete (requires UAC/admin privileges). Use `2>/dev/null` instead.
- **NEVER use `rmdir /s /q`** — Bash `rmdir` does not understand cmd.exe flags. Use `rm -rf` instead.
- **NEVER use `del`** — Not available in Bash. Use `rm` instead.
- **NEVER use `dir`** — Use `ls` instead.
- **For Windows-native commands**, wrap in `cmd /c "..."` explicitly.
- In general, always use Unix-style commands and paths in the Bash tool.
