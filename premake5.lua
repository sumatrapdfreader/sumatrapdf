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
 4005 - macro redefinition
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

-- TODO: test this option
-- usestandardpreprocessor 'On'

-- TODO: try appmanifest
-- files { "hello.appxmanifest" }
-- https://github.com/premake/premake-core/pull/1750/files

include("premake5.files.lua")


-- this is meant to make the binary win7 compatible but
-- does't seem to work
function winver7_defines()
  defines {
    "WIN32",
    "_WIN32",
    -- https://docs.microsoft.com/en-us/cpp/porting/modifying-winver-and-win32-winnt?view=vs-2019
    "WINVER=0x0601", -- force not using API not available in Win7
    "_WIN32_WINNT=0x0601",
     "NTDDI_VERSION=0x06010000"
  }

  -- v143 is the last that supports windows 7
  toolset "v143"   -- this is the official way in recent Premake versions
end

function winver_latest_defines()
  defines {
    "WIN32",
    "_WIN32",
    -- https://docs.microsoft.com/en-us/cpp/porting/modifying-winver-and-win32-winnt?view=vs-2019
    "WINVER=0x0605",
    "_WIN32_WINNT=0x0603"
  }
end

function winver_defines()
  winver7_defines()
  -- winver_latest_defines()
end

-- WebView2 (manual viewer, SimpleBrowserWindow, BrowserDocView). wingui/*.cpp is
-- included via wingui_files() in premake5.files.lua.
function webview_conf()
  includedirs { "packages/Microsoft.Web.WebView2.1.0.4022.49/build/native/include" }
  filter "platforms:x86"
  libdirs { "packages/Microsoft.Web.WebView2.1.0.4022.49/build/native/x86" }
  filter "platforms:x64 or x64_asan"
  libdirs { "packages/Microsoft.Web.WebView2.1.0.4022.49/build/native/x64" }
  filter "platforms:arm64"
  libdirs { "packages/Microsoft.Web.WebView2.1.0.4022.49/build/native/arm64" }
  filter {}
  links { "WebView2LoaderStatic.lib" }
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

  -- asan builds: no ltcg
  filter { "configurations:Release*", "platforms:x86 or x64 or arm64" }
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

  filter { "configurations:Release*", "platforms:x86 or x64 or arm64" }
  linktimeoptimization "On"
  filter {}
end

-- override the size optimization (set by optimized_conf / mixed_dbg_rel_conf)
-- with speed optimization for Release builds. Use for CPU-bound codec/render
-- libraries where decode/render speed matters more than a few KB of code size.
function favor_speed()
  filter "configurations:Release*"
  optimize "Speed"
  filter {}
end

-- per-workspace setting that differ in clang-cl.exe vs cl.exe builds
function clang_conf()
  filter "options:with-clang"
      location "vs2022-clang"
      toolset "clang"
      buildoptions { "-fms-compatibility", "-fms-extensions", "-Wno-microsoft-include", "-march=x86-64-v3", "-maes" }

      warnings "Off"
      exceptionhandling "On"
  filter {}

  filter { 'options:not with-clang' }
      warnings "Extra"
      exceptionhandling "Off"
  filter {}
end

function warnings_as_errors()
  filter { "configurations:not ReleaseAnalyze", "options:not with-clang" }
      fatalwarnings { "All" }
  filter {}
end

function zlib_defines()
  includedirs {
    "ext/a-zlib",
  }
end

-- add to a project that links zlib
function links_zlib()
  -- links { "zlib-ng" }
  links { "a-zlib" }
end

-- add to a project that needs to see zlib headers
function uses_zlib()
  --zlib_ng_defines()
  zlib_defines()
end

-- out/<cfg>/ holds only shipping binaries (*.exe, *.dll, *.pdb). Everything
-- else (.obj, .lib, .map, .exp, .ilk, static-lib .pdb, ...) lives under obj-s
-- (static SumatraPDF.exe chain) or obj (SumatraPDF-dll / libmupdf chain) so the
-- two flavors never share intermediates.
local function for_each_out_config(fn)
  fn("platforms:x86", "configurations:Release", "out/rel32")
  fn("platforms:x86", "configurations:ReleaseAnalyze", "out/rel32_prefast")
  fn("platforms:x86", "configurations:Debug", "out/dbg32")
  fn("platforms:x86", "configurations:DebugFull", "out/dbgfull32")
  fn("platforms:x64", "configurations:Release", "out/rel64")
  fn("platforms:x64", "configurations:ReleaseAnalyze", "out/rel64_prefast")
  fn("platforms:x64", "configurations:Debug", "out/dbg64")
  fn("platforms:x64", "configurations:DebugFull", "out/dbgfull64")
  fn("platforms:x64_asan", "configurations:Release", "out/rel64_asan")
  fn("platforms:x64_asan", "configurations:ReleaseAnalyze", "out/rel64_prefast_asan")
  fn("platforms:x64_asan", "configurations:Debug", "out/dbg64_asan")
  fn("platforms:x64_asan", "configurations:DebugFull", "out/dbgfull64_asan")
  fn("platforms:arm64", "configurations:Release", "out/arm64")
  fn("platforms:arm64", "configurations:ReleaseAnalyze", "out/arm64_prefast")
  fn("platforms:arm64", "configurations:Debug", "out/dbgarm64")
  fn("platforms:arm64", "configurations:DebugFull", "out/dbgfullarm64")
  filter {}
end

-- Premake normally appends %{cfg.platform}/%{cfg.buildcfg}/%{prj.name} to objdir;
-- the ! prefix disables that because out/<cfg> already encodes platform + config.
local function project_objdir(outDir, subdir)
  objdir("!" .. outDir .. "/" .. subdir .. "/%{prj.name}")
end

function static_intermediate_dirs()
  for_each_out_config(function(platform, config, outDir)
    filter { platform, config }
    targetdir(outDir .. "/obj-s")
    project_objdir(outDir, "obj-s")
  end)
end

function dll_intermediate_dirs()
  for_each_out_config(function(platform, config, outDir)
    filter { platform, config }
    targetdir(outDir .. "/obj")
    project_objdir(outDir, "obj")
  end)
end

function static_app_objdir()
  for_each_out_config(function(platform, config, outDir)
    filter { platform, config }
    project_objdir(outDir, "obj-s")
  end)
end

function dll_app_objdir()
  for_each_out_config(function(platform, config, outDir)
    filter { platform, config }
    project_objdir(outDir, "obj")
  end)
end

-- precompiled header for base/Base.h (~10% of compile time). base/Base.h is the
-- first include in almost every .cpp, so precompiling it speeds up clean builds.
-- Files that don't include it at all (external code, .c files) opt out via
-- enablepch "Off".
function setup_base_pch()
  pchheader "base/Base.h"
  pchsource "src/BasePch.cpp"
  files { "src/BasePch.cpp" }
  filter { "files:ext/**" }
    enablepch "Off"
  filter { "files:**.c" }
    enablepch "Off"
  filter {}
end

function static_linker_intermediates()
  mapfile "Off"
  for_each_out_config(function(platform, config, outDir)
    filter { platform, config }
    linkoptions('/IMPLIB:"../' .. outDir .. '/obj-s/%{prj.name}.lib"')
    linkoptions('/MAP:"../' .. outDir .. '/obj-s/%{prj.name}.map"')
  end)
end

function dll_linker_intermediates()
  mapfile "Off"
  for_each_out_config(function(platform, config, outDir)
    filter { platform, config }
    linkoptions('/IMPLIB:"../' .. outDir .. '/obj/%{prj.name}.lib"')
    linkoptions('/MAP:"../' .. outDir .. '/obj/%{prj.name}.map"')
  end)
end

-- SharedLib (.dll) projects: intermediates under obj/, but ship the .dll/.pdb
-- in out/<cfg>/.
function dll_shared_lib_dirs()
  dll_intermediate_dirs()
  mapfile "Off"
  for_each_out_config(function(platform, config, outDir)
    filter { platform, config }
    linkoptions('/OUT:"../' .. outDir .. '/%{prj.name}.dll"')
    linkoptions('/PDB:"../' .. outDir .. '/%{prj.name}.pdb"')
    linkoptions('/IMPLIB:"../' .. outDir .. '/obj/%{prj.name}.lib"')
    linkoptions('/MAP:"../' .. outDir .. '/obj/%{prj.name}.map"')
  end)
end

workspace "SumatraPDF"
  configurations { "Debug", "DebugFull", "Release", "ReleaseAnalyze", }
  platforms { "x86", "x64", "arm64", "x64_asan" }
  startproject "SumatraPDF"

  filter "platforms:x86"
    architecture "x86"
  filter {}

  filter "platforms:x64_asan"
    sanitize { "Address" }
    defines { "ASAN_BUILD=1" }
    incrementallink("Off")
    editandcontinue "Off"
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
  -- /utf-8 sets both source and execution charset to UTF-8
  -- fixes compilation on non-English Windows (e.g. Chinese) where
  -- default code page doesn't match source file encoding
  -- /we4840 makes passing Str/TempStr/WStr objects to printf-style varargs an error
  buildoptions { "/utf-8", "/we4840" }

  location "this_is_invalid_location"

  filter "action:vs2022"
    location "vs2022"
  filter {}

  clang_conf()

  filter { "platforms:x86", "configurations:Release" }
    targetdir "out/rel32"
  filter { "platforms:x86", "configurations:ReleaseAnalyze" }
    targetdir "out/rel32_prefast"
  filter { "platforms:x86", "configurations:Debug" }
    targetdir "out/dbg32"
  filter { "platforms:x86", "configurations:DebugFull" }
    targetdir "out/dbgfull32"

  filter { "platforms:x64", "configurations:Release" }
    targetdir "out/rel64"
  filter { "platforms:x64", "configurations:ReleaseAnalyze" }
    targetdir "out/rel64_prefast"
  filter { "platforms:x64", "configurations:Debug" }
    targetdir "out/dbg64"
  filter { "platforms:x64", "configurations:DebugFull" }
    targetdir "out/dbgfull64"
  filter {}

  filter { "platforms:x64_asan", "configurations:Release" }
    targetdir "out/rel64_asan"
  filter { "platforms:x64_asan", "configurations:ReleaseAnalyze" }
    targetdir "out/rel64_prefast_asan"
  filter { "platforms:x64_asan", "configurations:Debug" }
    targetdir "out/dbg64_asan"
  filter { "platforms:x64_asan", "configurations:DebugFull" }
    targetdir "out/dbgfull64_asan"
  filter {}

  filter { "platforms:arm64", "configurations:Release" }
    targetdir "out/arm64"
  filter { "platforms:arm64", "configurations:ReleaseAnalyze" }
    targetdir "out/arm64_prefast"
  filter { "platforms:arm64", "configurations:Debug" }
    targetdir "out/dbgarm64"
  filter { "platforms:arm64", "configurations:DebugFull" }
    targetdir "out/dbgfullarm64"
  filter {}

  -- https://github.com/premake/premake-core/wiki/symbols
  -- https://blogs.msdn.microsoft.com/vcblog/2016/10/05/faster-c-build-cycle-in-vs-15-with-debugfastlink/
  symbols "FastLink"
  filter { "configurations:Release" }
    symbols "Full"
  filter {}

  -- enable string pooling (/GF) in all configs; optimized builds get it via
  -- optimize, but debug builds don't, which would duplicate identical string
  -- literals (e.g. StrL("...")) per use site instead of merging them
  stringpooling "On"

  staticruntime "On"
  -- https://github.com/premake/premake-core/wiki/flags
  multiprocessorcompile("On")
  mapfile("On")
  rtti "Off"

  winver_defines()

  project "unrar"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C++"
    optimized_conf()
    defines { "UNRAR", "RARDLL", "SILENT" }
    -- os.hpp redefines WINVER, is there a better way?
    disablewarnings { "4005", "4100", "4201", "4211", "4244", "4310", "4389", "4456", "4459", "4505", "4701", "4702", "4706", "4709", "4731", "4828", "4996" }
    exceptionhandling "On"

    includedirs { "ext/unrar" }
    -- every unrar .cpp includes rar.hpp first; vendor rarpch.cpp creates the PCH
    pchheader "rar.hpp"
    pchsource "ext/unrar/rarpch.cpp"
    -- global.cpp defines INCLUDEGLOBAL before rar.hpp so ErrHandler is defined
    -- rather than extern; that must not use the shared PCH
    filter { "files:ext/unrar/global.cpp" }
      enablepch "Off"
    filter {}
    unrar_files()

  project "libdjvu"
    dll_intermediate_dirs()
    kind "StaticLib"
    characterset("MBCS")
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
    filter { "platforms:x64_asan" }
      defines { "DISABLE_MMX" }
    filter {}
    exceptionhandling "On"
    disablewarnings { "4100", "4189", "4244", "4267", "4302", "4311", "4312", "4505" }
    disablewarnings { "4456", "4457", "4459", "4701", "4702", "4703", "4706" }
    includedirs { "ext/libjpeg-turbo/src" }
    libdjvu_files()

  project "chm"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C"
    optimized_conf()
    defines { "_CRT_SECURE_NO_WARNINGS" }
    disablewarnings { "4018", "4244", "4267", "4996" }
    files { "ext/libchm/*.c", "ext/libchm/*.h" }

  -- cmark-gfm for markdown browser (MarkdownToc/MarkdownModel) and mupdf md.c.
  project "cmark-gfm"
    dll_intermediate_dirs()
    kind "StaticLib"
    language "C"
    optimized_conf()
    includedirs { "ext/cmark-gfm/src", "ext/cmark-gfm/extensions", "mupdf/scripts/cmark-gfm" }
    defines { "CMARK_GFM_STATIC_DEFINE", "_CRT_SECURE_NO_WARNINGS" }
    disablewarnings { "4013", "4018", "4090", "4100", "4101", "4127", "4130", "4132", "4146", "4201", "4204", "4232", "4244", "4245", "4267", "4305", "4306", "4310", "4312", "4389", "4456", "4457", "4459", "4505", "4701", "4702", "4706", "4805", "4819", "4996" }
    cmark_gfm_files()

  project "djvudec"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C"
    optimized_conf()
    -- the DjVu decoder is CPU-bound (ZP arithmetic decoder in the JB2 inner
    -- loop), so favor speed over size here (the rest of the tree uses /O1)
    optimize "Speed"
    defines { "_CRT_SECURE_NO_WARNINGS" }
    disablewarnings { "4018", "4101", "4244", "4267", "4996" }
    files { "ext/djvudec/djvu.c", "ext/djvudec/djvu.h" }

  -- zopfli / zopflipng: lossless PNG recompression, used to shrink PNGs we
  -- save (e.g. screenshots) on a background thread
  project "a-zopfli"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C++"
    optimized_conf()
    -- CPU-bound compression loops, favor speed over size
    optimize "Speed"
    defines { "_CRT_SECURE_NO_WARNINGS" }
    disablewarnings { "4018", "4100", "4127", "4244", "4267", "4305", "4334", "4457", "4459", "4477", "4530", "4702", "4996" }
    includedirs { "ext/a-zopfli" }
    files {
      "ext/a-zopfli/zopfli.cpp", "ext/a-zopfli/zopflipng/zopflipng_lib.h",
      "ext/a-zopfli/zopflipng/lodepng/lodepng.h", "ext/a-zopfli/version.txt",
    }

  project "libarchive"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C"
    optimized_conf()
    defines {
      "_CRT_SECURE_NO_WARNINGS",
      "LIBARCHIVE_STATIC",
      'PLATFORM_CONFIG_H="config_windows.h"',
    }
    warnings_as_errors()
    disablewarnings { "4018", "4054", "4055", "4090", "4098", "4100", "4127", "4130","4146", "4152", "4200", "4201", "4244", "4245", "4267", "4305", "4389", "4456", "4457", "4701", "4703", "4706", "4996" }
    uses_zlib()
    includedirs { "ext/libarchive/libarchive" }
    libarchive_files()
    -- bzip2 support for libarchive
    defines { "BZ_NO_STDIO" }
    includedirs { "ext/a-bzip2" }
    files { "ext/a-bzip2/bzip2.c", "ext/a-bzip2/bzlib.h", "ext/a-bzip2/version.txt" }
    -- LZMA files needed by LzmaSimpleArchive in utils
    includedirs { "ext/lzma/C" }
    files { "ext/lzma/C/LzmaDec.c", "ext/lzma/C/Bra86.c", "ext/lzma/C/Bra.c" }
    -- liblzma for LZMA/LZMA2/XZ decompression (needed for 7zip support in libarchive)
    defines { "HAVE_CONFIG_H", "LZMA_API_STATIC" }
    includedirs { "ext/liblzma/api", "ext/liblzma/common", "ext/liblzma/check",
                  "ext/liblzma/delta", "ext/liblzma/lz", "ext/liblzma/lzma",
                  "ext/liblzma/rangecoder", "ext/liblzma/simple", "ext/liblzma" }
    files {
      "ext/liblzma/common/alone_decoder.c",
      "ext/liblzma/common/auto_decoder.c",
      "ext/liblzma/common/block_decoder.c",
      "ext/liblzma/common/block_header_decoder.c",
      "ext/liblzma/common/block_util.c",
      "ext/liblzma/common/common.c",
      "ext/liblzma/common/filter_common.c",
      "ext/liblzma/common/filter_decoder.c",
      "ext/liblzma/common/filter_flags_decoder.c",
      "ext/liblzma/common/index.c",
      "ext/liblzma/common/index_decoder.c",
      "ext/liblzma/common/index_hash.c",
      "ext/liblzma/common/stream_decoder.c",
      "ext/liblzma/common/stream_flags_common.c",
      "ext/liblzma/common/stream_flags_decoder.c",
      "ext/liblzma/common/vli_decoder.c",
      "ext/liblzma/common/vli_size.c",
      "ext/liblzma/check/check.c",
      "ext/liblzma/check/crc32_fast.c",
      "ext/liblzma/check/crc64_fast.c",
      "ext/liblzma/lz/lz_decoder.c",
      "ext/liblzma/lzma/lzma_decoder.c",
      "ext/liblzma/lzma/lzma2_decoder.c",
      "ext/liblzma/rangecoder/price_table.c",
      "ext/liblzma/delta/delta_common.c",
      "ext/liblzma/delta/delta_decoder.c",
      "ext/liblzma/simple/simple_coder.c",
      "ext/liblzma/simple/simple_decoder.c",
      "ext/liblzma/simple/x86.c",
    }

  project "libwebp"
    static_intermediate_dirs()
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

  -- highway: SIMD dispatch library used by libjxl (ext/highway, v1.2.0)
  project "highway"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    optimized_conf()
    includedirs { "ext/highway" }
    disablewarnings { "4100", "4127", "4244", "4245", "4267", "4324", "4456", "4457", "4701", "4702", "4723", "5054", "4146", "4458" }
    highway_files()

  -- skcms: color management used by libjxl. Baseline only.
  project "a-skcms"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    optimized_conf()
    defines { "SKCMS_DISABLE_HSW", "SKCMS_DISABLE_SKX" }
    includedirs { "ext/a-skcms" }
    disablewarnings { "4100", "4201", "4244", "4245", "4267", "4310", "4456", "4701", "4702" }
    files { "ext/a-skcms/skcms.cpp", "ext/a-skcms/skcms.h", "ext/a-skcms/version.txt" }

  -- libjxl decoder (ext/libjxl, v0.11.2). Decoder subset only; uses skcms for
  -- color management and brotli for compressed metadata.
  project "libjxl"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    optimized_conf()
    exceptionhandling "On"
    rtti "On"
    defines { "JPEGXL_ENABLE_SKCMS=1", "JPEGXL_ENABLE_TRANSCODE_JPEG=0", "JPEGXL_BUNDLING_LIBJXL=1", "_CRT_SECURE_NO_WARNINGS" }
    includedirs { "ext/libjxl", "ext/libjxl/lib/include", "ext/highway", "ext/a-skcms", "ext/brotli/c/include" }
    disablewarnings { "4018", "4100", "4127", "4146", "4201", "4244", "4245", "4267", "4305", "4308", "4310", "4324", "4334", "4456", "4457", "4505", "4806", "4458", "4459", "4701", "4702", "4703", "4838", "4996", "5054" }
    buildoptions { "/bigobj" }
    libjxl_files()

  project "libheif"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C++"
    cppdialect "C++latest"
    optimized_conf()
    defines { "_CRT_SECURE_NO_WARNINGS", "HAVE_DAV1D", "LIBHEIF_STATIC_BUILD" }
    includedirs { "ext/libheif/libheif", "ext/libheif/libheif/api", "ext/dav1d/include" }
    disablewarnings { "4018", "4065", "4100", "4101", "4146", "4244", "4245", "4267", "4273", "4319", "4456", "4701", "4703", "4805", "4996" }
    -- TODO: I don't want RTTI and /EHsc
    rtti "On"
    buildoptions { "/EHsc" }
    libheif_files()

  project "dav1d"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C"
    optimized_conf()
    defines { "_CRT_SECURE_NO_WARNINGS" }
    -- 32-bit: C only (NASM x86 asm not worth it). x64: full SSE/AVX asm via NASM.
    filter { 'platforms:x86' }
      defines { "ARCH_X86_32=1", "ARCH_X86_64=0", "__SSE2__", "HAVE_ASM=0" }
    filter { 'platforms:x64 or x64_asan' }
      defines { "ARCH_X86_32=0", "ARCH_X86_64=1", "HAVE_ASM=1" }
    filter {}
    disablewarnings { "4057", "4090", "4100", "4152", "4200", "4201", "4244", "4245", "4389", "4456", "4457", "4701", "4703", "4706", "4819", "4996", "5287" }
    includedirs { "ext/dav1d/include/compat/msvc", "ext/dav1d", "ext/dav1d/include" }
    filter { 'files:**.asm', 'platforms:x64 or x64_asan' }
      buildmessage '%{file.relpath}'
      buildoutputs { '%{cfg.objdir}/%{file.basename}_asm.obj' }
      buildcommands {
        '..\\bin\\nasm.exe -f win64 -DARCH_X86_64=1 -DARCH_X86_32=0 -D__x86_64__ -DWIN64 -DMSVC -I ../ext/dav1d/src -I ../ext/dav1d/include -o "%{cfg.objdir}/%{file.basename}_asm.obj" "%{file.relpath}"'
      }
    filter {}
    dav1d_files()
    filter { 'platforms:x64 or x64_asan' }
      dav1d_x64_files()
    filter {}

  project "a-zlib"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C"
    optimized_conf()
    favor_speed()
    disablewarnings { "4005", "4131", "4244", "4245", "4267", "4996" }
    zlib_files()

  project "a-gumbo"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C"
    optimized_conf()
    disablewarnings { "4018", "4100", "4132", "4189", "4204", "4244", "4245", "4267",
      "4305", "4306", "4389", "4456", "4701", "4702" }
    includedirs { "ext/a-gumbo" }
    a_gumbo_files()

-- to make Visual Studio solution smaller
-- combine 9 libs only used by mupdf into a single project
-- instead of having 9 projects

  project "libjpeg-turbo"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C"
    optimized_conf()
    defines { "_CRT_SECURE_NO_WARNINGS" }
    disablewarnings { "4013", "4018", "4100", "4244", "4245", "4819" }
    includedirs { "ext/libjpeg-turbo/src" }
    -- libjpeg-turbo 3.x NASM SIMD: include dirs are simd/nasm (shared macros)
    -- and the per-arch dir (simd/i386 or simd/x86_64).
    filter { 'files:**.asm', 'platforms:x86' }
      buildmessage '%{file.relpath}'
      buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
      buildcommands {
        '..\\bin\\nasm.exe -f win32 -DWIN32 -I ../ext/libjpeg-turbo/simd/nasm/ -I ../ext/libjpeg-turbo/simd/i386/ -o "%{cfg.objdir}/%{file.basename}.obj" "%{file.relpath}"'
      }
    filter {}
    filter { 'files:**.asm', 'platforms:x64 or x64_asan' }
      buildmessage '%{file.relpath}'
      buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
      buildcommands {
        '..\\bin\\nasm.exe -f win64 -DWIN64 -D__x86_64__ -I ../ext/libjpeg-turbo/simd/nasm/ -I ../ext/libjpeg-turbo/simd/x86_64/ -o "%{cfg.objdir}/%{file.basename}.obj" "%{file.relpath}"'
      }
    filter {}
    libjpeg_turbo_files()

  project "a-jbig2dec"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C"
    optimized_conf()
    defines { "_CRT_SECURE_NO_WARNINGS", "HAVE_STRING_H=1", "JBIG_NO_MEMENTO" }
    disablewarnings { "4018", "4100", "4146", "4244", "4267", "4456", "4701" }
    includedirs { "ext/a-jbig2dec" }
    files { "ext/a-jbig2dec/jbig2dec.c", "ext/a-jbig2dec/jbig2.h", "ext/a-jbig2dec/version.txt" }

  project "a-openjpeg"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C"
    optimized_conf()
    disablewarnings { "4005", "4100", "4127", "4244", "4310", "4389", "4456", "4702" }
    defines { "_CRT_SECURE_NO_WARNINGS", "USE_JPIP", "OPJ_STATIC", "OPJ_EXPORTS" }
    includedirs { "ext/a-openjpeg" }
    files { "ext/a-openjpeg/openjpeg.c", "ext/a-openjpeg/*.h", "ext/a-openjpeg/version.txt" }

  project "freetype"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C"
    optimized_conf()
    defines {
      "FT2_BUILD_LIBRARY",
      "FT_CONFIG_MODULES_H=\"slimftmodules.h\"",
      "FT_CONFIG_OPTIONS_H=\"slimftoptions.h\"",
    }
    disablewarnings { "4018", "4100", "4101", "4244", "4267", "4312", "4701", "4706", "4996" }
    includedirs { "mupdf/scripts/freetype", "ext/freetype/include", "ext/brotli/c/include" }
    freetype_files()

  project "lcms2"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C"
    optimized_conf()
    disablewarnings { "4100", "4244" }
    includedirs { "ext/lcms2/include" }
    lcms2_files()

  project "harfbuzz"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C++"
    cppdialect "C++latest"
    optimized_conf()
    -- ext/harfbuzz/src is required so /Yu"hb.hh" and forceincludes can resolve
    -- hb.hh (sources also rely on same-dir includes for other headers).
    includedirs { "ext/harfbuzz/src", "ext/harfbuzz/src/hb-ucdn", "mupdf/scripts/freetype", "ext/freetype/include" }
    defines {
      "_CRT_SECURE_NO_WARNINGS",
      "HAVE_FALLBACK=1",
      "HAVE_OT",
      "HAVE_UCDN",
      "HAVE_FREETYPE",
      -- plain malloc/free wrappers (ext/mupdf_load_system_font.c) so that
      -- harfbuzz allocations don't depend on mupdf's thread-local fz_hb_secret
      -- context being set (it's NULL during atexit and when fz_hb_lock/unlock
      -- pairs nest via store scavenging)
      "hb_malloc_impl=sumatra_hb_malloc",
      "hb_calloc_impl=sumatra_hb_calloc",
      "hb_realloc_impl=sumatra_hb_realloc",
      "hb_free_impl=sumatra_hb_free"
    }
    filter "configurations:Debug or DebugFull"
    defines {
      "HAVE_ATEXIT",
    }
    filter {}
    disablewarnings { "4805", "4100", "4146", "4244", "4245", "4267", "4310", "4456", "4457", "4459", "4505", "4701", "4702", "4706", "4996" }
    -- precompiled header: hb.hh is re-parsed by every harfbuzz TU and dominates
    -- its compile time. forceincludes so MSVC /Yu finds the PCH marker even in
    -- TUs that include a secondary header first (which then pulls in hb.hh).
    pchheader "hb.hh"
    pchsource "src/HarfBuzzPch.cpp"
    files { "src/HarfBuzzPch.cpp" }
    forceincludes { "hb.hh" }
    harfbuzz_files()

  project "a-mujs"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C"
    optimized_conf()
    includedirs { "ext/a-mujs" }
    disablewarnings { "4090", "4100", "4146", "4310", "4702", "4706" }
    files { "ext/a-mujs/mujs.c", "ext/a-mujs/mujs.h", "ext/a-mujs/version.txt" }

  project "a-extract"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C"
    optimized_conf()
    disablewarnings {
      "4005", "4100", "4127", "4130", "4201", "4245", "4310", "4389", "4456", "4457", "4701", "4996"
    }
    includedirs { "ext/a-extract" }
    uses_zlib()
    files {
      "ext/a-extract/extract.c", "ext/a-extract/memento.h",
      "ext/a-extract/extract/*.h", "ext/a-extract/version.txt",
    }

  project "brotli"
    static_intermediate_dirs()
    kind "StaticLib"
    language "C"
    optimized_conf()
    disablewarnings { "4100", "4201" }
    includedirs { "ext/brotli/c/include" }
    brotli_files()

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

      filter { 'files:**.cff', 'platforms:x86' }
      buildmessage 'bin2coff %{file.basename}.cff (x86)'
      buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
      buildcommands {
        '..\\bin\\bin2coff.exe "%{file.relpath}" "%{cfg.objdir}/%{file.basename}.obj" _binary_%{file.basename}_cff x86'
      }
      filter { 'files:**.cff', 'platforms:x64 or x64_asan' }
      buildmessage 'bin2coff %{file.basename}.cff (x64)'
      buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
      buildcommands {
        '..\\bin\\bin2coff.exe "%{file.relpath}" "%{cfg.objdir}/%{file.basename}.obj" _binary_%{file.basename}_cff x86_64'
      }
      filter { 'files:**.cff', 'platforms:arm64' }
      buildmessage 'bin2coff %{file.basename}.cff (arm64)'
      buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
      buildcommands {
        '..\\bin\\bin2coff.exe "%{file.relpath}" "%{cfg.objdir}/%{file.basename}.obj" _binary_%{file.basename}_cff ARM64'
      }
      filter {}

      filter { 'files:**.ttf', 'platforms:x86' }
      buildmessage 'bin2coff %{file.basename}.ttf (x86)'
      buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
      buildcommands {
        '..\\bin\\bin2coff.exe "%{file.relpath}" "%{cfg.objdir}/%{file.basename}.obj" _binary_%{file.basename}_ttf x86'
      }
      filter { 'files:**.ttf', 'platforms:x64 or x64_asan' }
      buildmessage 'bin2coff %{file.basename}.ttf (x64)'
      buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
      buildcommands {
        '..\\bin\\bin2coff.exe "%{file.relpath}" "%{cfg.objdir}/%{file.basename}.obj" _binary_%{file.basename}_ttf x86_64'
      }
      filter { 'files:**.ttf', 'platforms:arm64' }
      buildmessage 'bin2coff %{file.basename}.ttf (arm64)'
      buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
      buildcommands {
        '..\\bin\\bin2coff.exe "%{file.relpath}" "%{cfg.objdir}/%{file.basename}.obj" _binary_%{file.basename}_ttf ARM64'
      }
      filter {}

      filter { 'files:**.otf', 'platforms:x86' }
      buildmessage 'bin2coff %{file.basename}.otf (x86)'
      buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
      buildcommands {
        '..\\bin\\bin2coff.exe "%{file.relpath}" "%{cfg.objdir}/%{file.basename}.obj" _binary_%{file.basename}_otf x86'
      }
      filter { 'files:**.otf', 'platforms:x64 or x64_asan' }
      buildmessage 'bin2coff %{file.basename}.otf (x64)'
      buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
      buildcommands {
        '..\\bin\\bin2coff.exe "%{file.relpath}" "%{cfg.objdir}/%{file.basename}.obj" _binary_%{file.basename}_otf x86_64'
      }
      filter { 'files:**.otf', 'platforms:arm64' }
      buildmessage 'bin2coff %{file.basename}.otf (arm64)'
      buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
      buildcommands {
        '..\\bin\\bin2coff.exe "%{file.relpath}" "%{cfg.objdir}/%{file.basename}.obj" _binary_%{file.basename}_otf ARM64'
      }
      filter {}
    end

  project "mupdf"
    static_intermediate_dirs()
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
    defines { "FZ_ENABLE_PDF=1", "FZ_ENABLE_SVG=1", "FZ_ENABLE_BROTLI=1", "FZ_ENABLE_BARCODE=0", "FZ_ENABLE_JS=1", "FZ_ENABLE_HYPHEN=0", "FZ_ENABLE_MD=1" }
    defines { "HAVE_LIBARCHIVE", "LIBARCHIVE_STATIC" }

    filter { "platforms:arm64" }
    defines { "ARCH_HAS_NEON=1" }
    filter {}

    disablewarnings {
      "4005", "4013", "4018", "4057", "4100", "4115", "4130", "4132", "4146", "4200", "4204", "4206", "4210",
      "4245", "4267", "4295", "4305", "4389", "4456", "4457", "4703", "4706", "4819", "5286"
    }
    -- force including mupdf/scripts/openjpeg/opj_config_private.h
    -- with our build over-rides

    uses_zlib()
    includedirs {
      "mupdf/include",
      "mupdf/generated",
      "ext/jbig2dec",
      "ext/libjpeg-turbo/src",
      "ext/a-openjpeg",
      "mupdf/scripts/freetype",
      "ext/freetype/include",
      "ext/a-mujs",
      "ext/brotli/c/include",
      "ext/cmark-gfm/src",
      "ext/cmark-gfm/extensions",
      "mupdf/scripts/cmark-gfm",
      "ext/harfbuzz/src",
      "ext/lcms2/include",
      "ext/a-gumbo",
      "ext/a-extract",
      "ext/libarchive",
    }
    fonts()

    mupdf_files()
    links {
      "cmark-gfm", "a-mujs", "a-extract", "harfbuzz", "freetype", "brotli",
      "lcms2", "a-openjpeg", "a-jbig2dec", "libjpeg-turbo", "libarchive", "a-gumbo"
    }

    -- mupdf
    -- this fixes "NAN" is not a constant in some version of msvc
    -- without this it's #define _UCRT_NAN (__ucrt_int_to_float(0x7FC00000))
    -- CMARK_GFM_STATIC_DEFINE: md.c includes cmark-gfm headers; we link the
    -- cmark-gfm static lib. FZ_ENABLE_MD defaults to 1
    -- in mupdf's config.h, enabling markdown support.
    defines { "_UCRT_NOISY_NAN", "CMARK_GFM_STATIC_DEFINE" }

  project "libmupdf"
    dll_shared_lib_dirs()
    kind "SharedLib"
    language "C"
    optimized_conf()
    disablewarnings { "4206", "4701", "4702" }
    defines { "FZ_ENABLE_PDF=1", "FZ_ENABLE_SVG=1", "FZ_ENABLE_BROTLI=1", "FZ_ENABLE_BARCODE=0", "FZ_ENABLE_JS=1", "FZ_ENABLE_HYPHEN=0", "FZ_ENABLE_MD=1" }

    filter { "platforms:arm64" }
    defines { "ARCH_HAS_NEON=1" }
    filter {}

    -- premake has logic in vs2010_vcxproj.lua that only sets PlatformToolset
    -- if there is a c/c++ file, so we add a no-op cpp file to force This logic
    files { "src/libmupdf.rc", "src/libmupdf.def", "src/no_op_for_premake.cpp" }
    implibname "libmupdf"
    -- TODO: is thre a better way to do it?
    -- linkoptions { "/DEF:..\\src\\libmupdf.def", "-IGNORE:4702" }
    linkoptions { "-IGNORE:4701", "-IGNORE:4702" }
    links_zlib()
    links { "mupdf", "libdjvu", "djvudec", "libwebp", "dav1d", "libheif", "libjxl", "highway", "a-skcms" }
    links {
      "advapi32", "kernel32", "user32", "gdi32", "comdlg32",
      "shell32", "windowscodecs", "comctl32", "msimg32",
      "winspool", "wininet", "urlmon", "gdiplus", "ole32",
      "oleAut32", "shlwapi", "version", "crypt32"
    }

  project "base"
    static_intermediate_dirs()
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
    disablewarnings { "4100", "4457", "4838" }
    uses_zlib()
    defines { "LIBARCHIVE_STATIC" }
    includedirs { "src", "ext/lzma/C", "ext/libarchive" }
    base_files()
    setup_base_pch()

---- executables

  project "test_util"
    dll_app_objdir()
    dll_linker_intermediates()
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"
    mixed_dbg_rel_conf()
    disablewarnings { "4838" }
    defines { "SUMATRA_TEST_UTIL=1" }
    includedirs { "src" }
    test_util_files()
    setup_base_pch()
    links { "gdiplus", "comctl32", "shlwapi", "Version", "wininet", "shcore", "wintrust", "crypt32" }

  project "test_engines"
    static_app_objdir()
    static_linker_intermediates()
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"
    mixed_dbg_rel_conf()
    disablewarnings { "4838" }
    includedirs { "src", "ext/djvudec", "ext/libarchive", "ext/unrar", "mupdf/include" }
    includedirs { "ext/libheif/libheif/api", "ext/libwebp/src", "ext/libjxl/lib/include" }
    defines { "LIBHEIF_STATIC_BUILD" }
    test_engines_files()
    links_zlib()
    links { "base", "djvudec", "libarchive", "unrar", "mupdf" }
    links { "libwebp", "dav1d", "libheif", "libjxl", "highway", "a-skcms" }
    links {
      "gdiplus", "gdi32", "user32", "comctl32", "shlwapi", "Version", "wininet",
      "shcore", "wintrust", "crypt32", "shell32", "ole32", "oleAut32", "urlmon",
      "windowscodecs"
    }

  project "bin2coff"
    static_app_objdir()
    static_linker_intermediates()
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"
    mixed_dbg_rel_conf()
    disablewarnings { "4200", "4838" }
    includedirs { "src" }
    bin2coff_files()
    links { "gdiplus", "comctl32", "shlwapi", "Version" }

  -- small console app that runs the mupdf command-line tools (draw, convert,
  -- info, ...). Console subsystem (so it works with cmd.exe / PowerShell) and
  -- links libmupdf.dll for everything, so the exe itself is tiny. It's embedded
  -- in SumatraPDF-dll.exe as a resource (see the InstallerData.dat prebuild).
  project "sumatrapdf-tool"
    dll_app_objdir()
    dll_linker_intermediates()
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"
    mixed_dbg_rel_conf()
    includedirs { "src" }
    sumatrapdf_tool_files()
    links { "libmupdf" }
    links { "shell32" }

  project "PdfFilter"
    dll_shared_lib_dirs()
    kind "SharedLib"
    language "C++"
    cppdialect "C++latest"
    mixed_dbg_rel_conf()
    disablewarnings { "4100", "4838" }
    defines { "HAVE_LIBARCHIVE", "LIBARCHIVE_STATIC" }
    filter { "configurations:Debug" }
    defines { "BUILD_TEX_IFILTER", "BUILD_EPUB_IFILTER" }
    filter {}
    includedirs { "src", "src/wingui", "mupdf/include", "ext/libarchive" }
    search_filter_files()
    links { "base", "unrar", "libmupdf", "libarchive" }
    links { "comctl32", "gdiplus", "shlwapi", "version", "wininet", "wintrust", "crypt32" }

  -- project "PdfFilter2"
  --   kind "SharedLib"
  --   language "C++"
  --   cppdialect "C++latest"
  --   mixed_dbg_rel_conf()
  --   disablewarnings { "4100", "4838" }
  --   includedirs { "src", "src/wingui" }
  --   search_filter2_files()
  --   links { "comctl32", "gdiplus", "shlwapi", "version", "wininet", "wintrust", "crypt32" }

  -- project "PdfPreview2"
  --   kind "SharedLib"
  --   language "C++"
  --   cppdialect "C++latest"
  --   mixed_dbg_rel_conf()
  --   disablewarnings { "4100", "4838" }
  --   includedirs {
  --     "src", "src/wingui"
  --   }
  --   pdf_preview2_files()
  --   -- TODO: "chm" should only be for Debug config but doing links { "chm" }
  --   -- in the filter breaks linking by setting LinkLibraryDependencies to false
  --   links { "comctl32", "gdiplus", "msimg32", "shlwapi", "version", "wininet", "wintrust", "crypt32" }

  project "PdfPreview"
    dll_shared_lib_dirs()
    kind "SharedLib"
    language "C++"
    cppdialect "C++latest"
    mixed_dbg_rel_conf()
    disablewarnings { "4100", "4838" }
    defines { "HAVE_LIBARCHIVE", "LIBARCHIVE_STATIC" }
    defines { "LIBHEIF_STATIC_BUILD" }
    includedirs {
      "src", "src/wingui", "mupdf/include",
      "ext/libdjvu", "ext/djvudec", "ext/libchm",
      "ext/libarchive",
      "ext/libheif/libheif/api", "ext/libwebp/src", "ext/libjxl/lib/include",
      "ext/brotli/c/include",
    }
    brotli_files()
    pdf_preview_files()
    -- TODO: "chm" should only be for Debug config but doing links { "chm" }
    -- in the filter breaks linking by setting LinkLibraryDependencies to false
    links { "base", "unrar", "libmupdf", "libarchive", "chm", "djvudec", "libwebp", "dav1d", "libheif", "libjxl", "highway", "a-skcms" }
    links { "comctl32", "gdiplus", "msimg32", "shlwapi", "version", "wininet", "wintrust", "crypt32" }

  -- a single static executable
  project "SumatraPDF-static"
    static_app_objdir()
    static_linker_intermediates()
    kind "WindowedApp"
    language "C++"
    cppdialect "C++latest"
    mixed_dbg_rel_conf()
    warnings_as_errors()
    entrypoint "WinMainCRTStartup"
    manifest("Off")
    defines { "LIBARCHIVE_STATIC", "LIBHEIF_STATIC_BUILD" }
    includedirs { "src", "mupdf/include" }
    includedirs { "ext/synctex", "ext/libdjvu", "ext/djvudec", "ext/libchm", "ext/libarchive", "ext/zopfli/src" }
    includedirs { "ext/cmark-gfm/src", "ext/cmark-gfm/extensions", "mupdf/scripts/cmark-gfm" }
    includedirs { "ext/libheif/libheif/api", "ext/libwebp/src", "ext/libjxl/lib/include" }

    -- MSVC's dynamic asan runtime ignores __asan_default_options/suppressions(),
    -- so asan options can only come from the environment.
    -- windows_hook_legacy_allocators=0: the Windows print dialog (PrintDlgEx ->
    -- Windows.Graphics.Printing / prntvpt) LocalAllocs/LocalFrees and GlobalLocks
    -- in ways asan's legacy-allocator interceptors mishandle, causing false-
    -- positive crashes; disabling that hooking routes them to the real allocators.
    -- the suppressions path must be quoted: asan parses ':' as flag separator, so
    -- an unquoted "C:\..." drive letter breaks parsing
    debugenvs { 'ASAN_OPTIONS=windows_hook_legacy_allocators=0:suppressions="$(SolutionDir)..\\asan.supp"' }

    includedirs { "ext/darkmodelib/include" }
    defines { "_DARKMODELIB_NO_INI_CONFIG" }
    darkmodelib_files()

    webview_conf()
    filter "configurations:Debug or DebugFull"
      files { "src/AppUnitTests.cpp" }
    filter {}

    synctex_files()
    mui_files()
    wingui_files()
    uia_files()
    engines_files()
    sumatrapdf_files()

    setup_base_pch()

    debugdir(".")

    defines { "_CRT_SECURE_NO_WARNINGS" }
    defines { "DISABLE_DOCUMENT_RESTRICTIONS" }
    defines { "CMARK_GFM_STATIC_DEFINE" }

    filter "configurations:ReleaseAnalyze"
    -- TODO: somehow /analyze- is default which creates warning about
    -- over-ride from cl.exe. Don't know how to disable the warning
    buildoptions { "/analyze" }
    disablewarnings { "28125", "28252", "28253" }
    filter {}

    -- for synctex
    disablewarnings { "4100", "4701", "4702", "4703", "4706", "4819", "6324" }
    -- 4244 (possible loss of data) is only needed by the external synctex
    -- code; keep it fatal for our own sources so silent int64->int truncation
    -- doesn't slip through.
    filter { "files:ext/synctex/**" }
      disablewarnings { "4244", "4267" }
    filter {}
    uses_zlib()
    includedirs { "ext/synctex" }

    -- for uia
    disablewarnings { "4302", "4311", "4838" }

    links_zlib()
    links {
      "djvudec", "libwebp", "dav1d", "libheif", "libjxl", "highway", "a-skcms", "mupdf", "libarchive", "base", "unrar", "chm", "a-zopfli"
    }
    links {
      "comctl32", "delayimp", "gdiplus", "msimg32", "shlwapi", "urlmon",
      "version", "windowscodecs", "wininet", "uiautomationcore.lib", "uxtheme", "wintrust", "crypt32"
    }
    -- this is to prevent dll hijacking
    linkoptions { "/DELAYLOAD:gdiplus.dll /DELAYLOAD:msimg32.dll /DELAYLOAD:shlwapi.dll" }
    linkoptions { "/DELAYLOAD:urlmon.dll /DELAYLOAD:wininet.dll" }
    linkoptions { "/DELAYLOAD:uiautomationcore.dll" }
    -- resolve static imports (uxtheme.dll etc.) from System32 only, so that
    -- a DLL planted next to the exe can't be side-loaded (seen in crash reports
    -- with our signed exe re-distributed alongside a fake UxTheme.dll).
    -- skip for x64_asan: ASan needs clang_rt.asan_dynamic-x86_64.dll beside the exe.
    filter "not platforms:x64_asan"
    linkoptions { "/DEPENDENTLOADFLAG:0x800" }
    filter "platforms:x64_asan"
    linkoptions { "/INFERASANLIBS" }
    filter {}
    dependson { "test_util" }
    prebuildcommands { "..\\bin\\MakeLZSA.exe ..\\translations\\translations.txt.lzsa ..\\translations\\translations-good.txt:translations-good.txt" }

  -- a dll version where most functionality is in libmupdf.dll
  project "SumatraPDF"
    dll_app_objdir()
    dll_linker_intermediates()
    kind "WindowedApp"
    language "C++"
    cppdialect "C++latest"
    mixed_dbg_rel_conf()
    warnings_as_errors()
    entrypoint "WinMainCRTStartup"
    manifest("Off")
    defines { "LIBARCHIVE_STATIC", "LIBHEIF_STATIC_BUILD" }
    includedirs { "src", "mupdf/include" }
    includedirs { "ext/synctex", "ext/libdjvu", "ext/djvudec", "ext/libchm", "ext/libarchive", "ext/zopfli/src" }
    includedirs { "ext/darkmodelib/include" }
    includedirs { "ext/libheif/libheif/api", "ext/libwebp/src", "ext/libjxl/lib/include" }

    -- MSVC's dynamic asan runtime ignores __asan_default_options/suppressions(),
    -- so asan options can only come from the environment.
    -- windows_hook_legacy_allocators=0: the Windows print dialog (PrintDlgEx ->
    -- Windows.Graphics.Printing / prntvpt) LocalAllocs/LocalFrees and GlobalLocks
    -- in ways asan's legacy-allocator interceptors mishandle, causing false-
    -- positive crashes; disabling that hooking routes them to the real allocators.
    -- the suppressions path must be quoted: asan parses ':' as flag separator, so
    -- an unquoted "C:\..." drive letter breaks parsing
    debugenvs { 'ASAN_OPTIONS=windows_hook_legacy_allocators=0:suppressions="$(SolutionDir)..\\asan.supp"' }

    includedirs { "ext/darkmodelib/include" }
    defines { "_DARKMODELIB_NO_INI_CONFIG" }
    darkmodelib_files()

    synctex_files()
    mui_files()
    wingui_files()
    uia_files()
    engines_files()
    sumatrapdf_files()
    filter "configurations:Debug or DebugFull"
      files { "src/AppUnitTests.cpp" }
    filter {}

    setup_base_pch()

    webview_conf()

    debugdir(".")

    defines { "_CRT_SECURE_NO_WARNINGS" }
    defines { "DISABLE_DOCUMENT_RESTRICTIONS" }

    filter "configurations:ReleaseAnalyze"
    -- TODO: somehow /analyze- is default which creates warning about
    -- over-ride from cl.exe. Don't know how to disable the warning
    buildoptions { "/analyze" }
    disablewarnings { "28125", "28252", "28253" }
    filter {}

    -- for synctex
    disablewarnings { "4100", "4701", "4702", "4703", "4706", "4819", "6324" }
    -- 4244/4267 are only needed by the external synctex code; keep them fatal
    -- for our own sources so silent truncation doesn't slip through.
    filter { "files:ext/synctex/**" }
      disablewarnings { "4244", "4267" }
    filter {}
    uses_zlib()
    includedirs { "ext/synctex" }

    -- for uia
    disablewarnings { "4302", "4311", "4838" }

    disablewarnings { "4819" }

    resdefines { "INSTALL_PAYLOAD_ZIP=.\\%{cfg.targetdir}\\InstallerData.dat" }

    files { "src/MuPDF_Exports.cpp" }

    includedirs { "ext/cmark-gfm/src", "ext/cmark-gfm/extensions", "mupdf/scripts/cmark-gfm" }
    defines { "CMARK_GFM_STATIC_DEFINE" }

    links {
      "libmupdf", "unrar", "libarchive", "base", "chm", "cmark-gfm", "a-zopfli"
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
    -- resolve static imports (uxtheme.dll etc.) from System32 only, so that
    -- a DLL planted next to the exe can't be side-loaded. doesn't affect
    -- delay-loaded libmupdf.dll which LoadLibmupdf() loads by full path
    linkoptions { "/DEPENDENTLOADFLAG:0x800" }
    dependson { "PdfFilter", "PdfPreview", "test_util", "sumatrapdf-tool" }
    prebuildcommands { "..\\bin\\MakeLZSA.exe ..\\translations\\translations.txt.lzsa ..\\translations\\translations-good.txt:translations-good.txt" }
    prebuildcommands { "cd %{cfg.targetdir} & ..\\..\\bin\\MakeLZSA.exe InstallerData.dat libmupdf.dll:libmupdf.dll PdfFilter.dll:PdfFilter.dll PdfPreview.dll:PdfPreview.dll sumatrapdf-tool.exe:sumatrapdf-tool.exe" }
    -- /INFERASANLIBS pulls in the *dynamic* ASan runtime, so
    -- clang_rt.asan_dynamic-x86_64.dll must sit next to the exe or it
    -- won't launch. Nothing copies it automatically, so do it here.
    -- $(VCToolsInstallDir) avoids hardcoding the MSVC toolset version.
    -- libmupdf.dll (also asan, delay-loaded from this exe's dir) is
    -- covered too since it shares this OutDir.
    filter "platforms:x64_asan"
      postbuildcommands {
        'copy /y "$(VCToolsInstallDir)bin\\Hostx64\\x64\\clang_rt.asan_dynamic-x86_64.dll" "$(OutDir)"'
      }
    filter {}

workspace "MakeLZSA"
  configurations { "Debug", "Release" }
  platforms { "x86", "x64", "arm64", "x64_asan" }
  startproject "MakeLZSA"

  filter "platforms:x86"
  architecture "x86"
  filter {}

  filter "platforms:x64_asan"
    sanitize { "Address" }
    incrementallink("Off")
    editandcontinue "Off"
  filter {}

  filter "platforms:x64 or x64_asan"
  architecture "x86_64"
  -- strangely this is not set by default for rc.exe
  resdefines { "_WIN64" }
  filter {}

  disablewarnings { "4127", "4189", "4324", "4458", "4522", "4611", "4702", "4800", "6319" }
  buildoptions { "/we4840" }

  location "this_is_invalid_location"

  filter "action:vs2022"
    location "vs2022"
  filter {}

  clang_conf()

  filter { "platforms:x86", "configurations:Release" }
    targetdir "out/rel32"
  filter { "platforms:x86", "configurations:Debug" }
    targetdir "out/dbg32"
  filter {}

  filter { "platforms:x64", "configurations:Release" }
    targetdir "out/rel64"
  filter { "platforms:x64", "configurations:Debug" }
    targetdir "out/dbg64"
  filter {}

  filter { "platforms:x64_asan", "configurations:Release" }
    targetdir "out/rel64_asan"
  filter { "platforms:x64_asan", "configurations:Debug" }
    targetdir "out/dbg64_asan"
  filter {}

  objdir "!%{cfg.targetdir}/obj/%{prj.name}"

  -- https://github.com/premake/premake-core/wiki/symbols
  -- https://blogs.msdn.microsoft.com/vcblog/2016/10/05/faster-c-build-cycle-in-vs-15-with-debugfastlink/
  symbols "Full"
  staticruntime "On"
  -- https://github.com/premake/premake-core/wiki/flags

  fatalwarnings { "All" }
  multiprocessorcompile("On")
  mapfile("On")

  winver_defines()

  project "MakeLZSA"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"
    mixed_dbg_rel_conf()

    makelzsa_files()
    disablewarnings { "4131", "4244", "4245", "4267", "4996" }
    -- 4456/4457: local/param shadowing in third-party LZMA C sources
    disablewarnings { "4456", "4457" }
    includedirs { "src", "ext/lzma/C" }
    -- build the LZMA encoder single-threaded (avoids LzFindMt/MtCoder/Threads)
    defines { "_7ZIP_ST" }

    -- for zlib
    disablewarnings { "4005", "4131", "4244", "4245", "4267", "4996" }
    zlib_files()
    uses_zlib()

    links { "shlwapi", "version", "comctl32", "wininet", "crypt32", "wintrust" }
