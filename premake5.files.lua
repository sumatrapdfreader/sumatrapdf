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
    "ColorUtil.*",
    "CmdLineParser.*",
    "Dpi.*",
    "FileUtil.*",
    "LzmaSimpleArchive.*",
    "StrconvUtil.*",
    "StringViewUtil.*",
    "StrUtil.*",
    "StrUtil_win.*",
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
    "list.*",
    "match.*",
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

function openjpeg_files()
  files_in_dir( "ext/openjpeg/src/lib/openjp2", {
    "bio.c",
    "cidx_manager.c",
    "cio.c",
    "dwt.c",
    "event.c",
    "function_list.c",
    "image.c",
    "invert.c",
    "j2k.c",
    "jp2.c",
    "mct.c",
    "mqc.c",
    "openjpeg.c",
    "opj_clock.c",
    "opj_malloc.c",
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

function libwebp_files()
  files("ext/libwebp/src/dec/*.c")

  files_in_dir("ext/libwebp/src/dsp", {
    "alpha_processing.c",
    "alpha_processing_sse2.c",
    "alpha_processing_sse41.c",
    "cost.c",
    "cpu.c",
    "dec.c",
    "dec_clip_tables.c",
    "dec_sse2.c",
    "dec_sse41.c",
    "filters.c",
    "filters_sse2.c",
    "lossless.c",
    "lossless_sse2.c",
    "rescaler.c",
    "rescaler_sse2.c",
    "ssim.c",
    "ssim_sse2.c",
    "upsampling.c",
    "upsampling_sse2.c",
    "upsampling_sse41.c",
    "yuv.c",
    "yuv_sse2.c",
    "yuv_sse41.c",
  })

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
  })

  --to build non-assembly version, use this:
  --files {"ext/libjpeg-turbo/jsimd_none.c"}

  filter {'platforms:x32 or x32_xp or x32_asan'}
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

  filter {'platforms:x64 or x64_ramicro'}
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
    "hb-ot-shape-complex-arabic.cc",
    "hb-ot-shape-complex-default.cc",
    "hb-ot-shape-complex-hangul.cc",
    "hb-ot-shape-complex-hebrew.cc",
    "hb-ot-shape-complex-indic-table.cc",
    "hb-ot-shape-complex-indic.cc",
    "hb-ot-shape-complex-khmer.cc",
    "hb-ot-shape-complex-myanmar.cc",
    "hb-ot-shape-complex-thai.cc",
    "hb-ot-shape-complex-use-table.cc",
    "hb-ot-shape-complex-use.cc",
    "hb-ot-shape-complex-vowel-constraints.cc",
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
  files_in_dir("src", {
    "AppColors.*",
    "AppPrefs.*",
    "AppTools.*",
    "AppUtil.*",
    "Caption.*",
    "Canvas.*",
    "ChmModel.*",
    "CrashHandler.*",
    "DisplayModel.*",
    "Doc.*",
    "EbookController.*",
    "EbookControls.*",
    "ExternalViewers.*",
    "Favorites.*",
    "FileHistory.*",
    "FileThumbnails.*",
    "Flags.*",
    "GetDocumentOutlines.*",
    "GlobalPrefs.*",
    "Installer.h",
    "Installer.cpp",
    "InstUninstCommon.cpp",
    "Uninstaller.cpp",
    "Menu.*",
    "MuiEbookPageDef.*",
    "Notifications.*",
    "PagesLayoutDef.*",
    "ParseBKM.*",
    "PdfSync.*",
    "Print.*",
    "ProgressUpdateUI.*",
    "RenderCache.*",
    "Search.*",
    "Selection.*",
    "SettingsStructs.*",
    "SumatraPDF.cpp",
    "SumatraPDF.rc",
    "SumatraStartup.cpp",
    "SumatraConfig.cpp",
    "SumatraAbout.*",
    "SumatraAbout2.*",
    "SumatraDialogs.*",
    "SumatraProperties.*",
    "StressTesting.*",
    "TabInfo.*",
    "TableOfContents.*",
    "Tabs.*",
    "Tester.*",
    "TextSearch.*",
    "TextSelection.*",
    "Theme.*",
    "TocEditor.*",
    "TocEditTitle.*",
    "Toolbar.*",
    "Translations.*",
    "Trans_sumatra_txt.cpp",
    "Version.h",
    "WindowInfo.*",

    "Tests.cpp",
    "regress/Regress.*",
  })
  test_app_files_2()
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

function utils_files()
  files_in_dir("src/utils", {
    "ApiHook.*",
    "Archive.*",
    "BaseUtil.*",
    "BitReader.*",
    "BuildConfig.h",
    "ByteOrderDecoder.*",
    "ByteWriter.*",
    "CmdLineParser.*",
    "ColorUtil.*",
    "CryptoUtil.*",
    "CssParser.*",
    "DbgHelpDyn.*",
    "Dict.*",
    "DirIter.*",
    "Dpi.*",
    "FileTypeSniff.*",
    "FileUtil.*",
    "FileWatcher.*",
    "FzImgReader.*",
    "GdiPlusUtil.*",
    "HtmlWindow.*",
    "HtmlParserLookup.*",
    "HtmlPullParser.*",
    "HtmlPrettyPrint.*",
    "HttpUtil.*",
    "JsonParser.*",
    "LzmaSimpleArchive.*",
    "PEB.h",
    "RegistryPaths.*",
    "Scoped.h",
    "ScopedWin.h",
    "SerializeTxt.*",
    "SettingsUtil.*",
    "Log.*",
    "StrconvUtil.*",
    "StrFormat.*",
    "StringViewUtil.*",
    "StrSlice.*",
    "StrUtil.*",
    "StrUtil_win.cpp",
    "SquareTreeParser.*",
    "ThreadUtil.*",
    "TgaReader.*",
    "TrivialHtmlParser.*",
    "TxtParser.*",
    "UITask.*",
    "WebpReader.*",
    "WinDynCalls.*",
    "WinUtil.*",
    "ZipUtil.*",
  })

  files_in_dir("src/wingui", {
    "*.h",
    "*.cpp",
  })
end

function mui_files()
  files_in_dir("src/mui", {
    "MuiBase.*",
    "Mui.*",
    "MuiCss.*",
    "MuiLayout.*",
    "MuiPainter.*",
    "MuiControl.*",
    "MuiButton.*",
    "MuiScrollBar.*",
    "MuiEventMgr.*",
    "MuiHwndWrapper.*",
    "MuiGrid.*",
    "SvgPath.*",
    "MuiDefs.*",
    "MuiFromText.*",
    "TextRender.*",
  })
end

function engines_files()
  files_in_dir("src", {
    "Engine*",
    "ChmDoc.*",
    "EbookDoc.*",
    "EbookFormatter.*",
    "FileModifications.*",
    "HtmlFormatter.*",
    "MobiDoc.*",
    "ParseBKM.*",
    "PdfCreator.*",

    "utils/PalmDbReader.*",
  })
end

function mupdf_files()
  --[[ files {
    "mupdf/font_base14.asm",
  }
  --]]

  files { "ext/mupdf_load_system_font.c" }

  filter {"platforms:x64 or x64_ramicro"}
    files {
      "mupdf/fonts_64.asm",
    }
  filter {}

  filter {"platforms:x32 or x32_xp or x32_asan"}
    files {
      "mupdf/fonts_32.asm",
    }
  filter {}

  files_in_dir("mupdf/source/cbz", {
    "mucbz.c",
    "muimg.c",
  })

  files { "mupdf/source/fitz/*.h" }
  files_in_dir("mupdf/source/fitz", {
    "archive.c",
    "bbox-device.c",
    "bidi.c",
    "bidi-std.c",
    "bitmap.c",
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
    "encodings.c",
    "error.c",
    "filter-basic.c",
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
    "halftone.c",
    "harfbuzz.c",
    "hash.c",
    "image.c",
    "jmemcust.c",
    "link.c",
    "list-device.c",
    "load-bmp.c",
    "load-gif.c",
    "load-jbig2.c",
    "load-jpeg.c",
    "load-jpx.c",
    "load-jxr.c",
    "load-png.c",
    "load-pnm.c",
    "load-tiff.c",
    "memento.c",
    "memory.c",
    "noto.c",
    "outline.c",
    "output.c",
    "output-cbz.c",
    "output-pcl.c",
    "output-pclm.c",
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
    "separation.c",
    "shade.c",
    "stext-device.c",
    "stext-output.c",
    "stext-search.c",
    "store.c",
    "stream-open.c",
    "stream-read.c",
    "string.c",
    "strtof.c",
    "svg-device.c",
    "test-device.c",
    "text.c",
    "time.c",
    "trace-device.c",
    "track-usage.c",
    "transition.c",
    "tree.c",
    "ucdn.c",
    "untar.c",
    "unzip.c",
    "util.c",
    "writer.c",
    "xml.c",
    "zip.c",
  })

  files_in_dir("mupdf/source/html", {
    "css-apply.c",
    "css-parse.c",
    "epub-doc.c",
    "html-doc.c",
    "html-font.c",
    "html-layout.c",
    "html-outline.c",
    "html-parse.c",
  })

  files_in_dir("mupdf/source/pdf", {
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
    "pdf-interpret.c",
    "pdf-js.c",
    "pdf-layer.c",
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
    "pdf-repair.c",
    "pdf-resources.c",
    "pdf-run.c",
    "pdf-shade.c",
    "pdf-signature.c",
    "pdf-store.c",
    "pdf-stream.c",
    "pdf-type3.c",
    "pdf-unicode.c",
    "pdf-util.c",
    "pdf-write.c",
    "pdf-xobject.c",
    "pdf-xref.c",
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
    "src/utils/BitManip.h",
    "src/utils/Dict*",
    "src/utils/StrUtil.*",
    "src/utils/StrUtil_win.cpp",
  }
end

function test_util_files()
  files_in_dir( "src/utils", {
    "BaseUtil.*",
    "BitManip.*",
    "ByteOrderDecoder.*",
    "CmdLineParser.*",
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
    "StringViewUtil.*",
    "StrUtil.*",
    "StrUtil_win.cpp",
    "SquareTreeParser.*",
    "TrivialHtmlParser.*",
    "UtAssert.*",
    --"VarintGob*",
    "Vec.*",
    "WinUtil.*",
    "WinDynCalls.*",
    "tests/*"
  })
  files_in_dir("src", {
    --"AppTools.*",
    --"StressTesting.*",
    "AppUtil.*",
    "Flags.*",
    "SumatraConfig.*",
    "SettingsStructs.*",
    "UnitTests.cpp",
    "mui/SvgPath*",
    "tools/test_util.cpp"
  })
end

function engine_dump_files()
  files_in_dir("src", {
    "EngineDump.cpp",
    "mui/MiniMui.*",
    "mui/TextRender.*"
  })
end

function pdf_preview_files()
  files_in_dir("src/previewer", {
    "PdfPreview.*",
    "PdfPreviewDll.cpp",
  })
  files {
    "src/MUPDF_Exports.cpp",
    "src/EngineBase.*",
    "src/EngineFzUtil.*",
    "src/EnginePdf.*",
    "src/EngineXps.*"
  }

  filter {"configurations:Debug"}
    files_in_dir("src", {
      "utils/PalmDbReader.*",
      "mui/MiniMui.*",
      "mui/TextRender.*",
      "ChmDoc.*",
      "EbookDoc.*",
      "EngineEbook.*",
      "EngineDjVu.*",
      "EngineImages.*",
      "EbookFormatter.*",
      "HtmlFormatter.*",
      "MobiDoc.*",
      "PdfCreator.*",
      "SumatraConfig.*",
    })
  filter {}
end

function pdf_filter_files()
  files_in_dir("src/ifilter", {
    "PdfFilter.*",
    "PdfFilterDll.cpp",
    "CPdfFilter.*",
    "FilterBase.h",
  })
  files {
    "src/MUPDF_Exports.cpp",
    "src/EngineBase.*",
    "src/EngineFzUtil.*",
    "src/EnginePdf.*",
    "src/EngineXps.*"
  }

  filter {"configurations:Debug"}
    files_in_dir("src/ifilter", {
      "CTeXFilter.*",
      "CEpubFilter.*",
    })
    files {
      "src/EbookDoc.*",
      "src/MobiDoc.*",
      "src/utils/PalmDbReader.*",
    }
  filter {}
end

function test_app_files_2()
  files_in_dir("tools/test-app", {
    "TestApp.cpp",
    "TestDirectDraw.cpp",
    "TestTab.cpp",
    "TestLayout.cpp",
  })
end

function test_app_files()
  files_in_dir("tools/test-app", {
    "resource.h",
    "small.ico",
    "targetver.h",
    "test-app.cpp",
    "test-app.h",
    "test-app.ico",
    "test-app.rc",
    "TestDirectDraw.cpp",
    "TestTab.cpp",
    "TestLayout.cpp",
  })

  files_in_dir("src/utils", {
    "BaseUtil.*",
    "ColorUtil.*",
    "Dpi.*",
    "FileUtil.*",
    "Scoped.h",
    "Log.*",
    "StrconvUtil.*",
    "StringViewUtil.*",
    "StrUtil.*",
    "StrUtil_win.cpp",
    "WinDynCalls.*",
    "WinUtil.*",
  })

  files_in_dir("src/wingui", {
    "*.h",
    "*.cpp",
  })
end
