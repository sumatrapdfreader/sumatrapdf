This is a C++ program for Windows, using mostly win32 windows API functions

We don't use STL but our own string / helper / container functions implemented in src\utils directory

Assume that Visual Studio command-line tools are available in the PATH environment variable (cl.exe, msbuild.exe etc.)

Our code is in src/ directory. External dependencies are in ext/ directory and mupdf\ directory

To build run: bun ./cmd/build.ts

This creates ./out/dbg64/SumatraPDF.exe executable

To debug run: `windbgx -Q -o -g ./out/dbg64/SumatraPDF.exe`

After making a change to .cpp, .c or .h file (and before running build.ts), run clang-format on those files to reformat them in place

Don't automatically commit changes. Always wait for explicit command to commit changes.

## Adding a new advanced setting

To add a new advanced setting:
- add definition in cmd/gen-settings.ts
- run "bun cmd/gen-settings.ts" to regenerate src/Settings.h and src/Settings.cpp

## Adding a new command

To add a new command:
- add to cmd/gen-commands.ts
- run "bun cmd/gen-commands.ts" to regenerate src/Commands.h and src/Commands.cpp
- document in docs/md/Commands.md
- document in docs/md/Version-history.md in **next** section

## Adding a new cmd-line flag

To add a new cmd-line flag:
- add to cmd/gen-flags.ts
- run "bun cmd/gen-flags.ts" to regenerate src/Flags.h and src/Flags.cpp
- implement handling in Flags.cpp
- document in docs/md/Version-history.md in **next** section

## Windows Shell Safety

The Bash tool runs under Git Bash (MSYS2), **not** cmd.exe. This causes critical issues with Windows-style commands:

- **NEVER use `2>nul`** — Bash interprets this literally and creates a file called `nul`. On Windows NTFS, `nul` is a reserved device name, making the file extremely difficult to delete (requires UAC/admin privileges). Use `2>/dev/null` instead.
- **NEVER use `rmdir /s /q`** — Bash `rmdir` does not understand cmd.exe flags. Use `rm -rf` instead.
- **NEVER use `del`** — Not available in Bash. Use `rm` instead.
- **NEVER use `dir`** — Use `ls` instead.
- **For Windows-native commands**, wrap in `cmd /c "..."` explicitly.
- In general, always use Unix-style commands and paths in the Bash tool.
