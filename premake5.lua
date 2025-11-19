--[[
To generate Visual Studio files (in vs2019/ or vs2022/ directory), run:
scripts\premake-regenerate-vs-projects.ps1

I'm using premake5 beta6 from https://premake.github.io/download/

Note about nasm: when providing "-I foo/bar/" flag to nasm.exe, it must be
"foo/bar/" and not just "foo/bar".

Notes about asan in msvc:
https://devblogs.microsoft.com/cppblog/addresssanitizer-asan-for-windows-with-msvc/
https://devblogs.microsoft.com/cppblog/msvc-address-sanitizer-one-dll-for-all-runtime-configurations/

Set this in debugger env properties:
PATH=%PATH%;c:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.37.32822\bin\Hostx64\x64
(see https://stackoverflow.com/questions/428085/how-do-i-set-a-path-in-visual-studio)
Make sure you're targeting the righat Configuration / Platform

Reference for warnings:
 4018 - signed/unsigned mismatch
 4057 - function X differs in indirection to slightly different base types
 4090 - different 'const' qualifiers
 4091 - 'typedef ': ignored on left of 'tagGPFIDL_FLAGS' when no variable is declared
 4100 - unreferenced formal parameter
 4127 - conditional expression is constant
 4131 - uses old-style declarator
 4189 - local variable is initialized but not referenced, defer macro triggers this
 4204 - non-standard extension: non-constant aggregate initializer
 4206 - non-standard extension: translation unit is empty
 4244 - 64bit, conversion with possible loss of data
 4245 - sign/unsigned mismatch
 4267 - 64bit, conversion with possible loss of data
 4302 - 64bit, type cast truncation
 4305 - 'initializing': truncation from 'double' to 'float'
 4310 - 64bit, cast truncates constant value
 4311 - 64bit, type cast pointer truncation
 4312 - 64bit, conversion to X of greater size
 4324 - 64bit, structure was padded
 4458 - declaraion of X hides class member
 4522 - multiple assignment operators specified
 4530 - exception mismatch
 4611 - interaction between '_setjmp' and C++ object destruction is non-portable
 4702 - unreachable code
 4706 - assignment within conditional expression
 4800 - forcing value to bool (performance warning)
 4819 - The file contains a character that cannot be represented in the current code page
 4838 - conversion from X to Y requires a narrowing conversion
 4996 - POSIX name deprecated

 Linker warning
 4221 - This object file does not define any previously undefined public symbols, so it will not be used by any link operation that consumes this library

Prefast:
 28125 - function X must be called in try/except (InitializeCriticalSection)
 28252 - Inconsistent annotation
 28253 - Inconsistent annotation
--]]

newoption {
   trigger = "with-clang",
   description = "use clang-cl.exe instead of cl.exe"
}

-- TODO: won't be needed with premake5 beta 8
newoption {
   trigger = "with-2026",
   description = "use vs2026 directory"
}

-- TODO: test this option
-- usestandardpreprocessor 'On'

-- TODO: try appmanifest
-- files { "hello.appxmanifest" }
-- https://github.com/premake/premake-core/pull/1750/files

include("premake5.files.lua")

-- TODO: could fold 9 libraries used by mupdf into a single
-- project mupdf-libs, to make solution smaller

-- setup WebView2 paths
function webview_conf()
  includedirs { "packages/Microsoft.Web.WebView2.1.0.992.28/build/native/include" }
  filter "platforms:x32"
    libdirs { "packages/Microsoft.Web.WebView2.1.0.992.28/build/native/x86" }
  filter "platforms:x64 or x64_asan"
    libdirs { "packages/Microsoft.Web.WebView2.1.0.992.28/build/native/x64" }
  filter "platforms:arm64"
    libdirs { "packages/Microsoft.Web.WebView2.1.0.992.28/build/native/arm64" }
  filter {}
  links { "WebView2LoaderStatic.lib"}
end

-- to make debug builds faster, we compile stable libraries (freetype, libjpeg etc.)
-- in release mode even in debug builds
function mixed_dbg_rel_conf()
  editandcontinue "Off"

  -- same as in optimized_conf()
  runtime "Release"
  defines { "_HAS_ITERATOR_DEBUGGING=0" }

  filter "configurations:Debug"
    defines { "DEBUG" }

  filter "configurations:DebugFull"
    defines { "DEBUG" }

  filter "configurations:Release*"
    defines { "NDEBUG" }
    optimize "Size"

  -- asan builds:
  -- * no ltcg
  -- TODO: or arm64 ?
  filter { "configurations:Release*", "platforms:x32 or x64" }
    linktimeoptimization "On"
  filter {}
end

-- config for stable libraries where debug build is done with optimization
function optimized_conf()
  editandcontinue "Off"
  optimize "Size"

  -- we mix Deubg / Release compilation between projects
  -- but all linked modules have to use the same type
  -- of c libraries, so we use release C libs
  -- and we disable C++ iterator debugging because it also
  -- must be consistent across modules
  runtime "Release"
  defines { "_HAS_ITERATOR_DEBUGGING=0" }

  filter "configurations:DebugFull"
    defines { "DEBUG" }

  filter "configurations:Debug or Release or ReleaseAnalyze"
    undefines { "DEBUG" }
    defines { "NDEBUG" }
  filter {}

  filter { "configurations:Release*", "platforms:x32 or x64 or arm64" }
    linktimeoptimization "On"
  filter {}
end

-- TODO: temporary, create 2022 project in 2026
-- directory and then needs manual retarget in vs2026
-- in premake5 beta 8 (not yet released) vs2026 action
-- is supported and we'll be able to switch to that
function conf_2026()
  filter { "options:with-2026" }
    location "vs2026"
  filter {}
end

-- per-workspace setting that differ in clang-cl.exe vs cl.exe builds
function clang_conf()
  filter "options:with-clang"
    location "vs2022-clang"
    toolset "clang"
    buildoptions {"-fms-compatibility", "-fms-extensions", "-Wno-microsoft-include", "-march=x86-64-v3", "-maes"}

    warnings "Off"
    exceptionhandling "On"
  filter {}

  filter {'options:not with-clang'}
    warnings "Extra"
    exceptionhandling "Off"
  filter {}
end


function warnings_as_errors()
  filter {"configurations:not ReleaseAnalyze" and "options:not with-clang"}
    fatalwarnings { "All" }
  filter {}
end

function zlib_defines()
  includedirs {
    "ext/zlib",
  }
end

-- add to a project that links zlib
function links_zlib()
  -- links { "zlib-ng" }
  links { "zlib" }
end

-- add to a project that needs to see zlib headers
function uses_zlib()
  --zlib_ng_defines()
  zlib_defines()
end

workspace "SumatraPDF"
  configurations { "Debug", "DebugFull", "Release", "ReleaseAnalyze", }
  platforms { "x32", "x64", "arm64", "x64_asan" }
  startproject "SumatraPDF"

  filter "platforms:x32"
     architecture "x86"
  filter {}

  filter "platforms:x64_asan"
    sanitize { "Address" }
    defines { "ASAN_BUILD=1" }
    -- disablewarnings { "4731" }
  filter {}

  filter "platforms:x64 or x64_asan"
     architecture "x86_64"
     -- strangely this is not set by default for rc.exe
     resdefines { "_WIN64" }
  filter {}

  filter "platforms:arm64"
     architecture "ARM64"
  filter {}

  disablewarnings { "4127", "4189", "4324", "4458", "4522", "4611", "4702", "4800", "6319" }

  location "this_is_invalid_location"

  filter "action:vs2022"
    location "vs2022"
  filter {}


  clang_conf()
  conf_2026()

  filter {"platforms:x32", "configurations:Release"}
    targetdir "out/rel32"
  filter {"platforms:x32", "configurations:ReleaseAnalyze"}
    targetdir "out/rel32_prefast"
  filter {"platforms:x32", "configurations:Debug"}
    targetdir "out/dbg32"
  filter {"platforms:x32", "configurations:DebugFull"}
    targetdir "out/dbgfull32"

  filter {"platforms:x64", "configurations:Release"}
    targetdir "out/rel64"
  filter {"platforms:x64", "configurations:ReleaseAnalyze"}
    targetdir "out/rel64_prefast"
  filter {"platforms:x64", "configurations:Debug"}
    targetdir "out/dbg64"
  filter {"platforms:x64", "configurations:DebugFull"}
    targetdir "out/dbgfull64"
  filter {}

  filter {"platforms:x64_asan", "configurations:Release"}
    targetdir "out/rel64_asan"
  filter {"platforms:x64_asan", "configurations:ReleaseAnalyze"}
    targetdir "out/rel64_prefast_asan"
  filter {"platforms:x64_asan", "configurations:Debug"}
    targetdir "out/dbg64_asan"
  filter {"platforms:x64_asan", "configurations:DebugFull"}
    targetdir "out/dbgfull64_asan"
  filter {}

  filter {"platforms:arm64", "configurations:Release"}
    targetdir "out/arm64"
  filter {"platforms:arm64", "configurations:ReleaseAnalyze"}
    targetdir "out/arm64_prefast"
  filter {"platforms:arm64", "configurations:Debug"}
    targetdir "out/dbgarm64"
  filter {"platforms:arm64", "configurations:DebugFull"}
    targetdir "out/dbgfullarm64"
  filter {}

  objdir "%{cfg.targetdir}/obj"

  -- https://github.com/premake/premake-core/wiki/symbols
  -- https://blogs.msdn.microsoft.com/vcblog/2016/10/05/faster-c-build-cycle-in-vs-15-with-debugfastlink/
  symbols "FastLink"
  filter {"configurations:Release"}
    symbols "Full"
  filter {}

  staticruntime  "On"
  -- https://github.com/premake/premake-core/wiki/flags
  flags {
    "MultiProcessorCompile",
    "Maps", -- generate map file
    -- "Unicode", TODO: breaks libdjuv?
  }
  rtti "Off"

  defines {
    "WIN32",
    "_WIN32",
    -- https://docs.microsoft.com/en-us/cpp/porting/modifying-winver-and-win32-winnt?view=vs-2019
    "WINVER=0x0605", -- latest Windows SDK
    "_WIN32_WINNT=0x0603"
  }

  project "unrar"
    kind "StaticLib"
    language "C++"
    optimized_conf()
    defines { "UNRAR", "RARDLL", "SILENT" }
    -- os.hpp redefines WINVER, is there a better way?
    disablewarnings { "4005", "4100", "4201", "4211", "4244", "4310", "4389", "4456", "4459", "4505", "4701", "4702", "4706", "4709", "4731", "4996" }
    -- unrar uses exception handling in savepos.hpp but I don't want to enable it
    -- as it seems to infect the Sumatra binary as well (i.e. I see bad alloc exception
    -- being thrown)
    -- exceptionhandling "On"
    disablewarnings { "4530" } -- warning about using C++ exception handler without exceptions enabled

    includedirs { "ext/unrar" }
    unrar_files()

  project "libdjvu"
    kind "StaticLib"
    characterset ("MBCS")
    language "C++"
    optimized_conf()
    defines {
      "_CRT_SECURE_NO_WARNINGS",
      "NEED_JPEG_DECODER",
      "WINTHREADS=1",
      "DDJVUAPI=/**/",
      "MINILISPAPI=/**/",
      "DEBUGLVL=0"
    }
    filter {"platforms:x64_asan"}
      defines { "DISABLE_MMX" }
    filter{}
    disablewarnings { "4100", "4189", "4244", "4267", "4302", "4311", "4312", "4505"}
    disablewarnings { "4456", "4457", "4459", "4530", "4701", "4702", "4703", "4706" }
    includedirs { "ext/libjpeg-turbo" }
    libdjvu_files()

  project "chm"
    kind "StaticLib"
    language "C"
    optimized_conf()
    defines { "_CRT_SECURE_NO_WARNINGS" }
    disablewarnings { "4018", "4244", "4267", "4996" }
    files { "ext/CHMLib/*.c", "ext/CHMLib/*.h" }

  project "unarrlib"
    kind "StaticLib"
    language "C"
    optimized_conf()
    -- TODO: for bzip2, need BZ_NO_STDIO and BZ_DEBUG=0
    -- TODO: for lzma, need _7ZIP_PPMD_SUPPPORT
    defines { "HAVE_ZLIB", "HAVE_BZIP2", "HAVE_7Z", "BZ_NO_STDIO", "_7ZIP_PPMD_SUPPPORT" }
    -- TODO: most of these warnings are due to bzip2 and lzma
    disablewarnings { "4100", "4244", "4267", "4456", "4457", "4996" }
    uses_zlib()
    includedirs { "ext/bzip2", "ext/lzma/C" }
    unarr_files()

  project "libwebp"
    kind "StaticLib"
    language "C"
    optimized_conf()
    disablewarnings { "4204", "4244", "4057", "4245", "4310", "4701" }
    includedirs { "ext/libwebp" }
    libwebp_files()

  -- ARGS = "-Isrc\libdav1d_entrypoint.a.p" "-Isrc" "-I..\src" "-I." "-I.." "-Iinclude\dav1d" "-I..\include\dav1d" "-Iinclude" "-I..\include" "-I..\include\compat\msvc" "-DNDEBUG" "/MD" "/nologo" "/showIncludes" "/utf-8" "/W3" "/O2" "/Gw" "-D_POSIX_C_SOURCE=200112L" "-wd4028" "-wd4090" "-wd4996" "/Fdsrc\libdav1d_entrypoint.a.p\thread_task.c.pdb"

  -- for asm files
  -- build src/libdav1d.a.p/looprestoration_avx2.obj: CUSTOM_COMMAND_DEP ../src/x86/looprestoration_avx2.asm | C$:/Users/kjk/AppData/Local/bin/NASM/nasm.EXE
  -- COMMAND = "C:\Users\kjk\AppData\Local\bin\NASM\nasm.EXE" "-f" "win64" "-I" "C:/Users/kjk/src/dav1d/src/" "-I" "C:/Users/kjk/src/dav1d/build/" "-MQ" "src/libdav1d.a.p/looprestoration_avx2.obj" "-MF" "src/libdav1d.a.p/looprestoration_avx2.obj.ndep" "../src/x86/looprestoration_avx2.asm" "-o" "src/libdav1d.a.p/looprestoration_avx2.obj"

  project "libheif"
    kind "StaticLib"
    language "C++"
    optimized_conf()
    defines { "_CRT_SECURE_NO_WARNINGS", "HAVE_DAV1D", "LIBHEIF_STATIC_BUILD" }
    includedirs { "ext/libheif", "ext/dav1d/include" }
    disablewarnings {  "4018", "4100", "4101","4146", "4244", "4245", "4267", "4273", "4456", "4701", "4703" }
    -- TODO: I don't want RTTI and /EHsc
    rtti "On"
    buildoptions { "/EHsc" }
    libheif_files()

  project "dav1d"
    kind "StaticLib"
    language "C"
    optimized_conf()
    defines { "_CRT_SECURE_NO_WARNINGS" }
    filter {'platforms:x32'}
      defines { "ARCH_X86_32=1", "ARCH_X86_64=0", "__SSE2__" }
    filter {'platforms:x64 or x64_asan'}
      defines { "ARCH_X86_32=0", "ARCH_X86_64=1" }
    filter{}
    disablewarnings { "4057", "4090", "4100", "4152", "4201", "4244", "4245", "4456", "4457", "4701", "4703", "4706", "4819", "4996", "5287" }
    includedirs { "ext/dav1d/include/compat/msvc", "ext/dav1d", "ext/dav1d/include" }
     -- nasm.exe -I .\ext\libjpeg-turbo\simd\
    -- -I .\ext\libjpeg-turbo\win\ -f win32
    -- -o .\obj-rel\jpegturbo\jsimdcpu.obj
    -- .\ext\libjpeg-turbo\simd\jsimdcpu.asm
    filter {'files:**.asm', 'platforms:x32'}
       buildmessage '%{file.relpath}'
       buildoutputs { '%{cfg.objdir}/%{file.basename}_asm.obj' }
       buildcommands {
          '..\\bin\\nasm.exe -f win32 -DPREFIX=1 -DARCH_X86_64=0 -DARCH_X86_32=1 -I ../ext/dav1d/src -I ../ext/dav1d/include -o "%{cfg.objdir}/%{file.basename}_asm.obj" "%{file.relpath}"'
       }
    filter {}
    filter {'files:**.asm', 'platforms:x64 or x64_asan'}
      buildmessage '%{file.relpath}'
      buildoutputs { '%{cfg.objdir}/%{file.basename}_asm.obj' }
      buildcommands {
        '..\\bin\\nasm.exe -f win64 -DARCH_X86_64=1 -DARCH_X86_32=0 -D__x86_64__ -DWIN64 -DMSVC -I ../ext/dav1d/src -I ../ext/dav1d/include -o "%{cfg.objdir}/%{file.basename}_asm.obj" "%{file.relpath}"'
      }
    filter {}

    dav1d_files()
    filter {'platforms:x32 or x64 or x64_asan'}
    dav1d_x68_files()
    filter {}

  project "zlib"
    kind "StaticLib"
    language "C"
    optimized_conf()
    disablewarnings { "4131", "4244", "4245", "4267", "4996" }
    zlib_files()

  -- to make Visual Studio solution smaller
  -- combine 9 libs only used by mupdf into a single project
  -- instead of having 9 projects

  project "mupdf-libs"
    kind "StaticLib"
    language "C"
    optimized_conf()

    -- zlib
    -- disablewarnings { "4131", "4244", "4245", "4267", "4996" }
    -- zlib_files()

    -- libjpeg-turbo
    defines { "_CRT_SECURE_NO_WARNINGS" }
    disablewarnings { "4018", "4100", "4244", "4245", "4819" }
    includedirs { "ext/libjpeg-turbo", "ext/libjpeg-turbo/simd" }
    -- nasm.exe -I .\ext\libjpeg-turbo\simd\
    -- -I .\ext\libjpeg-turbo\win\ -f win32
    -- -o .\obj-rel\jpegturbo\jsimdcpu.obj
    -- .\ext\libjpeg-turbo\simd\jsimdcpu.asm
    filter {'files:**.asm', 'platforms:x32'}
       buildmessage '%{file.relpath}'
       buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
       buildcommands {
          '..\\bin\\nasm.exe -f win32 -I ../ext/libjpeg-turbo/simd/ -I ../ext/libjpeg-turbo/win/ -o "%{cfg.objdir}/%{file.basename}.obj" "%{file.relpath}"'
       }
    filter {}
    filter {'files:**.asm', 'platforms:x64 or x64_asan'}
      buildmessage '%{file.relpath}'
      buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
      buildcommands {
        '..\\bin\\nasm.exe -f win64 -D__x86_64__ -DWIN64 -DMSVC -I ../ext/libjpeg-turbo/simd/ -I ../ext/libjpeg-turbo/win/ -o "%{cfg.objdir}/%{file.basename}.obj" "%{file.relpath}"'
      }
    filter {}
    libjpeg_turbo_files()

    -- jbig2dec
    defines { "_CRT_SECURE_NO_WARNINGS", "HAVE_STRING_H=1", "JBIG_NO_MEMENTO" }
    disablewarnings { "4018", "4100", "4146", "4244", "4267", "4456", "4701" }
    includedirs { "ext/jbig2dec" }
    jbig2dec_files()

    -- openjpeg
    disablewarnings { "4100", "4244", "4310", "4389", "4456" }
    -- openjpeg has opj_config_private.h for such over-rides
    -- but we can't change it because we bring openjpeg as submodule
    -- and we can't provide our own in a different directory because
    -- msvc will include the one in ext/openjpeg/src/lib/openjp2 first
    -- because #include "opj_config_private.h" searches current directory first
    defines { "_CRT_SECURE_NO_WARNINGS", "USE_JPIP", "OPJ_STATIC", "OPJ_EXPORTS" }
    openjpeg_files()

    -- freetype
    defines {
      "FT2_BUILD_LIBRARY",
      "FT_CONFIG_MODULES_H=\"slimftmodules.h\"",
      "FT_CONFIG_OPTIONS_H=\"slimftoptions.h\"",
    }
    disablewarnings { "4018", "4100", "4101", "4244", "4267", "4312", "4701", "4706", "4996" }
    includedirs { "mupdf/scripts/freetype", "ext/freetype/include" }
    freetype_files()

    -- lcms2
    disablewarnings { "4100" }
    includedirs { "ext/lcms2/include" }
    lcms2_files()

    -- harfbuzz
    includedirs { "ext/harfbuzz/src/hb-ucdn", "mupdf/scripts/freetype", "ext/freetype/include" }
    defines {
      --"_CRT_SECURE_NO_WARNINGS",
      "HAVE_FALLBACK=1",
      "HAVE_OT",
      "HAVE_UCDN",
      "HAVE_FREETYPE",
      "HB_NO_MT",
      "hb_malloc_impl=fz_hb_malloc",
      "hb_calloc_impl=fz_hb_calloc",
      "hb_realloc_impl=fz_hb_realloc",
      "hb_free_impl=fz_hb_free"
    }
    disablewarnings { "4805", "4100", "4146", "4244", "4245", "4267", "4456", "4457", "4459", "4701", "4702", "4706" }
    harfbuzz_files()

    -- mujs
    includedirs { "ext/mujs" }
    disablewarnings { "4090", "4100", "4310", "4702", "4706" }
    files { "ext/mujs/one.c", "ext/mujs/mujs.h" }

    -- gumbo
    disablewarnings { "4018", "4100", "4132", "4204", "4244", "4245", "4267",
    "4305", "4306", "4389", "4456", "4701" }
    includedirs { "ext/gumbo-parser/include", "ext/gumbo-parser/visualc/include" }
    gumbo_files()

    -- extract
    disablewarnings { "4005", "4201", "4130" }
    includedirs { "ext/extract/include" }
    uses_zlib()
    extract_files()

    -- iibheif
    defines { "LIBHEIF_STATIC_BUILD" }

  function fonts()
    files {

      "mupdf/resources/fonts/urw/Dingbats.cff",
      "mupdf/resources/fonts/urw/NimbusMonoPS-Regular.cff",
      "mupdf/resources/fonts/urw/NimbusMonoPS-Italic.cff",
      "mupdf/resources/fonts/urw/NimbusMonoPS-Bold.cff",
      "mupdf/resources/fonts/urw/NimbusMonoPS-BoldItalic.cff",
      "mupdf/resources/fonts/urw/NimbusRoman-Regular.cff",
      "mupdf/resources/fonts/urw/NimbusRoman-Italic.cff",
      "mupdf/resources/fonts/urw/NimbusRoman-Bold.cff",
      "mupdf/resources/fonts/urw/NimbusRoman-BoldItalic.cff",
      "mupdf/resources/fonts/urw/NimbusSans-Regular.cff",
      "mupdf/resources/fonts/urw/NimbusSans-Italic.cff",
      "mupdf/resources/fonts/urw/NimbusSans-Bold.cff",
      "mupdf/resources/fonts/urw/NimbusSans-BoldItalic.cff",
      "mupdf/resources/fonts/urw/StandardSymbolsPS.cff",
      "mupdf/resources/fonts/droid/DroidSansFallbackFull.ttf",
      "mupdf/resources/fonts/sil/CharisSIL.cff",
      "mupdf/resources/fonts/sil/CharisSIL-Bold.cff",
      "mupdf/resources/fonts/sil/CharisSIL-Italic.cff",
      "mupdf/resources/fonts/sil/CharisSIL-BoldItalic.cff",

      "mupdf/resources/fonts/noto/NotoSans-Regular.otf",
      "mupdf/resources/fonts/noto/NotoSansMath-Regular.otf",
      "mupdf/resources/fonts/noto/NotoSansSymbols-Regular.otf",
      "mupdf/resources/fonts/noto/NotoSansSymbols2-Regular.otf",
      "mupdf/resources/fonts/noto/NotoEmoji-Regular.ttf",
      "mupdf/resources/fonts/noto/NotoMusic-Regular.otf",
      "mupdf/resources/fonts/noto/NotoSerif-Regular.otf",
    }

    filter {'files:**.cff'}
       buildmessage  '..\\bin\\bin2coff.exe "%{file.relpath}" "%{cfg.objdir}/%{file.basename}.obj" _binary_%{file.basename}_cff %{cfg.architecture}'
       buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
       buildcommands {
          '..\\bin\\bin2coff.exe "%{file.relpath}" "%{cfg.objdir}/%{file.basename}.obj" _binary_%{file.basename}_cff %{cfg.architecture}'
       }
    filter {}

    filter {'files:**.ttf'}
       buildmessage '..\\bin\\bin2coff.exe "%{file.relpath}" "%{cfg.objdir}/%{file.basename}.obj" _binary_%{file.basename}_ttf %{cfg.architecture}'
       buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
       buildcommands {
          '..\\bin\\bin2coff.exe "%{file.relpath}" "%{cfg.objdir}/%{file.basename}.obj" _binary_%{file.basename}_ttf %{cfg.architecture}'
       }
    filter {}

    filter {'files:**.otf'}
       buildmessage '..\\bin\\bin2coff.exe "%{file.relpath}" "%{cfg.objdir}/%{file.basename}.obj" _binary_%{file.basename}_otf %{cfg.architecture}'
       buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
       buildcommands {
          '..\\bin\\bin2coff.exe "%{file.relpath}" "%{cfg.objdir}/%{file.basename}.obj" _binary_%{file.basename}_otf %{cfg.architecture}'
       }
    filter {}
  end

  project "mupdf"
    kind "StaticLib"
    language "C"
    mixed_dbg_rel_conf()
    -- for openjpeg, OPJ_STATIC is alrady defined in load-jpx.c
    -- so we can't double-define it
    defines { "USE_JPIP", "OPJ_EXPORTS", "HAVE_LCMS2MT=1" }
    defines { "OPJ_STATIC", "SHARE_JPEG" }
    -- this defines which fonts are to be excluded from being included directly
    -- we exclude the very big cjk fonts
    defines { "TOFU_NOTO", "TOFU_CJK_LANG", "TOFU_NOTO_SUMATRA" }
    defines { "FZ_ENABLE_SVG=1", "FZ_ENABLE_BROTLI=0", "FZ_ENABLE_BARCODE=0" }

    filter { "platforms:arm64" }
        defines { "ARCH_HAS_NEON=1" }
    filter {}

    disablewarnings {
      "4005", "4018", "4057", "4100", "4115", "4130", "4132", "4200", "4204", "4206", "4210",
      "4245", "4267", "4295", "4305", "4389", "4456", "4457", "4703", "4706", "4819", "5286"
    }
    -- force including mupdf/scripts/openjpeg/opj_config_private.h
    -- with our build over-rides

    uses_zlib()
    includedirs {
      "mupdf/include",
      "mupdf/generated",
      "ext/jbig2dec",
      "ext/libjpeg-turbo",
      "ext/openjpeg/src/lib/openjp2",
      "mupdf/scripts/freetype",
      "ext/freetype/include",
      "ext/mujs",
      "ext/harfbuzz/src",
      "ext/lcms2/include",
      "ext/gumbo-parser/src",
      "ext/extract/include",
    }
    fonts()

    mupdf_files()
    links { "mupdf-libs" }
    -- links { "mupdf-libs", "zlib", "freetype", "openjpeg", "libjpeg-turbo", "jbig2dec", "lcms2", "harfbuzz", "mujs", "gumbo" }

    -- mupdf
    -- this fixes "NAN" is not a constant in some version of msvc
    -- without this it's #define _UCRT_NAN (__ucrt_int_to_float(0x7FC00000))
    defines { "_UCRT_NOISY_NAN" }

  project "libmupdf"
    kind "SharedLib"
    language "C"
    optimized_conf()
    disablewarnings { "4206", "4702" }
    defines { "FZ_ENABLE_SVG=1", "FZ_ENABLE_BROTLI=0", "FZ_ENABLE_BARCODE=0" }

    filter { "platforms:arm64" }
        defines { "ARCH_HAS_NEON=1" }
    filter {}

    -- premake has logic in vs2010_vcxproj.lua that only sets PlatformToolset
    -- if there is a c/c++ file, so we add a no-op cpp file to force This logic
    files { "src/libmupdf.rc", "src/libmupdf.def", "src/no_op_for_premake.cpp" }
    implibname "libmupdf"
    -- TODO: is thre a better way to do it?
    -- linkoptions { "/DEF:..\\src\\libmupdf.def", "-IGNORE:4702" }
    linkoptions { "-IGNORE:4702" }
    links_zlib()
    links { "mupdf", "libdjvu", "libwebp", "dav1d", "libheif", "unarrlib" }
    links {
      "advapi32", "kernel32", "user32", "gdi32", "comdlg32",
      "shell32", "windowscodecs", "comctl32", "msimg32",
      "winspool", "wininet", "urlmon", "gdiplus", "ole32",
      "oleAut32", "shlwapi", "version", "crypt32"
    }

  project "utils"
    kind "StaticLib"
    language "C++"
    cppdialect "C++latest"
    mixed_dbg_rel_conf()
    warnings_as_errors()
    filter "configurations:ReleaseAnalyze"
      -- TODO: somehow /analyze- is default which creates warning about
      -- over-ride from cl.exe. Don't know how to disable the warning
      buildoptions { "/analyze" }
      disablewarnings { "28125", "28252", "28253" }
    filter {}

    -- QITABENT in shlwapi.h has incorrect definition and causes 4838
    disablewarnings { "4100", "4267", "4457", "4838" }
    uses_zlib()
    defines { "LIBHEIF_STATIC_BUILD" }
    includedirs { "src", "ext/lzma/C" }
    includedirs { "ext/libheif", "ext/libwebp/src", "ext/dav1d/include", "ext/unarr", "mupdf/include" }
    utils_files()

  ---- executables

  project "signfile"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"
    mixed_dbg_rel_conf()
    includedirs { "src", "mupdf/include"}
    files { "src/tools/signfile.cpp", "src/CrashHandlerNoOp.cpp" }
    links { "utils", "mupdf" }
    links { "crypt32", "shlwapi", "version", "Comctl32", "wininet", "wintrust" }


  project "plugin-test"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++latest"
    mixed_dbg_rel_conf()
    entrypoint "WinMainCRTStartup"
    includedirs { "src" }
    plugin_test_files()
    links { "utils", "mupdf" }
    links { "shlwapi", "version", "comctl32", "wininet", "wintrust" }

  project "test_util"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"
    mixed_dbg_rel_conf()
    disablewarnings { "4838" }
    includedirs { "src" }
    test_util_files()
    links { "gdiplus", "comctl32", "shlwapi", "Version", "wininet", "shcore", "wintrust", "crypt32" }

  project "sizer"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"
    mixed_dbg_rel_conf()
    disablewarnings { "4996", "4706", "4100", "4505" }
    includedirs { "tools/sizer" }
    sizer_files()
    links { "ole32.lib", "oleaut32.lib" }

  project "bin2coff"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"
    mixed_dbg_rel_conf()
    disablewarnings { "4200", "4838" }
    includedirs { "src" }
    bin2coff_files()
    links { "gdiplus", "comctl32", "shlwapi", "Version" }

  project "PdfFilter"
    kind "SharedLib"
    language "C++"
    cppdialect "C++latest"
    mixed_dbg_rel_conf()
    disablewarnings { "4100", "4838" }
    filter {"configurations:Debug"}
      defines { "BUILD_TEX_IFILTER", "BUILD_EPUB_IFILTER" }
    filter {}
    includedirs { "src", "src/wingui", "mupdf/include" }
    search_filter_files()
    links { "utils", "unrar", "libmupdf" }
    links { "comctl32", "gdiplus", "shlwapi", "version", "wininet", "wintrust" }

  project "PdfPreview"
    kind "SharedLib"
    language "C++"
    cppdialect "C++latest"
    mixed_dbg_rel_conf()
    disablewarnings { "4100", "4838" }
    includedirs {
      "src", "src/wingui", "mupdf/include",
      "ext/libdjvu", "ext/CHMLib"
    }
    pdf_preview_files()
    filter {"configurations:Debug"}
      -- defines { "BUILD_XPS_PREVIEW" }
      -- TODO: use EngineMupdf for XPS
      defines {
        "BUILD_EPUB_PREVIEW",
        "BUILD_FB2_PREVIEW", "BUILD_MOBI_PREVIEW"
      }
    filter {}
    -- TODO: "chm" should only be for Debug config but doing links { "chm" }
    -- in the filter breaks linking by setting LinkLibraryDependencies to false
    links { "utils", "unrar", "libmupdf", "chm" }
    links { "comctl32", "gdiplus", "msimg32", "shlwapi", "version", "wininet", "wintrust" }

    project "PdfPreviewTest"
      kind "ConsoleApp"
      language "C++"
      cppdialect "C++latest"
      mixed_dbg_rel_conf()
      disablewarnings { "4838" }
      includedirs { "src" }
      preview_test_files()
      links { "gdiplus", "comctl32", "shlwapi", "Version", "Ole32" }
      dependson { "PdfPreview" }

  -- a single static executable
  project "SumatraPDF"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++latest"
    mixed_dbg_rel_conf()
    warnings_as_errors()
    entrypoint "WinMainCRTStartup"
    flags { "NoManifest" }
    includedirs { "src", "mupdf/include" }
    includedirs { "ext/synctex", "ext/libdjvu", "ext/CHMLib" }

    includedirs { "ext/darkmodelib/include" }
    defines { "_DARKMODELIB_NO_INI_CONFIG" }
    darkmodelib_files()

    webview_conf()

    synctex_files()
    mui_files()
    wingui_files()
    uia_files()
    engines_files()
    sumatrapdf_files()

    defines { "_CRT_SECURE_NO_WARNINGS" }
    defines { "DISABLE_DOCUMENT_RESTRICTIONS" }

    filter "configurations:ReleaseAnalyze"
      -- TODO: somehow /analyze- is default which creates warning about
      -- over-ride from cl.exe. Don't know how to disable the warning
      buildoptions { "/analyze" }
      disablewarnings { "28125", "28252", "28253" }
    filter {}

    -- for synctex
    disablewarnings { "4100", "4244", "4267", "4702", "4706", "4819" }
    uses_zlib()
    includedirs { "ext/synctex" }

    -- for uia
    disablewarnings { "4302", "4311", "4838" }

    links_zlib()
    links {
      "libdjvu",  "libwebp", "dav1d", "libheif", "mupdf", "unarrlib", "utils", "unrar", "chm"
    }
    links {
      "comctl32", "delayimp", "gdiplus", "msimg32", "shlwapi", "urlmon",
      "version", "windowscodecs", "wininet", "uiautomationcore.lib", "uxtheme", "wintrust"
    }
    -- this is to prevent dll hijacking
    linkoptions { "/DELAYLOAD:gdiplus.dll /DELAYLOAD:msimg32.dll /DELAYLOAD:shlwapi.dll" }
    linkoptions { "/DELAYLOAD:urlmon.dll /DELAYLOAD:wininet.dll" }
    linkoptions { "/DELAYLOAD:uiautomationcore.dll" }
    filter "platforms:x64_asan"
      linkoptions { "/INFERASANLIBS" }
    filter {}
    -- dependson { "PdfFilter", "PdfPreview", "test_util" }

  -- a dll version where most functionality is in libmupdf.dll
  project "SumatraPDF-dll"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++latest"
    mixed_dbg_rel_conf()
    warnings_as_errors()
    entrypoint "WinMainCRTStartup"
    flags { "NoManifest" }
    includedirs { "src", "mupdf/include" }
    includedirs { "ext/synctex", "ext/libdjvu", "ext/CHMLib" }
    includedirs { "ext/darkmodelib/include" }

    includedirs { "ext/darkmodelib/include" }
    defines { "_DARKMODELIB_NO_INI_CONFIG" }
    darkmodelib_files()

    synctex_files()
    mui_files()
    wingui_files()
    uia_files()
    engines_files()
    sumatrapdf_files()

    webview_conf()

    defines { "_CRT_SECURE_NO_WARNINGS" }
    defines { "DISABLE_DOCUMENT_RESTRICTIONS" }

    filter "configurations:ReleaseAnalyze"
      -- TODO: somehow /analyze- is default which creates warning about
      -- over-ride from cl.exe. Don't know how to disable the warning
      buildoptions { "/analyze" }
      disablewarnings { "28125", "28252", "28253" }
    filter {}

    -- for synctex
    disablewarnings { "4100", "4244", "4267", "4702", "4706" }
    uses_zlib()
    includedirs { "ext/synctex" }

    -- for uia
    disablewarnings { "4302", "4311", "4838" }

    disablewarnings { "4819" }

    resdefines { "INSTALL_PAYLOAD_ZIP=.\\%{cfg.targetdir}\\InstallerData.dat" }

    files { "src/MuPDF_Exports.cpp" }

    links {
      "libmupdf", "unrar", "unarrlib", "utils", "chm"
    }
    links {
      "comctl32", "delayimp", "gdiplus", "msimg32", "shlwapi", "urlmon",
      "version", "wininet", "d2d1.lib", "uiautomationcore.lib", "uxtheme", "wintrust", "crypt32"
    }
    -- this is to prevent dll hijacking
    linkoptions { "/DELAYLOAD:libmupdf.dll" }
    linkoptions { "/DELAYLOAD:gdiplus.dll /DELAYLOAD:msimg32.dll /DELAYLOAD:shlwapi.dll" }
    linkoptions { "/DELAYLOAD:urlmon.dll /DELAYLOAD:wininet.dll" }
    linkoptions { "/DELAYLOAD:uiautomationcore.dll" }
    dependson { "PdfFilter", "PdfPreview", "test_util" }
    prebuildcommands { "cd %{cfg.targetdir} & ..\\..\\bin\\MakeLZSA.exe InstallerData.dat libmupdf.dll:libmupdf.dll PdfFilter.dll:PdfFilter.dll PdfPreview.dll:PdfPreview.dll"  }

workspace "MakeLZSA"
  configurations { "Debug", "Release" }
  platforms { "x32", "x64", "arm64", "x64_asan" }
  startproject "MakeLZSA"

  filter "platforms:x32"
      architecture "x86"
  filter {}

  filter "platforms:x64"
      architecture "x86_64"
      -- strangely this is not set by default for rc.exe
      resdefines { "_WIN64" }
  filter {}

  disablewarnings { "4127", "4189", "4324", "4458", "4522", "4611", "4702", "4800", "6319" }

  location "this_is_invalid_location"

  filter "action:vs2022"
    location "vs2022"
  filter {}

  clang_conf()
  conf_2026()

  filter {"platforms:x32", "configurations:Release"}
    targetdir "out/rel32"
  filter {"platforms:x32", "configurations:Debug"}
    targetdir "out/dbg32"
  filter {}

  filter {"platforms:x64", "configurations:Release"}
    targetdir "out/rel64"
  filter {"platforms:x64", "configurations:Debug"}
    targetdir "out/dbg64"
  filter {}

  objdir "%{cfg.targetdir}/obj"

  -- https://github.com/premake/premake-core/wiki/symbols
  -- https://blogs.msdn.microsoft.com/vcblog/2016/10/05/faster-c-build-cycle-in-vs-15-with-debugfastlink/
  symbols "Full"
  staticruntime  "On"
  -- https://github.com/premake/premake-core/wiki/flags

  fatalwarnings { "All" }
  flags {
    "MultiProcessorCompile",
    "Maps", -- generate map file
    --"Unicode",
  }

  defines {
    "WIN32",
    "_WIN32",
    -- https://docs.microsoft.com/en-us/cpp/porting/modifying-winver-and-win32-winnt?view=vs-2019
    "WINVER=0x0605", -- latest Windows SDK
    "_WIN32_WINNT=0x0603"
  }

  project "MakeLZSA"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"
    mixed_dbg_rel_conf()

    makelzsa_files()
    disablewarnings { "4131", "4244", "4245", "4267", "4996" }
    includedirs { "src", "ext/lzma/C", "ext/unarr" }

    -- for zlib
    disablewarnings { "4131", "4244", "4245", "4267", "4996" }
    zlib_files()
    uses_zlib()

    -- unarrlib
    -- TODO: for bzip2, need BZ_NO_STDIO and BZ_DEBUG=0
    -- TODO: for lzma, need _7ZIP_PPMD_SUPPPORT
    defines { "HAVE_ZLIB", "HAVE_BZIP2", "HAVE_7Z", "BZ_NO_STDIO", "_7ZIP_PPMD_SUPPPORT" }
    -- TODO: most of these warnings are due to bzip2 and lzma
    disablewarnings { "4100", "4244", "4267", "4456", "4457", "4996" }
    includedirs { "ext/bzip2", "ext/lzma/C" }
    unarr_files()
    links { "shlwapi", "version", "comctl32", "wininet" }
