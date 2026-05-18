# MuPDF - the core PDF rendering engine
# Built from source matching the Windows build

mupdf_src_dir = '../../../mupdf/source'
mupdf_inc_dir = '../../../mupdf/include'

mupdf_fitz_sources = files(
  mupdf_src_dir / 'fitz/archive.c',
  mupdf_src_dir / 'fitz/barcode.c',
  mupdf_src_dir / 'fitz/bbox-device.c',
  mupdf_src_dir / 'fitz/bidi-std.c',
  mupdf_src_dir / 'fitz/bidi.c',
  mupdf_src_dir / 'fitz/bitmap.c',
  mupdf_src_dir / 'fitz/brotli.c',
  mupdf_src_dir / 'fitz/buffer.c',
  mupdf_src_dir / 'fitz/color-fast.c',
  mupdf_src_dir / 'fitz/color-icc-create.c',
  mupdf_src_dir / 'fitz/color-lcms.c',
  mupdf_src_dir / 'fitz/colorspace.c',
  mupdf_src_dir / 'fitz/compress.c',
  mupdf_src_dir / 'fitz/compressed-buffer.c',
  mupdf_src_dir / 'fitz/context.c',
  mupdf_src_dir / 'fitz/crypt-aes.c',
  mupdf_src_dir / 'fitz/crypt-arc4.c',
  mupdf_src_dir / 'fitz/crypt-md5.c',
  mupdf_src_dir / 'fitz/crypt-sha2.c',
  mupdf_src_dir / 'fitz/deskew.c',
  mupdf_src_dir / 'fitz/device.c',
  mupdf_src_dir / 'fitz/directory.c',
  mupdf_src_dir / 'fitz/document-all.c',
  mupdf_src_dir / 'fitz/document.c',
  mupdf_src_dir / 'fitz/draw-affine.c',
  mupdf_src_dir / 'fitz/draw-blend.c',
  mupdf_src_dir / 'fitz/draw-device.c',
  mupdf_src_dir / 'fitz/draw-edge.c',
  mupdf_src_dir / 'fitz/draw-edgebuffer.c',
  mupdf_src_dir / 'fitz/draw-glyph.c',
  mupdf_src_dir / 'fitz/draw-mesh.c',
  mupdf_src_dir / 'fitz/draw-paint.c',
  mupdf_src_dir / 'fitz/draw-path.c',
  mupdf_src_dir / 'fitz/draw-rasterize.c',
  mupdf_src_dir / 'fitz/draw-scale-simple.c',
  mupdf_src_dir / 'fitz/draw-unpack.c',
  mupdf_src_dir / 'fitz/encode-basic.c',
  mupdf_src_dir / 'fitz/encode-fax.c',
  mupdf_src_dir / 'fitz/encode-jpx.c',
  mupdf_src_dir / 'fitz/encodings.c',
  mupdf_src_dir / 'fitz/error.c',
  mupdf_src_dir / 'fitz/filter-basic.c',
  mupdf_src_dir / 'fitz/filter-brotli.c',
  mupdf_src_dir / 'fitz/filter-dct.c',
  mupdf_src_dir / 'fitz/filter-fax.c',
  mupdf_src_dir / 'fitz/filter-flate.c',
  mupdf_src_dir / 'fitz/filter-jbig2.c',
  mupdf_src_dir / 'fitz/filter-leech.c',
  mupdf_src_dir / 'fitz/filter-lzw.c',
  mupdf_src_dir / 'fitz/filter-predict.c',
  mupdf_src_dir / 'fitz/filter-sgi.c',
  mupdf_src_dir / 'fitz/filter-thunder.c',
  mupdf_src_dir / 'fitz/font.c',
  mupdf_src_dir / 'fitz/ftoa.c',
  mupdf_src_dir / 'fitz/geometry.c',
  mupdf_src_dir / 'fitz/getopt.c',
  mupdf_src_dir / 'fitz/glyph.c',
  mupdf_src_dir / 'fitz/glyphbox.c',
  mupdf_src_dir / 'fitz/gz-doc.c',
  mupdf_src_dir / 'fitz/halftone.c',
  mupdf_src_dir / 'fitz/harfbuzz.c',
  mupdf_src_dir / 'fitz/hash.c',
  mupdf_src_dir / 'fitz/heap.c',
  mupdf_src_dir / 'fitz/hyphen.c',
  mupdf_src_dir / 'fitz/image.c',
  mupdf_src_dir / 'fitz/jmemcust.c',
  mupdf_src_dir / 'fitz/json.c',
  mupdf_src_dir / 'fitz/link.c',
  mupdf_src_dir / 'fitz/list-device.c',
  mupdf_src_dir / 'fitz/load-bmp.c',
  mupdf_src_dir / 'fitz/load-gif.c',
  mupdf_src_dir / 'fitz/load-jbig2.c',
  mupdf_src_dir / 'fitz/load-jpeg.c',
  mupdf_src_dir / 'fitz/load-jpx.c',
  mupdf_src_dir / 'fitz/load-png.c',
  mupdf_src_dir / 'fitz/load-pnm.c',
  mupdf_src_dir / 'fitz/load-psd.c',
  mupdf_src_dir / 'fitz/load-jxr.c',
  mupdf_src_dir / 'fitz/load-tiff.c',
  mupdf_src_dir / 'fitz/log.c',
  mupdf_src_dir / 'fitz/memento.c',
  mupdf_src_dir / 'fitz/memory.c',
  mupdf_src_dir / 'fitz/noto.c',
  mupdf_src_dir / 'fitz/ocr-device.c',
  mupdf_src_dir / 'fitz/outline.c',
  mupdf_src_dir / 'fitz/output-cbz.c',
  mupdf_src_dir / 'fitz/output-csv.c',
  mupdf_src_dir / 'fitz/output-docx.c',
  mupdf_src_dir / 'fitz/output-jpeg.c',
  mupdf_src_dir / 'fitz/output-pcl.c',
  mupdf_src_dir / 'fitz/output-pclm.c',
  mupdf_src_dir / 'fitz/output-pdfocr.c',
  mupdf_src_dir / 'fitz/output-png.c',
  mupdf_src_dir / 'fitz/output-pnm.c',
  mupdf_src_dir / 'fitz/output-ps.c',
  mupdf_src_dir / 'fitz/output-psd.c',
  mupdf_src_dir / 'fitz/output-pwg.c',
  mupdf_src_dir / 'fitz/output-svg.c',
  mupdf_src_dir / 'fitz/output.c',
  mupdf_src_dir / 'fitz/path.c',
  mupdf_src_dir / 'fitz/pixmap.c',
  mupdf_src_dir / 'fitz/pool.c',
  mupdf_src_dir / 'fitz/printf.c',
  mupdf_src_dir / 'fitz/random.c',
  mupdf_src_dir / 'fitz/separation.c',
  mupdf_src_dir / 'fitz/shade.c',
  mupdf_src_dir / 'fitz/skew.c',
  mupdf_src_dir / 'fitz/stext-boxer.c',
  mupdf_src_dir / 'fitz/stext-classify.c',
  mupdf_src_dir / 'fitz/stext-device.c',
  mupdf_src_dir / 'fitz/stext-iterator.c',
  mupdf_src_dir / 'fitz/stext-output.c',
  mupdf_src_dir / 'fitz/stext-para.c',
  mupdf_src_dir / 'fitz/stext-raft.c',
  mupdf_src_dir / 'fitz/stext-search.c',
  mupdf_src_dir / 'fitz/stext-table.c',
  mupdf_src_dir / 'fitz/store.c',
  mupdf_src_dir / 'fitz/stream-open.c',
  mupdf_src_dir / 'fitz/stream-read.c',
  mupdf_src_dir / 'fitz/string.c',
  mupdf_src_dir / 'fitz/strtof.c',
  mupdf_src_dir / 'fitz/subset-cff.c',
  mupdf_src_dir / 'fitz/subset-ttf.c',
  mupdf_src_dir / 'fitz/svg-device.c',
  mupdf_src_dir / 'fitz/test-device.c',
  mupdf_src_dir / 'fitz/text-decoder.c',
  mupdf_src_dir / 'fitz/text.c',
  mupdf_src_dir / 'fitz/time.c',
  mupdf_src_dir / 'fitz/trace-device.c',
  mupdf_src_dir / 'fitz/track-usage.c',
  mupdf_src_dir / 'fitz/transition.c',
  mupdf_src_dir / 'fitz/tree.c',
  mupdf_src_dir / 'fitz/ucdn.c',
  mupdf_src_dir / 'fitz/uncfb.c',
  mupdf_src_dir / 'fitz/unlibarchive.c',
  mupdf_src_dir / 'fitz/untar.c',
  mupdf_src_dir / 'fitz/unzip.c',
  mupdf_src_dir / 'fitz/util.c',
  mupdf_src_dir / 'fitz/warp.c',
  mupdf_src_dir / 'fitz/writer.c',
  mupdf_src_dir / 'fitz/xml-write.c',
  mupdf_src_dir / 'fitz/xml.c',
  mupdf_src_dir / 'fitz/xmltext-device.c',
  mupdf_src_dir / 'fitz/zip.c',
)

mupdf_pdf_sources = files(
  mupdf_src_dir / 'pdf/pdf-af.c',
  mupdf_src_dir / 'pdf/pdf-annot.c',
  mupdf_src_dir / 'pdf/pdf-appearance.c',
  mupdf_src_dir / 'pdf/pdf-clean-file.c',
  mupdf_src_dir / 'pdf/pdf-clean.c',
  mupdf_src_dir / 'pdf/pdf-cmap-load.c',
  mupdf_src_dir / 'pdf/pdf-cmap-parse.c',
  mupdf_src_dir / 'pdf/pdf-cmap.c',
  mupdf_src_dir / 'pdf/pdf-colorspace.c',
  mupdf_src_dir / 'pdf/pdf-crypt.c',
  mupdf_src_dir / 'pdf/pdf-device.c',
  mupdf_src_dir / 'pdf/pdf-event.c',
  mupdf_src_dir / 'pdf/pdf-font-add.c',
  mupdf_src_dir / 'pdf/pdf-font.c',
  mupdf_src_dir / 'pdf/pdf-form.c',
  mupdf_src_dir / 'pdf/pdf-function.c',
  mupdf_src_dir / 'pdf/pdf-graft.c',
  mupdf_src_dir / 'pdf/pdf-image-rewriter.c',
  mupdf_src_dir / 'pdf/pdf-image.c',
  mupdf_src_dir / 'pdf/pdf-interpret.c',
  mupdf_src_dir / 'pdf/pdf-js.c',
  mupdf_src_dir / 'pdf/pdf-label.c',
  mupdf_src_dir / 'pdf/pdf-layer.c',
  mupdf_src_dir / 'pdf/pdf-layout.c',
  mupdf_src_dir / 'pdf/pdf-lex.c',
  mupdf_src_dir / 'pdf/pdf-link.c',
  mupdf_src_dir / 'pdf/pdf-metrics.c',
  mupdf_src_dir / 'pdf/pdf-nametree.c',
  mupdf_src_dir / 'pdf/pdf-object.c',
  mupdf_src_dir / 'pdf/pdf-op-buffer.c',
  mupdf_src_dir / 'pdf/pdf-op-color.c',
  mupdf_src_dir / 'pdf/pdf-op-filter.c',
  mupdf_src_dir / 'pdf/pdf-op-run.c',
  mupdf_src_dir / 'pdf/pdf-op-vectorize.c',
  mupdf_src_dir / 'pdf/pdf-outline.c',
  mupdf_src_dir / 'pdf/pdf-page.c',
  mupdf_src_dir / 'pdf/pdf-parse.c',
  mupdf_src_dir / 'pdf/pdf-pattern.c',
  mupdf_src_dir / 'pdf/pdf-recolor.c',
  mupdf_src_dir / 'pdf/pdf-repair.c',
  mupdf_src_dir / 'pdf/pdf-resources.c',
  mupdf_src_dir / 'pdf/pdf-run.c',
  mupdf_src_dir / 'pdf/pdf-shade-recolor.c',
  mupdf_src_dir / 'pdf/pdf-shade.c',
  mupdf_src_dir / 'pdf/pdf-signature.c',
  mupdf_src_dir / 'pdf/pdf-store.c',
  mupdf_src_dir / 'pdf/pdf-stream.c',
  mupdf_src_dir / 'pdf/pdf-subset.c',
  mupdf_src_dir / 'pdf/pdf-type3.c',
  mupdf_src_dir / 'pdf/pdf-unicode.c',
  mupdf_src_dir / 'pdf/pdf-util.c',
  mupdf_src_dir / 'pdf/pdf-write.c',
  mupdf_src_dir / 'pdf/pdf-xobject.c',
  mupdf_src_dir / 'pdf/pdf-xref.c',
  mupdf_src_dir / 'pdf/pdf-zugferd.c',
)

mupdf_html_sources = files(
  mupdf_src_dir / 'html/css-apply.c',
  mupdf_src_dir / 'html/css-parse.c',
  mupdf_src_dir / 'html/epub-doc.c',
  mupdf_src_dir / 'html/html-doc.c',
  mupdf_src_dir / 'html/html-font.c',
  mupdf_src_dir / 'html/html-layout.c',
  mupdf_src_dir / 'html/html-outline.c',
  mupdf_src_dir / 'html/html-parse.c',
  mupdf_src_dir / 'html/mobi.c',
  mupdf_src_dir / 'html/office.c',
  mupdf_src_dir / 'html/story-writer.c',
  mupdf_src_dir / 'html/txt.c',
  mupdf_src_dir / 'html/xml-dom.c',
)

mupdf_cbz_sources = files(
  mupdf_src_dir / 'cbz/mucbz.c',
  mupdf_src_dir / 'cbz/muimg.c',
)

mupdf_svg_sources = files(
  mupdf_src_dir / 'svg/svg-color.c',
  mupdf_src_dir / 'svg/svg-doc.c',
  mupdf_src_dir / 'svg/svg-parse.c',
  mupdf_src_dir / 'svg/svg-run.c',
)

mupdf_xps_sources = files(
  mupdf_src_dir / 'xps/xps-common.c',
  mupdf_src_dir / 'xps/xps-doc.c',
  mupdf_src_dir / 'xps/xps-glyphs.c',
  mupdf_src_dir / 'xps/xps-gradient.c',
  mupdf_src_dir / 'xps/xps-image.c',
  mupdf_src_dir / 'xps/xps-link.c',
  mupdf_src_dir / 'xps/xps-outline.c',
  mupdf_src_dir / 'xps/xps-path.c',
  mupdf_src_dir / 'xps/xps-resource.c',
  mupdf_src_dir / 'xps/xps-tile.c',
  mupdf_src_dir / 'xps/xps-util.c',
  mupdf_src_dir / 'xps/xps-zip.c',
)

mupdf_reflow_sources = files(
  mupdf_src_dir / 'reflow/reflow-doc.c',
)

mupdf_helpers_sources = files(
  mupdf_src_dir / 'helpers/mu-threads/mu-threads.c',
)

mupdf_inc = include_directories(mupdf_inc_dir)

mupdf_all_sources = (
  mupdf_fitz_sources +
  mupdf_pdf_sources +
  mupdf_html_sources +
  mupdf_cbz_sources +
  mupdf_svg_sources +
  mupdf_xps_sources +
  mupdf_reflow_sources +
  mupdf_helpers_sources
)

# MuPDF defines for the build
mupdf_c_args = [
  '-DHAVE_OBJCOPY',             # Use ld -r -b binary for font/resource embedding
  '-DTOFU',                     # Reduced font embedding
  '-DTOFU_CJK_EXT',
  '-DSHARE_JPEG',               # Share libjpeg-turbo
  '-DHB_NO_MT',                 # HarfBuzz single-threaded
  '-DOPJ_STATIC',               # Static OpenJPEG
  '-DOPJ_HAVE_STDINT_H',
  '-DHAVE_LCMS2MT=0',
  '-DOCR_DISABLED',             # No OCR support for now
  '-DFZ_ENABLE_XPS=1',
  '-DFZ_ENABLE_SVG=1',
  '-DFZ_ENABLE_CBZ=1',
  '-DFZ_ENABLE_IMG=1',
  '-DFZ_ENABLE_HTML=1',
  '-DFZ_ENABLE_EPUB=1',
  '-DFZ_ENABLE_JS=1',
  '-include', meson.current_source_dir() / 'compat-linux.h',
]

mupdf_lib = static_library('mupdf', mupdf_all_sources,
  include_directories: [
    mupdf_inc,
    zlib_inc,
    freetype_inc,
    harfbuzz_inc,
    jpegturbo_inc,
    openjpeg_inc,
    jbig2dec_inc,
    lcms2_inc,
    brotli_inc,
    extract_inc,
    mujs_inc,
    gumbo_inc,
    libwebp_inc,
    include_directories('../../../ext/libarchive/libarchive'),
  ],
  c_args: mupdf_c_args + ['-D_GNU_SOURCE'],
  dependencies: [
    zlib_dep,
    freetype_dep,
    harfbuzz_dep,
    jpegturbo_dep,
    openjpeg_dep,
    jbig2dec_dep,
    lcms2_dep,
    brotli_dep,
    extract_dep,
    mujs_dep,
    gumbo_dep,
    libwebp_dep,
  ],
)

mupdf_dep = declare_dependency(
  link_with: mupdf_lib,
  include_directories: mupdf_inc,
)

# Embed font and hyphenation resources as linkable objects using ld -r -b binary.
# This matches MuPDF's own Makefile approach (HAVE_OBJCOPY path).
# Symbol names are derived from the relative path within the mupdf directory.

mupdf_root = meson.project_source_root() / 'mupdf'
embed_script = find_program(meson.current_source_dir() / 'embed-resource.sh')

font_resources = [
  'resources/fonts/urw/Dingbats.cff',
  'resources/fonts/urw/NimbusMonoPS-Bold.cff',
  'resources/fonts/urw/NimbusMonoPS-BoldItalic.cff',
  'resources/fonts/urw/NimbusMonoPS-Italic.cff',
  'resources/fonts/urw/NimbusMonoPS-Regular.cff',
  'resources/fonts/urw/NimbusRoman-Bold.cff',
  'resources/fonts/urw/NimbusRoman-BoldItalic.cff',
  'resources/fonts/urw/NimbusRoman-Italic.cff',
  'resources/fonts/urw/NimbusRoman-Regular.cff',
  'resources/fonts/urw/NimbusSans-Bold.cff',
  'resources/fonts/urw/NimbusSans-BoldItalic.cff',
  'resources/fonts/urw/NimbusSans-Italic.cff',
  'resources/fonts/urw/NimbusSans-Regular.cff',
  'resources/fonts/urw/StandardSymbolsPS.cff',
  'resources/fonts/droid/DroidSansFallback.ttf',
]

hyph_resources = [
  'resources/hyphen/hyph-std.zip',
  'resources/hyphen/hyph-all.zip',
]

mupdf_resource_objs = []
foreach res : font_resources + hyph_resources
  name = res.underscorify()
  mupdf_resource_objs += custom_target(name,
    output: name + '.o',
    command: [embed_script, mupdf_root, res, '@OUTPUT@'],
  )
endforeach
