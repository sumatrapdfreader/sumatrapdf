-- to generate Visual Studio files in vs-premake directory, run:
-- premake5 vs2013 or premake5 vs2015
-- premake4 is obsolete and doesn't support VS 2013+
-- I'm using premake5 alpha4 from http://premake.github.io/download.html#v5
-- TODO:
-- 1. the final output (SumatraPDF.exe etc.) all end up in the same location
--    Should go to different directories.
-- 2. generate mupdf/generated or check them in
-- 3. libmupdf doesn't have the right toolset set
-- 4. assembly for libjpeg-turbo
-- 5. a way to define SVN_PRE_RELEASE_VER, via build_config.h ?
-- 6. Installer

function files_in_dir(dir, files_in_dir)
  local paths = {}
  for _, file in ipairs(files_in_dir) do
    local path = dir .. "/" .. file
    table.insert(paths, path)
  end
  files(paths)
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
  files {
    "ext/openjpeg/bio.c",
    "ext/openjpeg/cidx_manager.c",
    "ext/openjpeg/cio.c",
    "ext/openjpeg/dwt.c",
    "ext/openjpeg/event.c",
    "ext/openjpeg/function_list.c",
    "ext/openjpeg/image.c",
    "ext/openjpeg/invert.c",
    "ext/openjpeg/j2k.c",
    "ext/openjpeg/jp2.c",
    "ext/openjpeg/mct.c",
    "ext/openjpeg/mqc.c",
    "ext/openjpeg/openjpeg.c",
    "ext/openjpeg/opj_clock.c",
    "ext/openjpeg/phix_manager.c",
    "ext/openjpeg/pi.c",
    "ext/openjpeg/ppix_manager.c",
    "ext/openjpeg/raw.c",
    "ext/openjpeg/t1.c",
    "ext/openjpeg/t2.c",
    "ext/openjpeg/tcd.c",
    "ext/openjpeg/tgt.c",
    "ext/openjpeg/thix_manager.c",
    "ext/openjpeg/tpix_manager.c",
  }
  -- TODO: probably can be
  -- files { "ext/openjpeg/*.c" }
  -- removefiles { "*t1_generate_luts.c"}
end

function libwebp_files()
  files {
    "ext/libwebp/dec/alpha.c",
    "ext/libwebp/dec/buffer.c",
    "ext/libwebp/dec/frame.c",
    "ext/libwebp/dec/idec.c",
    "ext/libwebp/dec/io.c",
    "ext/libwebp/dec/quant.c",
    "ext/libwebp/dec/tree.c",
    "ext/libwebp/dec/vp8.c",
    "ext/libwebp/dec/vp8l.c",
    "ext/libwebp/dec/webp.c",

    "ext/libwebp/dsp/alpha_processing.c",
    "ext/libwebp/dsp/cpu.c",
    "ext/libwebp/dsp/dec.c",
    "ext/libwebp/dsp/dec_sse2.c",
    "ext/libwebp/dsp/lossless.c",
    "ext/libwebp/dsp/lossless_sse2.c",
    "ext/libwebp/dsp/upsampling.c",
    "ext/libwebp/dsp/upsampling_sse2.c",
    "ext/libwebp/dsp/yuv.c",
    "ext/libwebp/dsp/yuv_sse2.c",
    "ext/libwebp/dsp/dec_clip_tables.c",
    "ext/libwebp/dsp/alpha_processing_sse2.c",

    "ext/libwebp/utils/bit_reader.c",
    "ext/libwebp/utils/color_cache.c",
    "ext/libwebp/utils/filters.c",
    "ext/libwebp/utils/huffman.c",
    "ext/libwebp/utils/quant_levels_dec.c",
    "ext/libwebp/utils/rescaler.c",
    "ext/libwebp/utils/thread.c",
    "ext/libwebp/utils/utils.c",
    "ext/libwebp/utils/random.c",
  }
end

function libjpeg_turbo_files()
  files {
    "ext/libjpeg-turbo/jcomapi.c",
    "ext/libjpeg-turbo/jdapimin.c",
    "ext/libjpeg-turbo/jdapistd.c",
    "ext/libjpeg-turbo/jdatadst.c",
    "ext/libjpeg-turbo/jdatasrc.c",
    "ext/libjpeg-turbo/jdcoefct.c",
    "ext/libjpeg-turbo/jdcolor.c",
    "ext/libjpeg-turbo/jddctmgr.c",
    "ext/libjpeg-turbo/jdhuff.c",
    "ext/libjpeg-turbo/jdinput.c",
    "ext/libjpeg-turbo/jdmainct.c",
    "ext/libjpeg-turbo/jdmarker.c",
    "ext/libjpeg-turbo/jdmaster.c",
    "ext/libjpeg-turbo/jdmerge.c",
    "ext/libjpeg-turbo/jdpostct.c",
    "ext/libjpeg-turbo/jdsample.c",
    "ext/libjpeg-turbo/jdtrans.c",
    "ext/libjpeg-turbo/jerror.c",
    "ext/libjpeg-turbo/jfdctflt.c",
    "ext/libjpeg-turbo/jfdctint.c",
    "ext/libjpeg-turbo/jidctflt.c",
    "ext/libjpeg-turbo/jidctfst.c",
    "ext/libjpeg-turbo/jidctint.c",
    "ext/libjpeg-turbo/jquant1.c",
    "ext/libjpeg-turbo/jquant2.c",
    "ext/libjpeg-turbo/jutils.c",
    "ext/libjpeg-turbo/jmemmgr.c",
    "ext/libjpeg-turbo/jmemnobs.c",
    "ext/libjpeg-turbo/jaricom.c",
    "ext/libjpeg-turbo/jdarith.c",
    "ext/libjpeg-turbo/jfdctfst.c",
    "ext/libjpeg-turbo/jdphuff.c",
    "ext/libjpeg-turbo/jidctred.c",

    -- TODO: make it asm code and jsimd_i386.c
    "ext/libjpeg-turbo/jsimd_none.c",
  }
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

  files {
    "ext/freetype2/src/cff/cff.c",
    "ext/freetype2/src/cid/type1cid.c",
    "ext/freetype2/src/psaux/psaux.c",
    "ext/freetype2/src/psnames/psnames.c",
    "ext/freetype2/src/smooth/smooth.c",
    "ext/freetype2/src/sfnt/sfnt.c",
    "ext/freetype2/src/truetype/truetype.c",
    "ext/freetype2/src/type1/type1.c",
    "ext/freetype2/src/raster/raster.c",
    "ext/freetype2/src/otvalid/otvalid.c",
    "ext/freetype2/src/pshinter/pshinter.c",
    "ext/freetype2/src/gzip/ftgzip.c",
  }

  filter "configurations:Debug*"
    files { "ext/freetype2/src/base/ftdebug.c" }
  filter {}

end

function sumatra_files()
  files {
    "src/SumatraPDF.rc",

    "src/AppPrefs.cpp",
    "src/DisplayModel.cpp",
    "src/CrashHandler.cpp",
    "src/Favorites.cpp",
    "src/TextSearch.cpp",
    "src/SumatraAbout.cpp",
    "src/SumatraAbout2.cpp",
    "src/SumatraDialogs.cpp",
    "src/SumatraProperties.cpp",
    "src/GlobalPrefs.cpp",
    "src/PdfSync.cpp",
    "src/RenderCache.cpp",
    "src/TextSelection.cpp",
    "src/WindowInfo.cpp",
    "src/ParseCommandLine.cpp",
    "src/StressTesting.cpp",
    "src/AppTools.cpp",
    "src/AppUtil.cpp",
    "src/TableOfContents.cpp",
    "src/Toolbar.cpp",
    "src/Print.cpp",
    "src/Notifications.cpp",
    "src/Selection.cpp",
    "src/Search.cpp",
    "src/Menu.cpp",
    "src/ExternalViewers.cpp",
    "src/EbookControls.cpp",
    "src/EbookController.cpp",
    "src/Doc.cpp",
    "src/MuiEbookPageDef.cpp",
    "src/PagesLayoutDef.cpp",
    "src/Tester.cpp",
    "src/Translations.cpp",
    "src/Trans_sumatra_txt.cpp",
    "src/Tabs.cpp",
    "src/FileThumbnails.cpp",
    "src/FileHistory.cpp",
    "src/ChmModel.cpp",
    "src/Caption.cpp",
    "src/Canvas.cpp",
    "src/TabInfo.cpp",

    "src/regress/Regress.cpp",

    "src/uia/Provider.cpp",
    "src/uia/StartPageProvider.cpp",
    "src/uia/DocumentProvider.cpp",
    "src/uia/PageProvider.cpp",
    "src/uia/TextRange.cpp",
  }
end

function utils_files()
  files {
    "src/utils/FileUtil.cpp",
    "src/utils/HttpUtil.cpp",
    "src/utils/StrUtil.cpp",
    "src/utils/WinUtil.cpp",
    "src/utils/GdiPlusUtil.cpp",
    "src/utils/FileTransactions.cpp",
    "src/utils/Touch.cpp",
    "src/utils/TrivialHtmlParser.cpp",
    "src/utils/HtmlWindow.cpp",
    "src/utils/DirIter.cpp",
    "src/utils/BitReader.cpp",
    "src/utils/HtmlPullParser.cpp",
    "src/utils/HtmlPrettyPrint.cpp",
    "src/utils/ThreadUtil.cpp",
    "src/utils/DebugLog.cpp",
    "src/utils/DbgHelpDyn.cpp",
    "src/utils/JsonParser.cpp",
    "src/utils/TgaReader.cpp",
    "src/utils/HtmlParserLookup.cpp",
    "src/utils/ByteOrderDecoder.cpp",
    "src/utils/CmdLineParser.cpp",
    "src/utils/UITask.cpp",
    "src/utils/StrFormat.cpp",
    "src/utils/Dict.cpp",
    "src/utils/BaseUtil.cpp",
    "src/utils/CssParser.cpp",
    "src/utils/FileWatcher.cpp",
    "src/utils/CryptoUtil.cpp",
    "src/utils/StrSlice.cpp",
    "src/utils/TxtParser.cpp",
    "src/utils/SerializeTxt.cpp",
    "src/utils/SquareTreeParser.cpp",
    "src/utils/SettingsUtil.cpp",
    "src/utils/WebpReader.cpp",
    "src/utils/FzImgReader.cpp",
    "src/utils/ArchUtil.cpp",
    "src/utils/ZipUtil.cpp",
    "src/utils/LzmaSimpleArchive.cpp",
    "src/utils/Dpi.cpp",

    "src/wingui/DialogSizer.cpp",
    "src/wingui/SplitterWnd.cpp",
    "src/wingui/LabelWithCloseWnd.cpp",
    "src/wingui/FrameRateWnd.cpp",
    "src/wingui/EditCtrl.cpp",
    "src/wingui/Win32Window.cpp",
  }
end

function mui_files()
  files {
    "src/mui/MuiBase.cpp",
    "src/mui/Mui.cpp",
    "src/mui/MuiCss.cpp",
    "src/mui/MuiLayout.cpp",
    "src/mui/MuiPainter.cpp",
    "src/mui/MuiControl.cpp",
    "src/mui/MuiButton.cpp",
    "src/mui/MuiScrollBar.cpp",
    "src/mui/MuiEventMgr.cpp",
    "src/mui/MuiHwndWrapper.cpp",
    "src/mui/MuiGrid.cpp",
    "src/mui/SvgPath.cpp",
    "src/mui/MuiDefs.cpp",
    "src/mui/MuiFromText.cpp",
    "src/mui/TextRender.cpp",
  }
end

function engines_files()
  files {
    "src/PdfEngine.cpp",
    "src/PsEngine.cpp",
    "src/PdfCreator.cpp",
    "src/ImagesEngine.cpp",
    "src/DjVuEngine.cpp",
    "src/EbookEngine.cpp",
    "src/EbookDoc.cpp",
    "src/MobiDoc.cpp",
    "src/HtmlFormatter.cpp",
    "src/EbookFormatter.cpp",
    "src/ChmDoc.cpp",
    "src/EngineManager.cpp",
    "src/FileModifications.cpp",

    "src/utils/PalmDbReader.cpp",

    "ext/CHMLib/src/chm_lib.c",
    "ext/CHMLib/src/lzx.c",
  }
end

function mupdf_files()
  -- TODO:
  -- .\ext\..\bin\nasm.exe -I .\mupdf\ -f win32 -o .\obj-rel\mupdf\font_base14.obj
  -- .\mupdf\font_base14.asm

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

function sumatrapdf_files()
  files {
    "src/SumatraPDF.cpp",
    "src/SumatraStartup.cpp",
    "ext/synctex/synctex_parser_utils.c",
    "ext/synctex/synctex_parser.c",
  }
end

-- TODO: rename solution to workspace. workspace is the documented name
-- but latest alpha4 doesn't recognize it yet
solution "efi"
  configurations { "Debug", "Release" }
  location "vs-premake5" -- this is where generated solution/project files go

  project "efi"
    kind "ConsoleApp"
    language "C++"
    --targetdir "%{cfg.buildcfg}"
    flags { "Symbols" }

    files {
      "tools/efi/*.h",
      "tools/efi/*.cpp",
      "src/utils/BaseUtil*",
      "src/utils/BitManip.h",
      "src/utils/Dict*",
      "src/utils/StrUtil*",
    }
    includedirs { "src/utils" }
    links { }

    filter "configurations:Debug"
      defines { "DEBUG" }

    filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

    filter {}

solution "SumatraPDF"
  configurations { "Debug", "Release" }
  platforms { "x32", "x64" }
  startproject "SumatraPDF"

  filter "platforms:x32"
     architecture "x86"
     toolset "v140_xp"

  filter "platforms:x64"
     architecture "x86_64"
     toolset "v140"
     -- TODO: 64bit has more warnigs. Fix warnings in Sumatra code,
     -- selectively disable for other projects
     disablewarnings {
       "4311",
       "4312",
       "4302",
       "4267",
       "4244"
     }

  filter {}

  location "this_is_invlid_location"
  filter "action:vs2015"
    location "vs2015"
  filter {}

  filter "action:vs2013"
    location "vs2013"
  filter {}

  -- https://github.com/premake/premake-core/wiki/flags
  flags {
    "Symbols",
    "NoExceptions",
    "NoRTTI",
    --"FatalWarnings", TODO: when ready
    --"MultiProcessorCompile",
    --"UndefinedIndentifiers", TODO: not yet in alpha4 ?
    -- "Unicode", TODO: breaks libdjuv?
  }

  defines { "WIN32", "_WIN32", "_CRT_SECURE_NO_WARNINGS", "WINVER=0x0501", "_WIN32_WINNT=0x0501" }

  filter "configurations:Debug"
    defines { "DEBUG" }

  filter "configurations:Release"
    defines { "NDEBUG" }
    flags {
      "LinkTimeOptimization",
    }
    optimize "On"

  filter {}

  project "zlib"
    kind "StaticLib"
    language "C"

    -- TODO: only for msvc
    disablewarnings { "4996" }

    zlib_files()

  project "libdjvu"
    kind "StaticLib"
    language "C++"

    -- TODO: try /D USE_EXCEPTION_EMULATION to see if it reduces the size
    -- and disables the exceptions warnings
    defines { "NEED_JPEG_DECODER", "THREADMODEL=0", "DDJVUAPI=/**/",  "MINILISPAPI=/**/", "DO_CHANGELOCALE=0" }
    includedirs { "ext/libjpeg-turbo" }

    -- TODO: only for msvc
    -- 4530 - exception mismatch
    disablewarnings { "2664", "4530" }
    libdjvu_files()

  project "unarr"
    kind "StaticLib"
    language "C++"

    defines { "HAVE_ZLIB", "HAVE_BZIP2", "HAVE_7Z" }
    includedirs { "ext/zlib", "ext/bzip2", "ext/lzma/C" }
    -- TODO: only for msvc
    disablewarnings { "4996" }

    unarr_files()

  project "jbig2dec"
    kind "StaticLib"
    language "C"
    --targetdir "%{cfg.buildcfg}"

    defines { "HAVE_STRING_H=1", "JBIG_NO_MEMENTO" }
    includedirs { "ext/jbig2dec" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018" }

    jbig2dec_files()

  project "openjpeg"
    kind "StaticLib"
    language "C"

    includedirs { "ext/openjpeg" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018" }

    openjpeg_files()

  project "libwebp"
    kind "StaticLib"
    language "C"

    includedirs { "ext/libwebp" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018" }

    libwebp_files()

  project "libjpeg-turbo"
    kind "StaticLib"
    language "C"

    includedirs { "ext/libjpeg-turbo" }
    includedirs { "ext/libjpeg-turbo/simd" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018" }

    libjpeg_turbo_files()

  project "freetype"
    kind "StaticLib"
    language "C"

    includedirs { "ext/freetype2/config" }
    includedirs { "ext/freetype2/include" }
    defines { "_HAS_EXCEPTIONS=0" }
    defines { "FT2_BUILD_LIBRARY", "FT_OPTION_AUTOFIT2"}
    -- TODO: only for msvc
    disablewarnings { "4996", "4018" }

    freetype_files()

  project "sumatra"
    kind "StaticLib"
    language "C++"
    --targetdir "%{cfg.buildcfg}"

    includedirs { "src/utils", "src/wingui", "src/mui", "ext/lzma/C" }
    includedirs { "ext/libwebp", "ext/unarr", "mupdf/include", "src" }
    includedirs { "ext/synctex", "ext/libdjvu", "ext/CHMLib/src" }

    defines { "_HAS_EXCEPTIONS=0" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018", "4800", "4838" }

    sumatra_files()

  project "utils"
    kind "StaticLib"
    language "C++"
    --targetdir "%{cfg.buildcfg}"

    includedirs { "src/utils", "src/wingui", "src/mui", "ext/zlib", "ext/lzma/C" }
    includedirs { "ext/libwebp", "ext/unarr", "mupdf/include" }

    defines { "_HAS_EXCEPTIONS=0" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018", "4800", "4838" }

    utils_files()

  project "mui"
    kind "StaticLib"
    language "C++"
    --targetdir "%{cfg.buildcfg}"

    includedirs { "src/utils", "src/wingui", "src/mui" }

    defines { "_HAS_EXCEPTIONS=0" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018", "4800", "4838" }

    mui_files()

  project "engines"
    kind "StaticLib"
    language "C++"
    --targetdir "%{cfg.buildcfg}"

    includedirs { "src/utils", "src/wingui", "src/mui" }
    includedirs { "ext/synctex", "ext/libdjvu", "ext/CHMLib/src", "ext/zlib", "mupdf/include" }

    defines { "_HAS_EXCEPTIONS=0" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018", "4800", "4838", "4244" }

    engines_files()

  project "mupdf"
    kind "StaticLib"
    language "C"

    includedirs {
      "mupdf/include", "mupdf/generated", "ext/zlib",
      "ext/freetype2/config", "ext/freetype2/include",
      "ext/jbig2dec", "ext/libjpeg-turbo", "ext/openjpeg"
    }

    -- .\ext\..\bin\nasm.exe -I .\mupdf\ -f win32 -o .\obj-rel\mupdf\font_base14.obj
    -- .\mupdf\font_base14.asm
    filter {'files:**.asm', 'platforms:x32'}
       -- A message to display while this build step is running (optional)
       buildmessage 'Compiling %{file.relpath}'

       -- Note: must be -I ../mupdf/, not just -I ../mupdf
       buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
       buildcommands {
          '..\\bin\\nasm.exe -f win32 -I ../mupdf/ -o "%{cfg.objdir}/%{file.basename}.obj" "%{file.relpath}"'
       }
    filter {}

    filter {'files:**.asm', 'platforms:x64'}
      -- A message to display while this build step is running (optional)
      buildmessage 'Compiling %{file.relpath}'

      -- Note: must be -I ../mupdf/, not just -I ../mupdf

      buildoutputs { '%{cfg.objdir}/%{file.basename}.obj' }
      buildcommands {
        '..\\bin\\nasm.exe -f win64 -DWIN64 -I ../mupdf/ -o "%{cfg.objdir}/%{file.basename}.obj" "%{file.relpath}"'
      }
    filter {}

    defines { "NOCJKFONT", "SHARE_JPEG" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018", "4800", "4838", "4244" }
    mupdf_files()

    links {
      "zlib",
      "freetype",
      "libjpeg-turbo",
      "jbig2dec",
      "openjpeg",
    }

  project "SumatraPDF"
    kind "WindowedApp"
    language "C++"
    --targetdir "%{cfg.buildcfg}"

    flags {
      "WinMain",
      "NoManifest",
    }

    defines { "_HAS_EXCEPTIONS=0" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018", "4800", "4838", "4244" }

    includedirs {
      "src/utils", "src/wingui", "src/mui", "ext/zlib", "ext/lzma/C",
      "ext/libwebp", "ext/unarr", "mupdf/include", "src", "ext/synctex",
      "ext/libdjvu", "ext/CHMLib/src"
    }

    sumatrapdf_files()

    -- TODO: is this necessary? This is also part of sumatra
    files {
      "src/SumatraPDF.rc",
    }

    links {
      "sumatra",
      "utils",
      "mui",
      "engines",
      "mupdf",
      "libdjvu",
      "unarr",
      "libwebp",
    }

    links {
      "advapi32.lib", "kernel32.lib", "user32.lib", "gdi32.lib", "comdlg32.lib",
      "shell32.lib", "WindowsCodecs.lib", "comctl32.lib", "Msimg32.lib",
      "Winspool.lib", "wininet.lib", "urlmon.lib", "gdiplus.lib", "ole32.lib",
      "OleAut32.lib", "shlwapi.lib", "version.lib", "crypt32.lib"
    }

  project "libmupdf"
    kind "SharedLib"
    language "C"

    implibname "libmupdf"

    -- TODO: not sure if it works at all - I don't see it in generated
    -- Is thre a better way to do it?
    -- TODO: only for windows
    linkoptions { "/DEF:..\\src\\libmupdf.def" }

    -- premake has logic in vs2010_vcxproj.lua that only sets PlatformToolset
    -- if there is a c/c++ file, so we add a no-op cpp file to force This logic
    files { "src/libmupdf.rc", "src/no_op_for_premake.cpp" }
    links {
      "mupdf",
      "libdjvu",
      "unarr",
      "libwebp",
    }

    links {
      "advapi32.lib", "kernel32.lib", "user32.lib", "gdi32.lib", "comdlg32.lib",
      "shell32.lib", "WindowsCodecs.lib", "comctl32.lib", "Msimg32.lib",
      "Winspool.lib", "wininet.lib", "urlmon.lib", "gdiplus.lib", "ole32.lib",
      "OleAut32.lib", "shlwapi.lib", "version.lib", "crypt32.lib"
    }

  project "SumatraPDF-no-MUPDF"
    kind "WindowedApp"
    language "C++"

    flags {
      "WinMain",
      "NoManifest",
    }

    defines { "_HAS_EXCEPTIONS=0" }
    -- TODO: only for msvc
    disablewarnings { "4996", "4018", "4800", "4838", "4244" }

    includedirs {
      "src/utils", "src/wingui", "src/mui", "ext/zlib", "ext/lzma/C",
      "ext/libwebp", "ext/unarr", "mupdf/include", "src", "ext/synctex",
      "ext/libdjvu", "ext/CHMLib/src"
    }

    sumatrapdf_files()
    -- TODO: is this necessary? This is also part of sumatra
    files {
      "src/SumatraPDF.rc",
    }
    files { "src/MuPDF_Exports.cpp" }

    links {
      "sumatra",
      "libmupdf",
      "utils",
      "mui",
      "engines",
      "unarr",
      "libwebp",
    }

    links {
      "advapi32.lib", "kernel32.lib", "user32.lib", "gdi32.lib", "comdlg32.lib",
      "shell32.lib", "WindowsCodecs.lib", "comctl32.lib", "Msimg32.lib",
      "Winspool.lib", "wininet.lib", "urlmon.lib", "gdiplus.lib", "ole32.lib",
      "OleAut32.lib", "shlwapi.lib", "version.lib", "crypt32.lib"
    }

  project "MakeLZSA"
    kind "ConsoleApp"
    language "C++"

    files {
      "src/tools/MakeLzSA.cpp"
    }

    disablewarnings { "4800" }

    includedirs {
      "src/utils", "ext/zlib", "ext/lzma/C", "ext/unarr"
    }

    links {
      "utils",
      "unarr",
      "zlib",
    }

    -- TODO: probably excessive
    links {
      "advapi32.lib", "kernel32.lib", "user32.lib", "gdi32.lib", "comdlg32.lib",
      "shell32.lib", "WindowsCodecs.lib", "comctl32.lib", "Msimg32.lib",
      "Winspool.lib", "wininet.lib", "urlmon.lib", "gdiplus.lib", "ole32.lib",
      "OleAut32.lib", "shlwapi.lib", "version.lib", "crypt32.lib"
    }

  project "PdfFilter"
    kind "SharedLib"
    language "C++"

    disablewarnings { "4800", "4838" }
    defines { "_HAS_EXCEPTIONS=0" }

    -- TODO: probably excessive
    includedirs {
      "src/utils", "src/wingui", "src/mui", "ext/zlib", "ext/lzma/C",
      "ext/libwebp", "ext/unarr", "mupdf/include", "src", "ext/synctex",
      "ext/libdjvu", "ext/CHMLib/src"
    }

    files_in_dir("src/ifilter", {
      "PdfFilter.rc",
      "PdfFilterDll.cpp",
      "CPdfFilter.cpp",
    })
    files { "src/MUPDF_Exports.cpp", "src/PdfEngine.cpp" }

    links {
      "utils",
      "libmupdf",
    }
    -- TODO: probably excessive
    links {
      "advapi32.lib", "kernel32.lib", "user32.lib", "gdi32.lib", "comdlg32.lib",
      "shell32.lib", "WindowsCodecs.lib", "comctl32.lib", "Msimg32.lib",
      "Winspool.lib", "wininet.lib", "urlmon.lib", "gdiplus.lib", "ole32.lib",
      "OleAut32.lib", "shlwapi.lib", "version.lib", "crypt32.lib"
    }

  project "PdfPreview"
    kind "SharedLib"
    language "C++"

    disablewarnings { "4800", "4838" }
    defines { "_HAS_EXCEPTIONS=0" }

    -- TODO: probably excessive
    includedirs {
      "src/utils", "src/wingui", "src/mui", "ext/zlib", "ext/lzma/C",
      "ext/libwebp", "ext/unarr", "mupdf/include", "src", "ext/synctex",
      "ext/libdjvu", "ext/CHMLib/src"
    }

    files_in_dir("src/previewer", {
      "PdfPreview.rc",
      "PdfPreviewDll.cpp",
      "PdfPreview.cpp",
    })
    files { "src/MUPDF_Exports.cpp", "src/PdfEngine.cpp" }

    links {
      "utils",
      "libmupdf",
    }
    -- TODO: probably excessive
    links {
      "advapi32.lib", "kernel32.lib", "user32.lib", "gdi32.lib", "comdlg32.lib",
      "shell32.lib", "WindowsCodecs.lib", "comctl32.lib", "Msimg32.lib",
      "Winspool.lib", "wininet.lib", "urlmon.lib", "gdiplus.lib", "ole32.lib",
      "OleAut32.lib", "shlwapi.lib", "version.lib", "crypt32.lib"
    }
