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

Compiler and sanitizer variants add suffixes, such as `dbg-clang` and
`dbg-asan`. `-all` builds every target; `test_util -run -- -for-ai` runs the
base unit tests. `-run` launches SumatraPDF with `-for-testing`.

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
