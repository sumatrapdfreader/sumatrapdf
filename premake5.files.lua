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
  files_in_dir("src/base", {
    "Arena.*",
    "Arena_win.cpp",
    "Base.h",
    "Base.cpp",
    "Base_win.cpp",
    "ByteOrderDecoder.*",
    "ByteWriter.*",
    "CmdLineArgsIter.h",
    "CmdLineArgsIter.cpp",
    "CmdLineArgsIter_win.cpp",
    "Color.*",
    "DirIter.h",
    "DirIter.cpp",
    "DirIter_win.cpp",
    "Dpi.h",
    "Dpi_win.cpp",
    "File.h",
    "File.cpp",
    "File_win.cpp",
    "Geom.*",
    "Log.h",
    "LogNoOp.cpp",
    "LzmaSimpleArchive.*",
    "Strconv.*",
    "StrFormatParse.*",
    "StrQueue.*",
    "Str.*",
    "StrUtf8.*",
    "StrVec.*",
    "Thread.*",
    "WinDynCalls.h",
    "WinDynCalls_win.cpp",
    "Win.*",
  })

  -- LZMA files needed by LzmaSimpleArchive (decode) and MakeLzSA.cpp (encode)
  files_in_dir("ext/lzma/C", {
    "Bra86.c",
    "LzFind.c",
    "LzmaDec.c",
    "LzmaEnc.c",
  })

  files {
    "src/CrashHandlerNoOp.cpp",
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

function libarchive_files()
  files { "ext/libarchive/libarchive/*.h" }
  removefiles { "ext/libarchive/libarchive/config_linux.h" }
  files_in_dir("ext/libarchive/libarchive", {
    -- core
    "archive_acl.c",
    "archive_check_magic.c",
    "archive_cmdline.c",
    "archive_cryptor.c",
    "archive_digest.c",
    "archive_entry.c",
    "archive_entry_copy_bhfi.c",
    "archive_entry_copy_stat.c",
    "archive_entry_link_resolver.c",
    "archive_entry_sparse.c",
    "archive_entry_stat.c",
    "archive_entry_strmode.c",
    "archive_entry_xattr.c",
    "archive_hmac.c",
    "archive_match.c",
    "archive_options.c",
    "archive_pack_dev.c",
    "archive_pathmatch.c",
    "archive_ppmd7.c",
    "archive_ppmd8.c",
    "archive_random.c",
    "archive_rb.c",
    "archive_string.c",
    "archive_string_sprintf.c",
    "archive_time.c",
    "archive_util.c",
    "archive_version_details.c",
    "archive_virtual.c",
    "archive_windows.c",
    "archive_blake2s_ref.c",
    "archive_blake2sp_ref.c",
    -- read core
    "archive_read.c",
    "archive_read_add_passphrase.c",
    "archive_read_append_filter.c",
    "archive_read_data_into_fd.c",
    "archive_read_extract.c",
    "archive_read_extract2.c",
    "archive_read_open_fd.c",
    "archive_read_open_file.c",
    "archive_read_open_filename.c",
    "archive_read_open_memory.c",
    "archive_read_set_format.c",
    "archive_read_set_options.c",
    -- read filters
    "archive_read_support_filter_all.c",
    "archive_read_support_filter_by_code.c",
    "archive_read_support_filter_bzip2.c",
    "archive_read_support_filter_compress.c",
    "archive_read_support_filter_grzip.c",
    "archive_read_support_filter_gzip.c",
    "archive_read_support_filter_lrzip.c",
    "archive_read_support_filter_lz4.c",
    "archive_read_support_filter_lzop.c",
    "archive_read_support_filter_none.c",
    "archive_read_support_filter_program.c",
    "archive_read_support_filter_rpm.c",
    "archive_read_support_filter_uu.c",
    "archive_read_support_filter_xz.c",
    "archive_read_support_filter_zstd.c",
    -- read formats
    "archive_read_support_format_7zip.c",
    "archive_read_support_format_all.c",
    "archive_read_support_format_ar.c",
    "archive_read_support_format_by_code.c",
    "archive_read_support_format_cab.c",
    "archive_read_support_format_cpio.c",
    "archive_read_support_format_empty.c",
    "archive_read_support_format_iso9660.c",
    "archive_read_support_format_lha.c",
    "archive_read_support_format_mtree.c",
    "archive_read_support_format_rar.c",
    "archive_read_support_format_rar5.c",
    "archive_read_support_format_raw.c",
    "archive_read_support_format_tar.c",
    "archive_read_support_format_warc.c",
    "archive_read_support_format_xar.c",
    "archive_read_support_format_zip.c",
    -- xxhash (needed by lz4 filter)
    "xxhash.c",
    -- read disk (needed for some entry operations)
    "archive_read_disk_set_standard_lookup.c",
    "archive_read_disk_windows.c",
    -- parse date (used by mtree, tar)
    "archive_parse_date.c",
    -- filter fork (needed by program filter)
    "filter_fork_windows.c",
  })
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
    "id_creator.*",
    "init.*",
    "logging.*",
    "mini.*",
    "nclx.*",
    "omaf_boxes.*",
    "plugin_registry.*",
    "region.*",
    "security_limits.*",
    "text.*",
  })
  files_in_dir("ext/libheif/libheif/image", {
    "image_description.*",
    "pixelimage.*",
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
    "bayer_bilinear.*",
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
    "heif_components.*",
    "heif_context.*",
    "heif_decoding.*",
    "heif_encoding.*",
    "heif_image.*",
    "heif_image_handle.*",
    "heif_metadata.*",
    "heif_omaf.*",
    "heif_plugin.*",
    "heif_security.*",
    "heif_sequences.*",
    "heif_tai_timestamps.*",
  })
end

function dav1d_x68_files()
  files_in_dir("ext/dav1d/src/x86", {
    "cpu.c",
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
    "itx16_avx512.asm",
    "itx16_sse.asm",
    "itx_avx2.asm",
    "itx_avx512.asm",
    "itx_sse.asm",
    "loopfilter16_avx2.asm",
    "loopfilter16_avx512.asm",
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
    "pal.asm",
    "refmvs.asm",
  })
end

function dav1d_files()
  files_in_dir("ext/dav1d/src", {
    "cdf.c",
    "cpu.c",
    "ctx.c",
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
    "pal.c",
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
    "lossless_avx2.c",
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
  -- libjpeg-turbo 3.x: core (precision-independent) sources
  files_in_dir("ext/libjpeg-turbo/src", {
    "jaricom.c", "jcapimin.c", "jcarith.c", "jchuff.c", "jcicc.c",
    "jcinit.c", "jclhuff.c", "jcmarker.c", "jcmaster.c", "jcomapi.c",
    "jcparam.c", "jcphuff.c", "jctrans.c", "jdapimin.c", "jdarith.c",
    "jdatadst.c", "jdatasrc.c", "jdhuff.c", "jdicc.c", "jdinput.c",
    "jdlhuff.c", "jdmarker.c", "jdmaster.c", "jdphuff.c", "jdtrans.c",
    "jerror.c", "jfdctflt.c", "jmemmgr.c", "jmemnobs.c", "jpeg_nbits.c",
  })

  -- libjpeg-turbo 3.x: per-precision wrappers (8/12/16-bit), each #includes
  -- the matching ../<name>.c with BITS_IN_JSAMPLE set. These provide run-time
  -- selectable data precision.
  files_in_dir("ext/libjpeg-turbo/src/wrapper", {
    "jcapistd-8.c", "jcapistd-12.c", "jcapistd-16.c",
    "jccoefct-8.c", "jccoefct-12.c",
    "jccolor-8.c", "jccolor-12.c", "jccolor-16.c",
    "jcdctmgr-8.c", "jcdctmgr-12.c",
    "jcdiffct-8.c", "jcdiffct-12.c", "jcdiffct-16.c",
    "jclossls-8.c", "jclossls-12.c", "jclossls-16.c",
    "jcmainct-8.c", "jcmainct-12.c", "jcmainct-16.c",
    "jcprepct-8.c", "jcprepct-12.c", "jcprepct-16.c",
    "jcsample-8.c", "jcsample-12.c", "jcsample-16.c",
    "jdapistd-8.c", "jdapistd-12.c", "jdapistd-16.c",
    "jdcoefct-8.c", "jdcoefct-12.c",
    "jdcolor-8.c", "jdcolor-12.c", "jdcolor-16.c",
    "jddctmgr-8.c", "jddctmgr-12.c",
    "jddiffct-8.c", "jddiffct-12.c", "jddiffct-16.c",
    "jdlossls-8.c", "jdlossls-12.c", "jdlossls-16.c",
    "jdmainct-8.c", "jdmainct-12.c", "jdmainct-16.c",
    "jdmerge-8.c", "jdmerge-12.c",
    "jdpostct-8.c", "jdpostct-12.c", "jdpostct-16.c",
    "jdsample-8.c", "jdsample-12.c", "jdsample-16.c",
    "jfdctfst-8.c", "jfdctfst-12.c",
    "jfdctint-8.c", "jfdctint-12.c",
    "jidctflt-8.c", "jidctflt-12.c",
    "jidctfst-8.c", "jidctfst-12.c",
    "jidctint-8.c", "jidctint-12.c",
    "jidctred-8.c", "jidctred-12.c",
    "jquant1-8.c", "jquant1-12.c",
    "jquant2-8.c", "jquant2-12.c",
    "jutils-8.c", "jutils-12.c", "jutils-16.c",
  })

  -- arm64: no SIMD (WITH_SIMD is left undefined in jconfig.h/jconfigint.h).

  filter { 'platforms:x86' }
  files_in_dir("ext/libjpeg-turbo/simd/i386", {
    "jsimdcpu.asm", "jfdctflt-3dn.asm", "jidctflt-3dn.asm", "jquant-3dn.asm",
    "jccolor-mmx.asm", "jcgray-mmx.asm", "jcsample-mmx.asm", "jdcolor-mmx.asm",
    "jdmerge-mmx.asm", "jdsample-mmx.asm", "jfdctfst-mmx.asm", "jfdctint-mmx.asm",
    "jidctfst-mmx.asm", "jidctint-mmx.asm", "jidctred-mmx.asm", "jquant-mmx.asm",
    "jfdctflt-sse.asm", "jidctflt-sse.asm", "jquant-sse.asm",
    "jccolor-sse2.asm", "jcgray-sse2.asm", "jchuff-sse2.asm", "jcphuff-sse2.asm",
    "jcsample-sse2.asm", "jdcolor-sse2.asm", "jdmerge-sse2.asm", "jdsample-sse2.asm",
    "jfdctfst-sse2.asm", "jfdctint-sse2.asm", "jidctflt-sse2.asm", "jidctfst-sse2.asm",
    "jidctint-sse2.asm", "jidctred-sse2.asm", "jquantf-sse2.asm", "jquanti-sse2.asm",
    "jccolor-avx2.asm", "jcgray-avx2.asm", "jcsample-avx2.asm", "jdcolor-avx2.asm",
    "jdmerge-avx2.asm", "jdsample-avx2.asm", "jfdctint-avx2.asm", "jidctint-avx2.asm",
    "jquanti-avx2.asm",
  })
  files { "ext/libjpeg-turbo/simd/i386/jsimd.c" }

  filter { 'platforms:x64 or x64_asan' }
  files_in_dir("ext/libjpeg-turbo/simd/x86_64", {
    "jsimdcpu.asm", "jfdctflt-sse.asm",
    "jccolor-sse2.asm", "jcgray-sse2.asm", "jchuff-sse2.asm", "jcphuff-sse2.asm",
    "jcsample-sse2.asm", "jdcolor-sse2.asm", "jdmerge-sse2.asm", "jdsample-sse2.asm",
    "jfdctfst-sse2.asm", "jfdctint-sse2.asm", "jidctflt-sse2.asm", "jidctfst-sse2.asm",
    "jidctint-sse2.asm", "jidctred-sse2.asm", "jquantf-sse2.asm", "jquanti-sse2.asm",
    "jccolor-avx2.asm", "jcgray-avx2.asm", "jcsample-avx2.asm", "jdcolor-avx2.asm",
    "jdmerge-avx2.asm", "jdsample-avx2.asm", "jfdctint-avx2.asm", "jidctint-avx2.asm",
    "jquanti-avx2.asm",
  })
  files { "ext/libjpeg-turbo/simd/x86_64/jsimd.c" }

  filter {}
end

function lcms2_files()
  files_in_dir("ext/lcms2/src", {
    "*.c", "*.h"
  })
  files { "ext/lcms/include.*.h" }
end

function harfbuzz_files()
  -- canonical hb_base_sources + hb_subset_sources + hb-ft.cc from
  -- harfbuzz src/meson.build (13.0.1); compiled subset only (no cairo,
  -- coretext, directwrite, wasm, raster, graphite, icu, tests).
  files_in_dir("ext/harfbuzz/src", {
    -- base (hb_base_sources)
    "OT/Var/VARC/VARC.cc",
    "hb-aat-layout.cc",
    "hb-aat-map.cc",
    "hb-blob.cc",
    "hb-buffer-serialize.cc",
    "hb-buffer-verify.cc",
    "hb-buffer.cc",
    "hb-common.cc",
    "hb-draw.cc",
    "hb-face-builder.cc",
    "hb-face.cc",
    "hb-fallback-shape.cc",
    "hb-font.cc",
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
    "hb-outline.cc",
    "hb-paint-bounded.cc",
    "hb-paint-extents.cc",
    "hb-paint.cc",
    "hb-set.cc",
    "hb-shape-plan.cc",
    "hb-shape.cc",
    "hb-shaper.cc",
    "hb-static.cc",
    "hb-style.cc",
    "hb-ucd.cc",
    "hb-unicode.cc",
    -- subset (hb_subset_sources)
    "graph/gsubgpos-context.cc",
    "hb-subset-cff-common.cc",
    "hb-subset-cff1.cc",
    "hb-subset-cff2-to-cff1.cc",
    "hb-subset-cff2.cc",
    "hb-subset-input.cc",
    "hb-subset-instancer-iup.cc",
    "hb-subset-instancer-solver.cc",
    "hb-subset-plan-layout.cc",
    "hb-subset-plan-var.cc",
    "hb-subset-plan.cc",
    "hb-subset-serialize.cc",
    "hb-subset-table-cff.cc",
    "hb-subset-table-color.cc",
    "hb-subset-table-layout.cc",
    "hb-subset-table-other.cc",
    "hb-subset-table-var.cc",
    "hb-subset.cc",
    -- freetype integration (hb_ft_sources)
    "hb-ft.cc",
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
    "gzip/ftgzip.c",
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
  files_in_dir("src", {
    "Accelerators.*",
    "Actions.*",
    "AvifReader.*",
    "AppSettings.*",
    "AppTools.*",
    "Canvas.*",
    "CanvasAboutUI.*",
    "ChmDump.*",
    "ChmModel.*",
    "MarkdownModel.*",
    "MarkdownToc.*",
    "AIChatCommon.*",
    "ClaudeCode.*",
    "CodexBuild.*",
    "GrokBuild.*",
    "CommandAvailability.*",
    "CommandPalette.*",
    "CommandPaletteCollect.*",
    "CommandPaletteDraw.*",
    "CommandPaletteFilter.*",
    "FilterHighlightDraw.*",
    "Commands.*",
    "CrashHandler.*",
    "ImageSaveCropResize.*",
    "DisplayMode.*",
    "DisplayModel.*",
    "DocController.h",
    "DocProperties.*",
    "EditAnnotations.*",
    "EngineDump.cpp",
    "ExifDump.*",
    "ExternalViewers.*",
    "Favorites.*",
    "FileHistory.*",
    "FileThumbnails.*",
    "Flags.*",
    "FindBar.*",
    "FindWindow.*",
    "FormFields.*",
    "GdiPlusExtFormats.*",
    "FzImgReader.h",
    "FzImgReader.cpp",
    "FzImgReader_win.cpp",
    "GlobalPrefs.*",
    "HomePage.*",
    "Installer.*",
    "InstallerCommon.cpp",
    "JxlReader.*",
    "MainWindow.*",
    "Menu.*",
    "NavFilesInFolder.*",
    "Notifications.*",
    "PdfSync.*",
    "PdfTools.*",
    "TipText.h",
    "Print.*",
    "ProgressUpdateUI.*",
    "ReadAloudHighlight.*",
    "ReadAloudPlaybackBar.*",
    "RefHover.*",
    "RefHoverCanvas.*",
    "RefHoverDetect.*",
    "RefHoverInternal.*",
    "RefHoverPopup.*",
    "RefHoverRender.*",
    "RefHoverShow.*",
    "RefHoverText.*",
    "RefHoverTextDetect.*",
    "RegistryInstaller.*",
    "RegistryPreview.*",
    "RegistrySearchFilter.*",
    "RenderCache.*",
    "resource.h",
    "SearchAndDDE.*",
    "OverlayScrollbar.*",
    "Screenshot.*",
    "Selection.*",
    "SelectionTranslate.*",
    "Settings.h",
    "SettingsStructs.*",
    "SimpleBrowserWindow.*",
    "StressTesting.*",
    "SumatraConfig.cpp",
    "SumatraControl.*",
    "SumatraDialogs.*",
    "SumatraPDF.cpp",
    "SumatraPDF.h",
    "SumatraPDF.rc",
    "SumatraProperties.*",
    "SumatraLog.*",
    "SumatraStartup.cpp",
    "SumatraTest.*",
    "SvgIcons.*",
    "TableOfContents.*",
    "Tabs.*",
    "TabGroupsManage.*",
    "Tester.*",
    "Tests.cpp",
    "TextSearch.*",
    "TextSelection.*",
    "TextToSpeech.*",
    "Theme.*",
    "Toolbar.*",
    "TranslationLangs.cpp",
    "Translations.*",
    "TreeModel.*",
    "Uninstaller.cpp",
    "UpdateCheck.*",
    "Version.h",
    "VirtWnd.*",
    "WebpReader.*",
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
  files_in_dir("src/base/tests", {
    "*.cpp",
  })
  files_in_dir("src/base", {
    "UtAssert.*",
    "Archive.*",
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

function base_files()
  files_in_dir("src/base", {
    "ApiHook.*",
    "Archive.*",
    "Arena.*",
    "Arena_win.cpp",
    "Base.h",
    "Base.cpp",
    "Base_win.cpp",
    "BitReader.*",
    "BuildConfig.h",
    "ByteOrderDecoder.*",
    "ByteReader.*",
    "ByteWriter.*",
    "CmdLineArgsIter.h",
    "CmdLineArgsIter.cpp",
    "CmdLineArgsIter_win.cpp",
    "Color.*",
    "Crypto.h",
    "Crypto_win.cpp",
    "CssParser.*",
    "DbgHelpDyn.h",
    "DbgHelpDyn_win.cpp",
    "Dict.*",
    "DirIter.h",
    "DirIter.cpp",
    "DirIter_win.cpp",
    "DirScan.h",
    "DirScan.cpp",
    "DirScan_win.cpp",
    "Dpi.h",
    "Dpi_win.cpp",
    "Exif.*",
    "File.h",
    "File.cpp",
    "File_win.cpp",
    "FileWatcher.*",
    "GdiPlus.cpp",
    "GdiPlus.h",
    "Geom.*",
    "GuessFileType.*",
    "GuessFileType_win.cpp",
    "HtmlTags.*",
    "Http.h",
    "Http.cpp",
    "Http_win.cpp",
    "JsonParser.*",
    "Log.h",
    "LzmaSimpleArchive.*",
    "Pixmap.*",
    "RegistryPaths.*",
    "Scoped.h",
    "ScopedWin.h",
    "SettingsUtil.*",
    "SquareTreeParser.*",
    "Strconv.*",
    "StrFormatParse.*",
    "StrQueue.*",
    "Str.*",
    "StrUtf8.*",
    "StrVec.*",
    "TgaReader.*",
    "TgaReader_win.cpp",
    "Thread.*",
    "TxtParser.*",
    "UITask.*",
    "Vec.h",
    "WinDynCalls.h",
    "WinDynCalls_win.cpp",
    "Win.*",
    "Zip.*",
  })
  filter { "configurations:Debug or DebugFull" }
      files_in_dir("src/base", {
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
    "EngineDjvuDec.*",
    "EngineEbook.*",
    "EngineImages.*",
    "EngineMupdf.*",
    "EngineMupdfImpl.*",
    "EnginePs.*",
    "GumboHtmlParser.*",
    "GumboHelpers.*",
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

-- cmark-gfm: markdown parser used by mupdf's source/html/md.c (FZ_ENABLE_MD).
-- Parser-only subset matching mupdf's Makelists CMARKGFM_SRC (no CLI main.c,
-- no commonmark/latex/man/xml/plaintext renderers). Generated config headers
-- (config.h, cmark-gfm_export.h, cmark-gfm_version.h) come from
-- mupdf/scripts/cmark-gfm. Build with -DCMARK_GFM_STATIC_DEFINE.
function cmark_gfm_files()
  files_in_dir("ext/cmark-gfm/src", {
    "arena.c", "blocks.c", "buffer.c", "cmark.c", "cmark_ctype.c",
    "footnotes.c", "houdini_href_e.c", "houdini_html_e.c", "houdini_html_u.c",
    "html.c", "inlines.c", "iterator.c", "linked_list.c", "map.c", "node.c",
    "plugin.c", "references.c", "registry.c", "scanners.c",
    "syntax_extension.c", "utf8.c",
  })
  files_in_dir("ext/cmark-gfm/extensions", {
    "autolink.c", "core-extensions.c", "ext_scanners.c", "strikethrough.c",
    "table.c", "tagfilter.c", "tasklist.c", "autoheaderid.c",
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
    "cull-device.c",
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
    "options.c",
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
    "md.c",
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
    "pdf-struct.c",
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
    "mupdf/source/helpers/pkcs7/pkcs7-windows.c",
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
    "src/base/Base.h",
    "src/base/Base.cpp",
    "src/base/Base_win.cpp",
    "src/base/BitManip.h",
    "src/base/Dict*",
    "src/base/Str.*",
    "src/base/StrUtf8.*",
    "tools/efi/*.cpp",
    "tools/efi/*.h",
  }
end

function test_util_files()
  files_in_dir("src/base", {
    "Arena.*",
    "Arena_win.cpp",
    "Base.h",
    "Base.cpp",
    "Base_win.cpp",
    "BitManip.*",
    "ByteOrderDecoder.*",
    "CmdLineArgsIter.h",
    "CmdLineArgsIter.cpp",
    "CmdLineArgsIter_win.cpp",
    "Color.*",
    "Crypto.h",
    "Crypto_win.cpp",
    "CssParser.*",
    "Dict.*",
    "DbgHelpDyn.h",
    "DbgHelpDyn_win.cpp",
    "DirScan.h",
    "DirScan.cpp",
    "DirScan_win.cpp",
    "Dpi.h",
    "Dpi_win.cpp",
    "File.h",
    "File.cpp",
    "File_win.cpp",
    "Geom.*",
    "HtmlTags.*",
    "JsonParser.*",
    "Log.h",
    "Pixmap.*",
    "Scoped.*",
    "SettingsUtil.*",
    "SquareTreeParser.*",
    "Strconv.*",
    "StrFormatParse.*",
    "StrQueue.*",
    "Str.*",
    "StrUtf8.*",
    "StrVec.*",
    "Thread.*",
    "tests/*",
    "UtAssert.*",
    "Vec.*",
    "WinDynCalls.h",
    "WinDynCalls_win.cpp",
    "Win.*",
  })
  files_in_dir("src", {
    --"AppTools.*",
    "Commands.*",
    "CrashHandlerNoOp.cpp",
    "DisplayMode.*",
    "Flags.*",
    "RefHoverDetect.*",
    "RefHoverTextDetect.*",
    "SettingsStructs.*",
    --"StressTesting.*",
    "SumatraConfig.*",
    "SumatraLog.*",
    "SumatraUnitTests.cpp",
    "SimpleLog_ut.cpp",
    "tools/test_util.cpp"
  })
end

function test_engines_files()
  files {
    "src/base/GuessFileType.cpp",
    "src/DocProperties.cpp",
    "src/DocProperties.h",
    "src/EngineAll.h",
    "src/EngineBase.cpp",
    "src/EngineBase.h",
    "src/EngineDjvuDec.cpp",
    "src/TreeModel.cpp",
    "src/TreeModel.h",
    "src/tools/test_engines.cpp",
  }
end

function pdf_preview_files()
  files_in_dir("src/previewer", {
    "PdfPreview.*",
    "PdfPreviewDll.cpp",
  })
  files_in_dir("src/base", {
    "Archive.*",
  })
  files_in_dir("src", {
    "ChmFile.*",
    "CrashHandlerNoOp.cpp",
    "DocProperties.*",
    "EbookDoc.*",
    "EbookFormatter.*",
    "EngineAll.h",
    "EngineBase.*",
    "EngineDjvuDec.*",
    "EngineEbook.*",
    "EngineImages.*",
    "EngineMupdf.*",
    "EngineMupdfImpl.*",
    "AvifReader.*",
    "FzImgReader.h",
    "FzImgReader.cpp",
    "FzImgReader_win.cpp",
    "GdiPlusExtFormats.*",
    "GumboHtmlParser.*",
    "GumboHelpers.*",
    "HtmlFormatter.*",
    "JxlReader.*",
    "MobiDoc.*",
    "mui/Mui.*",
    "mui/TextRender.*",
    "MUPDF_Exports.cpp",
    "PalmDbReader.*",
    "PdfCreator.*",
    "RegistryPreview.*",
    "SumatraConfig.*",
    "SumatraLog.*",
    "WebpReader.*",
  })
end

function search_filter_files()
  files_in_dir("src/ifilter", {
    "CPdfFilter.*",
    "FilterBase.h",
    "PdfFilter.*",
    "SearchFilterDll.cpp",
  })
  files_in_dir("src/base", {
    "Archive.*",
  })
  files_in_dir("src", {
    "CrashHandlerNoOp.cpp",
    "DocProperties.*",
    "EbookDoc.*",
    "EngineAll.h",
    "EngineBase.*",
    "EngineMupdf.*",
    "EngineMupdfImpl.*",
    "GumboHtmlParser.*",
    "GumboHelpers.*",
    "MobiDoc.*",
    "MUPDF_Exports.cpp",
    "PalmDbReader.*",
    "RegistrySearchFilter.*",
    "SumatraLog.*",
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
    "PdfPreviewDll.cpp",
  })

  files_in_dir("src", {
    "CrashHandlerNoOp.cpp",
    "mui/Mui.*",
    "RegistryPreview.*",
    "SumatraConfig.*",
    "base/Base.*",
    "base/Color.*",
    "base/Dpi.h",
    "base/Dpi_win.cpp",
    "base/File.h",
    "base/File.cpp",
    "base/File_win.cpp",
    "base/Geom.*",
    "base/Log.h",
    "base/LogNoOp.cpp",
    "base/Strconv.*",
    "base/StrFormatParse.*",
    "base/Str.*",
    "base/StrVec.*",
    "base/WinDynCalls.h",
    "base/WinDynCalls_win.cpp",
    "base/Win.*",
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
    "base/Base.*",
    "base/Color.*",
    "base/Dpi.h",
    "base/Dpi_win.cpp",
    "base/File.h",
    "base/File.cpp",
    "base/File_win.cpp",
    "base/Geom.*",
    "base/Log.h",
    "base/LogNoOp.cpp",
    "base/Strconv.*",
    "base/StrFormatParse.*",
    "base/Str.*",
    "base/StrVec.*",
    "base/WinDynCalls.h",
    "base/WinDynCalls_win.cpp",
    "base/Win.*",
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

-- highway (SIMD library, dependency of libjxl). Only the core runtime sources;
-- the rest is header-only.
function highway_files()
  files { "ext/highway/hwy/*.cc", "ext/highway/hwy/*.h", "ext/highway/hwy/ops/*.h" }
end

-- skcms (color management, dependency of libjxl). Baseline only (HSW/SKX
-- disabled), so no per-file /arch flags are needed.
function skcms_files()
  files {
    "ext/skcms/skcms.cc",
    "ext/skcms/skcms.h",
    "ext/skcms/src/skcms_TransformBaseline.cc",
    "ext/skcms/src/*.h",
  }
end

-- libjxl decoder. ext/libjxl/lib/jxl only contains the decoder subset we
-- vendored (see ext/versions.txt), so globbing it picks exactly those files.
function libjxl_files()
  files { "ext/libjxl/lib/jxl/**.cc", "ext/libjxl/lib/jxl/**.h", "ext/libjxl/lib/include/jxl/*.h" }
end

function sumatrapdf_tool_files()
  files_in_dir("src", {
    "sumatrapdf-tool.cpp"
  })
end
