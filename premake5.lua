--[[
To generate Visual Studio files in vs-premake directory, run:

premake5 vs2013
 or
premake5 vs2015

Project files are generated in vs2013 and vs2015 folders.

premake4 is obsolete and doesn't support VS 2013+

I'm using premake5 alpha4 from http://premake.github.io/download.html#v5

TODO:
* generate mupdf/generated or check them in
* Installer
* enginedump
* fix "LINK : warning LNK4068: /MACHINE not specified; defaulting to X86" in 32 bit build in sumatra.lib
* fix mutool and mudraw (they have wmain, libcmtd.lib wants main)
* a way to define SVN_PRE_RELEASE_VER, via build_config.h ?
* compare compilation flags nmake vs. us from compilation logs

Code fixes:
* fix 64bit warnings ("4311", "4312", "4302", "4244", "4264") in Sumatra code
  (not dependencies)
* figure out why CrashIf() in Debug gives us 4127
* fix all 4100 in our code
* figure out how to prevent 4189 in conditional (e.g. debug logging) code
  make CrashIf() an inline function?

Note about nasm: when providing "-I foo/bar/" flag to nasm.exe, it must be
"foo/bar/" and not just "foo/bar".

Reference for warnings:
 4057 - function X differs in indirection to slightly different base types
 4100 - unreferenced formal parameter
 4127 - conditional expression is constant
 4131 - uses old-style declarator
 4189 - local variable is initialized but not referenced
 4204 - non-standard extension: non-constant aggregate initializer
 4244 - 64bit, conversion with possible loss of data
 4267 - 64bit, conversion with possible loss of data
 4302 - 64bit, type caset truncation
 4311 - 64bit, type cast pointer truncation
 4312 - 64bit, conversion to X of greater size
 4458 - declaraion of X hides class member
 4530 - exception mismatch
 4702 - unreachable code
 4706 - assignment within conditional expression
 4800 - forcing value to bool (performance warning)

Prefast:
 28125 - function X must be called in try/except (InitializeCriticalSection)
 28252 - Inconsistent annotaion
 28253 - Inconsistent annotaion

--]]

dofile("premake5.files.lua")

-- TODO: rename solution to workspace. workspace is the documented name
-- but latest alpha4 doesn't recognize it yet
solution "SumatraPDF"
  configurations { "Debug", "Release", "ReleasePrefast" }
  platforms { "x32", "x64" }
  startproject "SumatraPDF"

  filter "platforms:x32"
     architecture "x86"
     toolset "v140_xp"
     filter "action:vs2013"
      toolset "v120_xp"
  filter {}

  filter "platforms:x64"
     architecture "x86_64"
     toolset "v140"
     filter "action:vs2013"
      toolset "v120"
  filter {}

  disablewarnings { "4100", "4127", "4189", "4458", "4800" }
  warnings "Extra"

  location "this_is_invalid_location"

  filter "action:vs2015"
    location "vs2015"
  filter {}

  filter "action:vs2013"
    location "vs2013"
  filter {}

  filter {"platforms:x32", "configurations:Release"}
    targetdir "rel"
  filter {"platforms:x32", "configurations:ReleasePrefast"}
    targetdir "relPrefast"
  filter {"platforms:x32", "configurations:Debug"}
    targetdir "dbg"

  filter {"platforms:x64", "configurations:Release"}
    targetdir "rel64"
  filter {"platforms:x64", "configurations:ReleasePrefast"}
    targetdir "relPrefast64"
  filter {"platforms:x64", "configurations:Debug"}
    targetdir "dbg64"
  filter {}
  objdir "%{cfg.targetdir}/obj"

  -- https://github.com/premake/premake-core/wiki/flags
  flags {
    --"FatalWarnings", TODO: when ready
    "MultiProcessorCompile",
    "NoExceptions",
    "NoRTTI",
    "StaticRuntime",
    "Symbols",
    --"UndefinedIndentifiers", TODO: not yet in alpha4 ?
    -- "Unicode", TODO: breaks libdjuv?
  }

  defines { "WIN32", "_WIN32", "_CRT_SECURE_NO_WARNINGS", "WINVER=0x0501", "_WIN32_WINNT=0x0501" }
  defines { "_HAS_EXCEPTIONS=0" }

  filter "configurations:Debug"
    defines { "DEBUG" }

  filter "configurations:Release*"
    defines { "NDEBUG" }
    flags {
      "LinkTimeOptimization",
    }
    optimize "On"

    filter "configurations:ReleasePrefast"
      toolset "v140" -- xp toolset doesn't have prefast
      -- TODO: somehow /analyze- is default which creates warning about
      -- over-ride from cl.exe. Don't know how to disable the warning
      buildoptions { "/analyze" }
      disablewarnings { "28125", "28252", "28253" }
  filter {}

  project "efi"
    kind "ConsoleApp"
    language "C++"

    disablewarnings { "4091", "4577" }

    efi_files()
    includedirs { "src/utils" }
    links { }

  project "zlib"
    kind "StaticLib"
    language "C"

    disablewarnings { "4127", "4131", "4244", "4996" }

    zlib_files()

  project "libdjvu"
    kind "StaticLib"
    language "C++"

    -- TODO: try /D USE_EXCEPTION_EMULATION to see if it reduces the size
    -- and disables the exceptions warnings
    defines { "NEED_JPEG_DECODER", "THREADMODEL=0", "DDJVUAPI=/**/",  "MINILISPAPI=/**/", "DO_CHANGELOCALE=0" }
    includedirs { "ext/libjpeg-turbo" }

    disablewarnings { "4100", "4127", "4189", "4244", "4267", "4302", "4311", "4312" }
    disablewarnings { "4456", "4457", "4459", "4530", "4611", "4701", "4702", "4703", "4706" }
    libdjvu_files()

  project "unarr"
    kind "StaticLib"
    language "C++"

    defines { "HAVE_ZLIB", "HAVE_BZIP2", "HAVE_7Z" }
    includedirs { "ext/zlib", "ext/bzip2", "ext/lzma/C" }
    disablewarnings { "4100", "4127", "4244", "4267", "4456", "4457", "4996" }

    unarr_files()

  project "jbig2dec"
    kind "StaticLib"
    language "C"

    defines { "HAVE_STRING_H=1", "JBIG_NO_MEMENTO" }
    includedirs { "ext/jbig2dec" }
    disablewarnings { "4018", "4100", "4127", "4244", "4267", "4701", "4996" }

    jbig2dec_files()

  project "openjpeg"
    kind "StaticLib"
    language "C"

    includedirs { "ext/openjpeg" }
    disablewarnings { "4018", "4127", "4244", "4996" }

    openjpeg_files()

  project "libwebp"
    kind "StaticLib"
    language "C"

    includedirs { "ext/libwebp" }
    disablewarnings { "4018", "4057", "4127", "4204", "4244", "4996" }

    libwebp_files()

  project "libjpeg-turbo"
    kind "StaticLib"
    language "C"

    includedirs { "ext/libjpeg-turbo" }
    includedirs { "ext/libjpeg-turbo/simd" }

    disablewarnings { "4018", "4100", "4127", "4244", "4245", "4996" }

    -- nasm.exe -I .\ext\libjpeg-turbo\simd\
    -- -I .\ext\libjpeg-turbo\win\ -f win32
    -- -o .\obj-rel\jpegturbo\jsimdcpu.obj
    -- .\ext\libjpeg-turbo\simd\jsimdcpu.asm
    filter {'files:**.asm', 'platforms:x32'}
       buildmessage 'Assembling %{file.relpath}'
       buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
       buildcommands {
          '..\\bin\\nasm.exe -f win32 -I ../ext/libjpeg-turbo/simd/ -I ../ext/libjpeg-turbo/win/ -o "%{cfg.objdir}/%{file.basename}.obj" "%{file.relpath}"'
       }
    filter {}

    filter {'files:**.asm', 'platforms:x64'}
      buildmessage 'Assembling %{file.relpath}'
      buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
      buildcommands {
        '..\\bin\\nasm.exe -f win64 -D__x86_64__ -DWIN64 -DMSVC -I ../ext/libjpeg-turbo/simd/ -I ../ext/libjpeg-turbo/win/ -o "%{cfg.objdir}/%{file.basename}.obj" "%{file.relpath}"'
      }
    filter {}

    libjpeg_turbo_files()

  project "freetype"
    kind "StaticLib"
    language "C"

    includedirs { "ext/freetype2/config" }
    includedirs { "ext/freetype2/include" }
    defines { "FT2_BUILD_LIBRARY", "FT_OPTION_AUTOFIT2"}

    disablewarnings { "4996", "4018" }

    freetype_files()

  project "sumatra"
    kind "StaticLib"
    language "C++"

    includedirs { "src/utils", "src/wingui", "src/mui", "ext/lzma/C" }
    includedirs { "ext/libwebp", "ext/unarr", "mupdf/include", "src" }
    includedirs { "ext/synctex", "ext/libdjvu", "ext/CHMLib/src" }

    disablewarnings { "4018", "4189", "4302", "4311", "4838", "4996" }

    sumatra_files()

  project "utils"
    kind "StaticLib"
    language "C++"

    includedirs { "src/utils", "src/wingui", "src/mui", "ext/zlib", "ext/lzma/C" }
    includedirs { "ext/libwebp", "ext/unarr", "mupdf/include" }

    disablewarnings { "4018", "4838", "4996" }
    -- TODO: DbgHelpDyn.cpp 64bit warnings only, fix the code
    disablewarnings { "4302", "4311", "4312" }

    utils_files()

  project "mui"
    kind "StaticLib"
    language "C++"

    includedirs { "src/utils", "src/wingui", "src/mui" }

    disablewarnings { "4996", "4018", "4838" }

    mui_files()

  project "engines"
    kind "StaticLib"
    language "C++"

    includedirs { "src/utils", "src/wingui", "src/mui" }
    includedirs { "ext/synctex", "ext/libdjvu", "ext/CHMLib/src", "ext/zlib", "mupdf/include" }

    disablewarnings { "4018", "4057", "4189", "4244", "4267", "4295" }
    disablewarnings { "4701", "4706", "4838", "4996"  }

    engines_files()

  project "mupdf"
    kind "StaticLib"
    language "C"

    includedirs {
      "mupdf/include", "mupdf/generated", "ext/zlib",
      "ext/freetype2/config", "ext/freetype2/include",
      "ext/jbig2dec", "ext/libjpeg-turbo", "ext/openjpeg"
    }

    -- .\ext\..\bin\nasm.exe -I .\mupdf\ -f win32 -o .\obj-rel\mupdf\font_base14.obj
    -- .\mupdf\font_base14.asm
    filter {'files:**.asm', 'platforms:x32'}
       buildmessage 'Compiling %{file.relpath}'
       buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
       buildcommands {
          '..\\bin\\nasm.exe -f win32 -I ../mupdf/ -o "%{cfg.objdir}/%{file.basename}.obj" "%{file.relpath}"'
       }
    filter {}

    filter {'files:**.asm', 'platforms:x64'}
      buildmessage 'Compiling %{file.relpath}'
      buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
      buildcommands {
        '..\\bin\\nasm.exe -f win64 -DWIN64 -I ../mupdf/ -o "%{cfg.objdir}/%{file.basename}.obj" "%{file.relpath}"'
      }
    filter {}

    defines { "NOCJKFONT", "SHARE_JPEG" }
    disablewarnings {  "4018", "4244", "4267", "4838", "4996", }
    mupdf_files()

    links {
      "zlib",
      "freetype",
      "libjpeg-turbo",
      "jbig2dec",
      "openjpeg",
    }

  -- TODO: libcmtd.lib wants main() but it has wmain for _MSC_VER
  project "mutool"
    kind "ConsoleApp"
    language "C"
    mutool_files()
    includedirs {
      "ext/zlib", "ext/lzma/C", "ext/unarr", "mupdf/include"
    }
    links { "mupdf" }
    links {
      "windowscodecs.lib"
    }

    -- TODO: libcmtd.lib wants main() but it has wmain for _MSC_VER
  project "mudraw"
    kind "ConsoleApp"
    language "C"
    mudraw_files()
    includedirs {
      "ext/zlib", "ext/lzma/C", "ext/unarr", "mupdf/include"
    }
    links { "mupdf" }
    links {
      "windowscodecs.lib"
    }

  project "libmupdf"
    kind "SharedLib"
    language "C"

    implibname "libmupdf"

    -- TODO: is thre a better way to do it?
    -- TODO: only for windows
    linkoptions { "/DEF:..\\src\\libmupdf.def" }

    -- premake has logic in vs2010_vcxproj.lua that only sets PlatformToolset
    -- if there is a c/c++ file, so we add a no-op cpp file to force This logic
    files { "src/libmupdf.rc", "src/no_op_for_premake.cpp" }
    links {
      "mupdf",
      "libdjvu",
      "unarr",
      "libwebp",
    }

    links {
      "advapi32.lib", "kernel32.lib", "user32.lib", "gdi32.lib", "comdlg32.lib",
      "shell32.lib", "WindowsCodecs.lib", "comctl32.lib", "Msimg32.lib",
      "Winspool.lib", "wininet.lib", "urlmon.lib", "gdiplus.lib", "ole32.lib",
      "OleAut32.lib", "shlwapi.lib", "version.lib", "crypt32.lib"
    }

  project "SumatraPDF"
    kind "WindowedApp"
    language "C++"

    flags {
      "NoManifest",
      "WinMain",
    }

    disablewarnings { "4018", "4244", "4267", "4702", "4706", "4838", "4996" }

    includedirs {
      "src/utils", "src/wingui", "src/mui", "ext/zlib", "ext/lzma/C",
      "ext/libwebp", "ext/unarr", "mupdf/include", "src", "ext/synctex",
      "ext/libdjvu", "ext/CHMLib/src"
    }

    sumatrapdf_files()

    -- TODO: is this necessary? This is also part of sumatra
    files {
      "src/SumatraPDF.rc",
    }

    links {
      "sumatra",
      "utils",
      "mui",
      "engines",
      "mupdf",
      "libdjvu",
      "unarr",
      "libwebp",
    }

    links {
      "advapi32.lib", "kernel32.lib", "user32.lib", "gdi32.lib", "comdlg32.lib",
      "shell32.lib", "windowscodecs.lib", "comctl32.lib", "Msimg32.lib",
      "Winspool.lib", "wininet.lib", "urlmon.lib", "gdiplus.lib", "ole32.lib",
      "OleAut32.lib", "shlwapi.lib", "version.lib", "crypt32.lib"
    }

  project "SumatraPDF-no-MUPDF"
    kind "WindowedApp"
    language "C++"

    flags {
      "NoManifest",
      "WinMain",
    }

    disablewarnings { "4018", "4244", "4264", "4838", "4702", "4706", "4996", }

    includedirs {
      "src/utils", "src/wingui", "src/mui", "ext/zlib", "ext/lzma/C",
      "ext/libwebp", "ext/unarr", "mupdf/include", "src", "ext/synctex",
      "ext/libdjvu", "ext/CHMLib/src"
    }

    sumatrapdf_files()
    -- TODO: is this necessary? This is also part of sumatra
    files {
      "src/SumatraPDF.rc",
    }
    files { "src/MuPDF_Exports.cpp" }

    links {
      "sumatra",
      "libmupdf",
      "utils",
      "mui",
      "engines",
      "unarr",
      "libwebp",
    }

    links {
      "advapi32.lib", "kernel32.lib", "user32.lib", "gdi32.lib", "comdlg32.lib",
      "shell32.lib", "WindowsCodecs.lib", "comctl32.lib", "Msimg32.lib",
      "Winspool.lib", "wininet.lib", "urlmon.lib", "gdiplus.lib", "ole32.lib",
      "OleAut32.lib", "shlwapi.lib", "version.lib", "crypt32.lib"
    }

  project "MakeLZSA"
    kind "ConsoleApp"
    language "C++"

    files {
      "src/tools/MakeLzSA.cpp"
    }

    includedirs { "src/utils", "ext/zlib", "ext/lzma/C", "ext/unarr" }

    links { "unarr", "utils", "zlib", }

    -- TODO: probably excessive
    links {
      "advapi32.lib", "kernel32.lib", "user32.lib", "gdi32.lib", "comdlg32.lib",
      "shell32.lib", "WindowsCodecs.lib", "comctl32.lib", "Msimg32.lib",
      "Winspool.lib", "wininet.lib", "urlmon.lib", "gdiplus.lib", "ole32.lib",
      "OleAut32.lib", "shlwapi.lib", "version.lib", "crypt32.lib"
    }

  project "PdfFilter"
    kind "SharedLib"
    language "C++"

    disablewarnings { "4838" }

    -- TODO: probably excessive
    includedirs {
      "src/utils", "src/wingui", "src/mui", "ext/zlib", "ext/lzma/C",
      "ext/libwebp", "ext/unarr", "mupdf/include", "src", "ext/synctex",
      "ext/libdjvu", "ext/CHMLib/src"
    }

    files_in_dir("src/ifilter", {
      "PdfFilter.rc",
      "PdfFilterDll.cpp",
      "CPdfFilter.cpp",
    })
    files { "src/MUPDF_Exports.cpp", "src/PdfEngine.cpp" }

    links {
      "utils",
      "libmupdf",
    }
    -- TODO: probably excessive
    links {
      "advapi32.lib", "kernel32.lib", "user32.lib", "gdi32.lib", "comdlg32.lib",
      "shell32.lib", "WindowsCodecs.lib", "comctl32.lib", "Msimg32.lib",
      "Winspool.lib", "wininet.lib", "urlmon.lib", "gdiplus.lib", "ole32.lib",
      "OleAut32.lib", "shlwapi.lib", "version.lib", "crypt32.lib"
    }

  project "PdfPreview"
    kind "SharedLib"
    language "C++"

    disablewarnings { "4838" }

    -- TODO: probably excessive
    includedirs {
      "src/utils", "src/wingui", "src/mui", "ext/zlib", "ext/lzma/C",
      "ext/libwebp", "ext/unarr", "mupdf/include", "src", "ext/synctex",
      "ext/libdjvu", "ext/CHMLib/src"
    }

    files_in_dir("src/previewer", {
      "PdfPreview.rc",
      "PdfPreviewDll.cpp",
      "PdfPreview.cpp",
    })
    files { "src/MUPDF_Exports.cpp", "src/PdfEngine.cpp" }

    links {
      "utils",
      "libmupdf",
    }
    -- TODO: probably excessive
    links {
      "advapi32.lib", "kernel32.lib", "user32.lib", "gdi32.lib", "comdlg32.lib",
      "shell32.lib", "WindowsCodecs.lib", "comctl32.lib", "Msimg32.lib",
      "Winspool.lib", "wininet.lib", "urlmon.lib", "gdiplus.lib", "ole32.lib",
      "OleAut32.lib", "shlwapi.lib", "version.lib", "crypt32.lib"
    }

  project "all"
    kind "ConsoleApp"
    language "C"

    -- premake has logic in vs2010_vcxproj.lua that only sets PlatformToolset
    -- if there is a c/c++ file, so we add a no-op cpp file to force This logic
    files { "src/no_op_console.c" }

    dependson {
      "PdfPreview", "PdfFilter", "SumatraPDF", "SumatraPDF-no-MUPDF"
    }
