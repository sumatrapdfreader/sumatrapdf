function files_in_dir(dir, files_in_dir)
  local paths = {}
  for _, file in ipairs(files_in_dir) do
    -- TODO: don't add "/" if dir ends with it of file starts with it
    local path = dir .. "/" .. file
    table.insert(paths, path)
  end
  files(paths)
end

function makelzsa_files()
  files_in_dir("src/utils", {
    "BaseUtil.*",
    "ByteOrderDecoder.*",
    "ByteWriter.*",
    "CmdLineArgsIter.*",
    "ColorUtil.*",
    "DirIter.*",
    "Dpi.*",
    "FileUtil.*",
    "GeomUtil.*",
    "Log.*",
    "LzmaSimpleArchive.*",
    "StrconvUtil.*",
    "StrFormat.*",
    "StrQueue.*",
    "StrUtil.*",
    "StrVec.*",
    "TempAllocator.*",
    "WinDynCalls.*",
    "WinUtil.*",
  })

  files {
    "src/tools/MakeLzSA.cpp",
  }
end

function brotli_files()
  files_in_dir("ext/brotli/c/common", {
    "*.h",
    "*.c",
  })
  files_in_dir("ext/brotli/c/dec", {
    "*.h",
    "*.c",
  })
  files_in_dir("ext/brotli/c/enc", {
    "*.h",
    "*.c",
  })
end

function zlib_files()
  files_in_dir("ext/zlib", {
    "adler32.c", "compress.c", "crc32.c", "deflate.c", "gzclose.c",
    "gzlib.c", "gzread.c", "gzwrite.c", "inffast.c", "inflate.c",
    "inftrees.c", "trees.c", "zutil.c",
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
    "ByteStream.cpp", "DataPool.cpp", "ddjvuapi.cpp", "debug.cpp",
    "DjVmDir.cpp", "DjVmDir0.cpp", "DjVmDoc.cpp", "DjVmNav.cpp",
    "DjVuAnno.cpp", "DjVuDocEditor.cpp", "DjVuDocument.cpp", "DjVuDumpHelper.cpp",
    "DjVuErrorList.cpp", "DjVuFile.cpp", "DjVuFileCache.cpp", "DjVuGlobal.cpp",
    "DjVuGlobalMemory.cpp", "DjVuImage.cpp", "DjVuInfo.cpp", "DjVuMessage.cpp",
    "DjVuMessageLite.cpp", "DjVuNavDir.cpp", "DjVuPalette.cpp", "DjVuPort.cpp",
    "DjVuText.cpp", "DjVuToPS.cpp", "GBitmap.cpp", "GContainer.cpp", "GException.cpp",
    "GIFFManager.cpp", "GMapAreas.cpp", "GOS.cpp", "GPixmap.cpp", "GRect.cpp",
    "GScaler.cpp", "GSmartPointer.cpp", "GString.cpp", "GThreads.cpp",
    "GUnicode.cpp", "GURL.cpp", "IFFByteStream.cpp", "IW44EncodeCodec.cpp",
    "IW44Image.cpp", "JB2EncodeCodec.cpp", "JB2Image.cpp",
    "JPEGDecoder.cpp", "miniexp.cpp", "MMRDecoder.cpp", "MMX.cpp",
    "UnicodeByteStream.cpp", "XMLParser.cpp", "XMLTags.cpp", "ZPCodec.cpp",
  })
end

function unarrr_lzmasdk_files()
  files_in_dir("ext/unarr/lzmasdk", {
    "CpuArch.c", "Ppmd7.c", "Ppmd7Dec.c", "Ppmd8.c", "Ppmd8Dec.c",
  })
end

function unarr_lzma_files()
  files_in_dir("ext/lzma/C", {
    "7zBuf.c", "7zDec.c", "7zIn.c", "7zStream.c", "Bcj2.c", "Bra.c", "Bra86.c",
    "LzFind.c", "LzFindMt.c", "Lzma2Dec.c", "LzmaDec.c", "LzmaEnc.c", "Threads.c",
  })
end

function unarr_files()
  files {
    "ext/bzip2/bzip_all.c",
    "ext/unarr/_7z/*",
    "ext/unarr/common/*",
    "ext/unarr/rar/*",
    "ext/unarr/tar/*",
    "ext/unarr/zip/*",
  }
  unarrr_lzmasdk_files()
  unarr_lzma_files()
end

function unarr_no_lzma_files()
  files {
    "ext/bzip2/bzip_all.c",
    "ext/unarr/_7z/*",
    "ext/unarr/common/*",
    "ext/unarr/lzmasdk/LzmaDec.*",
    "ext/unarr/rar/*",
    "ext/unarr/tar/*",
    "ext/unarr/zip/*",
  }
  unarrr_lzmasdk_files()
end

function unarr_no_bzip_files()
  files {
    "ext/unarr/_7z/*",
    "ext/unarr/common/*",
    "ext/unarr/rar/*",
    "ext/unarr/tar/*",
    "ext/unarr/zip/*",
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
    "jbig2_halftone.c",
    "jbig2_huffman.c",
    "jbig2_hufftab.c",
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
    "brands.*",
    "common_utils.*",
    "context.*",
    "error.*",
    "file.*",
    "file_layout.*",
    "init.*",
    "logging.*",
    "nclx.*",
    "pixelimage.*",
    "plugin_registry.*",
    "region.*",
    "security_limits.*",
    "text.*",
  })
  files_in_dir("ext/libheif/libheif/image-items", {
    "avc.*",
    "avif.*",
    "grid.*",
    "hevc.*",
    "iden.*",
    "image_item.*",
    "jpeg.*",
    "jpeg2000.*",
    "mask_image.*",
    "overlay.*",
    "tiled.*",
    "vvc.*",
  })
  files_in_dir("ext/libheif/libheif/codecs", {
    "avc_boxes.*",
    "avc_dec.*",
    "avc_enc.*",
    "avif_boxes.*",
    "avif_dec.*",
    "avif_enc.*",
    "decoder.*",
    "encoder.*",
    "hevc_boxes.*",
    "hevc_dec.*",
    "hevc_enc.*",
    "jpeg2000_boxes.*",
    "jpeg2000_dec.*",
    "jpeg2000_enc.*",
    "jpeg_boxes.*",
    "jpeg_dec.*",
    "jpeg_enc.*",
    "vvc_boxes.*",
    "vvc_dec.*",
    "vvc_enc.*",
  })
  files_in_dir("ext/libheif/libheif/color-conversion", {
    "alpha.*",
    "chroma_sampling.*",
    "colorconversion.*",
    "hdr_sdr.*",
    "monochrome.*",
    "rgb2rgb.*",
    "rgb2yuv.*",
    "rgb2yuv_sharp.*",
    "yuv2rgb.*",
  })
  files_in_dir("ext/libheif/libheif/plugins", {
    "decoder_dav1d.*",
    "encoder_mask.*",
  })
  files_in_dir("ext/libheif/libheif/sequences", {
    "chunk.*",
    "seq_boxes.*",
    "track.*",
    "track_metadata.*",
    "track_visual.*",
  })
  files_in_dir("ext/libheif/libheif/api/libheif", {
    "heif.*",
    "heif_brands.*",
    "heif_color.*",
    "heif_context.*",
    "heif_decoding.*",
    "heif_encoding.*",
    "heif_image.*",
    "heif_image_handle.*",
    "heif_plugin.*",
    "heif_security.*",
    "heif_sequences.*",
    "heif_tai_timestamps.*",
  })
end

function dav1d_x68_files()
  files_in_dir("ext/dav1d/src/x86", {
    "cpu.c",
    "msac_init.c",
    "refmvs_init.c",
  })

  files_in_dir("ext/dav1d/src/x86", {
    "cdef16_avx2.asm",
    "cdef16_avx512.asm",
    "cdef16_sse.asm",
    "cdef_avx2.asm",
    "cdef_avx512.asm",
    "cdef_sse.asm",
    "cpuid.asm",
    "filmgrain16_avx2.asm",
    "filmgrain16_avx512.asm",
    "filmgrain16_sse.asm",
    "filmgrain_avx2.asm",
    "filmgrain_avx512.asm",
    "filmgrain_sse.asm",
    "ipred16_avx2.asm",
    "ipred16_avx512.asm",
    "ipred16_sse.asm",
    "ipred_avx2.asm",
    "ipred_avx512.asm",
    "ipred_sse.asm",
    "itx16_avx2.asm",
    "itx16_sse.asm",
    "itx_avx2.asm",
    "itx_avx512.asm",
    "itx_sse.asm",
    "loopfilter16_avx2.asm",
    "loopfilter16_sse.asm",
    "loopfilter_avx2.asm",
    "loopfilter_avx512.asm",
    "loopfilter_sse.asm",
    "looprestoration16_avx2.asm",
    "looprestoration16_avx512.asm",
    "looprestoration16_sse.asm",
    "looprestoration_avx2.asm",
    "looprestoration_avx512.asm",
    "looprestoration_sse.asm",
    "mc16_avx2.asm",
    "mc16_avx512.asm",
    "mc16_sse.asm",
    "mc_avx2.asm",
    "mc_avx512.asm",
    "mc_sse.asm",
    "msac.asm",
    "refmvs.asm",
  })
end

function dav1d_files()
  files_in_dir("ext/dav1d/src", {
    "cdf.c",
    "cpu.c",
    "data.c",
    "decode.c",
    "dequant_tables.c",
    "getbits.c",
    "intra_edge.c",
    "itx_1d.c",
    "lf_mask.c",
    "lib.c",
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
    "thread_task.c",
    "warpmv.c",
    "wedge.c",
    "win32/thread.c",
  })

  files_in_dir("ext/dav1d/src", {
    "sumatra_bitdepth_16.c",
    "sumatra_bitdepth_16_2.c",
    "sumatra_bitdepth_8.c",
    "sumatra_bitdepth_8_2.c",
  })

  files("ext/dav1d/include/common/*.h")
  files("ext/dav1d/include/dav1d/*.h")
end

function openjpeg_files()
  files_in_dir("ext/openjpeg/src/lib/openjp2", {
    "*.h",
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
    "odt.*",
    "odt_template.*",
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
    "alpha_processing_neon.c",
    "alpha_processing_sse2.c",
    "alpha_processing_sse41.c",
    "cost.c",
    "cpu.c",
    "dec.c",
    "dec_clip_tables.c",
    "dec_neon.c",
    "dec_sse2.c",
    "dec_sse41.c",
    "filters.c",
    "filters_neon.c",
    "filters_sse2.c",
    "lossless.c",
    "lossless_neon.c",
    "lossless_sse2.c",
    "lossless_sse41.c",
    "rescaler.c",
    "rescaler_neon.c",
    "rescaler_sse2.c",
    "ssim.c",
    "ssim_sse2.c",
    "upsampling.c",
    "upsampling_neon.c",
    "upsampling_sse2.c",
    "upsampling_sse41.c",
    "yuv.c",
    "yuv_neon.c",
    "yuv_sse2.c",
    "yuv_sse41.c",
  })

  files("ext/libwebp/src/sharpyuv/*.c")

  files("ext/libwebp/src/utils/*.c")
end

function libjpeg_turbo_files()
  files_in_dir("ext/libjpeg-turbo", {
    "jaricom.c", "jcapimin.c", "jcapistd.c", "jcarith.c", "jccoefct.c",
    "jccolor.c", "jcdctmgr.c", "jchuff.c", "jcinit.c", "jcmainct.c",
    "jcmarker.c", "jcmaster.c", "jcomapi.c", "jcparam.c", "jcphuff.c",
    "jcprepct.c", "jcsample.c", "jdapimin.c", "jdapistd.c", "jdarith.c",
    "jdatadst.c", "jdatasrc.c", "jdcoefct.c", "jdcolor.c", "jddctmgr.c",
    "jdhuff.c", "jdinput.c", "jdmainct.c", "jdmarker.c", "jdmaster.c",
    "jdmerge.c", "jdphuff.c", "jdpostct.c", "jdsample.c", "jdtrans.c",
    "jerror.c", "jfdctflt.c", "jfdctfst.c", "jfdctint.c", "jidctflt.c",
    "jidctfst.c", "jidctint.c", "jidctred.c", "jmemmgr.c", "jmemnobs.c",
    "jquant1.c", "jquant2.c", "jutils.c",
  })

  filter { 'platforms:arm64' }
  files { "ext/libjpeg-turbo/jsimd_none.c" }

  filter { 'platforms:x86' }
  files_in_dir("ext/libjpeg-turbo/simd", {
    "jccolmmx.asm", "jccolss2.asm", "jcgrammx.asm", "jcgrass2.asm",
    "jcqnt3dn.asm", "jcqntmmx.asm", "jcqnts2f.asm", "jcqnts2i.asm",
    "jcqntsse.asm", "jcsammmx.asm", "jcsamss2.asm", "jdcolmmx.asm",
    "jdcolss2.asm", "jdmermmx.asm", "jdmerss2.asm", "jdsammmx.asm",
    "jdsamss2.asm", "jf3dnflt.asm", "jfmmxfst.asm", "jfmmxint.asm",
    "jfss2fst.asm", "jfss2int.asm", "jfsseflt.asm", "ji3dnflt.asm",
    "jimmxfst.asm", "jimmxint.asm", "jimmxred.asm", "jiss2flt.asm",
    "jiss2fst.asm", "jiss2int.asm", "jiss2red.asm", "jisseflt.asm",
    "jsimdcpu.asm",
  })
  files { "ext/libjpeg-turbo/simd/jsimd_i386.c" }

  filter { 'platforms:x64 or x64_asan' }
  files_in_dir("ext/libjpeg-turbo/simd", {
    "jccolss2-64.asm", "jcgrass2-64.asm", "jcqnts2f-64.asm", "jcqnts2i-64.asm",
    "jcsamss2-64.asm", "jdcolss2-64.asm", "jdmerss2-64.asm", "jdsamss2-64.asm",
    "jfss2fst-64.asm", "jfss2int-64.asm", "jfsseflt-64.asm",
    "jiss2flt-64.asm", "jiss2fst-64.asm", "jiss2int-64.asm", "jiss2red-64.asm",
  })
  files { "ext/libjpeg-turbo/simd/jsimd_x86_64.c" }

  filter {}
end

function lcms2_files()
  files_in_dir("ext/lcms2/src", {
    "*.c", "*.h"
  })
  files { "ext/lcms/include.*.h" }
end

function harfbuzz_files()
  files_in_dir("ext/harfbuzz/src", {
    "hb-aat-layout.cc",
    "hb-aat-map.cc",
    "hb-blob.cc",
    "hb-buffer-serialize.cc",
    "hb-buffer-verify.cc",
    "hb-buffer.cc",
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
    "hb-ot-shape-fallback.cc",
    "hb-ot-shape-normalize.cc",
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
    "hb-ot-tag.cc",
    "hb-ot-var.cc",
    "hb-set.cc",
    "hb-shape-plan.cc",
    "hb-shape.cc",
    "hb-shaper.cc",
    "hb-static.cc",
    "hb-subset-cff-common.cc",
    "hb-subset-cff1.cc",
    "hb-subset-cff2.cc",
    "hb-subset-input.cc",
    "hb-subset-plan.cc",
    "hb-subset.cc",
    "hb-ucd.cc",
    "hb-unicode.cc",
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
    -- TODO: temporary
    "ftotval.c",
    "ftstroke.c",
    "ftsynth.c",
    "ftsystem.c",
    "fttype1.c",
  })

  files_in_dir("ext/freetype/src", {
    "cff/cff.c",
    "cid/type1cid.c",
    "psaux/psaux.c",
    "pshinter/pshinter.c",
    "psnames/psnames.c",
    "raster/raster.c",
    "sfnt/sfnt.c",
    "smooth/smooth.c",
    "truetype/truetype.c",
    "type1/type1.c",
  })
end

files {
}

function sumatrapdf_files()
  files_in_dir(".", {
    "*.md",
    "*.txt",
    "*.yml",
    ".clang-format",
    ".github/*.yml",
    ".github/workflows/*.yml",
    ".gitignore",
    "cmd/*",
    "do/*.go",
    "docs/*.html",
    "docs/*.txt",
    "docs/*.css",
    "docs/md/*.md",
    "ext/versions.txt",
    "premake5.files.lua",
    "premake5.lua",
  })
  files_in_dir("src", {
    "Accelerators.*",
    "Actions.*",
    "AppColors.*",
    "AppSettings.*",
    "AppTools.*",
    "Canvas.*",
    "CanvasAboutUI.*",
    "ChmModel.*",
    "CommandPalette.*",
    "Commands.*",
    "CrashHandler.*",
    "ImageSaveCropResize.*",
    "DisplayMode.*",
    "DisplayModel.*",
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
    "PreviewPipe.*",
    "Print.*",
    "ProgressUpdateUI.*",
    "RegistryInstaller.*",
    "RegistryPreview.*",
    "RegistrySearchFilter.*",
    "RenderCache.*",
    "resource.h",
    "SearchAndDDE.*",
    "OverlayScrollbar.*",
    "Screenshot.*",
    "Selection.*",
    "Settings.h",
    "SettingsStructs.*",
    "SimpleBrowserWindow.*",
    "StressTesting.*",
    "SumatraConfig.cpp",
    "SumatraDialogs.*",
    "SumatraPDF.cpp",
    "SumatraPDF.h",
    "SumatraPDF.rc",
    "SumatraProperties.*",
    "SumatraStartup.cpp",
    "SvgIcons.*",
    "TableOfContents.*",
    "Tabs.*",
    "Tester.*",
    "Tests.cpp",
    "TextSearch.*",
    "TextSelection.*",
    "Theme.*",
    "Toolbar.*",
    "TranslationLangs.cpp",
    "Translations.*",
    "Uninstaller.cpp",
    "UpdateCheck.*",
    "Version.h",
    "VirtWnd.*",
    "WindowTab.*",
  })
  filter { "configurations:Debug or DebugFull" }
  files_in_dir("src", {
    "regress/Regress.*",
    "Scratch.*",
    "TestPlugin.cpp",
    "TestPreview.cpp",
  })
  files_in_dir("src/testcode", {
    "test-app.h",
    "TestApp.cpp",
    "TestLayout.cpp",
    --"TestLice.cpp",
    "TestTab.cpp",
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
    "DocumentProvider.*",
    "PageProvider.*",
    "Provider.*",
    "StartPageProvider.*",
    "TextRange.*",
  })
end

function darkmodelib_files()
  files_in_dir("ext/darkmodelib/src", {
    "*.h",
    "DarkModeSubclass.cpp",
    "DmlibColor.cpp",
    "DmlibDpi.cpp",
    "DmlibHook.cpp",
    "DmlibPaintHelper.cpp",
    "DmlibSubclass.cpp",
    "DmlibSubclassControl.cpp",
    "DmlibSubclassWindow.cpp",
    "DmlibWinApi.cpp",
  })
  files_in_dir("ext/darkmodelib/include", {
    "DarkModeSubclass.h",
  })
end

function utils_files()
  files_in_dir("src/utils", {
    "ApiHook.*",
    "Archive.*",
    "AvifReader.*",
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
    "FileUtil.*",
    "FileWatcher.*",
    "FzImgReader.*",
    "GdiPlusUtil.*",
    "GeomUtil.*",
    "GuessFileType.*",
    "HtmlParserLookup.*",
    "HtmlPrettyPrint.*",
    "HtmlPullParser.*",
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
    "StrQueue.*",
    "StrUtil.*",
    "StrVec.*",
    "TempAllocator.*",
    "TgaReader.*",
    "ThreadUtil.*",
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
  filter { "configurations:Debug or DebugFull" }
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
    "EbookDoc.*",
    "EbookFormatter.*",
    "EngineAll.h",
    "EngineBase.*",
    "EngineCreate.*",
    "EngineDjVu.*",
    "EngineEbook.*",
    "EngineImages.*",
    "EngineMupdf.*",
    "EngineMupdfImpl.*",
    "EnginePs.*",
    "HtmlFormatter.*",
    "MobiDoc.*",
    "PalmDbReader.*",
    "PdfCreator.*",
  })
end

function chm_files()
  files_in_dir("ext/CHMLib/src", {
    "chm_lib.c",
    "lzx.c",
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
    "barcode.c",
    "bbox-device.c",
    "bidi-std.c",
    "bidi.c",
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
    "document-all.c",
    "document.c",
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
    "hyphen.c",
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
    "load-jxr-win.c",
    -- "load-jxr.c",
    "load-png.c",
    "load-pnm.c",
    "load-psd.c",
    "load-tiff.c",
    "log.c",
    "memento.c",
    "memory.c",
    "noto.c",
    "ocr-device.c",
    "outline.c",
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
    "output.c",
    "path.c",
    "pixmap.c",
    "pool.c",
    "printf.c",
    "random.c",
    "separation.c",
    "shade.c",
    "skew.c",
    "stext-boxer.c",
    "stext-classify.c",
    "stext-device.c",
    "stext-iterator.c",
    "stext-output.c",
    "stext-para.c",
    "stext-raft.c",
    "stext-search.c",
    "stext-table.c",
    "store.c",
    "stream-open.c",
    "stream-read.c",
    "string.c",
    "strtof.c",
    "subset-cff.c",
    "subset-ttf.c",
    "svg-device.c",
    "test-device.c",
    "text-decoder.c",
    "text.c",
    "time.c",
    "trace-device.c",
    "track-usage.c",
    "transition.c",
    "tree.c",
    "ucdn.c",
    "uncfb.c",
    "unlibarchive.c",
    "untar.c",
    "unzip.c",
    "util.c",
    "warp.c",
    "writer.c",
    "xml-write.c",
    "xml.c",
    "xmltext-device.c",
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
    "*.h",
    "pdf-af.c",
    "pdf-annot.c",
    "pdf-appearance.c",
    "pdf-clean-file.c",
    "pdf-clean.c",
    "pdf-cmap-load.c",
    "pdf-cmap-parse.c",
    "pdf-cmap.c",
    "pdf-colorspace.c",
    "pdf-crypt.c",
    "pdf-device.c",
    "pdf-event.c",
    "pdf-font-add.c",
    "pdf-font.c",
    "pdf-form.c",
    "pdf-function.c",
    "pdf-graft.c",
    "pdf-image-rewriter.c",
    "pdf-image.c",
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
    "pdf-op-color.c",
    "pdf-op-filter.c",
    "pdf-op-run.c",
    "pdf-op-vectorize.c",
    "pdf-outline.c",
    "pdf-page.c",
    "pdf-parse.c",
    "pdf-pattern.c",
    "pdf-recolor.c",
    "pdf-repair.c",
    "pdf-resources.c",
    "pdf-run.c",
    "pdf-shade-recolor.c",
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
    "reflow-doc.c",
  })
  files_in_dir("mupdf/source/tools", {
    "muconvert.c",
    "mudraw.c",
    "mugrep.c",
    "muraster.c",
    "murun.c",
    "mutrace.c",
    "pdfaudit.c",
    "pdfbake.c",
    "pdfclean.c",
    "pdfcreate.c",
    "pdfextract.c",
    "pdfinfo.c",
    "pdfmerge.c",
    "pdfpages.c",
    "pdfposter.c",
    "pdfrecolor.c",
    "pdfshow.c",
    "pdfsign.c",
    "pdftrim.c",
  })
  files {
    "mupdf/include/mupdf/*.h",
    "mupdf/include/mupdf/fitz/*.h",
    "mupdf/include/mupdf/helpers/*.h",
    "mupdf/include/mupdf/pdf/*.h",
  }
  files {
    "mupdf/source/helpers/mu-threads/mu-threads.c",
    "mupdf/source/helpers/pkcs7/pkcs7-openssl.c",
  }
end

function synctex_files()
  files {
    "ext/synctex/synctex_parser_utils.c",
    "ext/synctex/synctex_parser.c",
  }
end

function efi_files()
  files {
    "CrashHandlerNoOp.cpp",
    "src/utils/BaseUtil*",
    "src/utils/BitManip.h",
    "src/utils/Dict*",
    "src/utils/StrUtil.*",
    "tools/efi/*.cpp",
    "tools/efi/*.h",
  }
end

function test_util_files()
  files_in_dir("src/utils", {
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
    "Log.*",
    "Scoped.*",
    "SettingsUtil.*",
    "SquareTreeParser.*",
    "StrconvUtil.*",
    "StrFormat.*",
    "StrQueue.*",
    "StrUtil.*",
    "StrVec.*",
    "TempAllocator.*",
    "tests/*",
    "TrivialHtmlParser.*",
    "UtAssert.*",
    "Vec.*",
    "WinDynCalls.*",
    "WinUtil.*",
  })
  files_in_dir("src", {
    --"AppTools.*",
    "Commands.*",
    "CrashHandlerNoOp.cpp",
    "DisplayMode.*",
    "Flags.*",
    "SettingsStructs.*",
    --"StressTesting.*",
    "SumatraConfig.*",
    "SumatraUnitTests.cpp",
    "tools/test_util.cpp"
  })
end

function pdf_preview_files()
  files_in_dir("src/previewer", {
    "PdfPreview.*",
    "PdfPreviewBase.h",
    "PdfPreviewDll.cpp",
  })

  files_in_dir("src", {
    "ChmFile.*",
    "CrashHandlerNoOp.cpp",
    "DocProperties.*",
    "EbookDoc.*",
    "EbookFormatter.*",
    "EngineAll.h",
    "EngineBase.*",
    "EngineDjVu.*",
    "EngineEbook.*",
    "EngineImages.*",
    "EngineMupdf.*",
    "EngineMupdfImpl.*",
    "FzImgReader.*",
    "HtmlFormatter.*",
    "MobiDoc.*",
    "mui/Mui.*",
    "mui/TextRender.*",
    "MUPDF_Exports.cpp",
    "PalmDbReader.*",
    "PdfCreator.*",
    "RegistryPreview.*",
    "SumatraConfig.*",
    "utils/Log.*",
  })
end

function search_filter_files()
  files_in_dir("src/ifilter", {
    "CPdfFilter.*",
    "FilterBase.h",
    "PdfFilter.*",
    "SearchFilterDll.cpp",
  })
  files_in_dir("src", {
    "CrashHandlerNoOp.cpp",
    "DocProperties.*",
    "EbookDoc.*",
    "EngineAll.h",
    "EngineBase.*",
    "EngineMupdf.*",
    "EngineMupdfImpl.*",
    "MobiDoc.*",
    "MUPDF_Exports.cpp",
    "PalmDbReader.*",
    "RegistrySearchFilter.*",
    "utils/Log.*",
  })

  filter { "configurations:Debug or DebugFull" }
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

function pdf_preview2_files()
  files_in_dir("src/previewer2", {
    "PdfPreview.*",
    "PdfPreviewBase.h",
    "PdfPreviewDll.cpp",
  })

  files_in_dir("src", {
    "CrashHandlerNoOp.cpp",
    "mui/Mui.*",
    "PreviewPipe.*",
    "RegistryPreview.*",
    "SumatraConfig.*",
    "utils/BaseUtil.*",
    "utils/ColorUtil.*",
    "utils/Dpi.*",
    "utils/FileUtil.*",
    "utils/GeomUtil.*",
    "utils/Log.*",
    "utils/StrconvUtil.*",
    "utils/StrFormat.*",
    "utils/StrUtil.*",
    "utils/StrVec.*",
    "utils/TempAllocator.*",
    "utils/WinDynCalls.*",
    "utils/WinUtil.*",
  })
end

function search_filter2_files()
  files_in_dir("src/ifilter2", {
    "CPdfFilter.*",
    "FilterBase.h",
    "PdfFilter.*",
    "SearchFilterDll.cpp",
  })
  files_in_dir("src", {
    "CrashHandlerNoOp.cpp",
    "mui/Mui.*",
    "RegistrySearchFilter.*",
    "SumatraConfig.*",
    "utils/BaseUtil.*",
    "utils/ColorUtil.*",
    "utils/Dpi.*",
    "utils/FileUtil.*",
    "utils/GeomUtil.*",
    "utils/Log.*",
    "utils/StrconvUtil.*",
    "utils/StrFormat.*",
    "utils/StrUtil.*",
    "utils/StrVec.*",
    "utils/TempAllocator.*",
    "utils/WinDynCalls.*",
    "utils/WinUtil.*",
  })
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
