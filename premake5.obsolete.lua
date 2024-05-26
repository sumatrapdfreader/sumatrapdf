function zlib_ng_defines()
  defines {
    "_CRT_SECURE_NO_DEPRECATE",
    "_CRT_NONSTDC_NO_DEPRECATE",
    "X86_FEATURES",
    "X86_PCLMULQDQ_CRC",
    "X86_SSE2",
    "X86_SSE42_CRC_INTRIN",
    "X86_SSE42_CRC_HASH",
    "X86_AVX2",
    "X86_AVX_CHUNKSET",
    "X86_SSE2_CHUNKSET",
    "UNALIGNED_OK",
    "UNALIGNED64_OK",
    "WITH_GZFILEOP",
    "ZLIB_COMPAT"
  }
  includedirs {
    "ext/zlib-ng",
  }
end

project "zlib-ng"
  kind "StaticLib"
  language "C"
  optconf()
  zlib_ng_defines()
  disablewarnings { "4244", "4267" }
  zlib_ng_files()

project "libjpeg-turbo"
  kind "StaticLib"
  language "C"
  optconf()
  defines { "_CRT_SECURE_NO_WARNINGS" }
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

  filter {'files:**.asm', 'platforms:x64 or x64_asan'}
    buildmessage '%{file.relpath}'
    buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
    buildcommands {
      '..\\bin\\nasm.exe -f win64 -D__x86_64__ -DWIN64 -DMSVC -I ../ext/libjpeg-turbo/simd/ -I ../ext/libjpeg-turbo/win/ -o "%{cfg.objdir}/%{file.basename}.obj" "%{file.relpath}"'
    }
  filter {}
  libjpeg_turbo_files()

project "jbig2dec"
  kind "StaticLib"
  language "C"
  optconf()
  defines { "_CRT_SECURE_NO_WARNINGS", "HAVE_STRING_H=1", "JBIG_NO_MEMENTO" }
  disablewarnings { "4018", "4100", "4146", "4244", "4267", "4456", "4701" }
  includedirs { "ext/jbig2dec" }
  jbig2dec_files()

project "openjpeg"
  kind "StaticLib"
  language "C"
  optconf()
  disablewarnings { "4100", "4244", "4310", "4389", "4456" }
  -- openjpeg has opj_config_private.h for such over-rides
  -- but we can't change it because we bring openjpeg as submodule
  -- and we can't provide our own in a different directory because
  -- msvc will include the one in ext/openjpeg/src/lib/openjp2 first
  -- because #include "opj_config_private.h" searches current directory first
  defines { "_CRT_SECURE_NO_WARNINGS", "USE_JPIP", "OPJ_STATIC", "OPJ_EXPORTS" }
  openjpeg_files()

  project "freetype"
  kind "StaticLib"
  language "C"
  optconf()
  defines {
    "FT2_BUILD_LIBRARY",
    "FT_CONFIG_MODULES_H=\"slimftmodules.h\"",
    "FT_CONFIG_OPTIONS_H=\"slimftoptions.h\"",
  }
  disablewarnings { "4018", "4100", "4244", "4267", "4312", "4701", "4706", "4996" }
  includedirs { "mupdf/scripts/freetype", "ext/freetype/include" }
  freetype_files()

project "lcms2"
  kind "StaticLib"
  language "C"
  optconf()
  disablewarnings { "4100" }
  includedirs { "ext/lcms2/include" }
  lcms2_files()

project "harfbuzz"
  kind "StaticLib"
  language "C"
  optconf()
  includedirs { "ext/harfbuzz/src/hb-ucdn", "mupdf/scripts/freetype", "ext/freetype/include" }
  defines {
    "_CRT_SECURE_NO_WARNINGS",
    "HAVE_FALLBACK=1",
    "HAVE_OT",
    "HAVE_UCDN",
    "HAVE_FREETYPE",
    "HB_NO_MT",
    "hb_malloc_impl=fz_hb_malloc",
    "hb_calloc_impl=fz_hb_calloc",
    "hb_realloc_impl=fz_hb_realloc",
    "hb_free_impl=fz_hb_free"
  }
  disablewarnings { "4100", "4146", "4244", "4245", "4267", "4456", "4457", "4459", "4701", "4702", "4706" }
  harfbuzz_files()

project "mujs"
  kind "StaticLib"
  language "C"
  optconf()
  includedirs { "ext/mujs" }
  disablewarnings { "4090", "4100", "4310", "4702", "4706" }
  files { "ext/mujs/one.c", "ext/mujs/mujs.h" }

project "gumbo"
  kind "StaticLib"
  language "C"
  optconf()
  disablewarnings { "4018", "4100", "4132", "4204", "4244", "4245", "4267",
  "4305", "4306", "4389", "4456", "4701" }
  includedirs { "ext/gumbo-parser/include", "ext/gumbo-parser/visualc/include" }
  gumbo_files()

project "efi"
  kind "ConsoleApp"
  language "C++"
  cppdialect "C++latest"
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


project "unarr"
  kind "ConsoleApp"
  language "C"
  disablewarnings { "4100" }
  files { "ext/unarr/main.c" }
  links { "unarrlib", "zlib" }
