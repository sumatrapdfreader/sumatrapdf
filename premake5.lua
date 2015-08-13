-- to generate Visual Studio files in vs-premake directory, run:
-- premake5 vs2013 or premake5 vs2015
-- premake4 is obsolete and doesn't support VS 2013+
-- I'm using premake5 alpha4 from http://premake.github.io/download.html#v5
-- TODO:
-- 1. the final output (SumatraPDF.exe etc.) all end up in the same location
--    Should go to different directories.
-- 2. generate mupdf/generated or check them in
-- 3. libmupdf doesn't have the right toolset set
-- 4. assembly for libjpeg-turbo
-- 5. a way to define SVN_PRE_RELEASE_VER, via build_config.h ?
-- 6. Installer
-- 7. I get "6>cl : Command line warning D9014: invalid value '2664' for '/wd'; assuming '5999'
-- 8. fix "LINK : warning LNK4068: /MACHINE not specified; defaulting to X86" in 32 bit build

dofile("premake5.files.lua")

-- TODO: rename solution to workspace. workspace is the documented name
-- but latest alpha4 doesn't recognize it yet
solution "SumatraPDF"
  configurations { "Debug", "Release" }
  platforms { "x32", "x64" }
  startproject "SumatraPDF"

  filter "platforms:x32"
     architecture "x86"
     toolset "v140_xp"

  filter "platforms:x64"
     architecture "x86_64"
     toolset "v140"
     -- TODO: 64bit has more warnigs. Fix warnings in Sumatra code,
     -- selectively disable for other projects
     disablewarnings {
       "4311",
       "4312",
       "4302",
       "4267",
       "4244"
     }

  filter {}

  location "this_is_invlid_location"
  filter "action:vs2015"
    location "vs2015"
  filter {}

  filter "action:vs2013"
    location "vs2013"
  filter {}

  -- https://github.com/premake/premake-core/wiki/flags
  flags {
    "Symbols",
    "NoExceptions",
    "NoRTTI",
    --"FatalWarnings", TODO: when ready
    --"MultiProcessorCompile",
    --"UndefinedIndentifiers", TODO: not yet in alpha4 ?
    -- "Unicode", TODO: breaks libdjuv?
  }

  defines { "WIN32", "_WIN32", "_CRT_SECURE_NO_WARNINGS", "WINVER=0x0501", "_WIN32_WINNT=0x0501" }

  filter "configurations:Debug"
    defines { "DEBUG" }

  filter "configurations:Release"
    defines { "NDEBUG" }
    flags {
      "LinkTimeOptimization",
    }
    optimize "On"

  filter {}

  project "efi"
    kind "ConsoleApp"
    language "C++"

    -- TODO: only for msvc
    disablewarnings { "4800", "4091", "4577" }

    efi_files()
    includedirs { "src/utils" }
    links { }

  project "zlib"
    kind "StaticLib"
    language "C"

    -- TODO: only for msvc
    disablewarnings { "4996" }

    zlib_files()

  project "libdjvu"
    kind "StaticLib"
    language "C++"

    -- TODO: try /D USE_EXCEPTION_EMULATION to see if it reduces the size
    -- and disables the exceptions warnings
    defines { "NEED_JPEG_DECODER", "THREADMODEL=0", "DDJVUAPI=/**/",  "MINILISPAPI=/**/", "DO_CHANGELOCALE=0" }
    includedirs { "ext/libjpeg-turbo" }

    -- TODO: only for msvc
    -- 4530 - exception mismatch
    disablewarnings { "2664", "4530" }
    libdjvu_files()

  project "unarr"
    kind "StaticLib"
    language "C++"

    defines { "HAVE_ZLIB", "HAVE_BZIP2", "HAVE_7Z" }
    includedirs { "ext/zlib", "ext/bzip2", "ext/lzma/C" }
    -- TODO: only for msvc
    disablewarnings { "4996" }

    unarr_files()

  project "jbig2dec"
    kind "StaticLib"
    language "C"
    --targetdir "%{cfg.buildcfg}"

    defines { "HAVE_STRING_H=1", "JBIG_NO_MEMENTO" }
    includedirs { "ext/jbig2dec" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018" }

    jbig2dec_files()

  project "openjpeg"
    kind "StaticLib"
    language "C"

    includedirs { "ext/openjpeg" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018" }

    openjpeg_files()

  project "libwebp"
    kind "StaticLib"
    language "C"

    includedirs { "ext/libwebp" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018" }

    libwebp_files()

  project "libjpeg-turbo"
    kind "StaticLib"
    language "C"

    includedirs { "ext/libjpeg-turbo" }
    includedirs { "ext/libjpeg-turbo/simd" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018" }

    libjpeg_turbo_files()

  project "freetype"
    kind "StaticLib"
    language "C"

    includedirs { "ext/freetype2/config" }
    includedirs { "ext/freetype2/include" }
    defines { "_HAS_EXCEPTIONS=0" }
    defines { "FT2_BUILD_LIBRARY", "FT_OPTION_AUTOFIT2"}
    -- TODO: only for msvc
    disablewarnings { "4996", "4018" }

    freetype_files()

  project "sumatra"
    kind "StaticLib"
    language "C++"
    --targetdir "%{cfg.buildcfg}"

    includedirs { "src/utils", "src/wingui", "src/mui", "ext/lzma/C" }
    includedirs { "ext/libwebp", "ext/unarr", "mupdf/include", "src" }
    includedirs { "ext/synctex", "ext/libdjvu", "ext/CHMLib/src" }

    defines { "_HAS_EXCEPTIONS=0" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018", "4800", "4838" }

    sumatra_files()

  project "utils"
    kind "StaticLib"
    language "C++"
    --targetdir "%{cfg.buildcfg}"

    includedirs { "src/utils", "src/wingui", "src/mui", "ext/zlib", "ext/lzma/C" }
    includedirs { "ext/libwebp", "ext/unarr", "mupdf/include" }

    defines { "_HAS_EXCEPTIONS=0" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018", "4800", "4838" }

    utils_files()

  project "mui"
    kind "StaticLib"
    language "C++"
    --targetdir "%{cfg.buildcfg}"

    includedirs { "src/utils", "src/wingui", "src/mui" }

    defines { "_HAS_EXCEPTIONS=0" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018", "4800", "4838" }

    mui_files()

  project "engines"
    kind "StaticLib"
    language "C++"
    --targetdir "%{cfg.buildcfg}"

    includedirs { "src/utils", "src/wingui", "src/mui" }
    includedirs { "ext/synctex", "ext/libdjvu", "ext/CHMLib/src", "ext/zlib", "mupdf/include" }

    defines { "_HAS_EXCEPTIONS=0" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018", "4800", "4838", "4244" }

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
       -- A message to display while this build step is running (optional)
       buildmessage 'Compiling %{file.relpath}'

       -- Note: must be -I ../mupdf/, not just -I ../mupdf
       buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
       buildcommands {
          '..\\bin\\nasm.exe -f win32 -I ../mupdf/ -o "%{cfg.objdir}/%{file.basename}.obj" "%{file.relpath}"'
       }
    filter {}

    filter {'files:**.asm', 'platforms:x64'}
      -- A message to display while this build step is running (optional)
      buildmessage 'Compiling %{file.relpath}'

      -- Note: must be -I ../mupdf/, not just -I ../mupdf

      buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
      buildcommands {
        '..\\bin\\nasm.exe -f win64 -DWIN64 -I ../mupdf/ -o "%{cfg.objdir}/%{file.basename}.obj" "%{file.relpath}"'
      }
    filter {}

    defines { "NOCJKFONT", "SHARE_JPEG" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018", "4800", "4838", "4244" }
    mupdf_files()

    links {
      "zlib",
      "freetype",
      "libjpeg-turbo",
      "jbig2dec",
      "openjpeg",
    }

  project "SumatraPDF"
    kind "WindowedApp"
    language "C++"
    --targetdir "%{cfg.buildcfg}"

    flags {
      "WinMain",
      "NoManifest",
    }

    defines { "_HAS_EXCEPTIONS=0" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018", "4800", "4838", "4244" }

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
      "shell32.lib", "WindowsCodecs.lib", "comctl32.lib", "Msimg32.lib",
      "Winspool.lib", "wininet.lib", "urlmon.lib", "gdiplus.lib", "ole32.lib",
      "OleAut32.lib", "shlwapi.lib", "version.lib", "crypt32.lib"
    }

  project "libmupdf"
    kind "SharedLib"
    language "C"

    implibname "libmupdf"

    -- TODO: not sure if it works at all - I don't see it in generated
    -- Is thre a better way to do it?
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

  project "SumatraPDF-no-MUPDF"
    kind "WindowedApp"
    language "C++"

    flags {
      "WinMain",
      "NoManifest",
    }

    defines { "_HAS_EXCEPTIONS=0" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018", "4800", "4838", "4244" }

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

    disablewarnings { "4800" }

    includedirs {
      "src/utils", "ext/zlib", "ext/lzma/C", "ext/unarr"
    }

    links {
      "utils",
      "unarr",
      "zlib",
    }

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

    disablewarnings { "4800", "4838" }
    defines { "_HAS_EXCEPTIONS=0" }

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

    disablewarnings { "4800", "4838" }
    defines { "_HAS_EXCEPTIONS=0" }

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
