# Our changes to mupdf, as patches

`ext/mupdf` is a vendored copy of mupdf that we have edited in place. That makes
updating painful: there is no record of *what* we changed or *why*, so every
update means re-deriving our changes from a 1400-file diff.

This directory is that record. Each `.patch` is one logical change against the
mupdf revision we vendored, in `git diff` format, with a description of what it
does and why.

**Base revision: mupdf `1.28.0-rc2`** (tag `1.28.0-rc2`, commit `a299549f8`), the
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
| `0017-xps-bound-metadata-recursion` | stack overflow on deeply nested XPS metadata (#5032) |
| `0018-stext-search-mujs-include-path` | we build the amalgamated `ext/a-mujs` |
| `0019-stext-device-guard-null-line` | null deref on a combining mark with no current line |
| `0020-svg-font-attributes-on-groups` | children of `<g>` inherit the font family |
| `0021-pdf-op-run-avoid-double-free` | double free when structure-tree repair throws |
| `0022-freetype-enable-zlib-and-brotli` | our freetype has them; upstream's slim config does not |
| `0023-fonts-noto-subset-for-sumatra` | `TOFU_NOTO_SUMATRA` subset of the Noto fallback fonts |

Then one that is **not** ours and must **not** be carried forward:

| Patch | What |
| --- | --- |
| `9001-NOT-A-CHANGE-whitespace-drift` | reindentation a stray formatter pass left behind |

It exists only so that applying the whole series reproduces `ext/mupdf` exactly,
which is what makes the series verifiable. Drop it on the next update. It does
contain one genuine bit of damage worth fixing rather than keeping: a newline
sitting in the middle of a getopt string literal in `platform/x11/x11_main.c`
(not built on Windows, which is why nobody noticed).

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

## Verifying them

Applying every patch to a pristine base must reproduce `ext/mupdf`:

```sh
git -C ~/src/mupdf -c core.autocrlf=false archive 1.28.0-rc2 | tar -x -C /tmp/check
cd /tmp/check
for p in ~/src/sumatrapdf/ext/patches/*.patch; do git apply "$p" || echo "FAIL $p"; done
diff -r --strip-trailing-cr /tmp/check ~/src/sumatrapdf/ext/mupdf   # only files we do not vendor
```

That check passes for all 1407 vendored files as of this writing.

## Line endings

The patches are LF-only. Most files under `ext/mupdf` are checked out with CRLF,
so compare with `diff --strip-trailing-cr` (or `git apply` on an LF tree) rather
than byte for byte. Line endings are a checkout artifact, not one of our
changes.

## Keeping this current

When you change something under `ext/mupdf`, add or update a patch here in the
same commit. A change that only lives in the vendored tree is a change that the
next update will silently drop.
