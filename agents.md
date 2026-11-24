# SumatraPDF

- The original app is Win32 C++ in `src/`. SumatraPDF ng (`src/ng/`) targets macOS, Linux and Wasm, with a Windows build for testing and comparison.
- Use the string, container and helper types in `src/base/`. Avoid STL. Write all helper scripts, including temporary ones, in Bun TypeScript.
- Dependencies live in `ext/`. For each change to `ext/mupdf`, include one patch per logical change in `ext/patches/` in the same commit, against the revision in `ext/versions.txt`. See `ext/patches/README.md`. Put our own files compiled into MuPDF in `src/mupdf/`.
- Update `ext/a-*` amalgamations with the relevant `.claude/skills/update-*/SKILL.md` and `cmd/amalgam.ts`. Never hand-edit them.

## Build and format

- Original app: `bun cmd/build.ts -dbg` builds `out/dbg64/SumatraPDF.exe`. See `-help` for other modes, including `-rel` and `-asan`. Keep `cmd/build.ts` and `cmd/ng-build.ts` as the public build entry points. Build internals live in `cmd/helper/`. Assume Visual Studio tools are on `PATH`.
- `SumatraPDF-static` builds `out/<config>/SumatraPDF-static.exe`. Debug with `windbgx -Q -o -g ./out/dbg64/SumatraPDF.exe`. Always pass `-for-testing` for ad-hoc app launches to isolate the instance and settings and skip session restore.
- Original unit tests are debug-only. Run `bun cmd/run-unit-tests.ts -dbg` (also `-32` or `-asan`) for `-unit-tests -for-ai` and diagnostics in `out/<config>/unit-tests-*.txt`.
- Before building, clang-format touched `.cpp`, `.c` and `.h` files under `src/`. Use `bun cmd/ng-format.ts` for ng C/C++. Keep vendor edits minimal and follow local style without running clang-format there.
- After editing TypeScript under `cmd/` or `tests/`, run `bun cmd/format.ts` or `bun cmd/format.ts -ts` for Prettier only. Run `bunx prettier --write <files>` on other touched Prettier-owned files.

## Coding and commits

- Keep changes focused. Use early returns and `continue`, blank lines between logical blocks, function names under 30 characters and enum parameters in place of booleans.
- Name recurring or meaningful constants and all spec-defined values. Leave self-explanatory one-offs inline.
- Keep low-level I/O in its own abstraction. Each layer calls only the layer immediately below it and works with domain concepts.
- Keep prose concise. Use rare one- or two-line comments that explain what and why. Put function comments at the `.cpp` definition. Header comments can cover types, constants, macros, sections and definitions that live in the header.
- Include order in `.cpp` and `.c`: `base/Base.h`, other base headers, project headers, the file's own header, then `SumatraLog.h` if used. Headers rely on this order. Avoid `#pragma once`.
- Use `StrL` and `WStrL` for literals, `Str` and `WStr` for runtime pointers, and `len(x)` except when assigning the length.
- `fmt()` and `logf()` take a C format string and typed arguments. Use `fmt(Tr("page %d").s, n)` for a translated format and pass `Str` or `WStr` objects to `%s`. Wrap literal arguments in `StrL`. Format before passing text to `str::Builder::Append`, `dbglayout` or `MaybeDelayedWarningNotification`.
- `Str` and `WStr` views may lack a NUL terminator. Use `CStrTemp` or `CWStrTemp` for C and Win32 APIs. `ToWStrTemp` and `ToUtf8Temp` already terminate their output. Pass the input view itself. Keep stable storage for anything that outlives the temp arena.
- Commit only on explicit request. Use a capitalized imperative subject without a period, aiming for 50 characters with a 72-character maximum. End an issue fix subject with `(fixes #<number>)`. Separate the body with a blank line, wrap at 72, and explain what changed and why. For AI-assisted work, end the message with one `prompt: ...` line containing the substantive request, omitting workflow instructions.

## Tests and bug fixes

- Before fixing a bug, find the existing repro in `C:\Users\kjk\OneDrive\!sumatra\bugs\`, write a test and observe it fail. Verify it passes with the fix. Save new repros there as `bug-<number><rest>` files or directories.
- Run focused checks only. Use `bun tests/issue-<number>.ts` for issue fixes, search `tests/` for affected features, and use `bun cmd/run-unit-tests.ts -dbg` for base or unit-test work. If there is no test, use a targeted manual check and report the gap. Run full suites only on explicit request.
- For a reported failing test, verify that test alone several times. For suspected unrelated flakes, compare repeated standalone runs on a clean tree and with the change. Never skip tests to get a suite further.
- After fixing a failing or flaky test, move its array entry in `tests/run-almost-all.ts` just below `issue-5978`, which stays first. For a test registered directly in `tests/run-pre-release.ts`, move it above `runAlmostAll`.
- Name issue tests `tests/issue-<number>.ts`, with one or two sibling fixtures named `issue-<number>.<rest>` or a directory `issue-<number>-data/` for larger sets. Include source and regeneration instructions for generated binary fixtures. Use `tmpPath()` from `tests/util.ts` for scratch files under `tests/tmp/`, or clean up OS temp files.
- Export `async function testit(): Promise<void>` and throw on failure. Keep builds and exits in the standalone runner from `tests/util.ts`:

  ```ts
  if (import.meta.main) {
    await runStandalone(testit);
  }
  ```

- Reuse `EXE`, `buildApp` and `runStandalone` from `tests/util.ts`. Standalone tests build unless passed `--no-build`. Print installation instructions and skip the affected check when an optional external tool is missing.
- Register new tests in `tests/run-almost-all.ts`, or in `slowTests` in `tests/run-all.ts` if inherently slow. Put checks that need large external corpora, git, network or excessive runtime in `tests/ad-hoc-<name>.ts` with the same runner and no suite registration.
- Windows daily CI builds the debug ASan static exe and points `SUMATRA_TEST_EXE` at it. If a test cannot run on the hosted runner, give the reason in `excludedTests` in `tests/run-github-ci.ts`. Poll for readiness to accommodate ASan. Runners select `quarter` or `workArea` geometry with `setTestWindowLayout()` in `tests/winapi.ts`.
- Prefer `tests/control.ts` and `-dbg-control <unique-pipe>` to drive and quit the app. Disable notifications with `client.setNotificationsEnabled(false)` before sampling document pixels. Use `-log-to-file <path>` to capture logs, including in runs that need to save settings.
- For GUI automation, reuse or extend raw Win32 wrappers in `tests/winapi.ts` and higher-level actions in `tests/win-automation.ts`. This machine supports cross-process window messages but drops `SendInput` and lacks an interactive desktop. Check suspicious GUI results against a clean build.
- Resolve commands with `cmdId("CmdName")` from `tests/util.ts`. `tests/lint-command-ids.ts` rejects hardcoded IDs.

## Generated code and docs

- Settings: edit `cmd/gen-settings.ts` and `cmd/ng-gen-settings.ts`. Use `.enum([...])` for fixed choices and `.emptyMeans("...")` when empty is valid. Run `bun cmd/gen-code.ts` or `bun cmd/gen-settings.ts` for original settings and docs, and the ng generator for ng.
- Commands: append to the end of `cmd/gen-commands.ts` to preserve IDs. Run `bun cmd/gen-code.ts` or `bun cmd/gen-commands.ts`. Document in `Commands.md` and add a **New commands** entry in the next `Version-history.md` section.
- Flags: edit `cmd/gen-flags.ts`, run it or `bun cmd/gen-code.ts`, and implement handling in `src/Flags.cpp`. Update `Command-line-arguments.md` as appropriate and add a **New command-line arguments** entry in the next version section.
- Edit the generator inputs in `cmd/gen-code.ts` for `docprop-*` and `virt-keys-num` maps. The generator handles zigzag-LEB128 encoding via `varIntCString`. Never hand-edit the generated blocks.
- User docs live in `../hack/webapps/sumatra-website/www/docs`. Follow `docs/writing-docs.md` for feature pages. Commit website changes there with the `sumatra-website:` prefix when authorized. Keep PDF example scripts only in the website's `www/docs/scripts/`.
- Version history describes features and behavior changes in user-facing prose. Omit plain bug fixes. End each section with lists for new commands (`CmdFoo`, label, optional shortcut or note) and flags (`-foo <arg>`, description, issue if relevant). Include new `-print-settings` tokens with new flags. Describe changes to existing commands or flags in the main bullets.
- `cmd/gen-docs.ts` stages pages reachable from `SumatraPDF-documentation.md` and scripts listed in `Tool-run-javascript-examples.md` into `.work/docs`. `cmd/gen-pdf-scripts.ts` bundles scripts into `.work/scripts.js`. Builds without the website checkout omit the manual and examples.
- The original app embeds `IDR_EMBEDDED_PAK` via `EMBEDDED_PAK` in `src/SumatraPDF.rc`. `cmd/pack-embedded-prebuild.cmd` uses `MakeLZSA.exe` to pack translations, JS and the staged manual into `embedded-static.lzsa` or `embedded.lzsa` with the installer payload. Build entry points refresh docs. Raw MSBuild and Ninja use the current staging.

## SumatraPDF ng

- Use GPUI for all UI. Match the original Windows UI and behavior, and document unavoidable differences and GPUI gaps. Preserve engines, settings format, shortcut syntax and command alignment. Prefer `*_win.cpp`, `*_posix.cpp`, `*_wasm.cpp` and `OS_WIN` / `OS_POSIX` / `OS_WASM` gates. Retain Windows-only features there until ported.
- Both apps share `ext/`. Reuse identical sources, tests and assets through `cmd/helper/ng-shared.ts`. Never edit files staged in `out/<platform>/<config>/generated/shared/`. To diverge one, copy it to `src/ng/`, remove its shared entry and update `cmd/helper/ng-targets.ts`.
- Put common code from differing app files in `src/shared/FooCommon.cpp`. Declare shared helpers and app callbacks (`// implemented by each app`) in `FooCommon.h`. Register new shared files in `premake5.files.lua`, `ng-shared.ts` and `ng-targets.ts`.
- Build with `bun cmd/ng-build.ts -dbg` or `-rel`. Select `-linux`, `-mac` or `-wasm` as needed. Outputs are under `out/<platform>/<config>` with optional `-clang`, `-asan` and `-profile` suffixes. `-clang` selects clang-cl on Windows and does not affect macOS output naming.
- `-run` and `cmd/ng-dbg.ts` save settings and restore the session. Pass `-- -for-testing` for an isolated launch. `cmd/ng-dbg-control.ts` already passes it and uses `-dbg-control <pipe>`.
- Run ng unit tests with `bun cmd/ng-build.ts -dbg test_util -run -- -for-ai`. Other targets include `test_engines` (open and render), `test_mupdf` and `plugin-test`. `cmd/ng-dbg.ts` runs an ASan debugger build.
- Use `cmd/ng-gen-commands.ts`, `cmd/ng-gen-settings.ts` and `cmd/ng-gen-embedded.ts`. `bun cmd/trans-dl.ts` writes `.work/translations.txt` and requires `TRANS_UPLOAD_SECRET` for translations. Without it builds are English-only.
- Linux builds use the native toolchain, including WSL Ubuntu (`bun cmd/ng-build.ts -linux -dbg`). WSLg needs `DISPLAY`. Install Bun and these Ubuntu packages:

  ```sh
  sudo apt install build-essential pkg-config libx11-dev libxcursor-dev libxi-dev libcairo2-dev libpango1.0-dev libgdk-pixbuf-2.0-dev libglib2.0-dev libssl-dev
  ```

- macOS needs Bun and Xcode command-line tools. Wasm needs Emscripten. Windows bootstraps `.work/emsdk` on first build. `-wasm -rel -run` serves `http://localhost:8085/`. Wasm runs threads inline and slices rendering on the main thread. Console targets use Node and NODERAWFS. See `docs/wasm-host-api.md` for browser integration.
- Ng uses `yy.mm.dd[.n]` from `cmd/helper/ng-version.ts`. `-rev <n>` sets the suffix. Version defines go to `SumatraConfig.cpp` only.
- Release scripts are in `../hack/sumatrapdf/build/` (`build-ng-mac.ts`, `build-ng-linux.ts`). Linux packages use `.github/workflows/ng-linux-release.yml`, `cmd/ng-linux-ci.sh` and the `cmd/ng-{deb,rpm,tar,arch}.ts` scripts. Desktop assets live in `src/ng/linux/` and `cmd/update-linux-icons.ts` regenerates icons.
- macOS writes `crashinfo/sumatrapdfcrash.txt` in app data and uploads on next launch, except in ASan and `-for-testing` runs. Use `bun cmd/sumatra-mac-crashes.ts` from `../hack/daily` to symbolicate reports with `.dSYM`, or pass `-file <path>` for a local report.

### GPUI

- Include `gui/GpuiBridge.h` first in files using both GPUI and base. Qualify GPUI types with `gp::` or `gpc::` and avoid `using namespace gpui`. Use `GStrL`, `ToGpui`, `FromGpui` and `GpuiDup`.
- Check APIs in `ext/gpui/gpui.h` and fetch examples with `bun cmd/ng-update-gpui.ts -examples`. Edit framework code in `../gpui-kit-cpp/src/gpui/`, never in the amalgams or updater patches. With explicit authorization to commit and push, publish framework main, run `bun cmd/update-dist.ts` there and vendor with `bun cmd/ng-update-gpui.ts`. Use `-commit <sha>` to pin a dist revision.

## SumatraPDF Web

- Home-page account files share `src/shared/ServerFiles.cpp`, `ContentHash.cpp` and `SumatraPDFWebCommon.cpp`. The server is `../hack/webapps/sumatra-web` (`server/store_files.go`). `GET /api/files` lists `name`, `size`, `sha1` and `uploadedAt`.
- Cache the list per account in `<appdata>/SumatraPDF-web-files.txt`. Fetch on first home page, after transfers and when older than five minutes on layout or activation. Retain the last list on failure unless the key was rejected.
- Hash history files with matching size in the background. `FileState.ContentSha1` stores `<sha1> <size> <mtime>` and is valid while size and mtime match. Transfers also populate it. Equal hashes match, same-name different hashes show `Server ≠`, and unhashed files use name and size. Cancel stalled I/O after five seconds and skip its share for ten minutes. Wasm uses server names under `/uploads` without hashing or caching.
- Tests include `sumatrapdf-web-home`, `server-sync-hash`, `server-sync-match` and `server-sync-cache`. The allowed test origin is loopback port 9332. Use `TestContentHash` via `-dbg-control` for hash test hooks.

## Windows shell

Git Bash uses Unix commands and paths. Redirect errors with `2>/dev/null`. `2>nul` creates a reserved Windows filename. Run Windows-native commands in their own shell.
