-- those are versions of projects where Debug configurations are compiled
-- with full otpimizations. This is done for mupdf etc., which we assume
-- are stable libraries so we don't need debug support for them
-- this only applies to libmupdf.dll build because in static build I can't
-- (easily, with premake) mix optimized and non-optimized compilation

    project "libwebp-opt"
        kind "StaticLib"
        language "C"
        optimize "On"
        undefines { "DEBUG" }
        defines { "NDEBUG" }

        disablewarnings { "4204", "4244", "4057", "4245", "4310" }
        includedirs { "ext/libwebp" }
        libwebp_files()


    project "libdjvu-opt"
        kind "StaticLib"
        characterset ("MBCS")
        language "C++"
        optimize "On"
        undefines { "DEBUG" }
        defines { "NDEBUG" }

        defines { "NEED_JPEG_DECODER", "WINTHREADS=1", "DDJVUAPI=/**/", "MINILISPAPI=/**/", "DEBUGLVL=0" }
        filter {"platforms:x32_asan"}
          defines { "DISABLE_MMX" }
        filter{}
        disablewarnings { "4100", "4189", "4244", "4267", "4302", "4311", "4312", "4505" }
        disablewarnings { "4456", "4457", "4459", "4530", "4611", "4701", "4702", "4703", "4706" }
        includedirs { "ext/libjpeg-turbo" }
        libdjvu_files()


    project "unarrlib-opt"
        kind "StaticLib"
        language "C"
        optimize "On"
        undefines { "DEBUG" }
        defines { "NDEBUG" }

        -- TODO: for bzip2, need BZ_NO_STDIO and BZ_DEBUG=0
        -- TODO: for lzma, need _7ZIP_PPMD_SUPPPORT
        defines { "HAVE_ZLIB", "HAVE_BZIP2", "HAVE_7Z", "BZ_NO_STDIO", "_7ZIP_PPMD_SUPPPORT" }
        -- TODO: most of these warnings are due to bzip2 and lzma
        disablewarnings { "4100", "4244", "4267", "4456", "4457", "4996" }
        includedirs { "ext/zlib", "ext/bzip2", "ext/lzma/C" }
        unarr_files()

    project "jbig2dec-opt"
        kind "StaticLib"
        language "C"
        optimize "On"
        undefines { "DEBUG" }
        defines { "NDEBUG" }

        defines { "HAVE_STRING_H=1", "JBIG_NO_MEMENTO" }
        disablewarnings { "4018", "4100", "4146", "4244", "4267", "4456", "4701" }
        includedirs { "ext/jbig2dec" }
        jbig2dec_files()


    project "openjpeg-opt"
        kind "StaticLib"
        language "C"
        optimize "On"
        undefines { "DEBUG" }
        defines { "NDEBUG" }

        disablewarnings { "4100", "4244", "4310", "4819" }
        -- openjpeg has opj_config_private.h for such over-rides
        -- but we can't change it because we bring openjpeg as submodule
        -- and we can't provide our own in a different directory because
        -- msvc will include the one in ext/openjpeg/src/lib/openjp2 first
        -- because #include "opj_config_private.h" searches current directory first
        defines { "USE_JPIP", "OPJ_STATIC", "OPJ_EXPORTS" }
        openjpeg_files()

    project "lcms2-opt"
        kind "StaticLib"
        language "C"
        optimize "On"
        undefines { "DEBUG" }
        defines { "NDEBUG" }

        includedirs { "ext/lcms2/include" }
        lcms2_files()

    project "harfbuzz-opt"
        kind "StaticLib"
        language "C"
        optimize "On"
        undefines { "DEBUG" }
        defines { "NDEBUG" }

        includedirs { "ext/harfbuzz/src/hb-ucdn", "ext/freetype-config", "ext/freetype/include" }
        defines {
        "HAVE_FALLBACK=1",
        "HAVE_OT",
        "HAVE_UCDN",
        "HB_NO_MT",
        "hb_malloc_impl=fz_hb_malloc",
        "hb_calloc_impl=fz_hb_calloc",
        "hb_realloc_impl=fz_hb_realloc",
        "hb_free_impl=fz_hb_free"
        }
        disablewarnings { "4100", "4146", "4244", "4245", "4267", "4456", "4457", "4459", "4701", "4702", "4706" }
        harfbuzz_files()

    project "mujs-opt"
        kind "StaticLib"
        language "C"
        optimize "On"
        undefines { "DEBUG" }
        defines { "NDEBUG" }

        includedirs { "ext/mujs" }
        disablewarnings { "4090", "4100", "4702", "4706" }
        files { "ext/mujs/one.c", "ext/mujs/mujs.h" }

    project "freetype-opt"
        kind "StaticLib"
        language "C"
        optimize "On"
        undefines { "DEBUG" }
        defines { "NDEBUG" }

        defines {
            "FT2_BUILD_LIBRARY",
            "FT_CONFIG_MODULES_H=\"slimftmodules.h\"",
            "FT_CONFIG_OPTIONS_H=\"slimftoptions.h\"",
        }
        disablewarnings { "4018", "4244", "4267", "4312", "4996" }
        includedirs { "ext/freetype/include", "ext/freetype-config" }
        freetype_files()

    project "libjpeg-turbo-opt"
    kind "StaticLib"
    language "C"
    optimize "On"
    undefines { "DEBUG" }
    defines { "NDEBUG" }

    disablewarnings { "4018", "4100", "4244", "4245" }
    includedirs { "ext/libjpeg-turbo", "ext/libjpeg-turbo/simd" }

    -- nasm.exe -I .\ext\libjpeg-turbo\simd\
    -- -I .\ext\libjpeg-turbo\win\ -f win32
    -- -o .\obj-rel\jpegturbo\jsimdcpu.obj
    -- .\ext\libjpeg-turbo\simd\jsimdcpu.asm
    filter {'files:**.asm', 'platforms:x32 or x32_xp or x32_asan'}
        buildmessage '%{file.relpath}'
        buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
        buildcommands {
            '..\\bin\\nasm.exe -f win32 -I ../ext/libjpeg-turbo/simd/ -I ../ext/libjpeg-turbo/win/ -o "%{cfg.objdir}/%{file.basename}.obj" "%{file.relpath}"'
        }
    filter {}

    filter {'files:**.asm', 'platforms:x64 or x64_ramicro'}
        buildmessage '%{file.relpath}'
        buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
        buildcommands {
        '..\\bin\\nasm.exe -f win64 -D__x86_64__ -DWIN64 -DMSVC -I ../ext/libjpeg-turbo/simd/ -I ../ext/libjpeg-turbo/win/ -o "%{cfg.objdir}/%{file.basename}.obj" "%{file.relpath}"'
        }
    filter {}
    libjpeg_turbo_files()

    project "zlib-opt"
    kind "StaticLib"
    language "C"
    optimize "On"
    undefines { "DEBUG" }
    defines { "NDEBUG" }

    disablewarnings { "4131", "4244", "4245", "4267", "4996" }
    zlib_files()

    project "mupdf-opt"
        kind "StaticLib"
        language "C"
        optimize "On"
        undefines { "DEBUG" }
        defines { "NDEBUG" }

        -- for openjpeg, OPJ_STATIC is alrady defined in load-jpx.c
        -- so we can't double-define it
        defines { "USE_JPIP", "OPJ_EXPORTS", "HAVE_LCMS2MT=1" }
        defines { "OPJ_STATIC", "SHARE_JPEG" }
        -- this defines which fonts are to be excluded from being included directly
        -- we exclude the very big cjk fonts
        defines { "TOFU", "TOFU_CJK_LANG" }
        disablewarnings { 
        "4005", "4028", "4100", "4115", "4130", "4204", "4206", "4244",
        "4245", "4267", "4295", "4389", "4456", "4457", "4459", "4702",
        "4703", "4706",
        }
        -- force including mupdf/scripts/openjpeg/opj_config_private.h
        -- with our build over-rides

        includedirs {
        "ext/freetype-config",  -- TODO: mupdf/scripts/freetype
        "mupdf/include",
        "mupdf/generated",
        "ext/jbig2dec", 
        "ext/libjpeg-turbo", 
        "ext/openjpeg/src/lib/openjp2",
        "ext/zlib",
        "ext/freetype/include",
        "ext/mujs",
        "ext/harfbuzz/src",
        "ext/lcms2/include",
        }
        -- .\ext\..\bin\nasm.exe -I .\mupdf\ -f win32 -o .\obj-rel\mupdf\font_base14.obj
        -- .\mupdf\font_base14.asm
        filter {'files:**.asm', 'platforms:x32 or x32_xp or x32_asan'}
        buildmessage 'Compiling %{file.relpath}'
        buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
        buildcommands {
            '..\\bin\\nasm.exe -f win32 -I ../mupdf/ -o "%{cfg.objdir}/%{file.basename}.obj" "%{file.relpath}"'
        }
        filter {}

        filter {'files:**.asm', 'platforms:x64 or x64_ramicro'}
        buildmessage 'Compiling %{file.relpath}'
        buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
        buildcommands {
            '..\\bin\\nasm.exe -f win64 -DWIN64 -I ../mupdf/ -o "%{cfg.objdir}/%{file.basename}.obj" "%{file.relpath}"'
        }
        filter {}
        mupdf_files()
        links { "zlib-opt", "libjpeg-turbo-opt", "freetype-opt", "jbig2dec-opt", "openjpeg-opt", "lcms2-opt", "harfbuzz-opt", "mujs-opt" }
