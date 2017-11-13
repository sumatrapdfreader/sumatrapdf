--[[
This is for generating unix makefile.
I'm using premake5 alpha12 from http://premake.github.io/download.html#v5
(premake4 won't work, it doesn't support VS 2013+)
--]]

--include("premake5.files.lua")

workspace "SumatraPDF"
  configurations { "Debug", "Release" }
  platforms { "x64" }
  startproject "test_unix"

  filter "platforms:x64"
     architecture "x86_64"
  filter {}

  warnings "Extra"

  location "."

  filter {"platforms:x64", "configurations:Release"}
    targetdir "rel64_unix"
  filter {"platforms:x64", "configurations:Debug"}
    targetdir "dbg64_unix"
  filter {}
  objdir "%{cfg.targetdir}/obj"

  symbols "On"

  -- https://github.com/premake/premake-core/wiki/flags
  flags {
    "MultiProcessorCompile",
    "StaticRuntime",
    -- "Unicode", TODO: breaks libdjuv?
  }

  -- expansion-to-defined reports as error commonly used pattern
  -- https://stackoverflow.com/questions/42074035/how-to-deal-with-clangs-3-9-wexpansion-to-defined-warning
  -- it's used in https://abseil.io/ headers so should be safe
  disablewarnings { "expansion-to-defined"}
  flags { "FatalWarnings" }

  exceptionhandling "Off"
  rtti "Off"

  defines { }

  filter "configurations:Debug"
    defines { "DEBUG" }

  filter "configurations:Release*"
    defines { "NDEBUG" }
    optimize "On"
  filter {}

  project "test_unix"
    kind "ConsoleApp"
    language "C++"
    includedirs { "src", "src/utils", "ext/zlib", "ext/unarr", "ext/lzma/C", "ext/bzip2" }
    files {
      "tools/test_unix/main.cpp",
    }
