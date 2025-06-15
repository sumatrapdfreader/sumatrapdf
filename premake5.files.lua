function files_in_dir(dir, files_in_dir)
  local paths = {}
  for _, file in ipairs(files_in_dir) do
    -- TODO: don't add "/" if dir ends with it of file starts with it
    local path = dir .. "/" .. file
    table.insert(paths, path)
  end
  files(paths)
end

function preview_test_files()
  files_in_dir("src/utils", {
    "BaseUtil.*",
    "TempAllocator.*",
    "StrFormat.*",
    "StrUtil.*",
    "StrVec.*",
    "StrconvUtil.*",
  })
  files {
    "src/tools/preview_test.cpp",
    "src/CrashHandlerNoOp.cpp",
  }
end

function makelzsa_files()
  files_in_dir("src/utils", {
    "BaseUtil.*",
    "ByteOrderDecoder.*",
    "ByteWriter.*",
    "ColorUtil.*",
    "CmdLineArgsIter.*",
    "DirIter.*",
    "Dpi.*",
    "FileUtil.*",
    "GeomUtil.*",
    "LzmaSimpleArchive.*",
    "StrconvUtil.*",
    "StrFormat.*",
    "StrUtil.*",
    "StrVec.*",
    "StrQueue.*",
    "TempAllocator.*",
    "Log.*",
    "WinDynCalls.*",
    "WinUtil.*",
  })

  files {
    "src/tools/MakeLzSA.cpp",
  }
end

function zlib_files()
  files_in_dir("ext/zlib", {
    "adler32.c", "compress.c", "crc32.c", "deflate.c", "inffast.c",
    "inflate.c", "inftrees.c", "trees.c", "zutil.c", "gzlib.c",
    "gzread.c", "gzwrite.c", "gzclose.c",
  })
end

function zlib_ng_files()
  files_in_dir("ext/zlib-ng", {
    "adler32.c", 
    "chunkset.c",
    "compare258.c",
    "compress.c",
    "crc32.c",
    "crc32_comb.c",
    "deflate.c",
    "deflate_fast.c",
    "deflate_medium.c",
    "deflate_quick.c",
    "deflate_slow.c",
    "functable.c",
    "gzlib.c",
    "gzread.c",
    "gzwrite.c",
    "infback.c",
    "inffast.c",
    "inflate.c",
    "inftrees.c",
    "insert_string.c",
    "trees.c",
    "uncompr.c",
    "zutil.c",
  })

  files_in_dir("ext/zlib-ng/arch/x86", {
    "*.c",
  })
end

function unrar_files()
  files_in_dir("ext/unrar", {
    "archive.*",
    "arcread.*",
    "blake2s.*",
    "cmddata.*",
    "consio.*",
    "crc.*",
    "crypt.*",
    "dll.*",
    "encname.*",
    "errhnd.*",
    "extinfo.*",
    "extract.*",
    "filcreat.*",
    "file.*",
    "filefn.*",
    "filestr.*",
    "find.*",
    "getbits.*",
    "global.*",
    "hash.*",
    "headers.*",
    "isnt.*",
    "largepage.*",
    "list.*",
    "match.*",
    --"model.*",
    "motw.*",
    "options.*",
    "pathfn.*",
    "qopen.*",
    "rarvm.*",
    "rawread.*",
    "rdwrfn.*",
    "recvol.*",
    "rijndael.*",
    "rs.*",
    "rs16.*",
    "scantree.*",
    "secpassword.*",
    "sha1.*",
    "sha256.*",
    "smallfn.*",
    "strfn.*",
    "strlist.*",
    "system.*",
    "threadpool.*",
    "timefn.*",
    "ui.*",
    "unicode.*",
    "unpack.*",
    "volume.*",
  })
end

function libdjvu_files()
  files_in_dir("ext/libdjvu", {
    "Arrays.cpp", "atomic.cpp", "BSByteStream.cpp", "BSEncodeByteStream.cpp",
    "ByteStream.cpp", "DataPool.cpp", "DjVmDir0.cpp", "DjVmDoc.cpp", "DjVmNav.cpp",
    "DjVuAnno.cpp", "DjVuDocEditor.cpp", "DjVuDocument.cpp", "DjVuDumpHelper.cpp",
    "DjVuErrorList.cpp", "DjVuFile.cpp", "DjVuFileCache.cpp", "DjVuGlobal.cpp",
    "DjVuGlobalMemory.cpp", "DjVuImage.cpp", "DjVuInfo.cpp", "DjVuMessage.cpp",
    "DjVuMessageLite.cpp", "DjVuNavDir.cpp", "DjVuPalette.cpp", "DjVuPort.cpp",
    "DjVuText.cpp", "DjVuToPS.cpp", "GBitmap.cpp", "GContainer.cpp", "GException.cpp",
    "GIFFManager.cpp", "GMapAreas.cpp", "GOS.cpp", "GPixmap.cpp", "GRect.cpp",
    "GScaler.cpp", "GSmartPointer.cpp", "GString.cpp", "GThreads.cpp",
    "GUnicode.cpp", "GURL.cpp", "IFFByteStream.cpp", "IW44EncodeCodec.cpp",
    "IW44Image.cpp", "JB2EncodeCodec.cpp", "DjVmDir.cpp", "JB2Image.cpp",
    "JPEGDecoder.cpp", "MMRDecoder.cpp", "MMX.cpp", "UnicodeByteStream.cpp",
    "XMLParser.cpp", "XMLTags.cpp", "ZPCodec.cpp", "ddjvuapi.cpp", "debug.cpp",
    "miniexp.cpp",
  })
end

function unarrr_lzmasdk_files()
  files_in_dir("ext/unarr/lzmasdk", {
    "CpuArch.c", "Ppmd7.c", "Ppmd7Dec.c", "Ppmd8.c", "Ppmd8Dec.c",
  })
end

function unarr_lzma_files()
  files_in_dir("ext/lzma/C", {
    "LzmaDec.c", "Bra86.c", "LzmaEnc.c", "LzFind.c", "LzFindMt.c", "Threads.c",
    "7zBuf.c", "7zDec.c", "7zIn.c", "7zStream.c", "Bcj2.c", "Bra.c", "Lzma2Dec.c",
  })
end

function unarr_files()
  files {
    "ext/unarr/common/*",
    "ext/unarr/rar/*",
    "ext/unarr/zip/*",
    "ext/unarr/tar/*",
    "ext/unarr/_7z/*",

    "ext/bzip2/bzip_all.c",
  }
  unarrr_lzmasdk_files()
  unarr_lzma_files()
end

function unarr_no_lzma_files()
  files {
    "ext/unarr/common/*",
    "ext/unarr/rar/*",
    "ext/unarr/zip/*",
    "ext/unarr/tar/*",
    "ext/unarr/_7z/*",

    "ext/unarr/lzmasdk/LzmaDec.*",
    "ext/bzip2/bzip_all.c",
  }
  unarrr_lzmasdk_files()
end


function unarr_no_bzip_files()
  files {
    "ext/unarr/common/*",
    "ext/unarr/rar/*",
    "ext/unarr/zip/*",
    "ext/unarr/tar/*",
    "ext/unarr/_7z/*",
  }
  unarrr_lzmasdk_files()
  unarr_lzma_files()
end

function jbig2dec_files()
  -- TODO: probably can be
  -- files { "ext/jbig2dec/jbig2*.c", "ext/jbig2dec/jbig2*.h" }
  files_in_dir("ext/jbig2dec", {
    "jbig2.c",
    "jbig2_arith.c",
    "jbig2_arith_iaid.c",
    "jbig2_arith_int.c",
    "jbig2_generic.c",
    "jbig2_huffman.c",
    "jbig2_hufftab.c",
    "jbig2_halftone.c",
    "jbig2_image.c",
    "jbig2_mmr.c",
    "jbig2_page.c",
    "jbig2_refinement.c",
    "jbig2_segment.c",
    "jbig2_symbol_dict.c",
    "jbig2_text.c",
  })
end

function libheif_files() 
  files_in_dir("ext/libheif/libheif", {
    "bitstream.*",
    "box.*",
    "error.*",
    "heif.*",
    "heif_avif.*",
    "heif_colorconversion.*",
    "heif_context.*",
    "heif_decoder_dav1d.*",
    "heif_file.*",
    "heif_hevc.*",
    "heif_image.*",
    "heif_plugin.*",
    "heif_plugin_registry.*",
    "nclx.*",
  })

end

function dav1d_x68_files()
  files_in_dir("ext/dav1d/src/x86", {
    "cpu.c",
    "msac_init.c",
    "refmvs_init.c",
  })

  files_in_dir("ext/dav1d/src/x86", {
    "cpuid.asm",
    "msac.asm",
    "refmvs.asm",
    "cdef_avx2.asm",
    "itx_avx2.asm",
    "looprestoration_avx2.asm",
    "cdef_sse.asm",
    "itx_sse.asm",
    "cdef_avx512.asm",
    "filmgrain_avx512.asm",
    "ipred_avx512.asm",
    "itx_avx512.asm",
    "loopfilter_avx512.asm",
    "looprestoration_avx512.asm",
    "mc_avx512.asm",
    "filmgrain_avx2.asm",
    "ipred_avx2.asm",
    "loopfilter_avx2.asm",
    "mc_avx2.asm",
    "filmgrain_sse.asm",
    "ipred_sse.asm",
    "loopfilter_sse.asm",
    "looprestoration_sse.asm",
    "mc_sse.asm",
    "cdef16_avx512.asm",
    "filmgrain16_avx512.asm",
    "ipred16_avx512.asm",
    "looprestoration16_avx512.asm",
    "mc16_avx512.asm",
    "cdef16_avx2.asm",
    "filmgrain16_avx2.asm",
    "ipred16_avx2.asm",
    "itx16_avx2.asm",
    "loopfilter16_avx2.asm",
    "looprestoration16_avx2.asm",
    "mc16_avx2.asm",
    "cdef16_sse.asm",
    "filmgrain16_sse.asm",
    "ipred16_sse.asm",
    "itx16_sse.asm",
    "loopfilter16_sse.asm",
    "looprestoration16_sse.asm",
    "mc16_sse.asm",
  })
end

function dav1d_files()
  files_in_dir("ext/dav1d/src", {
    "lib.c",
    "thread_task.c",
    "cdf.c",
    "cpu.c",
    "data.c",
    "decode.c",
    "dequant_tables.c",
    "getbits.c",
    "intra_edge.c",
    "itx_1d.c",
    "lf_mask.c",
    "log.c",
    "mem.c",
    "msac.c",
    "obu.c",
    "picture.c",
    "qm.c",
    "ref.c",
    "refmvs.c",
    "scan.c",
    "tables.c",
    "warpmv.c",
    "wedge.c",
    "win32/thread.c",
  })

  files_in_dir("ext/dav1d/src", {
    "sumatra_bitdepth_8.c",
    "sumatra_bitdepth_8_2.c",
    "sumatra_bitdepth_16.c",
    "sumatra_bitdepth_16_2.c",
  })

  files("ext/dav1d/include/common/*.h")
  files("ext/dav1d/include/dav1d/*.h")
end

function openjpeg_files()
  files_in_dir( "ext/openjpeg/src/lib/openjp2", {
    "bio.c",
    "cidx_manager.c",
    "cio.c",
    "dwt.c",
    "event.c",
    "function_list.c",
    "ht_dec.c",
    "image.c",
    "invert.c",
    "j2k.c",
    "jp2.c",
    "mct.c",
    "mqc.c",
    "openjpeg.c",
    "opj_clock.c",
    --"opj_malloc.c",
    "phix_manager.c",
    "pi.c",
    "ppix_manager.c",
    "sparse_array.c",
    "t1.c",
    "t2.c",
    "tcd.c",
    "tgt.c",
    "thix_manager.c",
    "thread.c",
    "tpix_manager.c",
    "*.h",
  })
end

function extract_files()
  files_in_dir("ext/extract/src", {
    "alloc.*",
    "astring.*",
    "boxer.*",
    "buffer.*",
    "document.*",
    "docx.*",
    "docx_template.*",
    "extract.*",
    "html.*",
    "join.*",
    "json.*",
    "mem.*",
    "memento.*",
    "odt_template.*",
    "odt.*",
    "outf.*",
    "rect.*",
    "sys.*",
    "text.*",
    "xml.*",
    "zip.*",
  })
  files_in_dir("ext/extract/include", {
    "*.h",
  })
end

function libwebp_files()
  files("ext/libwebp/src/dec/*.c")

  files_in_dir("ext/libwebp/src/dsp", {
    "alpha_processing.c",
    "alpha_processing_sse2.c",
    "alpha_processing_sse41.c",
    "alpha_processing_neon.c",
    "cost.c",
    "cpu.c",
    "dec.c",
    "dec_clip_tables.c",
    "dec_sse2.c",
    "dec_sse41.c",
    "dec_neon.c",
    "filters.c",
    "filters_sse2.c",
    "filters_neon.c",
    "lossless.c",
    "lossless_sse2.c",
    "lossless_sse41.c",
    "lossless_neon.c",
    "rescaler.c",
    "rescaler_sse2.c",
    "rescaler_neon.c",
    "ssim.c",
    "ssim_sse2.c",
    "upsampling.c",
    "upsampling_sse2.c",
    "upsampling_sse41.c",
    "upsampling_neon.c",
    "yuv.c",
    "yuv_sse2.c",
    "yuv_sse41.c",
    "yuv_neon.c",
  })

  files("ext/libwebp/src/sharpyuv/*.c")

  files("ext/libwebp/src/utils/*.c")
end

function libjpeg_turbo_files()
  files_in_dir("ext/libjpeg-turbo", {
    "jcomapi.c", "jdapimin.c", "jdapistd.c", "jdatadst.c", "jdatasrc.c",
    "jdcoefct.c", "jdcolor.c", "jddctmgr.c", "jdhuff.c", "jdinput.c", "jdmainct.c",
    "jdmarker.c", "jdmaster.c", "jdmerge.c", "jdpostct.c", "jdsample.c", "jdtrans.c",
    "jerror.c", "jfdctflt.c", "jfdctint.c", "jidctflt.c", "jidctfst.c",
    "jidctint.c", "jquant1.c", "jquant2.c", "jutils.c", "jmemmgr.c", "jmemnobs.c",
    "jaricom.c", "jdarith.c", "jfdctfst.c", "jdphuff.c", "jidctred.c",
    "jcapimin.c", "jcapistd.c", "jcarith.c", "jccoefct.c", "jccolor.c",
    "jcdctmgr.c", "jchuff.c", "jcinit.c", "jcmainct.c", "jcmarker.c",
    "jcmaster.c", "jcparam.c", "jcprepct.c", "jcsample.c", "jcphuff.c"
  })

  filter {'platforms:arm64'}
    files {"ext/libjpeg-turbo/jsimd_none.c"}

  filter {'platforms:x32'}
    files_in_dir("ext/libjpeg-turbo/simd", {
      "jsimdcpu.asm", "jccolmmx.asm", "jcgrammx.asm", "jdcolmmx.asm",
    	"jcsammmx.asm", "jdsammmx.asm", "jdmermmx.asm", "jcqntmmx.asm",
    	"jfmmxfst.asm", "jfmmxint.asm", "jimmxred.asm", "jimmxint.asm",
    	"jimmxfst.asm", "jcqnt3dn.asm", "jf3dnflt.asm", "ji3dnflt.asm",
    	"jcqntsse.asm", "jfsseflt.asm", "jisseflt.asm", "jccolss2.asm",
    	"jcgrass2.asm", "jdcolss2.asm", "jcsamss2.asm", "jdsamss2.asm",
    	"jdmerss2.asm", "jcqnts2i.asm", "jfss2fst.asm", "jfss2int.asm",
    	"jiss2red.asm", "jiss2int.asm", "jiss2fst.asm", "jcqnts2f.asm",
    	"jiss2flt.asm",
    })
    files {"ext/libjpeg-turbo/simd/jsimd_i386.c"}

  filter {'platforms:x64 or x64_asan'}
    files_in_dir("ext/libjpeg-turbo/simd", {
      "jfsseflt-64.asm", "jccolss2-64.asm", "jdcolss2-64.asm", "jcgrass2-64.asm",
    	"jcsamss2-64.asm", "jdsamss2-64.asm", "jdmerss2-64.asm", "jcqnts2i-64.asm",
    	"jfss2fst-64.asm", "jfss2int-64.asm", "jiss2red-64.asm", "jiss2int-64.asm",
    	"jiss2fst-64.asm", "jcqnts2f-64.asm", "jiss2flt-64.asm",
    })
    files {"ext/libjpeg-turbo/simd/jsimd_x86_64.c"}

  filter {}
end

function lcms2_files()
  files_in_dir("ext/lcms2/src", {
    "*.c", "*.h"
  })
  files{ "ext/lcms/include.*.h" }
end

function harfbuzz_files()
  files_in_dir("ext/harfbuzz/src", {
    "hb-aat-layout.cc",
    "hb-aat-map.cc",
    "hb-blob.cc",
    "hb-buffer.cc",
    "hb-buffer-verify.cc",
    "hb-buffer-serialize.cc",
    "hb-common.cc",
    "hb-face.cc",
    "hb-fallback-shape.cc",
    "hb-font.cc",
    "hb-ft.cc",
    "hb-map.cc",
    "hb-number.cc",
    "hb-ot-cff1-table.cc",
    "hb-ot-cff2-table.cc",
    "hb-ot-color.cc",
    "hb-ot-face.cc",
    "hb-ot-font.cc",
    "hb-ot-layout.cc",
    "hb-ot-map.cc",
    "hb-ot-math.cc",
    "hb-ot-meta.cc",
    "hb-ot-metrics.cc",
    "hb-ot-name.cc",
    "hb-ot-shape.cc",
    "hb-ot-shaper-arabic.cc",
    "hb-ot-shaper-default.cc",
    "hb-ot-shaper-hangul.cc",
    "hb-ot-shaper-hebrew.cc",
    "hb-ot-shaper-indic-table.cc",
    "hb-ot-shaper-indic.cc",
    "hb-ot-shaper-khmer.cc",
    "hb-ot-shaper-myanmar.cc",
    "hb-ot-shaper-syllabic.cc",
    "hb-ot-shaper-thai.cc",
    "hb-ot-shaper-use.cc",
    "hb-ot-shaper-vowel-constraints.cc",
    "hb-ot-shape-fallback.cc",
    "hb-ot-shape-normalize.cc",
    "hb-ot-tag.cc",
    "hb-ot-var.cc",
    "hb-set.cc",
    "hb-shape.cc",
    "hb-shape-plan.cc",
    "hb-shaper.cc",
    "hb-static.cc",
    "hb-unicode.cc",
    "hb-subset-cff-common.cc",
    "hb-subset-cff1.cc",
    "hb-subset-cff2.cc",
    "hb-subset-input.cc",
    "hb-subset-plan.cc",
    "hb-subset.cc",
    "hb-ucd.cc",
  })
end

function freetype_files()
  files_in_dir("ext/freetype/src/base", {
    "ftbase.c",
    "ftbbox.c",
    "ftbitmap.c",
    "ftdebug.c",
    "ftfstype.c",
    "ftgasp.c",
    "ftglyph.c",
    "ftinit.c",
    "ftstroke.c",
    "ftsynth.c",
    "ftsystem.c",
    "fttype1.c",

    -- TODO: temporary
    "ftotval.c"
  })

  files_in_dir("ext/freetype/src", {
    "cff/cff.c",
    "psaux/psaux.c",
    "pshinter/pshinter.c",
    "psnames/psnames.c",
    "raster/raster.c",
    "sfnt/sfnt.c",
    "smooth/smooth.c",
    "truetype/truetype.c",
    "type1/type1.c",
    "cid/type1cid.c",
  })
end

files {
}

function sumatrapdf_files()
  files_in_dir(".", {
    ".gitignore",
    "*.yml",
    ".github/*.yml",
    ".github/workflows/*.yml",
    "do/*.go",
    "docs/*.txt",
    "docs/*.html",
    "docs/md/*.md",
    "docs/www/*.css",
    "premake5.lua",
    "premake5.obsolete.lua",
    "premake5.files.lua",
  })
  files_in_dir("src", {
    "Accelerators.*",
    "Actions.*",
    "AppColors.*",
    "AppSettings.*",
    "AppTools.*",
    "Caption.*",
    "Canvas.*",
    "CanvasAboutUI.*",
    "ChmModel.*",
    "Commands.*",
    "CommandPalette.*",
    "CrashHandler.*",
    "DisplayModel.*",
    "DisplayMode.*",
    "DocController.h",
    "DocProperties.*",
    "EditAnnotations.*",
    "EngineDump.cpp",
    "ExternalViewers.*",
    "Favorites.*",
    "FileHistory.*",
    "FileThumbnails.*",
    "Flags.*",
    "FzImgReader.*",
    "GlobalPrefs.*",
    "HomePage.*",
    "Installer.*",
    "InstallerCommon.cpp",
    "MainWindow.*",
    "Menu.*",
    "Notifications.*",
    "PdfSync.*",
    "Print.*",
    "ProgressUpdateUI.*",
    "RenderCache.*",
    "RegistryInstaller.*",
    "RegistryPreview.*",
    "RegistrySearchFilter.*",
    "resource.h",
    "SearchAndDDE.*",
    "Selection.*",
    "Settings.h",
    "SettingsStructs.*",
    "SimpleBrowserWindow.*",
    "SumatraPDF.cpp",
    "SumatraPDF.h",
    "SumatraPDF.rc",
    "SumatraStartup.cpp",
    "SumatraConfig.cpp",
    "SumatraDialogs.*",
    "SumatraProperties.*",
    "StressTesting.*",
    "SvgIcons.*",
    "TableOfContents.*",
    "Tabs.*",
    "Tester.*",
    "TextSearch.*",
    "TextSelection.*",
    "Theme.*",
    "Toolbar.*",
    "Translations.*",
    "TranslationLangs.cpp",
    "UpdateCheck.*",
    "Version.h",
    "VirtWnd.*",
    "Uninstaller.cpp",
    "WindowTab.*",

    "ext/versions.txt",
    "scratch.txt",
  })
  filter {"configurations:Debug or DebugFull"}
    files_in_dir("src", {
      "Tests.cpp",
      "regress/Regress.*",
      "Scratch.*",
    })
    files_in_dir("src/testcode", {
      "test-app.h",
      "TestApp.cpp",
      "TestTab.cpp",
      "TestLayout.cpp",
      --"TestLice.cpp",
    })
    files_in_dir("src/utils/tests", {
      "*.cpp",
    })
    files_in_dir("src/utils", {
        "UtAssert.*",
    })
  filter {}
end

function uia_files()
  files_in_dir("src/uia", {
    "Provider.*",
    "StartPageProvider.*",
    "DocumentProvider.*",
    "PageProvider.*",
    "TextRange.*",
  })
end

function darkmodelib_files()
  files_in_dir("ext/darkmodelib/src", {
    "DarkMode.*",
    "DarkModeSubclass.cpp",
    "IatHook.h",
    "StdAfx.h",
    "UAHMenuBar.h",
    "Version.h",
  })
  files_in_dir("ext/darkmodelib/include", {
    "DarkModeSubclass.h",
  })
end

function utils_files()
  files_in_dir("src/utils", {
    "AvifReader.*",
    "ApiHook.*",
    "Archive.*",
    "BaseUtil.*",
    "BitReader.*",
    "BuildConfig.h",
    "ByteOrderDecoder.*",
    "ByteReader.*",
    "ByteWriter.*",
    "CmdLineArgsIter.*",
    "ColorUtil.*",
    "CryptoUtil.*",
    "CssParser.*",
    "DbgHelpDyn.*",
    "Dict.*",
    "DirIter.*",
    "Dpi.*",
    "GeomUtil.*",
    "GuessFileType.*",
    "FileUtil.*",
    "FileWatcher.*",
    "FzImgReader.*",
    "GdiPlusUtil.*",
    "HtmlParserLookup.*",
    "HtmlPullParser.*",
    "HtmlPrettyPrint.*",
    "HttpUtil.*",
    "JsonParser.*",
    "Log.*",
    "LzmaSimpleArchive.*",
    "RegistryPaths.*",
    "Scoped.h",
    "ScopedWin.h",
    "SettingsUtil.*",
    "SquareTreeParser.*",
    "StrconvUtil.*",
    "StrFormat.*",
    "StrUtil.*",
    "StrVec.*",
    "StrQueue.*",
    "TempAllocator.*",
    "ThreadUtil.*",
    "TgaReader.*",
    "TrivialHtmlParser.*",
    "TxtParser.*",
    "UITask.*",
    "Vec.h",
    "VecSegmented.h",
    "WebpReader.*",
    "WinDynCalls.*",
    "WinUtil.*",
    "ZipUtil.*",
  })
  filter {"configurations:Debug or DebugFull"}
    files_in_dir("src/utils", {
      "windrawlib.*",
    })
  filter {}
end

function wingui_files()
  files_in_dir("src/wingui", {
    "*.h",
    "*.cpp",
  })
end

function mui_files()
  files_in_dir("src/mui", {
    "Mui.*",
    "TextRender.*",
  })
end

function engines_files()
  files_in_dir("src", {
    "Annotation.*",
    "ChmFile.*",
    "DocProperties.*",
    "EngineBase.*",
    "EngineCreate.*",
    "EngineDjVu.*",
    "EngineEbook.*",
    "EngineImages.*",
    "EngineMupdf.*",
    "EngineMupdfImpl.*",
    "EnginePs.*",
    "EngineAll.h",
    "EbookDoc.*",
    "EbookFormatter.*",
    "HtmlFormatter.*",
    "MobiDoc.*",
    "PdfCreator.*",
    "PalmDbReader.*",
  })
end

function chm_files()
  files_in_dir("ext/CHMLib/src", {
    "chm_lib.c",
    "lzx.c" ,
  })
end

function mupdf_files()
  files { "ext/mupdf_load_system_font.c" }

  files_in_dir("mupdf/source/cbz", {
    "mucbz.c",
    "muimg.c",
  })

  files { "mupdf/source/fitz/*.h" }
  files_in_dir("mupdf/source/fitz", {
    "archive.c",
    -- "barcode.cpp",
    "bbox-device.c",
    "bidi.c",
    "bidi-std.c",
    "bitmap.c",
    "brotli.c",
    "buffer.c",
    "color-fast.c",
    "color-icc-create.c",
    "color-lcms.c",
    "colorspace.c",
    "compress.c",
    "compressed-buffer.c",
    "context.c",
    "crypt-aes.c",
    "crypt-arc4.c",
    "crypt-md5.c",
    "crypt-sha2.c",
    "deskew.c",
    "device.c",
    "directory.c",
    "document.c",
    "document-all.c",
    "draw-affine.c",
    "draw-blend.c",
    "draw-device.c",
    "draw-edge.c",
    "draw-edgebuffer.c",
    "draw-glyph.c",
    "draw-mesh.c",
    "draw-paint.c",
    "draw-path.c",
    "draw-rasterize.c",
    "draw-scale-simple.c",
    "draw-unpack.c",
    "encode-basic.c",
    "encode-fax.c",
    "encode-jpx.c",
    "encodings.c",
    "error.c",
    "filter-basic.c",
    "filter-brotli.c",
    "filter-dct.c",
    "filter-fax.c",
    "filter-flate.c",
    "filter-jbig2.c",
    "filter-leech.c",
    "filter-lzw.c",
    "filter-predict.c",
    "filter-sgi.c",
    "filter-thunder.c",
    "font.c",
    "ftoa.c",
    "geometry.c",
    "getopt.c",
    "glyph.c",
    "glyphbox.c",
    "gz-doc.c",
    "halftone.c",
    "harfbuzz.c",
    "hash.c",
    "heap.c",
    "image.c",
    "jmemcust.c",
    "json.c",
    "link.c",
    "list-device.c",
    "load-bmp.c",
    "load-gif.c",
    "load-jbig2.c",
    "load-jpeg.c",
    "load-jpx.c",
    -- "load-jxr.c",
    "load-jxr-win.c",
    "load-png.c",
    "load-pnm.c",
    "load-psd.c",
    "load-tiff.c",
    "log.c",
    "memento.c",
    "memory.c",
    "noto.c",
    "outline.c",
    "output.c",
    "output-cbz.c",
    "output-csv.c",
    "output-docx.c",
    "output-jpeg.c",
    "output-pcl.c",
    "output-pclm.c",
    "output-pdfocr.c",
    "output-png.c",
    "output-pnm.c",
    "output-ps.c",
    "output-psd.c",
    "output-pwg.c",
    "output-svg.c",
    "path.c",
    "pixmap.c",
    "pool.c",
    "printf.c",
    "random.c",
    "skew.c",
    "separation.c",
    "shade.c",
    "stext-boxer.c",
    "stext-device.c",
    "stext-output.c",
    "stext-para.c",
    "stext-search.c",
    "stext-table.c",
    "store.c",
    "stream-open.c",
    "stream-read.c",
    "string.c",
    "strtof.c",
    "svg-device.c",
    "test-device.c",
    "text.c",
    "text-decoder.c",
    "time.c",
    "trace-device.c",
    "track-usage.c",
    "transition.c",
    "tree.c",
    "ucdn.c",
    "uncfb.c",
    "unlibarchive.c",
    "subset-cff.c",
    "subset-ttf.c",
    "untar.c",
    "unzip.c",
    "util.c",
    "writer.c",
    "xml-write.c",
    "xml.c",
    "zip.c",
  })

  files_in_dir("mupdf/source/html", {
    "css-apply.c",
    "css-parse.c",
    "css-properties.h",
    "epub-doc.c",
    "html-doc.c",
    "html-font.c",
    "html-imp.h",
    "html-layout.c",
    "html-outline.c",
    "html-parse.c",
    "mobi.c",
    "office.c",
    "story-writer.c",
    "txt.c",
    "xml-dom.c",
  })

  files_in_dir("mupdf/source/pdf", {
    "pdf-af.c",
    "pdf-annot.c",
    "pdf-appearance.c",
    "pdf-clean.c",
    "pdf-clean-file.c",
    "pdf-cmap.c",
    "pdf-cmap-load.c",
    "pdf-cmap-parse.c",
    "pdf-colorspace.c",
    "pdf-crypt.c",
    "pdf-device.c",
    "pdf-event.c",
    "pdf-font.c",
    "pdf-font-add.c",
    "pdf-form.c",
    "pdf-function.c",
    "pdf-graft.c",
    "pdf-image.c",
    "pdf-image-rewriter.c",
    "pdf-interpret.c",
    "pdf-js.c",
    "pdf-label.c",
    "pdf-layer.c",
    "pdf-layout.c",
    "pdf-lex.c",
    "pdf-link.c",
    "pdf-metrics.c",
    "pdf-nametree.c",
    "pdf-object.c",
    "pdf-op-buffer.c",
    "pdf-op-filter.c",
    "pdf-op-run.c",
    "pdf-outline.c",
    "pdf-page.c",
    "pdf-parse.c",
    "pdf-pattern.c",
    "pdf-recolor.c",
    "pdf-repair.c",
    "pdf-resources.c",
    "pdf-run.c",
    "pdf-shade.c",
    "pdf-signature.c",
    "pdf-store.c",
    "pdf-stream.c",
    "pdf-subset.c",
    "pdf-type3.c",
    "pdf-unicode.c",
    "pdf-util.c",
    "pdf-write.c",
    "pdf-xobject.c",
    "pdf-xref.c",
    "pdf-zugferd.c",
    "*.h",
  })

  files_in_dir("mupdf/source/svg", {
    "svg-color.c",
    "svg-doc.c",
    "svg-parse.c",
    "svg-run.c",
  })

  files_in_dir("mupdf/source/xps", {
    "xps-common.c",
    "xps-doc.c",
    "xps-glyphs.c",
    "xps-gradient.c",
    "xps-image.c",
    "xps-link.c",
    "xps-outline.c",
    "xps-path.c",
    "xps-resource.c",
    "xps-tile.c",
    "xps-util.c",
    "xps-zip.c",
  })
  files_in_dir("mupdf/source/reflow", {
    "*.c",
  })
  files {
    "mupdf/include/mupdf/fitz/*.h",
    "mupdf/include/mupdf/helpers/*.h",
    "mupdf/include/mupdf/pdf/*.h",
    "mupdf/include/mupdf/*.h"
  }
end

function mudoc_files()
  files_in_dir("mupdf/source", {
    "cbz/mucbz.c",
    "img/muimage.c",
    "tiff/mutiff.c",
    "fitz/document-all.c",
    "fitz/document-no-run.c",
    "fitz/svg-device.c",
    "fitz/output-pcl.c",
    "fitz/output-pwg.c",
    "fitz/stream-prog.c",
    "fitz/test-device.c",
  })
end

function mutools_files()
  files_in_dir("mupdf/source/tools", {
      "mudraw.c",
      "mutool.c",
      "pdfclean.c",
      "pdfextract.c",
      "pdfinfo.c",
      "pdfposter.c",
      "pdfshow.c",
  })
end

function mutool_files()
  mudoc_files() -- TODO: could turn into a .lib
  files_in_dir("mupdf/source/tools", {
      "mutool.c",
      "pdfshow.c",
      "pdfclean.c",
      "pdfinfo.c",
      "pdfextract.c",
      "pdfposter.c",
  })
end

function mudraw_files()
  mudoc_files()
  files_in_dir("mupdf/source/tools", {
      "mudraw.c",
  })
end

function synctex_files()
  files {
    "ext/synctex/synctex_parser_utils.c",
    "ext/synctex/synctex_parser.c",
  }
end

function efi_files()
  files {
    "tools/efi/*.h",
    "tools/efi/*.cpp",
    "src/utils/BaseUtil*",
    "CrashHandlerNoOp.cpp",
    "src/utils/BitManip.h",
    "src/utils/Dict*",
    "src/utils/StrUtil.*",
  }
end

function sizer_files()
  files {
    "tools/sizer/*",
    "src/CrashHandlerNoOp.cpp",
  }
end

function test_util_files()
  files_in_dir( "src/utils", {
    "BaseUtil.*",
    "BitManip.*",
    "ByteOrderDecoder.*",
    "CmdLineArgsIter.*",
    "ColorUtil.*",
    "CryptoUtil.*",
    "CssParser.*",
    "Dict.*",
    "Dpi.*",
    "FileUtil.*",
    "GeomUtil.*",
    "HtmlParserLookup.*",
    "HtmlPrettyPrint.*",
    "HtmlPullParser.*",
    "JsonParser.*",
    "Scoped.*",
    "SettingsUtil.*",
    "Log.*",
    "StrconvUtil.*",
    "StrFormat.*",
    "StrUtil.*",
    "StrVec.*",
    "StrQueue.*",
    "SquareTreeParser.*",
    "TrivialHtmlParser.*",
    "TempAllocator.*",
    "UtAssert.*",
    "Vec.*",
    "WinUtil.*",
    "WinDynCalls.*",
    "tests/*"
  })
  files_in_dir("src", {
    --"StressTesting.*",
    --"AppTools.*",
    "Commands.*",
    "CrashHandlerNoOp.cpp",
    "DisplayMode.*",
    "Flags.*",
    "SumatraConfig.*",
    "SettingsStructs.*",
    "SumatraUnitTests.cpp",
    "tools/test_util.cpp"
  })
end

function plugin_test_files()
    files {
        "src/tools/plugin-test.cpp",
        "src/CrashHandlerNoOp.cpp"
    }
end

function pdf_preview_files()
  files_in_dir("src/previewer", {
    "PdfPreview.*",
    "PdfPreviewBase.h",
    "PdfPreviewDll.cpp",
  })

  files_in_dir("src", {
    "utils/Log.*",
    "mui/Mui.*",
    "mui/TextRender.*",
    "CrashHandlerNoOp.cpp",
    "ChmFile.*",
    "DocProperties.*",
    "EbookDoc.*",
    "EbookFormatter.*",
    "EngineBase.*",
    "EngineEbook.*",
    "EngineDjVu.*",
    "EngineImages.*",
    "EngineMupdf.*",
    "EngineMupdfImpl.*",
    "EngineAll.h",
    "FzImgReader.*",
    "HtmlFormatter.*",
    "RegistryPreview.*",
    "MobiDoc.*",
    "MUPDF_Exports.cpp",
    "PalmDbReader.*",
    "PdfCreator.*",
    "SumatraConfig.*",
  })
end

function search_filter_files()
  files_in_dir("src/ifilter", {
    "PdfFilter.*",
    "SearchFilterDll.cpp",
    "CPdfFilter.*",
    "FilterBase.h",
  })
  files_in_dir("src", {
    "utils/Log.*",
    "MUPDF_Exports.cpp",
    "CrashHandlerNoOp.cpp",
    "DocProperties.*",
    "EbookDoc.*",
    "EngineBase.*",
    "EngineAll.h",
    "EngineMupdf.*",
    "EngineMupdfImpl.*",
    "MobiDoc.*",
    "PalmDbReader.*",
    "RegistrySearchFilter.*",
  })

  filter {"configurations:Debug or DebugFull"}
    files_in_dir("src/ifilter", {
      "TeXFilter.*",
      "EpubFilter.*",
    })
    files {
      "src/EbookDoc.*",
      "src/MobiDoc.*",
      "src/PalmDbReader.*",
    }
  filter {}
end

function gumbo_files()
  files_in_dir("ext/gumbo-parser/src", {
    "*.c",
    "*.h",
  })
  files_in_dir("ext/gumbo-parser/include", {
    "*.h",
  })
end

function bin2coff_files()
  files_in_dir("tools", {
    "bin2coff.c"
  })
end