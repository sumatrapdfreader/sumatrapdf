--[[
To generate Visual Studio files (in vs2017/ directory), run:
scripts\premake-regenerate-vs-projects.ps1

I'm using premake5 alpha12 from https://premake.github.io/download.html#v5

Note about nasm: when providing "-I foo/bar/" flag to nasm.exe, it must be
"foo/bar/" and not just "foo/bar".

Notes about asan in msvc: https://devblogs.microsoft.com/cppblog/addresssanitizer-asan-for-windows-with-msvc/

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
 28252 - Inconsistent annotaion
 28253 - Inconsistent annotaion
--]]

include("premake5.files.lua")

-- TODO: could fold 9 libraries used by mupdf into a single
-- project mupdf-libs, to make solution smaller

-- to make debug builds faster, we compile stable libraries (freetype, libjpeg etc.)
-- in release mode even in debug builds
function regconf()
  defines { "_HAS_ITERATOR_DEBUGGING=0" }
  filter "configurations:Debug"
    defines { "DEBUG" }

  filter "configurations:Release*"
    defines { "NDEBUG" }
    optimize "Size"

  -- no ltcg in asan builds
  filter { "configurations:Release*", "platforms:x32 or x64" }
    flags {
      "LinkTimeOptimization",
    }

  filter {}
  runtime "Release"
end

-- config for stable libraries where debug build is done with optimization
function optconf()
  optimize "Size"
  undefines { "DEBUG" }
  defines { "NDEBUG" }

  -- we mix Deubg / Release compilation between projects
  -- but all linked modules have to use the same type
  -- of c libraries, so we use release C libs
  -- and we disable C++ iterator debugging because it also
  -- must be consistent across modules
  defines { "_HAS_ITERATOR_DEBUGGING=0" }
  runtime "Release"

  -- no ltcg in asan builds
  filter { "configurations:Release*", "platforms:x32 or x64" }
    flags {
      "LinkTimeOptimization",
    }

  filter {}
end

function zlib_ng_defines()
  defines { 
    "_CRT_SECURE_NO_DEPRECATE",
    "_CRT_NONSTDC_NO_DEPRECATE",
    "X86_FEATURES", 
    "X86_PCLMULQDQ_CRC",
    "X86_SSE2",
    "X86_SSE42_CRC_INTRIN",
    "X86_SSE42_CRC_HASH",
    "X86_AVX2",
    "X86_AVX_CHUNKSET",
    "X86_SSE2_CHUNKSET",
    "UNALIGNED_OK",
    "UNALIGNED64_OK",
    "WITH_GZFILEOP",
    "ZLIB_COMPAT"
  }
end

workspace "SumatraPDF"
  configurations { "Debug", "Release", "ReleaseAnalyze", }
  platforms { "x32", "x64", "x64_asan" }
  startproject "SumatraPDF"

  filter "platforms:x32 or x32_asan"
     architecture "x86"
  filter {}

  filter "platforms:x32_asan or x64_asan"
    buildoptions { "/fsanitize=address"}
    defines { "ASAN_BUILD=1" }
    -- disablewarnings { "4731" }
  filter {}

  filter "platforms:x64 or x64_asan"
     architecture "x86_64"
     -- strangely this is not set by default for rc.exe
     resdefines { "_WIN64" }
  filter {}

  disablewarnings { "4127", "4189", "4324", "4458", "4522", "4611", "4702", "4800", "6319" }
  warnings "Extra"

  location "this_is_invalid_location"

  filter "action:vs2019"
    location "vs2019"
  filter {}

  filter {"platforms:x32", "configurations:Release"}
    targetdir "out/rel32"
  filter {"platforms:x32", "configurations:ReleaseAnalyze"}
    targetdir "out/rel32_prefast"
  filter {"platforms:x32", "configurations:Debug"}
    targetdir "out/dbg32"
  filter {}

  filter {"platforms:x32_asan", "configurations:Release"}
    targetdir "out/rel32_asan"
  filter {"platforms:x32_asan", "configurations:ReleaseAnalyze"}
    targetdir "out/rel32_prefast_asan"
  filter {"platforms:x32_asan", "configurations:Debug"}
    targetdir "out/dbg32_asan"
  filter {}

  filter {"platforms:x64", "configurations:Release"}
    targetdir "out/rel64"
  filter {"platforms:x64", "configurations:ReleaseAnalyze"}
    targetdir "out/rel64_prefast"
  filter {"platforms:x64", "configurations:Debug"}
    targetdir "out/dbg64"
  filter {}

  filter {"platforms:x64_asan", "configurations:Release"}
    targetdir "out/rel64_asan"
  filter {"platforms:x64_asan", "configurations:ReleaseAnalyze"}
    targetdir "out/rel64_prefast_asan"
  filter {"platforms:x64_asan", "configurations:Debug"}
    targetdir "out/dbg64_asan"
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

  filter {"configurations:not ReleaseAnalyze"}
    flags { "FatalCompileWarnings" }
  filter {}

  exceptionhandling "Off"
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
    optconf()
    defines { "UNRAR", "RARDLL", "SILENT" }
    disablewarnings { "4100", "4201", "4211", "4244", "4389", "4456", "4459", "4701", "4702", "4706", "4709", "4731", "4996" }
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
    optconf()
    defines {
      "_CRT_SECURE_NO_WARNINGS",
      "NEED_JPEG_DECODER",
      "WINTHREADS=1",
      "DDJVUAPI=/**/",
      "MINILISPAPI=/**/",
      "DEBUGLVL=0"
    }
    filter {"platforms:x32_asan or x64_asan"}
      defines { "DISABLE_MMX" }
    filter{}
    disablewarnings { "4100", "4189", "4244", "4267", "4302", "4311", "4312", "4505"}
    disablewarnings { "4456", "4457", "4459", "4530", "4701", "4702", "4703", "4706" }
    includedirs { "ext/libjpeg-turbo" }
    libdjvu_files()

  project "chm"
    kind "StaticLib"
    language "C"
    optconf()
    defines { "_CRT_SECURE_NO_WARNINGS" }
    disablewarnings { "4018", "4244", "4267", "4996" }
    files { "ext/CHMLib/*.c", "ext/CHMLib/*.h" }

  project "engines"
    kind "StaticLib"
    language "C++"
    cppdialect "C++latest"
    optconf()
    disablewarnings {
      "4018", "4057", "4100", "4189", "4244", "4267", "4295", "4457",
      "4701", "4706", "4819", "4838"
    }
    includedirs { "src", "src/wingui" }
    zlib_ng_defines()
    includedirs { "ext/synctex", "ext/libdjvu", "ext/CHMLib", "ext/zlib-ng", "mupdf/include" }
    engines_files()
    links { "chm" }

  project "unarrlib"
    kind "StaticLib"
    language "C"
    optconf()
    -- TODO: for bzip2, need BZ_NO_STDIO and BZ_DEBUG=0
    -- TODO: for lzma, need _7ZIP_PPMD_SUPPPORT
    defines { "HAVE_ZLIB", "HAVE_BZIP2", "HAVE_7Z", "BZ_NO_STDIO", "_7ZIP_PPMD_SUPPPORT" }
    -- TODO: most of these warnings are due to bzip2 and lzma
    disablewarnings { "4100", "4244", "4267", "4456", "4457", "4996" }
    zlib_ng_defines()
    includedirs { "ext/zlib-ng", "ext/bzip2", "ext/lzma/C" }
    unarr_files()

  project "libwebp"
    kind "StaticLib"
    language "C"
    optconf()
    disablewarnings { "4204", "4244", "4057", "4245", "4310" }
    includedirs { "ext/libwebp" }
    libwebp_files()

  project "zlib-ng"
    kind "StaticLib"
    language "C"
    optconf()
    includedirs { "ext/zlib-ng" }
    zlib_ng_defines()
    disablewarnings { "4244", "4267" }
    zlib_ng_files()
    
  -- to make Visual Studio solution smaller
  -- combine 9 libs only used by mupdf into a single project
  -- instead of having 9 projects
  project "mupdf-libs"
    kind "StaticLib"
    language "C"
    optconf()

    -- zlib
    -- disablewarnings { "4131", "4244", "4245", "4267", "4996" }
    -- zlib_files()

    -- libjpeg-turbo
    defines { "_CRT_SECURE_NO_WARNINGS" }
    disablewarnings { "4018", "4100", "4244", "4245" }
    includedirs { "ext/libjpeg-turbo", "ext/libjpeg-turbo/simd" }
    -- nasm.exe -I .\ext\libjpeg-turbo\simd\
    -- -I .\ext\libjpeg-turbo\win\ -f win32
    -- -o .\obj-rel\jpegturbo\jsimdcpu.obj
    -- .\ext\libjpeg-turbo\simd\jsimdcpu.asm
    filter {'files:**.asm', 'platforms:x32 or x32_asan'}
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
    disablewarnings { "4018", "4100", "4244", "4267", "4312", "4701", "4706", "4996" }
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
    disablewarnings { "4100", "4146", "4244", "4245", "4267", "4456", "4457", "4459", "4701", "4702", "4706" }
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
    disablewarnings { "4005", "4201" }
    includedirs { "ext/extract/include" }
    includedirs { "ext/zlib-ng" }
    extract_files()

--[[
  project "zlib"
    kind "StaticLib"
    language "C"
    optconf()
    disablewarnings { "4131", "4244", "4245", "4267", "4996" }
    zlib_files()

  project "libjpeg-turbo"
    kind "StaticLib"
    language "C"
    optconf()
    defines { "_CRT_SECURE_NO_WARNINGS" }
    disablewarnings { "4018", "4100", "4244", "4245" }
    includedirs { "ext/libjpeg-turbo", "ext/libjpeg-turbo/simd" }

    -- nasm.exe -I .\ext\libjpeg-turbo\simd\
    -- -I .\ext\libjpeg-turbo\win\ -f win32
    -- -o .\obj-rel\jpegturbo\jsimdcpu.obj
    -- .\ext\libjpeg-turbo\simd\jsimdcpu.asm
    filter {'files:**.asm', 'platforms:x32 or x32_asan'}
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

  project "jbig2dec"
    kind "StaticLib"
    language "C"
    optconf()
    defines { "_CRT_SECURE_NO_WARNINGS", "HAVE_STRING_H=1", "JBIG_NO_MEMENTO" }
    disablewarnings { "4018", "4100", "4146", "4244", "4267", "4456", "4701" }
    includedirs { "ext/jbig2dec" }
    jbig2dec_files()

  project "openjpeg"
    kind "StaticLib"
    language "C"
    optconf()
    disablewarnings { "4100", "4244", "4310", "4389", "4456" }
    -- openjpeg has opj_config_private.h for such over-rides
    -- but we can't change it because we bring openjpeg as submodule
    -- and we can't provide our own in a different directory because
    -- msvc will include the one in ext/openjpeg/src/lib/openjp2 first
    -- because #include "opj_config_private.h" searches current directory first
    defines { "_CRT_SECURE_NO_WARNINGS", "USE_JPIP", "OPJ_STATIC", "OPJ_EXPORTS" }
    openjpeg_files()

    project "freetype"
    kind "StaticLib"
    language "C"
    optconf()
    defines {
      "FT2_BUILD_LIBRARY",
      "FT_CONFIG_MODULES_H=\"slimftmodules.h\"",
      "FT_CONFIG_OPTIONS_H=\"slimftoptions.h\"",
    }
    disablewarnings { "4018", "4100", "4244", "4267", "4312", "4701", "4706", "4996" }
    includedirs { "mupdf/scripts/freetype", "ext/freetype/include" }
    freetype_files()

  project "lcms2"
    kind "StaticLib"
    language "C"
    optconf()
    disablewarnings { "4100" }
    includedirs { "ext/lcms2/include" }
    lcms2_files()

  project "harfbuzz"
    kind "StaticLib"
    language "C"
    optconf()
    includedirs { "ext/harfbuzz/src/hb-ucdn", "mupdf/scripts/freetype", "ext/freetype/include" }
    defines {
      "_CRT_SECURE_NO_WARNINGS",
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
    disablewarnings { "4100", "4146", "4244", "4245", "4267", "4456", "4457", "4459", "4701", "4702", "4706" }
    harfbuzz_files()

  project "mujs"
    kind "StaticLib"
    language "C"
    optconf()
    includedirs { "ext/mujs" }
    disablewarnings { "4090", "4100", "4310", "4702", "4706" }
    files { "ext/mujs/one.c", "ext/mujs/mujs.h" }

  project "gumbo"
    kind "StaticLib"
    language "C"
    optconf()
    disablewarnings { "4018", "4100", "4132", "4204", "4244", "4245", "4267", 
    "4305", "4306", "4389", "4456", "4701" }
    includedirs { "ext/gumbo-parser/include", "ext/gumbo-parser/visualc/include" }
    gumbo_files()
--]]

  project "mupdf"
    kind "StaticLib"
    language "C"
    regconf()
    -- for openjpeg, OPJ_STATIC is alrady defined in load-jpx.c
    -- so we can't double-define it
    defines { "USE_JPIP", "OPJ_EXPORTS", "HAVE_LCMS2MT=1" }
    defines { "OPJ_STATIC", "SHARE_JPEG" }
    -- this defines which fonts are to be excluded from being included directly
    -- we exclude the very big cjk fonts
    defines { "TOFU", "TOFU_CJK_LANG" }

    disablewarnings {
      "4005", "4018", "4057", "4100", "4115", "4130", "4204", "4206", "4245", "4267",
      "4295", "4305", "4389", "4456", "4457", "4703", "4706"
    }
    -- force including mupdf/scripts/openjpeg/opj_config_private.h
    -- with our build over-rides

    zlib_ng_defines()
    includedirs {
      "mupdf/include",
      "mupdf/generated",
      "ext/jbig2dec",
      "ext/libjpeg-turbo",
      "ext/openjpeg/src/lib/openjp2",
      "ext/zlib-ng",
      "mupdf/scripts/freetype",
      "ext/freetype/include",
      "ext/mujs",
      "ext/harfbuzz/src",
      "ext/lcms2/include",
      "ext/gumbo-parser/src",
      "ext/extract/include",
  }
    -- .\ext\..\bin\nasm.exe -I .\mupdf\ -f win32 -o .\obj-rel\mupdf\font_base14.obj
    -- .\mupdf\font_base14.asm
    filter {'files:**.asm', 'platforms:x32 or x32_asan'}
       buildmessage 'Compiling %{file.relpath}'
       buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
       buildcommands {
          '..\\bin\\nasm.exe -f win32 -I ../mupdf/ -o "%{cfg.objdir}/%{file.basename}.obj" "%{file.relpath}"'
       }
    filter {}

    filter {'files:**.asm', 'platforms:x64 or x64_asan'}
      buildmessage 'Compiling %{file.relpath}'
      buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
      buildcommands {
        '..\\bin\\nasm.exe -f win64 -DWIN64 -I ../mupdf/ -o "%{cfg.objdir}/%{file.basename}.obj" "%{file.relpath}"'
      }
    filter {}

    mupdf_files()
    links { "mupdf-libs" }
    -- links { "mupdf-libs", "zlib", "freetype", "openjpeg", "libjpeg-turbo", "jbig2dec", "lcms2", "harfbuzz", "mujs", "gumbo" }

  project "libmupdf"
    kind "SharedLib"
    language "C"
    optconf()
    disablewarnings { "4206", "4702" }
    defines { "FZ_ENABLE_SVG" }
    -- premake has logic in vs2010_vcxproj.lua that only sets PlatformToolset
    -- if there is a c/c++ file, so we add a no-op cpp file to force This logic
    files { "src/libmupdf.rc", "src/libmupdf.def", "src/no_op_for_premake.cpp" }
    implibname "libmupdf"
    -- TODO: is thre a better way to do it?
    -- linkoptions { "/DEF:..\\src\\libmupdf.def", "-IGNORE:4702" }
    linkoptions { "-IGNORE:4702" }
    links { "mupdf", "zlib-ng", "libdjvu", "libwebp", "unarrlib" }
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
    regconf()
    filter "configurations:ReleaseAnalyze"
      -- TODO: somehow /analyze- is default which creates warning about
      -- over-ride from cl.exe. Don't know how to disable the warning
      buildoptions { "/analyze" }
      disablewarnings { "28125", "28252", "28253" }
    filter {}

    -- QITABENT in shlwapi.h has incorrect definition and causes 4838
    disablewarnings { "4100", "4267", "4457", "4838" }
    zlib_ng_defines()
    includedirs { "src", "ext/zlib-ng", "ext/lzma/C" }
    includedirs { "ext/libwebp/src", "ext/unarr", "mupdf/include" }
    utils_files()


  ---- executables
  --[[
  project "efi"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"
    disablewarnings { "4091", "4577" }
    includedirs { "src" }
    efi_files()


  project "mutool"
    kind "ConsoleApp"
    language "C"
    disablewarnings { "4100", "4267" }
    includedirs { "ext/zlib", "ext/lzma/C", "ext/unarr", "mupdf/include" }
    mutool_files()
    links { "mupdf" }
    links { "windowscodecs" }
    entrypoint "wmainCRTStartup"


  project "mudraw"
    kind "ConsoleApp"
    language "C"
    disablewarnings { "4100", "4267" }
    includedirs { "ext/zlib", "ext/lzma/C", "ext/unarr", "mupdf/include" }
    mudraw_files()
    links { "mupdf" }
    links { "windowscodecs" }
    linkoptions { "/ENTRY:\"wmainCRTStartup\"" }
    entrypoint "wmainCRTStartup"


  project "unarr"
    kind "ConsoleApp"
    language "C"
    disablewarnings { "4100" }
    files { "ext/unarr/main.c" }
    links { "unarrlib", "zlib" }


  project "signfile"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"
    includedirs { "src", "mupdf/include"}
    files { "src/tools/signfile.cpp" }
    links { "utils", "mupdf" }
    links { "crypt32", "shlwapi" }


  project "plugin-test"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++latest"
    regconf()
    entrypoint "WinMainCRTStartup"
    includedirs { "src" }
    files { "src/tools/plugin-test.cpp" }
    links { "utils", "mupdf" }
    links { "shlwapi", "version", "comctl32" }
  --]]

  project "enginedump"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"
    regconf()
    includedirs { "src", "src/wingui", "mupdf/include" }
    disablewarnings { "4100", "4267", "4457" }
    engine_dump_files()
    links { "engines", "utils", "zlib-ng", "unrar", "mupdf", "unarrlib", "libwebp", "libdjvu" }
    links {
      "comctl32", "gdiplus", "msimg32", "shlwapi",
      "version", "windowscodecs"
    }

  project "test_util"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"
    regconf()
    disablewarnings { "4838" }
    includedirs { "src" }
    test_util_files()
    links { "gdiplus", "comctl32", "shlwapi", "Version" }

  project "logview"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"
    regconf()
    disablewarnings { "4838" }
    includedirs { "src" }
    logview_files()
    links { "gdiplus", "comctl32", "shlwapi", "Version" }

  project "PdfFilter"
    kind "SharedLib"
    language "C++"
    cppdialect "C++latest"
    regconf()
    disablewarnings { "4100", "4838" }
    filter {"configurations:Debug"}
      defines { "BUILD_TEX_IFILTER", "BUILD_EPUB_IFILTER" }
    filter {}
    includedirs { "src", "src/wingui", "mupdf/include" }
    pdf_filter_files()
    links { "utils", "unrar", "libmupdf" }
    links { "comctl32", "gdiplus", "shlwapi", "version"  }

  project "PdfPreview"
    kind "SharedLib"
    language "C++"
    cppdialect "C++latest"
    regconf()
    disablewarnings { "4100", "4838" }
    includedirs {
      "src", "src/wingui", "mupdf/include",
      "ext/libdjvu", "ext/CHMLib", "ext/zlib-ng"
    }
    pdf_preview_files()
    filter {"configurations:Debug"}
      -- defines { "BUILD_XPS_PREVIEW" }
      -- TODO: use EngineMupdf for XPS
      defines {
        "BUILD_DJVU_PREVIEW", "BUILD_EPUB_PREVIEW",
        "BUILD_FB2_PREVIEW", "BUILD_MOBI_PREVIEW", "BUILD_CBZ_PREVIEW",
        "BUILD_CBR_PREVIEW", "BUILD_CB7_PREVIEW", "BUILD_CBT_PREVIEW",
        "BUILD_TGA_PREVIEW"
      }
    filter {}
    -- TODO: "chm" should only be for Debug config but doing links { "chm" }
    -- in the filter breaks linking by setting LinkLibraryDependencies to false
    links { "utils", "unrar", "libmupdf", "chm" }
    links { "comctl32", "gdiplus", "msimg32", "shlwapi", "version" }

    project "PdfPreviewTest"
      kind "ConsoleApp"
      language "C++"
      cppdialect "C++latest"
      regconf()
      disablewarnings { "4838" }
      includedirs { "src" }
      preview_test_files()
      links { "gdiplus", "comctl32", "shlwapi", "Version" }
      dependson { "PdfPreview" }

  -- a single static executable
  project "SumatraPDF"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++latest"
    regconf()
    entrypoint "WinMainCRTStartup"
    flags { "NoManifest" }
    includedirs { "src", "mupdf/include" }

    synctex_files()
    mui_files()
    uia_files()
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
    disablewarnings { "4100", "4244", "4267", "4702", "4706" }
    zlib_ng_defines()
    includedirs { "ext/zlib-ng", "ext/synctex" }

    -- for uia
    disablewarnings { "4302", "4311", "4838" }

    links {
      "engines", "zlib-ng", "libdjvu",  "libwebp", "mupdf", "unarrlib", "utils", "unrar"
    }
    links {
      "comctl32", "delayimp", "gdiplus", "msimg32", "shlwapi", "urlmon",
      "version", "windowscodecs", "wininet", "uiautomationcore.lib"
    }
    -- this is to prevent dll hijacking
    linkoptions { "/DELAYLOAD:gdiplus.dll /DELAYLOAD:msimg32.dll /DELAYLOAD:shlwapi.dll" }
    linkoptions { "/DELAYLOAD:urlmon.dll /DELAYLOAD:version.dll /DELAYLOAD:wininet.dll" }
    linkoptions { "/DELAYLOAD:uiautomationcore.dll" }
    -- dependson { "PdfFilter", "PdfPreview", "test_util" }


  -- a dll version where most functionality is in libmupdf.dll
  project "SumatraPDF-dll"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++latest"
    regconf()
    entrypoint "WinMainCRTStartup"
    flags { "NoManifest" }
    includedirs { "src", "mupdf/include" }

    synctex_files()
    mui_files()
    uia_files()
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
    disablewarnings { "4100", "4244", "4267", "4702", "4706" }
    zlib_ng_defines()
    includedirs { "ext/zlib-ng", "ext/synctex" }

    -- for uia
    disablewarnings { "4302", "4311", "4838" }

    resdefines { "INSTALL_PAYLOAD_ZIP=.\\%{cfg.targetdir}\\InstallerData.dat" }

    files { "src/MuPDF_Exports.cpp" }

    links {
      "libmupdf", "unrar", "unarrlib", "utils", "engines"
    }
    links {
      "comctl32", "delayimp", "gdiplus", "msimg32", "shlwapi", "urlmon",
      "version", "wininet", "d2d1.lib", "uiautomationcore.lib"
    }
    -- this is to prevent dll hijacking
    linkoptions { "/DELAYLOAD:libmupdf.dll" }
    linkoptions { "/DELAYLOAD:gdiplus.dll /DELAYLOAD:msimg32.dll /DELAYLOAD:shlwapi.dll" }
    linkoptions { "/DELAYLOAD:urlmon.dll /DELAYLOAD:version.dll /DELAYLOAD:wininet.dll" }
    linkoptions { "/DELAYLOAD:uiautomationcore.dll" }
    dependson { "PdfFilter", "PdfPreview", "test_util" }
    prebuildcommands { "cd %{cfg.targetdir} & ..\\..\\bin\\MakeLZSA.exe InstallerData.dat libmupdf.dll:libmupdf.dll PdfFilter.dll:PdfFilter.dll PdfPreview.dll:PdfPreview.dll"  }

workspace "MakeLZSA"
  configurations { "Debug", "Release" }
  platforms { "x32", "x64" }
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
  warnings "Extra"

  location "this_is_invalid_location"

  filter "action:vs2019"
    location "vs2019"
  filter {}

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
  flags {
    "MultiProcessorCompile",
    "Maps", -- generate map file
    --"Unicode",
    "FatalCompileWarnings"
  }
  exceptionhandling "Off"
  rtti "Off"

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
    regconf()

    makelzsa_files()
    zlib_ng_defines()
    includedirs { "src", "ext/zlib-ng", "ext/lzma/C", "ext/unarr" }

    -- for zlib
    -- disablewarnings { "4131", "4244", "4245", "4267", "4996" }
    -- zlib_files()

    -- unarrlib
    -- TODO: for bzip2, need BZ_NO_STDIO and BZ_DEBUG=0
    -- TODO: for lzma, need _7ZIP_PPMD_SUPPPORT
    defines { "HAVE_ZLIB", "HAVE_BZIP2", "HAVE_7Z", "BZ_NO_STDIO", "_7ZIP_PPMD_SUPPPORT" }
    -- TODO: most of these warnings are due to bzip2 and lzma
    disablewarnings { "4100", "4244", "4267", "4456", "4457", "4996" }
    includedirs { "ext/zlib", "ext/bzip2", "ext/lzma/C" }
    unarr_files()

    links { "shlwapi", "version", "comctl32", "zlib-ng" }
