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
    "Arena.cpp",
    "Base.h",
    "Base.cpp",
    "ByteReaderWriter.*",
    "CmdLineArgs.h",
    "CmdLineArgs.cpp",
    "DirScan.h",
    "DirScan.cpp",
    "File.h",
    "File.cpp",
    "Log.h",
    "LogNoOp.cpp",
    "LzmaSimpleArchive.*",
    "Pixmap.*",
    "StrQueue.*",
    "WinDynCalls.h",
    "WinDynCalls.cpp",
    "Win.*",
  })
  files_in_dir("src/gui", {
    "Dpi.h",
    "Dpi.cpp",
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

function zlib_files()
  files_in_dir("ext/a-zlib", {
    "zlib.c",
    "zlib.h",
    "version.txt",
    "LICENSE",
    "zlib.3.pdf",
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

-- x64 only: avx2 NASM SIMD + shared helpers. Not used for 32-bit (HAVE_ASM=0).
-- SSE/AVX-512 *.asm omitted; x86/*_sumatra.h only wires avx2 symbols.
function dav1d_x64_files()
  files_in_dir("ext/dav1d/src/x86", {
    "cpu.c",
  })

  files_in_dir("ext/dav1d/src/x86", {
    "cdef16_avx2.asm",
    "cdef_avx2.asm",
    "cpuid.asm",
    "filmgrain16_avx2.asm",
    "filmgrain_avx2.asm",
    "ipred16_avx2.asm",
    "ipred_avx2.asm",
    "itx16_avx2.asm",
    "itx_avx2.asm",
    "loopfilter16_avx2.asm",
    "loopfilter_avx2.asm",
    "looprestoration16_avx2.asm",
    "looprestoration_avx2.asm",
    "mc16_avx2.asm",
    "mc_avx2.asm",
    -- mc_sse.asm: provides mc_warp_filter2 (and related RODATA) used by mc_avx2
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

  -- libjpeg-turbo 3.x: 8-bit precision wrappers only. Each #includes the
  -- matching ../<name>.c with BITS_IN_JSAMPLE=8. 12/16-bit wrappers are
  -- omitted (MuPDF only uses the 8-bit API); jcinit/jdmaster/jdtrans reject
  -- higher data_precision with JERR_BAD_PRECISION.
  files_in_dir("ext/libjpeg-turbo/src/wrapper", {
    "jcapistd-8.c",
    "jccoefct-8.c",
    "jccolor-8.c",
    "jcdctmgr-8.c",
    "jcdiffct-8.c",
    "jclossls-8.c",
    "jcmainct-8.c",
    "jcprepct-8.c",
    "jcsample-8.c",
    "jdapistd-8.c",
    "jdcoefct-8.c",
    "jdcolor-8.c",
    "jddctmgr-8.c",
    "jddiffct-8.c",
    "jdlossls-8.c",
    "jdmainct-8.c",
    "jdmerge-8.c",
    "jdpostct-8.c",
    "jdsample-8.c",
    "jfdctfst-8.c",
    "jfdctint-8.c",
    "jidctflt-8.c",
    "jidctfst-8.c",
    "jidctint-8.c",
    "jidctred-8.c",
    "jquant1-8.c",
    "jquant2-8.c",
    "jutils-8.c",
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

files {
}

function sumatrapdf_files()
  files_in_dir("src/base", {
    "CrashHandler.h",
    "CrashHandler.cpp",
  })
  files_in_dir("src", {
    "Accelerators.*",
    "ShortcutParse.*",
    "Actions.*",
    "AvifReader.*",
    "DarkMode.*",
    "AddFavoriteDialog.*",
    "AdvancedSettingsDialog.*",
    "ChangeColorDialog.*",
    "ChangeLanguageDialog.*",
    "ChangeScrollbarDialog.*",
    "ChangeThemeDialog.*",
    "CustomZoomDialog.*",
    "PageGridDialog.*",
    "SignDocumentDialog.*",
    "EbookSettingsDialog.*",
    "GetPasswordDialog.*",
    "GoToPageDialog.*",
    "InverseSearchDialog.*",
    "SettingsDialog.*",
    "AppSettings.*",
    "PagePosition.*",
    "AppTools.*",
    "Canvas.*",
    "AnnotPlacement.*",
    "AnnotTextPopup.*",
    "AnnotEditToolbar.*",
    "AnnotFilterToolbar.*",
    "AnnotSearch.*",
    "CanvasAboutUI.*",
    "CaptionGlyphs.*",
    "ChmDump.*",
    "ChmModel.*",
    "MarkdownModel.*",
    "MarkdownToc.*",
    "EmbeddedResources.*",
    "AIChatCommon.*",
    "AIChatPanel.*",
    "AIAntiGravity.*",
    "AIClaudeCode.*",
    "AICodexBuild.*",
    "AIGrokBuild.*",
    "CommandAvailability.*",
    "CommandPalette.*",
    "FilterUtil.*",
    "FilterHighlightDraw.*",
    "Commands.*",
    "ImageSaveCropResize.*",
    "ImageEditHostSumatra.cpp",
    "DisplayMode.*",
    "DisplayModel.*",
    "DocumentLayout.*",
    "PageRenderPolicy.*",
    "PageRenderService.*",
    "ReaderModel.*",
    "DocController.*",
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
    "ImageReader.h",
    "ImageReader.cpp",
    "GlobalHotkeys.*",
    "GoogleLens.*",
    "HangDetector.*",
    "HomePage.*",
    "Installer.*",
    "InstallerCommon.cpp",
    "JxlReader.*",
    "KeyboardHelp.*",
    "LinkFollow.*",
    "MainWindow.*",
    "Menu.*",
    "NavFilesInFolder.*",
    "Notifications.*",
    "PdfSync.*",
    "PdfTools.*",
    "PngOptimizer.*",
    "Print.*",
    "PrintWin11.*",
    "ProgressUpdateUI.*",
    "ReadAloud.*",
    "ReadingAutoScroll.*",
    "RefHover.h",
    "RefHover.cpp",
    "RefHoverCanvas.cpp",
    "RefHoverDetect.cpp",
    "RefHoverInternal.cpp",
    "RefHoverPopup.cpp",
    "RefHoverRender.cpp",
    "RefHoverShow.cpp",
    "RefHoverText.cpp",
    "RefHoverTextDetect.cpp",
    "RegistryInstaller.*",
    "RegistryPreview.*",
    "RegistrySearchFilter.*",
    "RenderCache.*",
    "resource.h",
    "SearchAndDDE.*",
    "OverlayScrollbar.*",
    "ExplorerQuickLook.*",
    "Screenshot.*",
    "ScreenshotCapture.*",
    "SelectTextKeyboard.*",
    "Selection.*",
    "SelectionHandlers.*",
    "SelectionToolbar.*",
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
    "DocumentProperties.*",
    "EutlTrust.*",
    "SumatraLog.*",
    "SumatraTest.*",
    "SvgIcons.*",
    "TableOfContents.*",
    "Tabs.*",
    "TabGroupsManage.*",
    "Tester.*",
    "Tests.cpp",
    "TextSearch.*",
    "TextSelection.*",
    "TextViewWnd.*",
    "Theme.*",
    "Toolbar.*",
    "TranslationLangs.cpp",
    "Translations.*",
    "Uninstaller.cpp",
    "UpdateCheck.*",
    "BuildConfig.h",
    "Version.h",
    "VirtWnd.*",
    "WebpReader.*",
    "WindowTab.*",
  })
  filter { "configurations:Debug or DebugFull" }
  files_in_dir("src", {
    "regress/Regress.*",
    "TestPlugin.cpp",
    "TestPreview.cpp",
  })
  files_in_dir("src/tests", {
    "*.cpp",
  })
  files_in_dir("src/testcode", {
    "test-app.h",
    "TestApp.cpp",
    "TestLayout.cpp",
    --"TestLice.cpp",
    "TestTab.cpp",
  })
  files_in_dir("src/gui/tests", {
    "*.cpp",
  })
  files_in_dir("src/base/tests", {
    "*.cpp",
    "UtAssert.h",
  })
  -- linux-only, like the FileWatcher_linux.cpp it tests
  removefiles { "src/base/tests/FileWatcher_linux_ut.cpp" }
  files_in_dir("src/base", {
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
    "Darkmodelib.cpp",
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
    "Darkmodelib.h",
    "DarkModeSubclass.h",
  })
end

function base_files()
  files_in_dir("src/base", {
    "AppendStore.h",
    "AppendStore.cpp",
    "ApiHook.*",
    "Archive.*",
    "Arena.cpp",
    "Base.h",
    "Base.cpp",
    "ByteReaderWriter.*",
    "CmdLineArgs.h",
    "CmdLineArgs.cpp",
    "Crypto.h",
    "Crypto.cpp",
    "CssParser.*",
    "DbgHelpDyn.h",
    "DbgHelpDyn.cpp",
    "Dict.*",
    "DirScan.h",
    "DirScan.cpp",
    "Exif.*",
    "File.h",
    "File.cpp",
    "FileWatcher.h",
    "FileWatcher.cpp",
    "GdiPlusUtil.cpp",
    "GdiPlusUtil.h",
    "GuessFileType.*",
    "HtmlTags.*",
    "Http.h",
    "Http.cpp",
    "JsonParser.*",
    "Log.h",
    "LzmaSimpleArchive.*",
    "Pixmap.*",
    "RegistryPaths.*",
    "ScopedWin.h",
    "SettingsUtil.*",
    "SquareTreeParser.*",
    "StrQueue.*",
    "TgaReader.*",
    "TxtParser.*",
    "UITask.*",
    "WinDynCalls.h",
    "WinDynCalls.cpp",
    "Win.*",
    "Zip.*",
  })
  files_in_dir("src/gui", {
    "Dpi.h",
    "Dpi.cpp",
  })
end

function gui_files()
  files_in_dir("src/gui", {
    "DocumentView.h",
    "Gfx.h",
    "Gfx.cpp",
    "GfxGdiplus.cpp",
    "GfxDirect2D.cpp",
    "GuiColors.*",
    "Layout.*",
    "PasswordDialog.*",
    "PlatformFont.*",
    "PlatformCanvas.h",
    "PlatformText.*",
    "PlatformWindow.h",
    "UIModels.*",
    "VirtCtrl.*",
    "VirtHost.h",
    "VirtHost.*",
    "UiPlatform.*",
  })
  files_in_dir("src/gui/win", {
    "*.h",
    "*.cpp",
  })
end

function engines_files()
  files_in_dir("src", {
    "Annotation.*",
    "PdfSign.*",
    "ChapterTable.*",
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
    "LitDoc.*",
    "MobiDoc.*",
    "PalmDbReader.*",
    "PdfCad.*",
    "PdfCreator.*",
    "PdfDarkMode.h",
    "PdfDarkModeInternal.h",
    "PdfDarkModeAnalysis.cpp",
    "PdfDarkModeCache.cpp",
    "PdfDarkModeColor.cpp",
    "PdfDarkModeDevice.cpp",
    "PdfDarkModeEngineCache.cpp",
    "PdfDarkModeImageBgBlend.cpp",
    "PdfDarkModeImageClassifier.cpp",
    "PdfDarkModeImageRules.cpp",
    "PdfDarkModeImageStats.cpp",
    "PdfDarkModeOklab.cpp",
    "PdfDarkModeProfile.cpp",
    "PdfDarkModeScanProcess.cpp",
  })
end

-- cmark-gfm: markdown parser used by mupdf's source/html/md.c (FZ_ENABLE_MD).
-- Parser-only subset matching mupdf's Makelists CMARKGFM_SRC (no CLI main.c,
-- no commonmark/latex/man/xml/plaintext renderers). Generated config headers
-- (config.h, cmark-gfm_export.h, cmark-gfm_version.h) come from
-- ext/mupdf/scripts/cmark-gfm. Build with -DCMARK_GFM_STATIC_DEFINE.
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
  -- our additions to mupdf (not patches): see src/mupdf/README.md
  files {
    "src/mupdf/mupdf_load_system_font.c",
    "src/mupdf/noto_sumatra.c",
    "src/mupdf/noto_sumatra.h",
    "src/mupdf/pkcs7-windows.c",
    "src/mupdf/pkcs7-windows.h",
  }

  files_in_dir("ext/mupdf/source/cbz", {
    "mucbz.c",
    "muimg.c",
  })

  files { "ext/mupdf/source/fitz/*.h" }
  files_in_dir("ext/mupdf/source/fitz", {
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
    "load-webp.c",
    "log.c",
    "memento.c",
    "memory.c",
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

  files_in_dir("ext/mupdf/source/html", {
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

  files_in_dir("ext/mupdf/source/pdf", {
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

  files_in_dir("ext/mupdf/source/svg", {
    "svg-color.c",
    "svg-doc.c",
    "svg-parse.c",
    "svg-run.c",
  })

  files_in_dir("ext/mupdf/source/xps", {
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
  files_in_dir("ext/mupdf/source/reflow", {
    "reflow-doc.c",
  })
  files_in_dir("ext/mupdf/source/tools", {
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
    "ext/mupdf/include/mupdf/*.h",
    "ext/mupdf/include/mupdf/fitz/*.h",
    "ext/mupdf/include/mupdf/helpers/*.h",
    "ext/mupdf/include/mupdf/pdf/*.h",
  }
  files { "ext/mupdf/source/helpers/mu-threads/mu-threads.c" }
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
    "src/base/Arena.cpp",
    "src/base/BitManip.h",
    "src/base/Dict*",
    "src/tools/efi/*.cpp",
    "src/tools/efi/*.h",
  }
end

function test_engines_files()
  files {
    "src/base/GuessFileType.cpp",
    "src/AvifReader.cpp",
    "src/ChapterTable.cpp",
    "src/ChapterTable.h",
    "src/DocProperties.cpp",
    "src/DocProperties.h",
    "src/EbookDoc.cpp",
    "src/EmbeddedResources.cpp",
    "src/EngineAll.h",
    "src/EngineBase.cpp",
    "src/EngineBase.h",
    "src/EngineDjvuDec.cpp",
    "src/EngineImages.cpp",
    "src/EngineMupdf.cpp",
    "src/ImageReader.cpp",
    "src/GumboHtmlParser.cpp",
    "src/GumboHelpers.cpp",
    "src/JxlReader.cpp",
    "src/LitDoc.cpp",
    "src/LitDoc.h",
    "src/MobiDoc.cpp",
    "src/PalmDbReader.cpp",
    "src/PdfCad.cpp",
    "src/PdfCad.h",
    "src/PdfDarkMode.h",
    "src/PdfDarkModeNoOp.cpp",
    "src/TextSearch.cpp",
    "src/TextSearch.h",
    "src/TextSelection.cpp",
    "src/TextSelection.h",
    "src/WebpReader.cpp",
    "src/gui/UIModels.cpp",
    "src/gui/UIModels.h",
    "src/tools/test_engines.cpp",
  }
end

function bench_image_files()
  files {
    "src/tools/bench_image.cpp",
  }
end

function preview_test_files()
  files {
    "src/tools/preview_test.cpp",
  }
end

function plugin_test_files()
  files {
    "src/tools/plugin-test.cpp",
  }
end

function logview_files()
  files {
    "src/tools/logview/logview.cpp",
  }
  -- the subset of gui logview's UI needs (no tree view, tabs, web view, ...)
  files_in_dir("src/gui", {
    "UIModels.*",
    "Layout.*",
    "PlatformFont.*",
    "PlatformText.*",
    "Gfx.h",
    "Gfx.cpp",
    "GfxGdiplus.cpp",
    "GfxDirect2D.cpp",
    "GuiColors.*",
    "UiPlatform.*",
    "VirtCtrl.*",
  })
  files_in_dir("src/gui/win", {
    "WindowBase.*",
    "Edit.*",
    "Tooltip.*",
  })
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
    "ChapterTable.*",
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
    "EmbeddedResources.*",
    "AvifReader.*",
    "ImageReader.h",
    "ImageReader.cpp",
    "GumboHtmlParser.*",
    "GumboHelpers.*",
    "HtmlFormatter.*",
    "JxlReader.*",
    "MobiDoc.*",
    "gui/PlatformFont.*",
    "gui/PlatformText.*",
    "MUPDF_Exports.cpp",
    "PalmDbReader.*",
    "PdfCad.*",
    "PdfDarkMode.h",
    "PdfDarkModeNoOp.cpp",
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
    "ChapterTable.*",
    "CrashHandlerNoOp.cpp",
    "DocProperties.*",
    "EbookDoc.*",
    "EngineAll.h",
    "EngineBase.*",
    "EngineMupdf.*",
    "EngineMupdfImpl.*",
    "EmbeddedResources.*",
    "GumboHtmlParser.*",
    "GumboHelpers.*",
    "MobiDoc.*",
    "MUPDF_Exports.cpp",
    "PalmDbReader.*",
    "PdfCad.*",
    "PdfDarkMode.h",
    "PdfDarkModeNoOp.cpp",
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
    "RegistryPreview.*",
    "SumatraConfig.*",
    "base/Base.*",
    "base/Arena.cpp",
    "gui/Dpi.h",
    "gui/Dpi.cpp",
    "base/File.h",
    "base/File.cpp",
    "base/Log.h",
    "base/LogNoOp.cpp",
    "base/WinDynCalls.h",
    "base/WinDynCalls.cpp",
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
    "RegistrySearchFilter.*",
    "SumatraConfig.*",
    "base/Base.*",
    "base/Arena.cpp",
    "gui/Dpi.h",
    "gui/Dpi.cpp",
    "base/File.h",
    "base/File.cpp",
    "base/Log.h",
    "base/LogNoOp.cpp",
    "base/WinDynCalls.h",
    "base/WinDynCalls.cpp",
    "base/Win.*",
  })
end

function a_gumbo_files()
  files {
    "ext/a-gumbo/gumbo.c",
    "ext/a-gumbo/gumbo.h",
    "ext/a-gumbo/version.txt",
  }
end

function sumatrapdf_tool_files()
  files_in_dir("src", {
    "CrashHandlerNoOp.cpp",
    "EmbeddedResources.*",
    "SumatraLog.*",
    "sumatrapdf-tool.cpp",
  })
end
