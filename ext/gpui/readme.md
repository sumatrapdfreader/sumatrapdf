# gpui-kit-cpp-dist

The amalgamated build of [gpui-kit-cpp](https://github.com/kjk/gpui-kit-cpp): GPUI is
`gpui.h` and `gpui.cpp`, its QuickJS-NG engine is the separate generated
`quickjs/quickjs.h` and `quickjs/quickjs.c`, and `extras/` holds the ported
library crates as standalone amalgams, one header + one source each (see
"What is here"). Everything beside those generated files
is here so you can run them before you commit to them — every example, the
assets they load, and the build and run scripts used by the source repo.

Nothing here is written by hand, so issues and pull requests belong in the
source repo.

## Try it

First install [bun](https://bun.sh/), then a compiler:

- **Windows** — Visual Studio 2026; the free Community edition is fine, and
  2022 works too. `build.ts` finds it with vswhere, so no developer prompt.
- **Linux** — `g++` or `clang++`, plus `pkg-config`, X11, cairo, pangocairo and gdk-pixbuf.
  Optional: libcurl (remote images) and WebKitGTK 4.1 (the webview).
- **macOS** — the Xcode command line tools (`xcode-select --install`).

Then:

```
bun run.ts story
```

That builds and launches a comprehensive showcase of the available
functionality ([examples/story](./examples/story)) — a sidebar gallery with a
page per widget. `bun run.ts showcase` is the shorter tour, and `bun run.ts` with no
arguments lists every example and every option.

```
bun build.ts -all              # build every example, do not run one
bun run.ts -dbg input          # debug build
bun run.ts -wasm story         # build for the browser, serve it, open a tab
bun run.ts gpui_shell -- examples/js_todolist --dev
```

`gpui_shell` is the desktop JavaScript host. Its `check <directory>` command
loads and renders once without opening a window, and `types <directory>` writes
the matching `gpui.d.ts` declarations.

Windows builds Direct2D by default. Pass
`--win-backend=d2d|d3d11|d3d12|all` to `build.ts` or `run.ts` to choose what
the executable contains. An `all` build retains the process-start
`__paint=d2d|d3d11|d3d12` selector; `__msaa=1|2|4|8` controls the custom
renderers' sample count, and `__scene=off|replay|cache|skip|damage` selects the
scene optimization level (`skip` by default). `__layout_reuse=off|on` rebuilds
the taffy tree every frame when off. Pass those after `run.ts`'s `--`; the
runtime removes them from `argv` before calling `GpuiMain`.

## What is here

```
gpui.h, gpui.cpp     the C++20 library amalgam
quickjs/             pinned QuickJS-NG as one C11 header and one C source
extras/              the ported library crates as standalone amalgams, one
                     header + one source each:
  autocorrect/       the autocorrect CJK copywriting linter, crate
                     autocorrect 2.14.2. NOT part of
                     gpui.cpp: the editor example (and the tests) compile
                     and link it beside gpui.cpp
  taffy/             the flexbox/grid/block layout engine, crate taffy
                     0.13.0. Also inside gpui.cpp — this copy is
                     for using the library without gpui
  markdown/          the CommonMark + GFM parser, crate markdown
                     1.0.0. Also inside gpui.cpp — this copy
                     is for using the parser without gpui
  markdown-mini/     a smaller parser implementing the same markdown.h
                     API (not an upstream crate); inside gpui.cpp only
                     when built with -markdown=mini
  html5ever/         the HTML parser, crate html5ever 0.27.0.
                     Also inside gpui.cpp — this copy is standalone
  html5ever-mini/    a smaller parser implementing the same html5ever.h API;
                     selected with -html=mini
  wry/               the webview, crate lb-wry 0.53.3 (WebView2 on
                     Windows, WKWebView on macOS, stubs elsewhere). Also
                     inside gpui.cpp
  unicode-linebreak/  Unicode 15.0 line breaks, crate unicode-linebreak
                     0.1.5. Also inside gpui.cpp
gpui_shell/           command-line JavaScript application host
examples/            every example, including story/ and showcase/
assets/              icons, images and documents the examples load at runtime
web/shell.html       the page a -wasm build is served in
build.ts, run.ts     the source repo's own build and run scripts
```

`out/` is where builds land. Nothing else is generated in place.

## Use it

Drop the GPUI pair and `quickjs/` into your tree, `#include "gpui.h"` where you
need the API, compile `gpui.cpp` as C++20, and compile `quickjs/quickjs.c` as
C11. The platform halves are already
inside `gpui.cpp` behind `GPUI_OS_*`
guards, so the same source set builds on all four:

- **Windows** — `cl /std:c++20 /EHsc /utf-8 /DUNICODE /D_UNICODE`, static CRT;
  define one of `WIN_BACKEND_DIRECT2D`, `WIN_BACKEND_D3D11` or
  `WIN_BACKEND_D3D12` and link its import libraries. With no definition the
  compatibility default is Direct2D. `WIN_BACKEND_ALL` compiles and links all
  three. The custom backends already contain their shader bytecode and do not
  require `d3dcompiler.lib` or `D3DCompiler_47.dll`.
- **Linux** — `g++ -std=c++20` with `pkg-config --cflags --libs x11 cairo pangocairo gdk-pixbuf-2.0`.
  Add `libcurl` and `-DGPUI_HAVE_CURL=1` for remote images,
  `webkit2gtk-4.1` and `-DGPUI_HAVE_WEBKITGTK=1` for the webview, and
  `alsa` and `-DGPUI_HAVE_ALSA=1` for the speech microphone.
- **macOS** — `clang++ -std=c++20 -x objective-c++` with the Cocoa, CoreText,
  IOKit, AudioToolbox, AVFoundation and Speech frameworks. The file is
  Objective-C++ because the mac half is.
- **wasm** — `em++ -std=c++20` with `-sALLOW_MEMORY_GROWTH`; the browser half
  draws through Canvas2D and needs no library at all. em++ rather than emcc:
  the link needs the C++ runtime and emcc leaves it out.

Compile QuickJS with `/TC /std:c11 /experimental:c11atomics` under MSVC
(`/experimental:c11atomics` is not needed by clang-cl), or with
`-x c -std=gnu11` under clang, GCC and emscripten. `build.ts` supplies the full
warning and platform flags.

The headers private to the bundled library ports (markdown's tokenizer,
taffy's layout internals, autocorrect's internals) sit behind
`#if GPUI_INCLUDE_PRIVATE_API`, which defaults to 0: including `gpui.h`
gives you the public API only. Define `GPUI_INCLUDE_PRIVATE_API 1` before
the include if you want to reach the internals; `gpui.cpp` and the
`extras/` sources already do, being the implementation.

## The extras/ pairs

Each `extras/` directory is one of the ported library crates as a
standalone amalgam: include its header, compile its `.cpp` as C++20. Their
headers inline the shared base layer behind a `GPUI_BASE_H_` guard that
`gpui.h` also wraps its own copy in, so a pair header and `gpui.h` can meet
in one translation unit in either order; their private headers sit behind
the same `GPUI_INCLUDE_PRIVATE_API` gate as above.

`extras/autocorrect/` is the one pair **not** inside `gpui.cpp`: it holds
declarations plus the linter only, and links _beside_ `gpui.cpp`, which
provides the base implementation — this is exactly how the editor example
and the tests build.

`extras/taffy/`, `extras/markdown/`, `extras/markdown-mini/`,
`extras/html5ever/`, `extras/html5ever-mini/`, `extras/wry/` and
`extras/unicode-linebreak/` are **also
inside `gpui.cpp`**; these copies exist for using
one library on its own, without gpui. Each therefore carries the base
implementation (with its platform halves behind `GPUI_OS_*` guards), which
means two things: never link one of them beside `gpui.cpp` — the symbols
would be there twice — and provide the one seam base leaves to the
application, `void base::log(base::Str)` (the examples' `AppLog.cpp` is the
reference). `markdown/` and `markdown-mini/` implement the same
`markdown.h`, so they are drop-in swaps for each other and never link
together. The same is true of `html5ever/` and `html5ever-mini/` and their
`html5ever.h`. On Windows, `wry/` additionally links `ole32.lib user32.lib
comctl32.lib shlwapi.lib advapi32.lib shell32.lib`; taffy and the markdown
parsers need no libraries at all.

No other dependencies, no nested build system, no STL containers.

## This copy

Amalgamated from gpui-kit-cpp [`f267f753c332cb13bced635b1650966f9907971a`](https://github.com/kjk/gpui-kit-cpp/commit/f267f753c332cb13bced635b1650966f9907971a).

[What has changed in gpui-kit-cpp since](https://github.com/kjk/gpui-kit-cpp/compare/f267f753c332cb13bced635b1650966f9907971a...main)
shows every commit this copy is behind by; if that page is empty, it is current.
