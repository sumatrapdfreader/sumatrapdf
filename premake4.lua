-- to generate Visual Studio files in vs-premake directory, run:
-- premake4 vs2010 or premake4 vs2008

-- common settings for solutions
function solution_common()
  configurations { "Debug", "Release" }
  location "vs-premake" -- this is where generated solution/project files go

  -- Symbols - generate .pdb files
  -- StaticRuntime - statically link crt
  -- ExtraWarnings - set max compiler warnings level
  -- FatalWarnings - compiler warnigs are errors'
  -- NoMinimalRebuild - disable /Gm because it clashes with /MP
  flags {
    "Symbols", "StaticRuntime", "ExtraWarnings", "FatalWarnings",
    "NoRTTI", "Unicode", "NoExceptions", "NoMinimalRebuild"
  }

  configuration "Debug"
    targetdir "obj-dbg" -- this is where the .exe/.lib etc. files wil end up
    defines { "_DEBUG", "DEBUG" }

  configuration "Release"
    targetdir "obj-rel"
    flags { "Optimize" }
    defines { "NDEBUG" }
    -- 4189 - variable not used, happens with CrashIf() macros that are no-op
    --        in release builds
    buildoptions { "/wd4189" }

  configuration {"vs*"}
    -- defines { "_WIN32", "WIN32", "WINDOWS", "_CRT_SECURE_NO_WARNINGS" }
    defines { "_WIN32", "WIN32", "WINDOWS" }
    -- 4800 - int -> bool coversion
    -- 4127 - conditional expression is constant
    -- 4100 - unreferenced formal parameter
    -- 4244 - possible loss of data due to conversion
    -- 4428 - universal-character-name encountered in source
    -- /MP  - use multi-cores for compilation
    buildoptions {
        "/wd4800", "/wd4127", "/wd4100", "/wd4244", "/wd4428"
    }
end

solution "sertxt"
  solution_common()

  project "sertxt_test"
    kind "ConsoleApp"
    language "C++"
    files {
      "tools/sertxt_test/*.h",
      "tools/sertxt_test/*.cpp",
      "tools/sertxt_test/*.txt",
      "src/utils/BaseUtil*",
      "src/utils/FileUtil*",
      "src/utils/SerializeTxt*",
      "src/utils/StrSlice*",
      "src/utils/StrUtil*",
      "src/utils/TxtParser*",
      "src/utils/VarintGob*",
    }
    excludes
    {
      "src/utils/*_ut.cpp",
    }
    includedirs { "src/utils", "src/utils/msvc" }
    links { "Shlwapi" }

solution "efi"
  solution_common()

  project "efi"
    kind "ConsoleApp"
    language "C++"

    files {
      "tools/efi/*.h",
      "tools/efi/*.cpp",
      "src/utils/BaseUtil*",
      "src/utils/BitManip.h",
      "src/utils/Dict*",
      "src/utils/StrUtil*",
    }
    includedirs { "src/utils", "src/utils/msvc" }
    links { }

    --configuration {"vs*"}
      -- Note: don't understand why this is needed
      --linkoptions {"/NODEFAULTLIB:\"msvcrt.lib\""}

--[[
  project "utils"
    kind "StaticLib"
    language "C++"
    files { "src/utils/*.cpp", "src/utils/*.h" }
    excludes
    {
      "src/utils/*_ut.cpp",
      "src/utils/Zip*",
      "src/utils/HtmlWindow*",
      "src/utils/LzmaSimpleArchive*",
      "src/utils/Experiments*",
      "src/utils/UtilTests.cpp",
      "src/utils/Touch*",
    }
    includedirs { "src/utils", "src/utils/msvc" }
--]]

solution "all_tests"
  solution_common()

  project "test_util"
    kind "ConsoleApp"
    language "C++"
    files {
      "src/utils/BaseUtil*",
      "src/utils/BencUtil*",
      "src/utils/BitManip*",
      "src/utils/ByteOrderDecoder*",
      "src/utils/CmdLineParser*",
      "src/utils/CssParser*",
      "src/utils/Dict*",
      "src/utils/DebugLog*",
      "src/utils/FileUtil*",
      "src/utils/HtmlParserLookup*",
      "src/utils/HtmlPrettyPrint*",
      "src/utils/HtmlPullParser*",
      "src/utils/JsonParser*",
      "src/utils/SettingsUtil*",
      "src/utils/SimpleLog*",
      "src/utils/StrFormat*",
      "src/utils/StrUtil*",
      "src/utils/SquareTreeParser*",
      "src/utils/TrivialHtmlParser*",
      "src/utils/UtAssert*",
      "src/utils/VarintGob*",
      "src/utils/WinUtil*",
      "src/utils/tests/*",
      --"src/AppTools.*",
      --"src/ParseCommandLine.*",
      --"src/StressTesting.*",
      "src/AppUtil*",
      "src/UnitTests.cpp",
      "src/mui/SvgPath*",
      "tools/tests/UnitMain.cpp"
    }
    includedirs { "src/utils", "src/utils/msvc" }
    links { "gdiplus", "comctl32", "shlwapi", "Version" }


solution "muitest"
  solution_common()

  project "muitest"
    kind "WindowedApp"
    language "C++"

    flags { "NoManifest", "WinMain" }
    defines { "NO_LIBWEBP" } -- building without WebP support (for compilation speed)

    files {
      "tools/mui_test/*",
      "src/utils/BaseUtil*",
      "src/utils/BitManip.h",
      "src/utils/Dict*",
      "src/utils/DebugLog*",
      "src/utils/FileUtil*",
      "src/utils/GdiPlusUtil*",
      "src/utils/HtmlParserLookup*",
      "src/utils/SerializeTxt*",
      "src/utils/StrSlice*",
      "src/utils/StrUtil*",
      "src/utils/TgaReader*",
      "src/utils/TxtParser*",
      "src/utils/WebpReader*",
      "src/utils/WinUtil*",
      "src/mui/*.h",
      "src/mui/*.cpp",
    }
    excludes
    {
      "src/mui/*_ut.cpp",
      "src/mui/MiniMui*",
    }
    includedirs { "src", "src/utils", "src/utils/msvc", "src/mui"}
    links { "gdiplus", "comctl32", "shlwapi", "Version", "WindowsCodecs" }
