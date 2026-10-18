# Our changes to mupdf, as patches

`ext/mupdf` is a vendored copy of mupdf that we have edited in place. That makes
updating painful: there is no record of *what* we changed or *why*, so every
update means re-deriving our changes from a 1400-file diff.

This directory is that record. Each `.patch` is one logical change against the
mupdf revision we vendored, in `git diff` format, with a description of what it
does and why. Whole files of our own that are compiled into the `mupdf` project
are not patches: they live in `src/mupdf/` (see its README) and are not part
of the vendored tree at all.

**Base revision: mupdf `1.28.5`** (tag `1.28.5`, commit `8ad45e92f`), the
version recorded for mupdf in `ext/versions.txt`. Paths in the patches are
relative to `ext/mupdf`, so `-p1` from inside that directory.

## The patches

| Patch | What |
| --- | --- |
| `0001-tools-usage-say-sumatrapdf` | usage text names `SumatraPDF <tool>`, not `mutool` |
| `0002-tools-reset-fz-optind` | tool mains can be called more than once in-process |
| `0003-signatures-windows-pkcs7` | tools use our Windows CryptoAPI pkcs7 helper (`src/mupdf/pkcs7-windows.[ch]`) instead of OpenSSL |
| `0004-console-io-for-gui-subsystem-exe` | stdio for a GUI-subsystem exe (#5677, #5665, #5681) |
| `0005-pdfinfo-to-buffer` | `mutool info` output as a buffer, for the properties window |
| `0006-jpeg-xr-via-windows-wic` | JPEG-XR decoding through the Windows WIC codec |
| `0007-thread-local-secret-contexts` | harfbuzz / openjpeg context smuggling is not thread safe |
| `0008-report-last-uncaught-throw` | an uncaught `fz_throw` reaches the crash report |
| `0009-forms-cp1250-latin2-encoding` | Central European Latin in form fields (#5404) |
| `0010-tounicode-from-coded-glyph-names` | `G45` / `g0045` / `C65` glyph names (#3219) |
| `0011-tounicode-cp1251-cyrillic` | Cyrillic Type1 fonts with identity-Latin ToUnicode (#5873) |
| `0012-pdf-external-file-streams` | app callback for streams that point at an external file |
| `0013-xml-recover-from-mismatched-close-tags` | badly nested FB2 / HTML (#5792) |
| `0014-html-bound-generate-boxes-recursion` | stack overflow on deeply nested markup |
| `0015-css-user-stylesheet-important-wins` | user-origin `!important` outranks inline style |
| `0019-freetype-enable-zlib-and-brotli` | our freetype has them; upstream's slim config does not |
| `0025-webp-images` | decode WebP via libwebp (`HAVE_WEBP`) so EPUB/HTML/MOBI/CBZ can show `.webp` (#3415) |
| `0027-webp-iccp-without-demux` | apply a WebP `ICCP` chunk via our own RIFF walk (no libwebp demux) |
| `0032-pdf-appearance-unrendered-annots` | placeholder AP for Movie/Screen/3D/RichMedia/Watermark/PrinterMark/TrapNet/Projection |
| `0033-pdf-appearance-markup-movie-poster` | highlight default yellow, markup `/Rect` if no QuadPoints, skip 0-width unfilled Square/Circle, Movie `/Poster` as AP |
| `0036-ocg-usage-event-on-visible` | PrintState/ViewState ON draws the OCG even if it is in the config `/OFF` list (#6101) |
| `0038-html-css-background-image` | CSS `background-image` / `-size` / `-position` / `-repeat` on block boxes; fixed-layout scan EPUBs were blank (#6131) |
| `0039-md-empty-buffer-nul-scan` | empty markdown: `len-1` underflow in `fz_md_to_html` (#6143) |
| `0040-svg-css-class-styles` | SVG `class="st0"` resolved against the `<style>` sheet; such files drew all black (#2155) |
| `0042-merge-backwards-range-bookmarks` | `merge` with a range like `3-1` renumbers the bookmarks too |
| `0043-merge-exit-code-on-failure` | `merge` exits 1 when an input or the save fails |
| `0044-svg-unsized-image` | `<image>` with no width/height uses the raster's pixel size; a percentage `<svg>` with no viewBox adopts that size (#6266) |
| `0045-console-utf8-via-writeconsolew` | UTF-8 to a Windows console through `WriteConsoleW`; the CRT failed the write on a DBCS code page (#6276) |
| `0046-grep-keep-page-of-pending-line` | `grep` read a text page the search had already dropped (#6276) |
| `0047-jpeg-xl-pdf` | JPEG XL images and PDF `/JXLDecode` via jxldec (`src/mupdf/load-jxl.cpp`) |
| `0048-pdf-compress-enum` | `compress=flate` and `compress=brotli` reach the enum; a non-boolean used to be taken as success |
| `0049-merge-catch-bad-write-options` | `merge -O` reports an unknown option instead of aborting with no `fz_try` |
| `0050-rearrange-pages-keep-old-parent-tree` | `pdf_rearrange_pages` keeps the old structure `/ParentTree` alive while it is still read; Merge PDF on a tagged PDF used freed memory |
| `0051-js-execute-contain-errors` | `pdf_js_execute` reports an error instead of rethrowing it on the JS context, which has no `fz_try` on a render thread |
| `0052-js-event-results-no-throw` | `pdf_js_event_*` results return NULL when out of memory and the `pdf-form.c` callers throw on their own context |
| `0053-pdf-sign-docmdp-certify` | a signature field with `/SumatraCertify` 1..3 signs as a certifying signature: DocMDP `/Reference` and catalog `/Perms` written inside the signed section; a field's `/Lock /P` (PDF 2.0 lock after signing) is copied into the FieldMDP transform (discussion #6315) |
| `0054-epub-linear-no-last` | EPUB spine items with `linear="no"` (footnotes, endnotes) are paged after the reading order, not where they sit in the spine; XHTML manifest items no itemref names are paged after those, so links reach them (#5803) |
| `0055-svg-data-uri-parameters` | SVG image data URIs accept media-type parameters before `;base64` (#6322) |
| `0056-html-svg-percent-size` | SVG percentage dimensions resolve against the HTML page viewport (#6324) |
| `0057-scalable-image-device-size` | SVG / display-list images rasterize at the zoomed size instead of a fixed 96 dpi; fixed-layout EPUB pages were blurry (#6330) |
| `0058-unwrap-display-list-image` | `fz_unwrap_display_list_image`: the one raster an SVG draws, so Save Image / copy keep its bytes and resolution (#6330) |
| `0059-svg-text-path` | SVG `<textPath>` places glyphs along the referenced path; such text used to pile up at the origin (#6332) |
| `0060-pdf-update-object-keeps-stream` | `pdf_update_object` carries a stream over when the entry moves to a new xref section; a stream added in the session lost its data on a `garbage=3` save |
| `0061-stext-tall-font-glyph-ink` | a font box taller than 3em (Cambria Math bbox used as ascent/descent) takes glyph ink for the vertical selection edges (#6348) |
| `0062-svg-rgba-font-face` | SVG `rgba()` colors and `@font-face` data URI fonts (WOFF/TTF) in `<style>`; such text drew black in Times (#6339) |
| `0063-epub-fixed-layout-page` | an EPUB chapter whose viewport gives width and height is one page of exactly that size, without the `@page` margin; such books had blank pages between chapters (#1293) |
| `0064-html-css-transform` | CSS `transform` / `transform-origin` on blocks and `position:absolute` on inline boxes; InDesign fixed-layout text (`scale(0.05)` around per-word positioned spans) ran off the page (#1293) |

That is the whole list: `ext/mupdf` is byte-for-byte `1.28.5` plus these
patches, and nothing else.

## Backports

A patch named `backport-*` is an upstream commit we apply early, because it is
on mupdf `master` but not in the release we vendor (the `1.28.x` tags come off a
maintenance branch that forked before it). **Delete it when the vendored mupdf
moves past the commit it names** — otherwise the next update will try to apply
a change the base already contains.

Prefer a backport to a patch of our own whenever upstream has fixed the same
thing: it is code we do not have to re-merge, and upstream usually covers more
cases. Check before writing a new patch, and check again at each update, since
upstream may have caught up. At 1.28.5 every backport we carried was in the
release, and so were our own svg `<g>` font attributes, the pdf-op-run double
free, the poster page-tree fix and the mujs regexp include (mupdf now has its
own regex engine).

## Applying them

```sh
git -C ~/src/mupdf worktree add /tmp/mupdf-new <new-tag>
cd /tmp/mupdf-new
git config core.autocrlf false   # else git apply writes CRLF on Windows
for p in ~/src/sumatrapdf/ext/patches/0*.patch; do
    git apply --3way "$p" || echo "needs hand-merging: $p"
done
```

Work through whatever needs hand-merging, copy the result over `ext/mupdf`
(keeping our file selection — we vendor a subset, not the whole tree), update
`ext/versions.txt`, and then regenerate this directory so the patches are
against the *new* base.

A conflict is sometimes good news: upstream may have fixed the same thing. That
is what happened to the old `0019-stext-device-guard-null-line` in 1.28.2 —
mupdf added the identical `cur_line &&` guard, so the patch was deleted rather
than re-merged. Always read the conflict before resolving it.

## Verifying them

Applying every patch to a pristine base must reproduce `ext/mupdf` exactly:

```sh
mkdir /tmp/check
git -C ~/src/mupdf -c core.autocrlf=false -c core.eol=lf archive 1.28.5 | tar -x -C /tmp/check
cd /tmp/check
for p in ~/src/sumatrapdf/ext/patches/*.patch; do git -c core.autocrlf=false apply "$p" || echo "FAIL $p"; done
diff -r /tmp/check ~/src/sumatrapdf/ext/mupdf   # only reports files we do not vendor
```

That is a byte comparison, and it passes for all 1410 vendored files as of this
writing. Keep it that way: if it starts reporting a vendored file, either a
patch is missing or the vendored tree drifted.

## Line endings

`ext/mupdf` uses LF throughout, like upstream, and the patches are LF-only. Do
not let an editor or a checkout rewrite it to CRLF: the repo's `.gitattributes`
sets `* -text`, so whatever bytes get committed are what everyone gets, and a
CRLF file would break the byte comparison above for no reason.

## Keeping this current

When you change something under `ext/mupdf`, add or update a patch here in the
same commit. A change that only lives in the vendored tree is a change that the
next update will silently drop.
