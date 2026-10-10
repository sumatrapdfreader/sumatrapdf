# SumatraPDF ng

The sources copied from `sumatrapdf-ng` evolve alongside the original Windows
app in `src/`. Ng targets macOS, Linux and WebAssembly; its Windows build is
for testing and comparison. Both use dependencies under `ext/`. Identical
files are shared from the original tree; `cmd/helper/ng-shared.ts` lists them.

Build with `bun cmd/ng-build.ts -dbg` or `-rel`. The host platform is the
default; `-mac`, `-linux` and `-wasm` select a platform. Linux builds launched
on Windows run in WSL Ubuntu. Wasm needs Emscripten.

Outputs live in `out/<platform>/<build-type>`:

| Platform | Debug           | Release         |
| -------- | --------------- | --------------- |
| Windows  | `out/win/dbg`   | `out/win/rel`   |
| macOS    | `out/mac/dbg`   | `out/mac/rel`   |
| Linux    | `out/linux/dbg` | `out/linux/rel` |
| Wasm     | `out/wasm/dbg`  | `out/wasm/rel`  |

On macOS the build also wraps the executable in `SumatraPDF.app`, which
declares the document types Finder offers it for (`cmd/helper/ng-mac-bundle.ts`).

Compiler and sanitizer variants add suffixes, such as `dbg-clang` and
`dbg-asan`. `-all` builds every target; `test_util -run -- -for-ai` runs the
base unit tests. `-run` launches SumatraPDF as a normal session, so it
saves and restores settings. Pass `-- -for-testing` for a throwaway run.

## Keyboard shortcuts

`src/shared/Accelerators.cpp` holds the Windows defaults and a per-platform
layer over them. Shortcut strings in the settings take `Cmd` (also `Command`,
`Super`, `Meta`) off Windows: Command on macOS, Super on Linux.

macOS: `Ctrl` in a default becomes `Cmd`, except for switching tabs
(`Ctrl + Tab`, `Ctrl + Page Up / Down`). `Alt + Left / Right`, `Ctrl + F4` and
`Ctrl + Insert` are dropped. These differ or are added:

| Shortcut              | Command             | Windows default         |
| --------------------- | ------------------- | ----------------------- |
| `Cmd + G`             | Find next           | `F3`                    |
| `Cmd + Shift + G`     | Find previous       | `Shift + F3`            |
| `Cmd + Alt + G`       | Go to page          | `Ctrl + G`              |
| `Cmd + ,`             | Settings            | none                    |
| `Cmd + I`             | Properties          | `Ctrl + D`              |
| `Cmd + D`             | Add favorite        | `Ctrl + B`              |
| `Cmd + ?`             | Manual              | `F1`                    |
| `Cmd + 0`             | Actual size         | `Ctrl + 1`              |
| `Cmd + 9`             | Fit page            | `Ctrl + 0`              |
| `Ctrl + Cmd + F`      | Fullscreen          | `F11`                   |
| `Cmd + Shift + F`     | Presentation        | `F5`                    |
| `Ctrl + Cmd + S`      | Bookmarks sidebar   | `F12`                   |
| `Cmd + Alt + T`       | Toolbar             | `F8`                    |
| `Cmd + {` / `Cmd + }` | Previous / next tab | `Ctrl + Page Up / Down` |
| `Cmd + [` / `Cmd + ]` | Back / forward      | `Alt + Left / Right`    |
| `Cmd + Up` / `Down`   | First / last page   | `Home` / `End`          |
| `Alt + Up` / `Down`   | Page up / down      | `Ctrl + Up / Down`      |
| `Cmd + Backspace`     | Delete annotation   | `Ctrl + Delete`         |

The Windows default keeps working where its key is still free (`F3`, `F11`,
`Cmd + B`, `Cmd + 1`). Shortcuts are shown as the menu bar writes them (`⇧⌘G`).

Linux: the Windows defaults already follow GNOME and KDE; `Ctrl + ,` opens
Settings.

Ng scripts use the `cmd/ng-` prefix, with build helpers under `cmd/helper/ng-`.
Use `ng-gen-commands.ts` and `ng-gen-settings.ts` for ng generated headers;
the original generators still write the original app's files. Format ng
C/C++ with `bun cmd/ng-format.ts`. Embedded byte arrays are generated and
ignored; shared sources and headers are staged under each build's
`generated/shared/` so their includes use ng headers. To diverge a shared file,
copy it into `src/ng` and remove it from `ng-shared.ts`, then update its target.
Translation staging lives in `.work/ng`. The manual uses the same
website checkout and `.work/docs` staging as the original app.

Daily CI builds ng on Windows, macOS and Linux, and Wasm on Linux. The jobs
run base unit tests and upload binaries; Linux also checks rendering and
printing. MinGW and Wine builds are retired.
