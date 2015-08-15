--[[
To generate Visual Studio files in vs2015 directory, run: premake5 vs2015

I'm using premake5 alpha4 from http://premake.github.io/download.html#v5
(premake4 won't work, it doesn't support vs 2013+)

TODO:
* generate mupdf/generated or check them in
* Installer
* fix "LINK : warning LNK4068: /MACHINE not specified; defaulting to X86" in 32 bit build in sumatra.lib
* a way to define SVN_PRE_RELEASE_VER, via build_config.h ? or msbuild
* compare compilation flags nmake vs. us from compilation logs

Code fixes:
* fix 64bit warnings ("4311", "4312", "4302", "4244", "4264") in Sumatra code
  (not dependencies)
* fix all 4100 in our code (by using UNREFERENCED_PARAMETER, not by deleting parameter names from interface definitions)

Note about nasm: when providing "-I foo/bar/" flag to nasm.exe, it must be
"foo/bar/" and not just "foo/bar".

Reference for warnings:
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

Prefast:
 28125 - function X must be called in try/except (InitializeCriticalSection)
 28252 - Inconsistent annotaion
 28253 - Inconsistent annotaion

--]]

include("premake5.files.lua")

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

  disablewarnings { "4100", "4324", "4458", "4800" }
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
    disablewarnings { "4100", "4127", "4189", "4244", "4267", "4302", "4311", "4312" }
    disablewarnings { "4456", "4457", "4459", "4530", "4611", "4701", "4702", "4703", "4706" }
    includedirs { "ext/libjpeg-turbo" }
    libdjvu_files()


  project "unarr"
    kind "StaticLib"
    language "C"
    defines { "HAVE_ZLIB", "HAVE_BZIP2", "HAVE_7Z" }
    disablewarnings { "4100", "4127", "4244", "4267", "4456", "4457", "4996" }
    includedirs { "ext/zlib", "ext/bzip2", "ext/lzma/C" }
    unarr_files()


  project "jbig2dec"
    kind "StaticLib"
    language "C"
    defines { "HAVE_STRING_H=1", "JBIG_NO_MEMENTO" }
    disablewarnings { "4018", "4100", "4127", "4244", "4267", "4701", "4996" }
    includedirs { "ext/jbig2dec" }
    jbig2dec_files()


  project "openjpeg"
    kind "StaticLib"
    language "C"
    disablewarnings { "4018", "4127", "4244", "4996" }
    includedirs { "ext/openjpeg" }
    openjpeg_files()


  project "libwebp"
    kind "StaticLib"
    language "C"
    disablewarnings { "4018", "4057", "4127", "4204", "4244", "4996" }
    includedirs { "ext/libwebp" }
    libwebp_files()


  project "libjpeg-turbo"
    kind "StaticLib"
    language "C"
    disablewarnings { "4018", "4100", "4127", "4244", "4245", "4996" }
    includedirs { "ext/libjpeg-turbo" }
    includedirs { "ext/libjpeg-turbo/simd" }

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
    disablewarnings { "4018", "4127", "4996" }
    includedirs { "ext/freetype2/config" }
    includedirs { "ext/freetype2/include" }
    freetype_files()


  project "chm"
    kind "StaticLib"
    language "C"
    defines { "UNICODE", "_UNICODE", "PPC_BSTR"}
    disablewarnings { "4018", "4057", "4127", "4189", "4244", "4267", "4295", "4701", "4706", "4996" }
    includedirs { }
    files { "ext/CHMLib/src/chm_lib.c", "ext/CHMLib/src/lzx.c" }


  project "engines"
    kind "StaticLib"
    language "C++"
    disablewarnings { "4018", "4057", "4189", "4244", "4267", "4295" }
    disablewarnings { "4701", "4706", "4838"  }
    includedirs { "src/utils", "src/wingui", "src/mui" }
    includedirs { "ext/synctex", "ext/libdjvu", "ext/CHMLib/src", "ext/zlib", "mupdf/include" }
    engines_files()
    links { "chm" }


  project "mupdf"
    kind "StaticLib"
    language "C"
    defines { "NOCJKFONT", "SHARE_JPEG" }
    disablewarnings {  "4018", "4244", "4267", "4838", "4996", }
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
    links { "mupdf", "libdjvu", "unarr", "libwebp" }
    links {
      "advapi32", "kernel32", "user32", "gdi32", "comdlg32",
      "shell32", "windowscodecs", "comctl32", "msimg32",
      "winspool", "wininet", "urlmon", "gdiplus", "ole32",
      "oleAut32", "shlwapi", "version", "crypt32"
    }


  project "synctex"
    kind "StaticLib"
    language "C"
    disablewarnings { "4127", "4244", "4267", "4702", "4706" }
    includedirs { "ext/zlib", "ext/synctex" }
    synctex_files()


  project "utils"
    kind "StaticLib"
    language "C++"
    disablewarnings { "4018", "4838" }
    -- TODO: DbgHelpDyn.cpp 64bit warnings only, fix the code
    disablewarnings { "4302", "4311", "4312" }
    includedirs { "src/utils", "src/wingui", "src/mui", "ext/zlib", "ext/lzma/C" }
    includedirs { "ext/libwebp", "ext/unarr", "mupdf/include" }
    utils_files()


  project "mui"
    kind "StaticLib"
    language "C++"
    disablewarnings { "4018", "4838" }
    includedirs { "src/utils", "src/wingui", "src/mui" }
    mui_files()


  project "sumatra"
    kind "StaticLib"
    language "C++"
    disablewarnings { "4018", "4302", "4311", "4838" }
    includedirs { "src/utils", "src/wingui", "src/mui", "ext/lzma/C" }
    includedirs { "ext/libwebp", "ext/unarr", "mupdf/include", "src" }
    includedirs { "ext/synctex", "ext/libdjvu", "ext/CHMLib/src" }
    sumatra_files()


  ---- executables
  project "efi"
    kind "ConsoleApp"
    language "C++"
    disablewarnings { "4091", "4577" }
    includedirs { "src/utils" }
    efi_files()
    links { }


  project "mutool"
    kind "ConsoleApp"
    language "C"
    disablewarnings { "4267" }
    includedirs { "ext/zlib", "ext/lzma/C", "ext/unarr", "mupdf/include" }
    mutool_files()
    links { "mupdf" }
    links { "windowscodecs" }
    linkoptions { "/ENTRY:\"wmainCRTStartup\"" }
    -- TODO: a better fix should be entrypoint, which was added in
    -- https://github.com/premake/premake-core/commit/b9402b0e67448ec6926713b0498bd954e86a116a
    -- and is not in alpha4
    --entrypoint "wmainCRTStartup"


  project "mudraw"
    kind "ConsoleApp"
    language "C"
    disablewarnings { "4267" }
    includedirs { "ext/zlib", "ext/lzma/C", "ext/unarr", "mupdf/include" }
    mudraw_files()
    links { "mupdf" }
    links { "windowscodecs" }
    linkoptions { "/ENTRY:\"wmainCRTStartup\"" }
    -- TODO: a better fix should be entrypoint, which was added in
    -- https://github.com/premake/premake-core/commit/b9402b0e67448ec6926713b0498bd954e86a116a
    -- and is not in alpha4
    --entrypoint "wmainCRTStartup"


  project "cmapdump"
    kind "ConsoleApp"
    language "C"
    disablewarnings { "4267" }
    includedirs { "mupdf/include" }
    files { "mupdf/scripts/cmapdump.c" }


  project "enginedump"
    kind "ConsoleApp"
    language "C++"
    includedirs { "src", "src/utils", "src/mui", "mupdf/include" }
    files {
      "src/EngineDump.cpp",
      "src/mui/MiniMui.*",
      "src/mui/TextRender.*",
    }
    links { "engines", "utils", "mupdf", "unarr", "libwebp", "libdjvu" }
    links {
      "comctl32", "gdiplus", "msimg32", "shlwapi",
      "version", "windowscodecs"
    }


  project "test_util"
    kind "ConsoleApp"
    language "C++"
    disablewarnings { "4838" }
    defines { "NO_LIBMUPDF" }
    includedirs { "src/utils" }
    files_in_dir( "src/utils", {
      "BaseUtil*",
      "BitManip*",
      "ByteOrderDecoder*",
      "CmdLineParser*",
      "CryptoUtil*",
      "CssParser*",
      "Dict*",
      "DebugLog*",
      "FileUtil*",
      "GeomUtil.*",
      "HtmlParserLookup*",
      "HtmlPrettyPrint*",
      "HtmlPullParser*",
      "JsonParser*",
      "Scoped.*",
      "SettingsUtil*",
      "SimpleLog*",
      "StrFormat*",
      "StrUtil*",
      "SquareTreeParser*",
      "TrivialHtmlParser*",
      "UtAssert*",
      "VarintGob*",
      "Vec.*",
      "WinUtil*",
      "tests/*"
    })
    files_in_dir("src", {
      --"AppTools.*",
      --"ParseCommandLine.*",
      --"StressTesting.*",
      "AppUtil.*",
      "UnitTests.cpp",
      "mui/SvgPath*",
      "tools/test_util.cpp"
    })
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
    includedirs { "src/utils"}
    files { "src/tools/plugin-test.cpp" }
    links { "utils", "mupdf" }
    links { "shlwapi" }


  project "MakeLZSA"
    kind "ConsoleApp"
    language "C++"
    files { "src/tools/MakeLzSA.cpp" }
    includedirs { "src/utils", "ext/zlib", "ext/lzma/C", "ext/unarr" }
    links { "unarr", "utils", "zlib" }
    links { "shlwapi" }


    --[[
    TODO: implement this logic
    !if "$(CFG)"=="dbg"
    # build all optional IFilters for debug builds
    BUILD_TEX_IFILTER = 1
    BUILD_EPUB_IFILTER = 1
    !endif

    !if "$(BUILD_TEX_IFILTER)"!=""
    PDFFILTER_OBJS = $(PDFFILTER_OBJS) $(ODLL)\CTeXFilter.obj
    PDFFILTER_CFLAGS = $(PDFFILTER_CFLAGS) /D "BUILD_TEX_IFILTER"
    !endif

    !if "$(BUILD_EPUB_IFILTER)"!=""
    PDFFILTER_OBJS = $(PDFFILTER_OBJS) $(ODLL)\CEpubFilter.obj \
    	$(OS)\EbookDoc.obj $(OS)\MobiDoc.obj $(OU)\PalmDbReader.obj
    PDFFILTER_CFLAGS = $(PDFFILTER_CFLAGS) /D "BUILD_EPUB_IFILTER"
    !endif
  --]]
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
    links { "utils", "libmupdf" }
    links { "comctl32", "shlwapi", "version"  }


    --[[
    TODO: implement this logic
    !if "$(CFG)"=="dbg"
    # build all optional previews for debug builds
    BUILD_XPS_PREVIEW = 1
    BUILD_DJVU_PREVIEW = 1
    BUILD_EPUB_PREVIEW = 1
    BUILD_FB2_PREVIEW = 1
    BUILD_MOBI_PREVIEW = 1
    BUILD_CBZ_PREVIEW = 1
    BUILD_CBR_PREVIEW = 1
    BUILD_CB7_PREVIEW = 1
    BUILD_CBT_PREVIEW = 1
    BUILD_TGA_PREVIEW = 1
    !endif

    !if "$(BUILD_XPS_PREVIEW)"!=""
    PDFPREVIEW_CFLAGS = $(PDFPREVIEW_CFLAGS) /D "BUILD_XPS_PREVIEW"
    !endif

    !if "$(BUILD_DJVU_PREVIEW)"!=""
    PDFPREVIEW_CFLAGS = $(PDFPREVIEW_CFLAGS) /D "BUILD_DJVU_PREVIEW"
    PDFPREVIEW_OBJS = $(PDFPREVIEW_OBJS) $(OS)\DjVuEngine.obj
    !endif

    !if "$(BUILD_EPUB_PREVIEW)$(BUILD_FB2_PREVIEW)$(BUILD_MOBI_PREVIEW)"!=""
    PDFPREVIEW_OBJS = $(PDFPREVIEW_OBJS) $(OS)\EbookEngine.obj \
    	$(EBOOK_OBJS) $(OS)\ChmDoc.obj $(CHMLIB_OBJS) \
    	$(OMUI)\MiniMui.obj $(OMUI)\TextRender.obj
    !if "$(BUILD_EPUB_PREVIEW)"!=""
    PDFPREVIEW_CFLAGS = $(PDFPREVIEW_CFLAGS) /D "BUILD_EPUB_PREVIEW"
    !endif
    !if "$(BUILD_FB2_PREVIEW)"!=""
    PDFPREVIEW_CFLAGS = $(PDFPREVIEW_CFLAGS) /D "BUILD_FB2_PREVIEW"
    !endif
    !if "$(BUILD_MOBI_PREVIEW)"!=""
    PDFPREVIEW_CFLAGS = $(PDFPREVIEW_CFLAGS) /D "BUILD_MOBI_PREVIEW"
    !endif
    !endif

    !if "$(BUILD_CBZ_PREVIEW)$(BUILD_CBR_PREVIEW)$(BUILD_CB7_PREVIEW)$(BUILD_CBT_PREVIEW)$(BUILD_TGA_PREVIEW)"!=""
    PDFPREVIEW_OBJS = $(PDFPREVIEW_OBJS) $(OS)\ImagesEngine.obj $(OS)\PdfCreator.obj
    !if "$(BUILD_CBZ_PREVIEW)"!=""
    PDFPREVIEW_CFLAGS = $(PDFPREVIEW_CFLAGS) /D "BUILD_CBZ_PREVIEW"
    !endif
    !if "$(BUILD_CBR_PREVIEW)"!=""
    PDFPREVIEW_CFLAGS = $(PDFPREVIEW_CFLAGS) /D "BUILD_CBR_PREVIEW"
    !endif
    !if "$(BUILD_CB7_PREVIEW)"!=""
    PDFPREVIEW_CFLAGS = $(PDFPREVIEW_CFLAGS) /D "BUILD_CB7_PREVIEW"
    !endif
    !if "$(BUILD_CBT_PREVIEW)"!=""
    PDFPREVIEW_CFLAGS = $(PDFPREVIEW_CFLAGS) /D "BUILD_CBT_PREVIEW"
    !endif
    !if "$(BUILD_TGA_PREVIEW)"!=""
    PDFPREVIEW_CFLAGS = $(PDFPREVIEW_CFLAGS) /D "BUILD_TGA_PREVIEW"
    !endif
    !endif
  --]]
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
    links { "utils", "libmupdf" }
    links { "comctl32", "gdiplus", "shlwapi", "version" }


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
      "synctex", "sumatra", "utils", "mui", "engines", "mupdf",
      "libdjvu", "unarr", "libwebp"
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
      "unarr", "libwebp"
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
    files {
      "src/CrashHandler.*",
      "src/Translations.*",
      "src/installer/Installer.cpp",
      "src/installer/Installer.h",
      "src/installer/Trans_installer_txt.cpp",

      "src/installer/Installer.rc",
    }
    includedirs {
      "src", "src/utils", "ext/zlib", "ext/unarr", "ext/lzma/C"
    }
    links { "utils", "zlib", "unarr" }
    links {
      "comctl32", "gdiplus", "msimg32", "shlwapi", "urlmon",
       "version", "windowscodecs", "wininet"
    }


  --[[
  TODO:
  INSTALLER_DATA  = $(O)\InstallerData.dat

  $(INSTALLER_DATA): $(MAKELZSA_APP) $(SUMATRA_APP_NO_MUPDF) $(LIBMUPDF_DLL)
  $(CJK_FALLBACK_FONT) $(PDFFILTER_DLL) $(PDFPREVIEW_DLL) $(UNINSTALLER_APP)

  $(MAKELZSA_APP) $@ $(SUMATRA_APP_NO_MUPDF):SumatraPDF.exe $(LIBMUPDF_DLL):libmupdf.dll
   $(CJK_FALLBACK_FONT):DroidSansFallback.ttf $(PDFFILTER_DLL):PdfFilter.dll
   $(PDFPREVIEW_DLL):PdfPreview.dll $(UNINSTALLER_APP):uninstall.exe

  $(INSTALLER_RES): $(SRCDIR)\installer\Installer.rc $(SRCDIR)\installer\Resource.h $(SRCDIR)\Version.h $(INSTALLER_DATA)
  	rc /r /fo$@ $(RC_FLAGS) /D "INSTALL_PAYLOAD_ZIP=..\..\$(INSTALLER_DATA)" $(SRCDIR)\installer\Installer.rc
  --]]

  -- TODO: fails at linking. Can't find symbols that should be in "utils" etc.
  -- and I don't see utils.lib on link.exe list.
  -- Looks like it's triggered by adding dependson
  -- Possible solution:
  -- * use linkoptions to force linking utils/zlib/unarr
  -- * specify source files instead of linking projects
  -- * move building of installer data to a separate project which will have
  --   those dependencies instead
  -- Full description of the problem: https://github.com/premake/premake-core/issues/208
  project "Installer"
    kind "WindowedApp"
    language "C++"
    flags { "NoManifest", "WinMain" }
    disablewarnings { "4018", "4244", "4264", "4838", "4702", "4706" }
    files {
      "src/CrashHandler.*",
      "src/Translations.*",
      "src/installer/Installer.cpp",
      "src/installer/Installer.h",
      "src/installer/Trans_installer_txt.cpp",

      "src/installer/Installer.rc",
    }
    includedirs {
      "src", "src/utils", "ext/zlib", "ext/unarr", "ext/lzma/C"
    }
    links { "utils", "zlib", "unarr" }
    links {
      "comctl32", "gdiplus", "msimg32", "shlwapi", "urlmon",
      "version", "windowscodecs", "wininet"
    }
    --dependson { "MakeLZSA", "SumatraPDF-no-MUPDF", "PdfFilter", "PdfPreview", "Uninstaller" }


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
      "mutool", "mudraw", "Uninstaller", "enginedump", "efi"
    }
