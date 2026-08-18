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
| `0003-signatures-windows-pkcs7` | Windows CryptoAPI pkcs7 helper instead of OpenSSL |
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
| `0015-html-fb2-author-and-annotation` | FB2 author + annotation as document metadata |
| `0016-css-user-stylesheet-important-wins` | user-origin `!important` outranks inline style |
| `0017-stext-search-mujs-include-path` | we build the amalgamated `ext/a-mujs` |
| `0018-svg-font-attributes-on-groups` | children of `<g>` inherit the font family |
| `0019-pdf-op-run-avoid-double-free` | double free when structure-tree repair throws |
| `0020-freetype-enable-zlib-and-brotli` | our freetype has them; upstream's slim config does not |
| `0021-fonts-noto-subset-for-sumatra` | `TOFU_NOTO_SUMATRA` subset of the Noto fallback fonts |

And two that are not ours but that we carry ahead of the release we vendor:

| Patch | What |
| --- | --- |
| `0022-backport-709471-single-line-field-box` | upstream fix for the single-line field content box and a zero `/DA` font size |
| `0023-backport-709480-bound-xml-recursion` | upstream depth limits for XPS metadata and epub outlines (covers #5032) |

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
cases. `0023` replaced our own XPS depth limit for exactly that reason — it
guards the two epub outline parsers as well. Check before writing a new patch,
and check again at each update, since upstream may have caught up.

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
