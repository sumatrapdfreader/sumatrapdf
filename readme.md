![](https://github.com/sumatrapdfreader/sumatrapdf/workflows/Build/badge.svg)

## SumatraPDF Reader

SumatraPDF is a multi-format (PDF, EPUB, MOBI, FB2, CHM, XPS, DjVu) reader
for Windows under (A)GPLv3 license, with some code under BSD license (see
AUTHORS).

More information:
* [main website](http://www.sumatrapdfreader.org) with downloads and documentation
* [manual](https://www.sumatrapdfreader.org/manual.html)
* [all other docs](https://www.sumatrapdfreader.org/docs/SumatraPDF-documentation-fed36a5624d443fe9f7be0e410ecd715.html)

To compile you need Visual Studio 2019. [Free Community edition](https://www.visualstudio.com/vs/community/) works.

I tend to update to the latest release of Visual Studio. Lately C++ evolves quickly
and Visual Studio constantly adds latest capabilities. If things don't compile,
first make sure you're using the latest version of Visual Studio.

Open `vs2019/SumatraPDF.sln` when using Visual Studio 2019

You need at least version 16.4 of Visual Studio 2019.

Notes on targets:
* `x32_sp` target is for building for Windows XP and requires v141_xp toolset, which is an optional component of Visual Studio setup
* `x32_asan` target is for enabling address sanitizer, only works in 32-bit Release build and requires installing an optional "C++ AddressSanitizers" component (see https://devblogs.microsoft.com/cppblog/addresssanitizer-asan-for-windows-with-msvc/ for more information)

### Asan notes

Flags: https://github.com/google/sanitizers/wiki/SanitizerCommonFlags
Can be set with env variable:
* `ASAN_OPTIONS=allocator_may_return_null=1:verbosity=1:check_malloc_usable_size=false:suppressions="C:\Users\kjk\src\sumatrapdf\asan.supp"`

In Visual Studio, this is in  `Debugging`, `Environment` section.

Supressing issues: https://clang.llvm.org/docs/AddressSanitizer.html#issue-suppression
Note: I couldn't get supressing to work.