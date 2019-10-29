--[[
To generate Visual Studio files (in vs2017/ directory), run:
scripts\premake-regenerate-vs-projects.ps1

I'm using premake5 alpha12 from http://premake.github.io/download.html#v5
(premake4 won't work, it doesn't support VS 2013+)

Note about nasm: when providing "-I foo/bar/" flag to nasm.exe, it must be
"foo/bar/" and not just "foo/bar".

Reference for warnings:
 4018 - signed/unsigned mismatch
 4057 - function X differs in indirection to slightly different base types
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
 4310 - 64bit, cast truncates constant value
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

 Linker warning
 4221 - This object file does not define any previously undefined public symbols, so it will not be used by any link operation that consumes this library

Prefast:
 28125 - function X must be called in try/except (InitializeCriticalSection)
 28252 - Inconsistent annotaion
 28253 - Inconsistent annotaion
--]]

include("premake5.files.lua")

workspace "SumatraPDF"
  configurations { "Debug", "Release", "ReleasePrefast" }
  platforms { "x32", "x32_xp", "x64" }
  startproject "SumatraPDF-mupdf-dll"

  filter "platforms:x32_xp"
    architecture "x86"
    toolset "v141_xp"
    buildoptions { "/arch:IA32" } -- disable the default /arch:SSE2 for 32-bit builds
  filter {}

  filter "platforms:x32"
     architecture "x86"
  filter {}

  filter "platforms:x64"
     architecture "x86_64"
     -- strangely this is not set by default for rc.exe
     resdefines { "_WIN64" }
  filter {}

  disablewarnings { "4127", "4189", "4324", "4458", "4800" }
  warnings "Extra"

  location "this_is_invalid_location"

  filter "action:vs2017"
    location "vs2017"
  filter {}

  filter "action:vs2019"
    location "vs2019"
  filter {}

  filter "action:gmake"
    location "gmake"
  filter {}

  filter {"platforms:x32", "configurations:Release"}
    targetdir "out/rel32"
  filter {"platforms:x32", "configurations:ReleasePrefast"}
    targetdir "out/rel32_prefast"
  filter {"platforms:x32", "configurations:Debug"}
    targetdir "out/dbg32"

  filter {"platforms:x32_xp", "configurations:Release"}
    targetdir "out/rel32_xp"
  filter {"platforms:x32_xp", "configurations:ReleasePrefast"}
    targetdir "out/rel32_prefast_xp"
  filter {"platforms:x32_xp", "configurations:Debug"}
    targetdir "out/dbg32_xp"

  filter {"platforms:x64", "configurations:Release"}
    targetdir "out/rel64"
  filter {"platforms:x64", "configurations:ReleasePrefast"}
    targetdir "out/rel64_prefast"
  filter {"platforms:x64", "configurations:Debug"}
    targetdir "out/dbg64"
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
    -- "Unicode", TODO: breaks libdjuv?
  }

  filter {"configurations:not ReleasePrefast"}
    flags { "FatalCompileWarnings" }
  filter {}

  exceptionhandling "Off"
  rtti "Off"

  defines { "WIN32", "_WIN32", "_CRT_SECURE_NO_WARNINGS", "WINVER=0x0501", "_WIN32_WINNT=0x0501" }

  filter "configurations:Debug"
    defines { "DEBUG" }

  filter "configurations:Release*"
    defines { "NDEBUG" }
    flags {
      "LinkTimeOptimization",
    }
    optimize "On"

    filter "configurations:ReleasePrefast"
      -- TODO: somehow /analyze- is default which creates warning about
      -- over-ride from cl.exe. Don't know how to disable the warning
      buildoptions { "/analyze" }
      disablewarnings { "28125", "28252", "28253" }
  filter {}


  project "zlib"
    kind "StaticLib"
    language "C"
    disablewarnings { "4131", "4244", "4245", "4267", "4996" }
    zlib_files()

  project "libdjvu"
    kind "StaticLib"
    characterset ("MBCS")
    language "C++"
    -- TODO: try /D USE_EXCEPTION_EMULATION to see if it reduces the size
    -- and disables the exceptions warnings
    defines { "NEED_JPEG_DECODER", "THREADMODEL=0", "DDJVUAPI=/**/",  "MINILISPAPI=/**/", "DO_CHANGELOCALE=0", "DEBUGLVL=0" }
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
    disablewarnings { "4018", "4100", "4146", "4244", "4267", "4456", "4701" }
    includedirs { "ext/jbig2dec" }
    jbig2dec_files()


  project "openjpeg"
    kind "StaticLib"
    language "C"
    disablewarnings { "4100", "4244", "4310", "4819" }

    -- openjpeg has opj_config_private.h for such over-rides
    -- but we can't change it because we bring openjpeg as submodule
    -- and we can't provide our own in a different directory because
    -- msvc will include the one in ext/openjpeg/src/lib/openjp2 first
    -- because #include "opj_config_private.h" searches current directory first
    defines { "USE_JPIP", "OPJ_STATIC", "OPJ_EXPORTS" }
    openjpeg_files()


  project "libwebp"
    kind "StaticLib"
    language "C"
    disablewarnings { "4204", "4244", "4057", "4245", "4310" }
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
    filter {'files:**.asm', 'platforms:x32 or x32_xp'}
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
    cppdialect "C++17"
    disablewarnings { "4018", "4057", "4189", "4244", "4267", "4295", "4819" }
    disablewarnings { "4701", "4706", "4838"  }
    includedirs { "src" }
    includedirs { "ext/synctex", "ext/libdjvu", "ext/CHMLib/src", "ext/zlib", "mupdf/include" }
    engines_files()
    links { "chm" }


  project "mupdf"
    kind "StaticLib"
    language "C"

    -- for openjpeg, OPJ_STATIC is alrady defined in load-jpx.c
    -- so we can't double-define it
    defines { "USE_JPIP", "OPJ_EXPORTS" }

    defines { "NOCJKFONT", "SHARE_JPEG" }
    disablewarnings {  "4244", "4267", }
    -- force including mupdf/scripts/openjpeg/opj_config_private.h
    -- with our build over-rides
    includedirs { "mupdf/scripts/openjpeg" }

    includedirs {
      "mupdf/include", "mupdf/generated", "ext/zlib",
      "ext/freetype2/config", "ext/freetype2/include",
      "ext/jbig2dec", "ext/libjpeg-turbo", "ext/openjpeg/src/lib/openjp2"
    }
    -- .\ext\..\bin\nasm.exe -I .\mupdf\ -f win32 -o .\obj-rel\mupdf\font_base14.obj
    -- .\mupdf\font_base14.asm
    filter {'files:**.asm', 'platforms:x32 or x32_xp'}
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
    cppdialect "C++17"
    -- QITABENT in shlwapi.h has incorrect definition and causes 4838
    disablewarnings { "4838" }
    includedirs { "src", "ext/zlib", "ext/lzma/C" }
    includedirs { "ext/libwebp/src", "ext/unarr", "mupdf/include" }
    utils_files()


  project "mui"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    includedirs { "src" }
    mui_files()


  project "uia"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    disablewarnings { "4302", "4311", "4838" }
    includedirs { "src" }
    uia_files()


  project "sumatra"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    -- TODO: 4838 only in settingsstructs.h(642)
    disablewarnings { "4838" }
    includedirs { "src", "ext/synctex" }
    sumatra_files()


  ---- executables
  project "efi"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
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
    cppdialect "C++17"
    includedirs { "src", "mupdf/include" }
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
    cppdialect "C++17"
    disablewarnings { "4838" }
    defines { "NO_LIBMUPDF" }
    includedirs { "src" }
    test_util_files()
    links { "gdiplus", "comctl32", "shlwapi", "Version" }


  project "signfile"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    includedirs { "src", "mupdf/include"}
    files { "src/tools/signfile.cpp" }
    links { "utils", "mupdf" }
    links { "crypt32", "shlwapi" }


  project "plugin-test"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++17"
    entrypoint "WinMainCRTStartup"
    includedirs { "src" }
    files { "src/tools/plugin-test.cpp" }
    links { "utils", "mupdf" }
    links { "shlwapi" }


  project "MakeLZSA"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    makelzsa_files()
    includedirs { "src", "ext/zlib", "ext/lzma/C", "ext/unarr" }
    links { "unarrlib", "zlib" }
    links { "shlwapi" }


  project "PdfFilter"
    kind "SharedLib"
    language "C++"
    cppdialect "C++17"
    disablewarnings { "4838" }
    filter {"configurations:Debug"}
      defines { "BUILD_TEX_IFILTER", "BUILD_EPUB_IFILTER" }
    filter {}
    includedirs { "src", "mupdf/include" }
    pdf_filter_files()
    links { "utils", "libmupdf" }
    links { "comctl32", "gdiplus", "shlwapi", "version"  }


  project "PdfPreview"
    kind "SharedLib"
    language "C++"
    cppdialect "C++17"
    disablewarnings { "4838" }
    includedirs {
      "src", "mupdf/include",
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
    cppdialect "C++17"
    entrypoint "WinMainCRTStartup"
    flags { "NoManifest" }
    includedirs { "src" }
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
      "comctl32", "delayimp", "gdiplus", "msimg32", "shlwapi", "urlmon",
      "version", "windowscodecs", "wininet"
    }
    -- this is to prevent dll hijacking
    linkoptions { "/DELAYLOAD:gdiplus.dll /DELAYLOAD:msimg32.dll /DELAYLOAD:shlwapi.dll /DELAYLOAD:urlmon.dll /DELAYLOAD:version.dll /DELAYLOAD:wininet.dll"}


  project "SumatraPDF-mupdf-dll"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++17"
    entrypoint "WinMainCRTStartup"
    flags { "NoManifest" }
    includedirs { "src", "mupdf/include" }
    sumatrapdf_files()
    files { "src/MuPDF_Exports.cpp" }
    links {
      "synctex", "sumatra", "libmupdf", "utils", "mui", "engines",
      "uia", "unarrlib", "libwebp"
    }
    links {
      "comctl32", "delayimp", "gdiplus", "msimg32", "shlwapi", "urlmon",
      "version", "wininet"
    }
    -- this is to prevent dll hijacking
    linkoptions { "/DELAYLOAD:gdiplus.dll /DELAYLOAD:msimg32.dll /DELAYLOAD:shlwapi.dll /DELAYLOAD:urlmon.dll /DELAYLOAD:version.dll /DELAYLOAD:wininet.dll"}


  project "Uninstaller"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++17"
    entrypoint "WinMainCRTStartup"
    flags { "NoManifest" }
    disablewarnings { "4018", "4244", "4264", "4838", "4702", "4706" }
    uninstaller_files()
    includedirs { "src", "ext/zlib", "ext/unarr", "ext/lzma/C" }
    links { "utils", "zlib", "unarrlib" }
    links {
      "comctl32", "gdiplus", "shlwapi", "version", "wininet"
    }


  -- faster to compile than Installer
  project "InstallerNoData"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++17"
    entrypoint "WinMainCRTStartup"
    flags { "NoManifest" }
    defines { "NO_LIBWEBP", "NO_LIBMUPDF", "HAVE_ZLIB", "HAVE_BZIP2", "HAVE_7Z" }
    disablewarnings {
      "4018", "4100", "4131", "4244", "4245", "4267", "4302", "4311", "4312", "4456",
      "4457", "4838", "4702", "4706", "4996"
    }
    installer_files()
    includedirs { "src", "ext/zlib", "ext/unarr", "ext/lzma/C", "ext/bzip2" }
    links {
      "comctl32", "delayimp", "gdiplus", "shlwapi", "version", "wininet"
    }
    -- this is to prevent dll hijacking
    linkoptions { "/DELAYLOAD:gdiplus.dll /DELAYLOAD:shlwapi.dll /DELAYLOAD:version.dll /DELAYLOAD:wininet.dll"}


  project "Installer"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++17"
    entrypoint "WinMainCRTStartup"
    flags { "NoManifest" }
    defines { "NO_LIBWEBP", "NO_LIBMUPDF", "HAVE_ZLIB", "HAVE_BZIP2", "HAVE_7Z" }
    resdefines { "INSTALL_PAYLOAD_ZIP=.\\%{cfg.targetdir}\\InstallerData.dat" }
    disablewarnings {
      "4018", "4100", "4131", "4244", "4245", "4267", "4302", "4311",
      "4312", "4456", "4457", "4838", "4702", "4706", "4996"
    }
    installer_files()
    includedirs { "src", "ext/zlib", "ext/unarr", "ext/lzma/C", "ext/bzip2" }
    links {
      "comctl32", "delayimp", "gdiplus", "shlwapi", "version", "wininet"
    }
    -- this is to prevent dll hijacking
    linkoptions { "/DELAYLOAD:gdiplus.dll /DELAYLOAD:shlwapi.dll /DELAYLOAD:version.dll /DELAYLOAD:wininet.dll"}

    dependson { "SumatraPDF-mupdf-dll", "PdfFilter", "PdfPreview", "Uninstaller" }
    prebuildcommands { "cd %{cfg.targetdir} & ..\\..\\bin\\MakeLZSA.exe InstallerData.dat SumatraPDF-mupdf-dll.exe:SumatraPDF.exe libmupdf.dll:libmupdf.dll PdfFilter.dll:PdfFilter.dll PdfPreview.dll:PdfPreview.dll Uninstaller.exe:uninstall.exe ..\\..\\mupdf\\resources\\fonts\\droid\\DroidSansFallback.ttf:DroidSansFallback.ttf"  }

  project "TestApp"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++17"
    entrypoint "WinMainCRTStartup"
    flags { "NoManifest" }
    includedirs { "src" }
    test_app_files()
    links {
      "comctl32", "gdiplus", "msimg32", "shlwapi", "urlmon",
      "version", "wininet", "d2d1.lib",
    }

  -- dummy project that builds all other projects
  project "all"
    kind "ConsoleApp"
    language "C"
    -- premake has logic in vs2010_vcxproj.lua that only sets PlatformToolset
    -- if there is a c/c++ file, so we add a no-op cpp file to force This logic
    files { "tools/premake/no_op_console.c" }
    dependson {
      "PdfPreview", "PdfFilter", "SumatraPDF", "SumatraPDF-mupdf-dll",
      "test_util", "cmapdump", "signfile", "plugin-test", "MakeLZSA",
      "mutool", "mudraw", "Uninstaller", "enginedump", "efi", "unarr"
    }
