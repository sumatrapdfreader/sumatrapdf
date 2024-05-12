# Build system

Our build system uses [Premake5.](https://premake.github.io/) For convenience we have `premake5.exe` binary in `bin` directory.

Premake generates Visual Studio solution in directory `vs2022` from `premake5.lua` file. Generated solution files are stored in the repository for convenience.

## When to run premake

Premake re-generates Visual Studio project files from `premake5.*.lua` files.

You only need to do that if you add or remove source files.

To re-generate:

- install [Go](https://golang.org/)
- make changes to `premake5.*.lua` files, most likely `premake5.files.lua`
- run `.\doit.bat -premake` which runs a Go program in `do` directory with `-premake` flag

Relevant files:

```
PS C:\Users\kjk\src\sumatrapdf> ls *.lua
Mode                LastWriteTime         Length Name
----                -------------         ------ ----
-a----         4/24/2020 12:20 AM          22947 premake5.files.lua
-a----          5/4/2020  7:51 PM          23565 premake5.lua
```

## Customizing build

Sometimes we want to customize the build with `#ifdef`. We could do it by adding additional configurations, but that can spiral out of control quickly.

Instead we have `src\utils\BuildConfig.h` file. It's empty by default but you can changed it to add your `#define` customization.

## Build variants

We have `Debug`, `Release` and `ReleaseAnalyze` configurations. `ReleaseAnalyze` runs code analysis.

We have platforms:

- `Win32` : 32-bit build
- `x64` : 64-bit build
- `x64_asan` : 64-build with additional runtime [Address Sanitizers](https://devblogs.microsoft.com/cppblog/addresssanitizer-asan-for-windows-with-msvc/) checks. Only 64-bit build for simplicity

## How official builds are made

I use a Go program in `do` directory, executed with `.\doit.bat -build-pre-rel` or `.\doit.bat -build-release`.