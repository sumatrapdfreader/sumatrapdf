--[[
This is for generating unix makefile.
I'm using premake5 alpha12 from http://premake.github.io/download.html#v5
(premake4 won't work, it doesn't support VS 2013+)
--]]

include("premake5.files.lua")

workspace "SumatraPDF"
  toolset "gcc"
  configurations { "Debug", "Release" }
  platforms { "x64" }
  startproject "test_unix"

  filter "platforms:x64"
     architecture "x86_64"
  filter {}

  warnings "Extra"

  location "."

  filter {"platforms:x64", "configurations:Release"}
    targetdir "out/rel64_unix"
  filter {"platforms:x64", "configurations:Debug"}
    targetdir "out/dbg64_unix"
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
  -- but not present in mac os x clang
  -- https://stackoverflow.com/questions/42074035/how-to-deal-with-clangs-3-9-wexpansion-to-defined-warning
  -- it's used in https://abseil.io/ headers so should be safe
  disablewarnings { "implicit-fallthrough" }
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

  project "unarrlib"
    kind "StaticLib"
    language "C"
    -- TODO: for bzip2, need BZ_NO_STDIO and BZ_DEBUG=0
    -- TODO: for lzma, need _7ZIP_PPMD_SUPPPORT
    defines { "HAVE_ZLIB", "HAVE_BZIP2", "BZ_NO_STDIO" }
    -- TODO: most of these warnings are due to bzip2 and lzma

    filter "toolset:gcc"
      disablewarnings { "unused-parameter", "unused-but-set-variable", "int-conversion", "implicit-function-declaration", "type-limits", "sign-compare" }
    filter {}

    includedirs { "ext/zlib", "ext/bzip2" }
    unarr_no_lzma_files()

  project "zlib"
    kind "StaticLib"
    language "C"
    filter "toolset:gcc"
      disablewarnings { "implicit-function-declaration", "shift-negative-value" }
    filter {}
    zlib_files()

  project "unarr"
    kind "ConsoleApp"
    language "C"
    disablewarnings { "4100" }
    files { "ext/unarr/main.c" }
    links { "unarrlib", "zlib" }

  project "test_unix"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"

    includedirs { "src", "src/utils", "ext/unarr" }

    links { "unarrlib", "zlib" }

    files {
      "src/utils/Archive.cpp",
      "src/utils/BaseUtil.cpp",
      "src/utils/FileUtil.cpp",
      "src/utils/StrUtil.cpp",
      "src/utils/UtAssert.cpp",
      "tools/test_unix/main.cpp",
    }
