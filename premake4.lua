-- to generate Visual Studio files in vs directory, run:
-- premake4 vs2010 or premake4 vs2008
solution "everything"
  configurations { "Debug", "Release" }
  location "vs" -- this is where generated solution/project files go

  flags { "Symbols", "StaticRuntime" }

  -- those are inherited by projects that follow
  configuration "Debug"
    targetdir "dbg" -- this is where the .exe/.lib etc. files wil end up
    defines { "_DEBUG", "DEBUG" }

  configuration "Release"
     targetdir "rel"
     flags { "Optimize" }
     defines { "NDEBUG" }

  configuration {"vs*"}
    -- defines { "_WIN32", "WIN32", "WINDOWS", "_CRT_SECURE_NO_WARNINGS" }
    defines { "_WIN32", "WIN32", "WINDOWS" }
    -- 4800 - int -> bool coversion
    buildoptions { "/wd4800"}

  project "sizer"
    kind "ConsoleApp"
    language "C++"
    files {
      "tools/sizer/*.h",
      "tools/sizer/*.cpp",
      "src/utils/BaseUtil*",
      "src/utils/BitManip.h",
      "src/utils/Dict*",
      "src/utils/StrUtil*",
      "src/utils/"
    }
    excludes
    {
      "src/utils/*_ut.cpp",
    }
    includedirs { "src/utils" }
    links { }

    configuration {"vs*"}
      buildoptions {"/wd4996", "/wd4244", "/wd4305" }
      -- Note: don't understand why this is needed
      linkoptions {"/NODEFAULTLIB:\"msvcrt.lib\""}

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
