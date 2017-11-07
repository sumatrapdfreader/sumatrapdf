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
    "BaseUtil.cpp",
    "ByteOrderDecoder.cpp",
    "StrUtil.cpp",
    "FileUtil.cpp",
    "CmdLineParser.cpp",
    "LzmaSimpleArchive.cpp",
  })

  files {
    "src/tools/MakeLzSA.cpp",
  }
end

function zlib_files()
  files_in_dir("ext/zlib", {
    "adler32.c",
    "compress.c",
    "crc32.c",
    "deflate.c",
    "inffast.c",
    "inflate.c",
    "inftrees.c",
    "trees.c",
    "zutil.c",
    "gzlib.c",
    "gzread.c",
    "gzwrite.c",
    "gzclose.c",
  })
end

function libdjvu_files()
  files_in_dir("ext/libdjvu", {
    "Arrays.cpp",
    "atomic.cpp",
    "BSByteStream.cpp",
    "BSEncodeByteStream.cpp",
    "ByteStream.cpp",
    "DataPool.cpp",
    "DjVmDir0.cpp",
    "DjVmDoc.cpp",
    "DjVmNav.cpp",
    "DjVuAnno.cpp",
    "DjVuDocEditor.cpp",
    "DjVuDocument.cpp",
    "DjVuDumpHelper.cpp",
    "DjVuErrorList.cpp",
    "DjVuFile.cpp",
    "DjVuFileCache.cpp",
    "DjVuGlobal.cpp",
    "DjVuGlobalMemory.cpp",
    "DjVuImage.cpp",
    "DjVuInfo.cpp",
    "DjVuMessage.cpp",
    "DjVuMessageLite.cpp",
    "DjVuNavDir.cpp",
    "DjVuPalette.cpp",
    "DjVuPort.cpp",
    "DjVuText.cpp",
    "DjVuToPS.cpp",
    "GBitmap.cpp",
    "GContainer.cpp",
    "GException.cpp",
    "GIFFManager.cpp",
    "GMapAreas.cpp",
    "GOS.cpp",
    "GPixmap.cpp",
    "GRect.cpp",
    "GScaler.cpp",
    "GSmartPointer.cpp",
    "GString.cpp",
    "GThreads.cpp",
    "GUnicode.cpp",
    "GURL.cpp",
    "IFFByteStream.cpp",
    "IW44EncodeCodec.cpp",
    "IW44Image.cpp",
    "JB2EncodeCodec.cpp",
    "DjVmDir.cpp",
    "JB2Image.cpp",
    "JPEGDecoder.cpp",
    "MMRDecoder.cpp",
    "MMX.cpp",
    "UnicodeByteStream.cpp",
    "XMLParser.cpp",
    "XMLTags.cpp",
    "ZPCodec.cpp",
    "ddjvuapi.cpp",
    "debug.cpp",
    "miniexp.cpp",
  })
end

function unarr_files()
  files {
    "ext/unarr/common/conv.c",
    "ext/unarr/common/crc32.c",
    "ext/unarr/common/stream.c",
    "ext/unarr/common/unarr.c",

    "ext/unarr/lzmasdk/CpuArch.c",
    "ext/unarr/lzmasdk/Ppmd7.c",
    "ext/unarr/lzmasdk/Ppmd7Dec.c",
    "ext/unarr/lzmasdk/Ppmd8.c",
    "ext/unarr/lzmasdk/Ppmd8Dec.c",

    "ext/unarr/rar/filter-rar.c",
    "ext/unarr/rar/parse-rar.c",
    "ext/unarr/rar/rar.c",
    "ext/unarr/rar/rarvm.c",
    "ext/unarr/rar/uncompress-rar.c",
    "ext/unarr/rar/huffman-rar.c",

    "ext/unarr/zip/parse-zip.c",
    "ext/unarr/zip/uncompress-zip.c",
    "ext/unarr/zip/zip.c",
    "ext/unarr/zip/inflate.c",

    "ext/unarr/_7z/_7z.c",

    "ext/unarr/tar/tar.c",
    "ext/unarr/tar/parse-tar.c",

    "ext/bzip2/bzip_all.c",

    "ext/lzma/C/LzmaDec.c",
    "ext/lzma/C/Bra86.c",
    "ext/lzma/C/LzmaEnc.c",
    "ext/lzma/C/LzFind.c",
    "ext/lzma/C/LzFindMt.c",
    "ext/lzma/C/Threads.c",
    "ext/lzma/C/7zBuf.c",
    "ext/lzma/C/7zDec.c",
    "ext/lzma/C/7zIn.c",
    "ext/lzma/C/7zStream.c",
    "ext/lzma/C/Bcj2.c",
    "ext/lzma/C/Bra.c",
    "ext/lzma/C/Lzma2Dec.c",
  }
end

function unarr_no_bzip_files()
  files {
    "ext/unarr/common/conv.c",
    "ext/unarr/common/crc32.c",
    "ext/unarr/common/stream.c",
    "ext/unarr/common/unarr.c",

    "ext/unarr/lzmasdk/CpuArch.c",
    "ext/unarr/lzmasdk/Ppmd7.c",
    "ext/unarr/lzmasdk/Ppmd7Dec.c",
    "ext/unarr/lzmasdk/Ppmd8.c",
    "ext/unarr/lzmasdk/Ppmd8Dec.c",

    "ext/unarr/rar/filter-rar.c",
    "ext/unarr/rar/parse-rar.c",
    "ext/unarr/rar/rar.c",
    "ext/unarr/rar/rarvm.c",
    "ext/unarr/rar/uncompress-rar.c",
    "ext/unarr/rar/huffman-rar.c",

    "ext/unarr/zip/parse-zip.c",
    "ext/unarr/zip/uncompress-zip.c",
    "ext/unarr/zip/zip.c",
    "ext/unarr/zip/inflate.c",

    "ext/unarr/_7z/_7z.c",

    "ext/unarr/tar/tar.c",
    "ext/unarr/tar/parse-tar.c",

    "ext/lzma/C/LzmaDec.c",
    "ext/lzma/C/Bra86.c",
    "ext/lzma/C/LzmaEnc.c",
    "ext/lzma/C/LzFind.c",
    "ext/lzma/C/LzFindMt.c",
    "ext/lzma/C/Threads.c",
    "ext/lzma/C/7zBuf.c",
    "ext/lzma/C/7zDec.c",
    "ext/lzma/C/7zIn.c",
    "ext/lzma/C/7zStream.c",
    "ext/lzma/C/Bcj2.c",
    "ext/lzma/C/Bra.c",
    "ext/lzma/C/Lzma2Dec.c",
  }
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
    "jbig2_metadata.c",
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
    "*.c",
    "*.h",
  })
end

function libwebp_files()
  files_in_dir("ext/libwebp", {
    "dec/alpha.c",
    "dec/buffer.c",
    "dec/frame.c",
    "dec/idec.c",
    "dec/io.c",
    "dec/quant.c",
    "dec/tree.c",
    "dec/vp8.c",
    "dec/vp8l.c",
    "dec/webp.c",

    "dsp/alpha_processing.c",
    "dsp/cpu.c",
    "dsp/dec.c",
    "dsp/dec_sse2.c",
    "dsp/lossless.c",
    "dsp/lossless_sse2.c",
    "dsp/upsampling.c",
    "dsp/upsampling_sse2.c",
    "dsp/yuv.c",
    "dsp/yuv_sse2.c",
    "dsp/dec_clip_tables.c",
    "dsp/alpha_processing_sse2.c",

    "utils/bit_reader.c",
    "utils/color_cache.c",
    "utils/filters.c",
    "utils/huffman.c",
    "utils/quant_levels_dec.c",
    "utils/rescaler.c",
    "utils/thread.c",
    "utils/utils.c",
    "utils/random.c",
  })
end

function libjpeg_turbo_files()
  files_in_dir("ext/libjpeg-turbo", {
    "jcomapi.c",
    "jdapimin.c",
    "jdapistd.c",
    "jdatadst.c",
    "jdatasrc.c",
    "jdcoefct.c",
    "jdcolor.c",
    "jddctmgr.c",
    "jdhuff.c",
    "jdinput.c",
    "jdmainct.c",
    "jdmarker.c",
    "jdmaster.c",
    "jdmerge.c",
    "jdpostct.c",
    "jdsample.c",
    "jdtrans.c",
    "jerror.c",
    "jfdctflt.c",
    "jfdctint.c",
    "jidctflt.c",
    "jidctfst.c",
    "jidctint.c",
    "jquant1.c",
    "jquant2.c",
    "jutils.c",
    "jmemmgr.c",
    "jmemnobs.c",
    "jaricom.c",
    "jdarith.c",
    "jfdctfst.c",
    "jdphuff.c",
    "jidctred.c",
  })

  --to build non-assembly version, use this:
  --files {"ext/libjpeg-turbo/jsimd_none.c"}

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

  filter {'platforms:x64'}
    files_in_dir("ext/libjpeg-turbo/simd", {
      "jfsseflt-64.asm", "jccolss2-64.asm", "jdcolss2-64.asm", "jcgrass2-64.asm",
    	"jcsamss2-64.asm", "jdsamss2-64.asm", "jdmerss2-64.asm", "jcqnts2i-64.asm",
    	"jfss2fst-64.asm", "jfss2int-64.asm", "jiss2red-64.asm", "jiss2int-64.asm",
    	"jiss2fst-64.asm", "jcqnts2f-64.asm", "jiss2flt-64.asm",
    })
    files {"ext/libjpeg-turbo/simd/jsimd_x86_64.c"}

  filter {}

end

function freetype_files()
  files_in_dir("ext/freetype2/src/base", {
    "ftbase.c",
    "ftbbox.c",
    "ftbitmap.c",
    "ftgasp.c",
    "ftglyph.c",
    "ftinit.c",
    "ftstroke.c",
    "ftsynth.c",
    "ftsystem.c",
    "fttype1.c",
    "ftxf86.c",
    "ftotval.c",
  })

  files_in_dir("ext/freetype2/src", {
    "cff/cff.c",
    "cid/type1cid.c",
    "psaux/psaux.c",
    "psnames/psnames.c",
    "smooth/smooth.c",
    "sfnt/sfnt.c",
    "truetype/truetype.c",
    "type1/type1.c",
    "raster/raster.c",
    "otvalid/otvalid.c",
    "pshinter/pshinter.c",
    "gzip/ftgzip.c",
  })

  filter "configurations:Debug*"
    files { "ext/freetype2/src/base/ftdebug.c" }
  filter {}

end

function sumatra_files()
  files_in_dir("src", {
    "AppPrefs.*",
    "AppTools.*",
    "AppUtil.*",
    "Caption.*",
    "Canvas.*",
    "ChmModel.*",
    "Colors.*",
    "CrashHandler.*",
    "DisplayModel.*",
    "Doc.*",
    "EbookController.*",
    "EbookControls.*",
    "ExternalViewers.*",
    "Favorites.*",
    "FileHistory.*",
    "FileThumbnails.*",
    "GlobalPrefs.*",
    "Menu.*",
    "MuiEbookPageDef.*",
    "Notifications.*",
    "PagesLayoutDef.*",
    "ParseCommandLine.*",
    "PdfSync.*",
    "Print.*",
    "RenderCache.*",
    "Search.*",
    "Selection.*",
    "SettingsStructs.*",
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
    "Toolbar.*",
    "Translations.*",
    "Trans_sumatra_txt.cpp",
    "Version.h",
    "WindowInfo.*",

    "regress/Regress.*",
  })
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
    "ArchUtil.*",
    "BaseUtil.*",
    "BitReader.*",
    "BuildConfig.h",
    "ByteOrderDecoder.*",
    "CmdLineParser.*",
    "CryptoUtil.*",
    "CssParser.*",
    "DbgHelpDyn.*",
    "DebugLog.*",
    "Dict.*",
    "DirIter.*",
    "Dpi.*",
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
    "SerializeTxt.*",
    "SettingsUtil.*",
    "StrUtil.*",
    "StrFormat.*",
    "StrSlice.*",
    "SquareTreeParser.*",
    "ThreadUtil.*",
    "TgaReader.*",
    "TrivialHtmlParser.*",
    "TxtParser.*",
    "UITask.*",
    "ZipUtil.*",
    "WebpReader.*",
    "WinDynCalls.*",
    "WinUtil.*",
  })

  files_in_dir("src/wingui", {
    "DialogSizer.*",
    "EditCtrl.*",
    "FrameRateWnd.*",
    "LabelWithCloseWnd.*",
    "SplitterWnd.*",
    --"TabsCtrl.*",
    "Win32Window.*",
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
    "ChmDoc.*",
    "DjVuEngine.*",
    "EbookDoc.*",
    "EbookEngine.*",
    "EbookFormatter.*",
    "EngineManager.*",
    "FileModifications.*",
    "HtmlFormatter.*",
    "ImagesEngine.*",
    "MobiDoc.*",
    "PdfCreator.*",
    "PdfEngine.*",
    "PsEngine.*",

    "utils/PalmDbReader.*",
  })
end

function mupdf_files()
  files {
    "mupdf/font_base14.asm",
  }

  files_in_dir("mupdf/source/fitz", {
    "bbox-device.c",
    "bitmap.c",
    "buffer.c",
    "colorspace.c",
    "compressed-buffer.c",
    "context.c",
    "crypt-aes.c",
    "crypt-arc4.c",
    "crypt-md5.c",
    "crypt-sha2.c",
    "device.c",
    "error.c",
    "filter-basic.c",
    "filter-dct.c",
    "filter-fax.c",
    "filter-flate.c",
    "filter-jbig2.c",
    "filter-lzw.c",
    "filter-predict.c",
    "font.c",
    "function.c",
    "geometry.c",
    "getopt.c",
    "halftone.c",
    "hash.c",
    "image.c",
    "link.c",
    "list-device.c",
    "load-jpeg.c",
    "load-jpx.c",
    "load-jxr.c",
    "load-png.c",
    "load-tiff.c",
    "memory.c",
    "outline.c",
    "output.c",
    "path.c",
    "pixmap.c",
    "shade.c",
    "stext-device.c",
    "stext-output.c",
    "stext-paragraph.c",
    "stext-search.c",
    "store.c",
    "stream-open.c",
    "stream-read.c",
    "string.c",
    "text.c",
    "time.c",
    "trace-device.c",
    "transition.c",
    "ucdn.c",
    "xml.c",
    "glyph.c",
    "tree.c",
    "document.c",
    "filter-leech.c",
    "printf.c",
    "strtod.c",
    "ftoa.c",
    "unzip.c",
    "draw-affine.c",
    "draw-blend.c",
    "draw-device.c",
    "draw-edge.c",
    "draw-glyph.c",
    "draw-mesh.c",
    "draw-paint.c",
    "draw-path.c",
    "draw-scale-simple.c",
    "draw-unpack.c",
  })

  files_in_dir("mupdf/source/pdf", {
    "pdf-annot.c",
    "pdf-cmap-load.c",
    "pdf-cmap-parse.c",
    "pdf-cmap-table.c",
    "pdf-cmap.c",
    "pdf-colorspace.c",
    "pdf-crypt.c",
    "pdf-device.c",
    "pdf-encoding.c",
    "pdf-event.c",
    "pdf-field.c",
    "pdf-font.c",
    "pdf-fontfile.c",
    "pdf-form.c",
    "pdf-ft-tools.c",
    "pdf-function.c",
    "pdf-image.c",
    "pdf-interpret.c",
    "pdf-lex.c",
    "pdf-metrics.c",
    "pdf-nametree.c",
    "pdf-object.c",
    "pdf-outline.c",
    "pdf-page.c",
    "pdf-parse.c",
    "pdf-pattern.c",
    "pdf-pkcs7.c",
    "pdf-repair.c",
    "pdf-shade.c",
    "pdf-store.c",
    "pdf-stream.c",
    "pdf-type3.c",
    "pdf-unicode.c",
    "pdf-write.c",
    "pdf-xobject.c",
    "pdf-xref-aux.c",
    "pdf-xref.c",
    "pdf-appearance.c",
    "pdf-run.c",
    "pdf-op-run.c",
    "pdf-op-buffer.c",
    "pdf-op-filter.c",
    "pdf-clean.c",
    "pdf-annot-edit.c",
  })

  files_in_dir("mupdf/source/xps", {
    "xps-common.c",
    "xps-doc.c",
    "xps-glyphs.c",
    "xps-gradient.c",
    "xps-image.c",
    "xps-outline.c",
    "xps-path.c",
    "xps-resource.c",
    "xps-tile.c",
    "xps-util.c",
    "xps-zip.c",
  })

  files {
    "mupdf/source/pdf/js/pdf-js-none.c",
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

function sumatrapdf_files()
  files {
    "src/SumatraPDF.cpp",
    "src/SumatraStartup.cpp",
    "src/Tests.cpp",
    "src/SumatraPDF.rc",
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
    "tools/efi/*.h",
    "tools/efi/*.cpp",
    "src/utils/BaseUtil*",
    "src/utils/BitManip.h",
    "src/utils/Dict*",
    "src/utils/StrUtil*",
  }
end

function test_util_files()
  files_in_dir( "src/utils", {
    "BaseUtil*",
    "BitManip*",
    "ByteOrderDecoder*",
    "CmdLineParser*",
    "CryptoUtil*",
    "CssParser*",
    "Dict*",
    "DebugLog*",
    "FileUtil*",
    "GeomUtil.*",
    "HtmlParserLookup*",
    "HtmlPrettyPrint*",
    "HtmlPullParser*",
    "JsonParser*",
    "Scoped.*",
    "SettingsUtil*",
    "SimpleLog*",
    "StrFormat*",
    "StrUtil*",
    "SquareTreeParser*",
    "TrivialHtmlParser*",
    "UtAssert*",
    "VarintGob*",
    "Vec.*",
    "WinUtil*",
    "WinDynCalls.*",
    "tests/*"
  })
  files_in_dir("src", {
    --"AppTools.*",
    --"StressTesting.*",
    "AppUtil.*",
    "ParseCommandLine.*",
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
    "mui/TextRender.*",
  })
end

function pdf_preview_files()
  files_in_dir("src/previewer", {
    "PdfPreview.*",
    "PdfPreviewDll.cpp",
  })
  files { "src/MUPDF_Exports.cpp", "src/PdfEngine.*" }

  filter {"configurations:Debug"}
    files_in_dir("src", {
      "ChmDoc.*",
      "DjVuEngine.*",
      "EbookDoc.*",
      "EbookEngine.*",
      "EbookFormatter.*",
      "HtmlFormatter.*",
      "ImagesEngine.*",
      "MobiDoc.*",
      "PdfCreator.*",
      "utils/PalmDbReader.*",
      "mui/MiniMui.*",
      "mui/TextRender.*",
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
  files { "src/MUPDF_Exports.cpp", "src/PdfEngine.cpp" }

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

function installer_utils_files()
  files_in_dir("src/utils", {
    "ArchUtil.*",
    "BaseUtil.*",
    "BitReader.*",
    "ByteOrderDecoder.*",
    "CmdLineParser.*",
    "DbgHelpDyn.*",
    "DebugLog.*",
    "Dict.*",
    "DirIter.*",
    "Dpi.*",
    "FileUtil.*",
    "FzImgReader.*",
    "GdiPlusUtil.*",
    "HttpUtil.*",
    "LzmaSimpleArchive.*",
    "StrUtil.*",
    "StrFormat.*",
    "StrSlice.*",
    "ThreadUtil.*",
    "TgaReader.*",
    "UITask.*",
    "WebpReader.*",
    "WinDynCalls.*",
    "WinUtil.*",
  })
end

function installer_files()
  zlib_files()
  unarr_files()
  installer_utils_files()
  files_in_dir( "src", {
    "CrashHandler.*",
    "Translations.*",
    "installer/Installer.cpp",
    "installer/Installer.h",
    "installer/Trans_installer_txt.cpp",
    "installer/Resource.h",
    "installer/Installer.rc",
  })
end

function uninstaller_files()
  files_in_dir("src", {
    "CrashHandler.*",
    "Translations.*",
    "installer/Installer.cpp",
    "installer/Installer.h",
    "installer/Trans_installer_txt.cpp",
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
  })

  files_in_dir("src/utils", {
    "BaseUtil.*",
    "DebugLog.*",
    "Dpi.*",
    "FileUtil.*",
    "Scoped.h",
    "StrUtil.*",
    "WinDynCalls.*",
    "WinUtil.*",
  })

  files_in_dir("src/wingui", {
    "TabsCtrl.*",
    "Win32Window.*",
  })
end
