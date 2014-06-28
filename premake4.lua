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
    defines { "_WIN32", "WIN32", "WINDOWS" }
    -- 4800 - int -> bool coversion
    -- 4100 - unreferenced formal parameter
    -- 4428 - universal-character-name encountered in source
    buildoptions {
        "/wd4800", "/wd4127", "/wd4100", "/wd4244", "/wd4428"
    }
end

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
      "src/utils/CryptoUtil*",
      "src/utils/CssParser*",
      "src/utils/Dict*",
      "src/utils/DebugLog*",
      "src/utils/FileUtil*",
      "src/utils/GeomUtil.*",
      "src/utils/HtmlParserLookup*",
      "src/utils/HtmlPrettyPrint*",
      "src/utils/HtmlPullParser*",
      "src/utils/JsonParser*",
      "src/utils/Scoped.*",
      "src/utils/SettingsUtil*",
      "src/utils/SimpleLog*",
      "src/utils/StrFormat*",
      "src/utils/StrUtil*",
      "src/utils/SquareTreeParser*",
      "src/utils/TrivialHtmlParser*",
      "src/utils/UtAssert*",
      "src/utils/VarintGob*",
      "src/utils/Vec.*",
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
    defines { "NO_LIBMUPDF" }
    includedirs { "src/utils", "src/utils/msvc" }
    links { "gdiplus", "comctl32", "shlwapi", "Version" }

solution "plugin-test"
  solution_common()

  project "plugin-test"
    kind "WindowedApp"
    language "C++"
    files {
      "src/utils/BaseUtil.*",
      "src/utils/CmdLineParser.*",
      "src/utils/FileUtil.*",
      "src/utils/GeomUtil.*",
      "src/utils/Scoped.*",
      "src/utils/StrUtil.*",
      "src/utils/Vec.*",
      "tools/plugin-test/plugin-test.cpp"
    }
    includedirs { "src/utils", "src/utils/msvc" }
    flags { "NoManifest", "WinMain" }
    links { "shlwapi" }

solution "signfile"
  solution_common()

  project "signfile"
    kind "ConsoleApp"
    language "C++"
    files {
      "src/utils/BaseUtil.*",
      "src/utils/CmdLineParser.*",
      "src/utils/CryptoUtil.*",
      "src/utils/FileUtil.*",
      "src/utils/Scoped.*",
      "src/utils/StrUtil.*",
      "src/utils/Vec.*",
      "tools/signfile/signfile.cpp"
    }
    defines { "NO_LIBMUPDF" }
    includedirs { "src/utils", "src/utils/msvc" }
    links { "shlwapi", "crypt32" }

solution "unarr"
  solution_common()

  project "unarr"
    kind "ConsoleApp"
    language "C"
    files {
      "tools/unarr/*.*",
      "tools/unarr/rar/*"
    }
    defines { "_CRT_SECURE_NO_WARNINGS" }
    includedirs { "src/utils/msvc" }
