--[[
To generate Visual Studio files in vs2015 directory, run: premake5 vs2015

I'm using premake5 alpha6 from http://premake.github.io/download.html#v5
(premake4 won't work, it doesn't support VS 2013+)

TODO:
* compare compilation flags nmake vs. us from compilation logs

Note about nasm: when providing "-I foo/bar/" flag to nasm.exe, it must be
"foo/bar/" and not just "foo/bar".

Reference for warnings:
 4018 - signed/unsigned mismatch
 4057 - function X differs in indirection to slightly different base types
 4100 - unreferenced formal parameter
 4127 - conditional expression is constant
 4131 - uses old-style declarator
 4189 - local variable is initialized but not referenced
 4204 - non-standard extension: non-constant aggregate initializer
 4206 - non-standard extension: translation unit is empty
 4244 - 64bit, conversion with possible loss of data
 4267 - 64bit, conversion with possible loss of data
 4302 - 64bit, type caset truncation
 4311 - 64bit, type cast pointer truncation
 4312 - 64bit, conversion to X of greater size
 4324 - 64bit, structure was padded
 4458 - declaraion of X hides class member
 4530 - exception mismatch
 4702 - unreachable code
 4706 - assignment within conditional expression
 4800 - forcing value to bool (performance warning)
 4819 - The file contains a character that cannot be represented in the current code page
 4838 - conversion from X to Y requires a narrowing conversion
 4996 - POSIX name deprecated

Prefast:
 28125 - function X must be called in try/except (InitializeCriticalSection)
 28252 - Inconsistent annotaion
 28253 - Inconsistent annotaion
--]]

include("premake5.files.lua")

workspace "SumatraPDF"
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
     toolset "v140_xp"
     filter "action:vs2013"
      toolset "v120"
  filter {}

  disablewarnings { "4127", "4324", "4458", "4800" }
  warnings "Extra"

  location "this_is_invalid_location"

  filter "action:vs2015"
    location "vs2015"
  filter {}

  filter "action:vs2013"
    location "vs2013"
  filter {}

  filter "action:gmake"
    location "gmake"
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
    "MultiProcessorCompile",
    "StaticRuntime",
    "Symbols",
    -- "Unicode", TODO: breaks libdjuv?
  }

  filter {"configurations:not ReleasePrefast"}
    flags { "FatalWarnings" }
  filter {}

  exceptionhandling "Off"
  rtti "Off"

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


  project "zlib"
    kind "StaticLib"
    language "C"
    disablewarnings { "4131", "4244", "4996" }
    zlib_files()


  project "libdjvu"
    kind "StaticLib"
    language "C++"
    -- TODO: try /D USE_EXCEPTION_EMULATION to see if it reduces the size
    -- and disables the exceptions warnings
    defines { "NEED_JPEG_DECODER", "THREADMODEL=0", "DDJVUAPI=/**/",  "MINILISPAPI=/**/", "DO_CHANGELOCALE=0" }
    disablewarnings { "4100", "4189", "4244", "4267", "4302", "4311", "4312" }
    disablewarnings { "4456", "4457", "4459", "4530", "4611", "4701", "4702", "4703", "4706" }
    includedirs { "ext/libjpeg-turbo" }
    libdjvu_files()


  project "unarrlib"
    kind "StaticLib"
    language "C"
    -- TODO: for bzip2, need BZ_NO_STDIO and BZ_DEBUG=0
    -- TODO: for lzma, need _7ZIP_PPMD_SUPPPORT
    defines { "HAVE_ZLIB", "HAVE_BZIP2", "HAVE_7Z", "BZ_NO_STDIO", "_7ZIP_PPMD_SUPPPORT" }
    -- TODO: most of these warnings are due to bzip2 and lzma
    disablewarnings { "4100", "4244", "4267", "4456", "4457", "4996" }
    includedirs { "ext/zlib", "ext/bzip2", "ext/lzma/C" }
    unarr_files()


  project "jbig2dec"
    kind "StaticLib"
    language "C"
    defines { "HAVE_STRING_H=1", "JBIG_NO_MEMENTO" }
    disablewarnings { "4018", "4100", "4244", "4267", "4701" }
    includedirs { "ext/jbig2dec" }
    jbig2dec_files()


  project "openjpeg"
    kind "StaticLib"
    language "C"
    disablewarnings { "4100", "4244", "4819" }
    includedirs { "ext/openjpeg" }
    openjpeg_files()


  project "libwebp"
    kind "StaticLib"
    language "C"
    disablewarnings { "4204", "4244", "4057" }
    includedirs { "ext/libwebp" }
    libwebp_files()


  project "libjpeg-turbo"
    kind "StaticLib"
    language "C"
    disablewarnings { "4018", "4100", "4244", "4245" }
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

    filter {'files:**.asm', 'platforms:x64'}
      buildmessage '%{file.relpath}'
      buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
      buildcommands {
        '..\\bin\\nasm.exe -f win64 -D__x86_64__ -DWIN64 -DMSVC -I ../ext/libjpeg-turbo/simd/ -I ../ext/libjpeg-turbo/win/ -o "%{cfg.objdir}/%{file.basename}.obj" "%{file.relpath}"'
      }
    filter {}
    libjpeg_turbo_files()


  project "freetype"
    kind "StaticLib"
    language "C"
    defines { "FT2_BUILD_LIBRARY", "FT_OPTION_AUTOFIT2"}
    disablewarnings { "4018", "4996" }
    includedirs { "ext/freetype2/config" }
    includedirs { "ext/freetype2/include" }
    freetype_files()


  project "chm"
    kind "StaticLib"
    language "C"
    defines { "UNICODE", "_UNICODE", "PPC_BSTR"}
    disablewarnings { "4018", "4057", "4189", "4244", "4267", "4295", "4701", "4706", "4996" }
    files { "ext/CHMLib/src/chm_lib.c", "ext/CHMLib/src/lzx.c" }


  project "engines"
    kind "StaticLib"
    language "C++"
    disablewarnings { "4018", "4057", "4189", "4244", "4267", "4295", "4819" }
    disablewarnings { "4701", "4706", "4838"  }
    includedirs { "src/utils", "src/wingui", "src/mui" }
    includedirs { "ext/synctex", "ext/libdjvu", "ext/CHMLib/src", "ext/zlib", "mupdf/include" }
    engines_files()
    links { "chm" }


  project "mupdf"
    kind "StaticLib"
    language "C"
    defines { "NOCJKFONT", "SHARE_JPEG" }
    disablewarnings {  "4244", "4267", }
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
    mupdf_files()
    links { "zlib", "freetype", "libjpeg-turbo", "jbig2dec", "openjpeg" }
    dependson "buildcmap"


  project "libmupdf"
    kind "SharedLib"
    language "C"
    disablewarnings { "4206" }
    -- premake has logic in vs2010_vcxproj.lua that only sets PlatformToolset
    -- if there is a c/c++ file, so we add a no-op cpp file to force This logic
    files { "src/libmupdf.rc", "tools/premake/no_op_for_premake.cpp" }
    implibname "libmupdf"
    -- TODO: is thre a better way to do it?
    -- TODO: only for windows
    linkoptions { "/DEF:..\\src\\libmupdf.def" }
    links { "mupdf", "libdjvu", "unarrlib", "libwebp" }
    links {
      "advapi32", "kernel32", "user32", "gdi32", "comdlg32",
      "shell32", "windowscodecs", "comctl32", "msimg32",
      "winspool", "wininet", "urlmon", "gdiplus", "ole32",
      "oleAut32", "shlwapi", "version", "crypt32"
    }


  project "synctex"
    kind "StaticLib"
    language "C"
    disablewarnings { "4100", "4244", "4267", "4702", "4706" }
    includedirs { "ext/zlib", "ext/synctex" }
    synctex_files()


  project "utils"
    kind "StaticLib"
    language "C++"
    -- QITABENT in shlwapi.h has incorrect definition and causes 4838
    disablewarnings { "4838" }
    includedirs { "src/utils", "src/wingui", "src/mui", "ext/zlib", "ext/lzma/C" }
    includedirs { "ext/libwebp", "ext/unarr", "mupdf/include" }
    utils_files()


  project "mui"
    kind "StaticLib"
    language "C++"
    includedirs { "src/utils", "src/wingui", "src/mui" }
    mui_files()


  project "uia"
    kind "StaticLib"
    language "C++"
    disablewarnings { "4302", "4311", "4838" }
    includedirs { "src", "src/utils" }
    uia_files()


  project "sumatra"
    kind "StaticLib"
    language "C++"
    -- TODO: 4838 only in settingsstructs.h(642)
    disablewarnings { "4838" }
    includedirs { "src", "src/utils", "src/wingui", "src/mui", "ext/synctex" }
    sumatra_files()


  ---- executables
  project "efi"
    kind "ConsoleApp"
    language "C++"
    disablewarnings { "4091", "4577" }
    includedirs { "src/utils" }
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


  project "cmapdump"
    kind "ConsoleApp"
    language "C"
    -- force 32build so that we can compile 64-bit Sumatra even on 32bit machines
    -- that couldn't run 64-bit cmapdump
    architecture "x86"
    disablewarnings { "4100", "4267" }
    includedirs { "mupdf/include" }
    files { "mupdf/scripts/cmapdump.c" }


  -- unfortunate we need buildcmap re-direction but we can do
  -- dependson { "cmapdump" } in "mupdf" as it'll break mupdf project
  project "buildcmap"
    kind "ConsoleApp"
    language "C"
    -- premake has logic in vs2010_vcxproj.lua that only sets PlatformToolset
    -- if there is a c/c++ file, so we add a no-op cpp file to force This logic
    files { "tools/premake/no_op_console.c" }
    dependson { "cmapdump" }
    postbuildcommands { "{COPY} %{cfg.targetdir}\\cmapdump.exe ..\\bin" }
    postbuildcommands { "cd .. & call scripts\\gen_mupdf_generated.bat bin\\cmapdump.exe"}


  project "enginedump"
    kind "ConsoleApp"
    language "C++"
    includedirs { "src", "src/utils", "src/mui", "mupdf/include" }
    engine_dump_files()
    links { "engines", "utils", "mupdf", "unarrlib", "libwebp", "libdjvu" }
    links {
      "comctl32", "gdiplus", "msimg32", "shlwapi",
      "version", "windowscodecs"
    }


  project "unarr"
    kind "ConsoleApp"
    language "C"
    disablewarnings { "4100" }
    files { "ext/unarr/main.c" }
    links { "unarrlib", "zlib" }


  project "test_util"
    kind "ConsoleApp"
    language "C++"
    disablewarnings { "4838" }
    defines { "NO_LIBMUPDF" }
    includedirs { "src/utils" }
    test_util_files()
    links { "gdiplus", "comctl32", "shlwapi", "Version" }


  project "signfile"
    kind "ConsoleApp"
    language "C++"
    includedirs { "src/utils", "mupdf/include"}
    files { "src/tools/signfile.cpp" }
    links { "utils", "mupdf" }
    links { "crypt32", "shlwapi" }


  project "plugin-test"
    kind "WindowedApp"
    language "C++"
    flags { "WinMain" }
    includedirs { "src/utils" }
    files { "src/tools/plugin-test.cpp" }
    links { "utils", "mupdf" }
    links { "shlwapi" }


  project "MakeLZSA"
    kind "ConsoleApp"
    language "C++"
    files { "src/tools/MakeLzSA.cpp" }
    includedirs { "src/utils", "ext/zlib", "ext/lzma/C", "ext/unarr" }
    links { "unarrlib", "utils", "zlib" }
    links { "shlwapi" }


  project "PdfFilter"
    kind "SharedLib"
    language "C++"
    disablewarnings { "4838" }
    filter {"configurations:Debug"}
      defines { "BUILD_TEX_IFILTER", "BUILD_EPUB_IFILTER" }
    filter {}
    includedirs { "src", "src/utils", "src/wingui", "src/mui", "mupdf/include" }
    pdf_filter_files()
    links { "utils", "libmupdf" }
    links { "comctl32", "gdiplus", "shlwapi", "version"  }


  project "PdfPreview"
    kind "SharedLib"
    language "C++"
    disablewarnings { "4838" }
    includedirs {
      "src", "src/utils", "src/wingui", "src/mui", "mupdf/include",
      "ext/libdjvu", "ext/CHMLib/src", "ext/zlib"
    }
    pdf_preview_files()
    filter {"configurations:Debug"}
      defines {
        "BUILD_XPS_PREVIEW", "BUILD_DJVU_PREVIEW", "BUILD_EPUB_PREVIEW",
        "BUILD_FB2_PREVIEW", "BUILD_MOBI_PREVIEW", "BUILD_CBZ_PREVIEW",
        "BUILD_CBR_PREVIEW", "BUILD_CB7_PREVIEW", "BUILD_CBT_PREVIEW",
        "BUILD_TGA_PREVIEW"
      }
    filter {}
    -- TODO: "chm" should only be for Debug config but doing links { "chm" }
    -- in the filter breaks linking by setting LinkLibraryDependencies to false
    links { "utils", "libmupdf", "chm" }
    links { "comctl32", "gdiplus", "msimg32", "shlwapi", "version" }


  project "SumatraPDF"
    kind "WindowedApp"
    language "C++"
    flags { "NoManifest", "WinMain" }
    includedirs { "src", "src/utils", "src/wingui", "src/mui" }
    sumatrapdf_files()
    files {
      "docs/releasenotes.txt",
      "docs/releaseplan.txt",
    }
    links {
      "engines", "libdjvu",  "libwebp", "mui", "mupdf", "sumatra", "synctex",
      "uia", "unarrlib", "utils"
    }
    links {
      "comctl32", "gdiplus", "msimg32", "shlwapi", "urlmon",
      "version", "windowscodecs", "wininet"
    }


  project "SumatraPDF-no-MUPDF"
    kind "WindowedApp"
    language "C++"
    flags { "NoManifest", "WinMain" }
    includedirs { "src", "src/utils", "src/wingui", "src/mui", "mupdf/include" }
    sumatrapdf_files()
    files { "src/MuPDF_Exports.cpp" }
    links {
      "synctex", "sumatra", "libmupdf", "utils", "mui", "engines",
      "uia", "unarrlib", "libwebp"
    }
    links {
      "comctl32", "gdiplus", "msimg32", "shlwapi", "urlmon",
      "version", "windowscodecs", "wininet"
    }


  project "Uninstaller"
    kind "WindowedApp"
    language "C++"
    defines { "BUILD_UNINSTALLER" }
    flags { "NoManifest", "WinMain" }
    disablewarnings { "4018", "4244", "4264", "4838", "4702", "4706" }
    uninstaller_files()
    includedirs { "src", "src/utils", "ext/zlib", "ext/unarr", "ext/lzma/C" }
    links { "utils", "zlib", "unarrlib" }
    links {
      "comctl32", "gdiplus", "msimg32", "shlwapi", "urlmon",
       "version", "windowscodecs", "wininet"
    }


  -- faster to compile than Installer
  project "InstallerNoData"
    kind "WindowedApp"
    language "C++"
    flags { "NoManifest", "WinMain" }
    defines { "NO_LIBWEBP", "NO_LIBMUPDF", "HAVE_ZLIB", "HAVE_BZIP2", "HAVE_7Z" }
    disablewarnings {
      "4018", "4100", "4131", "4244", "4267", "4302", "4311", "4312", "4456",
      "4457", "4838", "4702", "4706", "4996"
    }
    installer_files()
    includedirs { "src", "src/utils", "ext/zlib", "ext/unarr", "ext/lzma/C", "ext/bzip2" }
    links {
      "comctl32", "gdiplus", "msimg32", "shlwapi", "urlmon",
      "version", "windowscodecs", "wininet"
    }


  project "Installer"
    kind "WindowedApp"
    language "C++"
    flags { "NoManifest", "WinMain" }
    defines { "NO_LIBWEBP", "NO_LIBMUPDF", "HAVE_ZLIB", "HAVE_BZIP2", "HAVE_7Z" }
    resdefines { "INSTALL_PAYLOAD_ZIP=.\\%{cfg.targetdir}\\InstallerData.dat" }
    disablewarnings {
      "4018", "4100", "4131", "4244", "4267", "4302", "4311",
      "4312", "4456", "4457", "4838", "4702", "4706", "4996"
    }
    installer_files()
    includedirs { "src", "src/utils", "ext/zlib", "ext/unarr", "ext/lzma/C", "ext/bzip2" }
    links {
      "comctl32", "gdiplus", "msimg32", "shlwapi", "urlmon",
      "version", "windowscodecs", "wininet"
    }
    dependson { "MakeLZSA", "SumatraPDF-no-MUPDF", "PdfFilter", "PdfPreview", "Uninstaller" }
    -- Note: to allow 64-bit builds on 32-bit machine, always use 32-bit MakeLZSA.exe
    -- TODO: checkin MakeLZSA.exe to bin and use that because this might still fail
    -- if we didn't build 32-bit build first
    prebuildcommands { "cd %{cfg.targetdir} & ..\\rel\\MakeLZSA.exe InstallerData.dat SumatraPDF-no-MUPDF.exe:SumatraPDF.exe libmupdf.dll:libmupdf.dll PdfFilter.dll:PdfFilter.dll PdfPreview.dll:PdfPreview.dll Uninstaller.exe:uninstall.exe ..\\mupdf\\resources\\fonts\\droid\\DroidSansFallback.ttf:DroidSansFallback.ttf"  }


  -- dummy project that builds all other projects
  project "all"
    kind "ConsoleApp"
    language "C"
    -- premake has logic in vs2010_vcxproj.lua that only sets PlatformToolset
    -- if there is a c/c++ file, so we add a no-op cpp file to force This logic
    files { "tools/premake/no_op_console.c" }
    dependson {
      "PdfPreview", "PdfFilter", "SumatraPDF", "SumatraPDF-no-MUPDF",
      "test_util", "cmapdump", "signfile", "plugin-test", "MakeLZSA",
      "mutool", "mudraw", "Uninstaller", "enginedump", "efi", "unarr"
    }
