# Linux GTK4 port progress

This file tracks implementation of [linux-port.md](linux-port.md). Each completed stage is committed separately
after its relevant Windows, Linux, and portable checks pass.

## Status

| Stage | Status      | Summary                                      |
| ----- | ----------- | -------------------------------------------- |
| 1     | Complete    | GTK4 application executable and file opening |
| 2     | Not started | Shared portable reader model                 |
| 3     | Not started | Embeddable canvas and document viewer        |
| 4     | Not started | Portable asynchronous rendering              |
| 5     | Not started | Application shell, tabs, and commands        |
| 6     | Not started | Reader features                              |
| 7     | Not started | Linux desktop services                       |
| 8     | Not started | Packaging and deferred features              |

## Stage 1: Linux application target

Completed 2026-08-16.

- Added `src/linux/SumatraLinux.cpp`, `LinuxApp.*`, and `LinuxWindow.*`.
- Added a `GtkApplication` with activation and command-line file-open handling.
- Added a native application window that displays the requested local path.
- Extended `cmd/helper/linux-build.ts` to build `out/linux-<config>64/SumatraPDF` while retaining the GTK4
  keyboard-help executable and existing test targets.

Verification:

- `bun cmd/build.ts -debug`: passed with no warnings.
- `bun cmd/build.ts -linux -debug`: passed; Linux `test_util` passed 102,782 assertions and the application,
  keyboard-help viewer, and `test_engines` linked.
- `timeout 3s ./out/linux-dbg64/SumatraPDF ext/a-zlib/zlib.3.pdf` under WSLg: application stayed alive until the
  expected timeout; Mesa emitted non-fatal renderer warnings.
