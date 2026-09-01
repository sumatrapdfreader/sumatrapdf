# Our changes to mupdf, as patches

`ext/mupdf` is a vendored copy of mupdf that we have edited in place. That makes
updating painful: there is no record of *what* we changed or *why*, so every
update means re-deriving our changes from a 1400-file diff.

This directory is that record. Each `.patch` is one logical change against the
mupdf revision we vendored, in `git diff` format, with a description of what it
does and why.

**Base revision: mupdf `1.28.2`** (tag `1.28.2`, commit `fe374accd`), the
version recorded for mupdf in `ext/versions.txt`. Paths in the patches are
relative to `ext/mupdf`, so `-p1` from inside that directory.

## The patches

| Patch | What |
| --- | --- |
| `0001-tools-usage-say-sumatrapdf` | usage text names `SumatraPDF <tool>`, not `mutool` |
| `0002-tools-reset-fz-optind` | tool mains can be called more than once in-process |
| `0003-signatures-windows-pkcs7` | Windows CryptoAPI pkcs7 helper instead of OpenSSL (`pkcs7-windows.[ch]` are ours) |
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
| `0016-stext-search-mujs-include-path` | we build the amalgamated `ext/a-mujs` |
| `0017-svg-font-attributes-on-groups` | children of `<g>` inherit the font family |
| `0018-pdf-op-run-avoid-double-free` | double free when structure-tree repair throws |
| `0019-freetype-enable-zlib-and-brotli` | our freetype has them; upstream's slim config does not |
| `0020-fonts-noto-subset-for-sumatra` | `TOFU_NOTO_SUMATRA` subset of the Noto fallback fonts |
| `0025-webp-images` | decode WebP via libwebp (`HAVE_WEBP`) so EPUB/HTML/MOBI/CBZ can show `.webp` (#3415) |
| `0027-webp-iccp-without-demux` | apply a WebP `ICCP` chunk via our own RIFF walk (no libwebp demux) |
| `0030-pdf-subset-base14-font-name` | strip `ABCDEF+` subset tags so a non-embedded `XXXXXX+Symbol` uses the builtin Symbol font (#4655) |
| `0031-html-image-page-height` | shrink every reflow image against the fixed page height, not advancing block bounds (#6007) |
| `0032-pdf-appearance-unrendered-annots` | placeholder AP for Movie/Screen/3D/RichMedia/Watermark/PrinterMark/TrapNet/Projection |
| `0033-pdf-appearance-markup-movie-poster` | highlight default yellow, markup `/Rect` if no QuadPoints, skip 0-width unfilled Square/Circle, Movie `/Poster` as AP |
| `0034-backport-709678-cjk-fullwidth-punctuation` | half/fullwidth forms and CJK punctuation stay on the non-embedded CJK path (covers #6082) |
| `0035-backport-709680-flow-anchor-top` | HTML/EPUB link targets use the top of the flow node, not its baseline (covers #6095) |
| `0036-ocg-usage-event-on-visible` | PrintState/ViewState ON draws the OCG even if it is in the config `/OFF` list (#6101) |
| `0037-backport-709648-inline-context-after-block` | stop adding to an inline context after a block interrupts it (covers #5943) |

And nine that are not ours but that we carry ahead of the release we vendor:

| Patch | What |
| --- | --- |
| `0021-backport-709471-single-line-field-box` | upstream fix for the single-line field content box and a zero `/DA` font size |
| `0022-backport-709480-bound-xml-recursion` | upstream depth limits for XPS metadata and epub outlines (covers #5032) |
| `0023-backport-709574-html-metadata` | upstream title/author/subject metadata for HTML and FB2 (covers #2254) |
| `0024-backport-5e5ef9e-pool-asprintf` | upstream `fz_pool_asprintf` (needed by 0026) |
| `0026-backport-709657-fb2-author` | upstream FB2 author walk: every `<author>`, first-name + last-name (covers #2254) |
| `0029-backport-709660-tj-array-tc-tw` | recover after `Tc`/`Tw` inside a `TJ` array so the rest of the page still draws (covers #4157) |
| `0034-backport-709678-cjk-fullwidth-punctuation` | half/fullwidth forms and CJK punctuation use the CJK fonts, not an embedded fallback (#6082) |
| `0035-backport-709680-flow-anchor-top` | HTML/EPUB link targets use the top of the flow node, not its baseline (#6095) |
| `0037-backport-709648-inline-context-after-block` | nested `<span id>` wrapping a block no longer all jump to the chapter start (#5943) |

That is the whole list: `ext/mupdf` is byte-for-byte `1.28.2` plus these
patches, and nothing else.

## Backports

A patch named `backport-*` is an upstream commit we apply early, because it is
on mupdf `master` but not in the release we vendor (the `1.28.x` tags come off a
maintenance branch that forked before it). **Delete it when the vendored mupdf
moves past the commit it names** — otherwise the next update will try to apply
a change the base already contains.

Prefer a backport to a patch of our own whenever upstream has fixed the same
thing: it is code we do not have to re-merge, and upstream usually covers more
cases. `0022` replaced our own XPS depth limit for exactly that reason — it
guards the two epub outline parsers as well, and `0023` is our own FB2 metadata
patch after Artifex upstreamed it ("Based on a patch from Krzysztof Kowalczyk of
SumatraPDF"). `0026` replaced our FB2 author-name helper the same way, and
`0029` replaced our TJ `Tc`/`Tw` break. Check before writing a new patch, and
check again at each update, since upstream may have caught up.

## Applying them

```sh
git -C ~/src/mupdf worktree add /tmp/mupdf-new <new-tag>
cd /tmp/mupdf-new
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
git -C ~/src/mupdf -c core.autocrlf=false -c core.eol=lf archive 1.28.2 | tar -x -C /tmp/check
cd /tmp/check
for p in ~/src/sumatrapdf/ext/patches/*.patch; do git apply "$p" || echo "FAIL $p"; done
diff -r /tmp/check ~/src/sumatrapdf/ext/mupdf   # only reports files we do not vendor
```

That is a byte comparison, and it passes for all 1407 vendored files as of this
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
