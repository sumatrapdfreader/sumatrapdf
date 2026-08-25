# Build system

Our build system uses [Premake 5](https://premake.github.io/). For convenience, we keep the `premake5.exe` binary in the `bin` directory.

Premake generates the Visual Studio solution in the `vs2022` directory from the `premake5.lua` file. The generated solution files are stored in the repository for convenience.

## When to run premake

Premake regenerates Visual Studio project files from the `premake5.*.lua` files.

You only need to do that if you add or remove source files.

To regenerate them:

- install [bun](https://bun.com/)
- make changes to `premake5.*.lua` files, most likely `premake5.files.lua`
- run `bun .\cmd\premake.ts`

Relevant files:

```
PS C:\Users\kjk\src\sumatrapdf> ls *.lua
Mode                LastWriteTime         Length Name
----                -------------         ------ ----
-a----         4/24/2020 12:20 AM          22947 premake5.files.lua
-a----          5/4/2020  7:51 PM          23565 premake5.lua
```

## Customize the build

Sometimes we want to customize the build with `#ifdef`. We could do it by adding additional configurations, but that can spiral out of control quickly.

Instead, we have the `src\BuildConfig.h` file. It's empty by default, but you can change it to add your `#define` customization.

## Build variants

We have `Debug`, `Release`, and `ReleaseAnalyze` configurations. `ReleaseAnalyze` runs code analysis.

We have platforms:

- `Win32` : 32-bit build
- `x64` : 64-bit build
- `x64_asan` : 64-bit build with additional runtime [AddressSanitizer](https://devblogs.microsoft.com/cppblog/addresssanitizer-asan-for-windows-with-msvc/) checks. Only 64-bit builds support AddressSanitizer, for simplicity
