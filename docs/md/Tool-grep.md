# SumatraPDF grep

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

To search text in `file.pdf`:

`SumatraPDF grep -i foo file.pdf`

`-i` ignores case i.e. does case-insensitive search, which is probably a good default.

Other options:

- `-a` ignore accents (diacritics)

## Search with a regular expression

`SumatraPDF grep -i -G x.* file.pdf`

- `-G` search pattern is a regex
- `-i` you can combine it with case-insensitive flag
- `x.*` is a regular expression that says: every string that starts with `x`

## All options

```
usage: SumatraPDF grep [options] pattern input.pdf [ input2.pdf ... ]
  -p -    password for encrypted PDF files
  -G      pattern is a regexp
  -a      ignore accents (diacritics)
  -i      ignore case
  -H      print filename for each match
  -n      print page number for each match
advanced options:
  -S      comma-separated list of search options
  -O      comma-separated list of stext options
  -b      search pages in backwards order
  -[ -    start marker
  -] -    end marker
  -v      verbose
  -q      quiet (don't print warnings and errors)

Search options:
  exact: match exact, case sensitive pattern
  ignore-case: case insensitive search
  ignore-diacritics: ignore character diacritics
  regexp: interpret search pattern as regular expression
  keep-lines: preserve line breaks so pattern can match them
  keep-paragraphs: preserve paragraph breaks so pattern can match them
  keep-hyphens: preserve hyphens, avoiding joining lines

Structured text options:
  preserve-images: keep images in output
  preserve-ligatures: do not expand ligatures into constituent characters
  preserve-spans: do not merge spans on the same line
  preserve-whitespace: do not convert all whitespace into space characters
  inhibit-spaces: don't add spaces between gaps in the text
  paragraph-break: break blocks at paragraph boundaries
  dehyphenate: attempt to join up hyphenated words
  ignore-actualtext: do not apply ActualText replacements
  use-cid-for-unknown-unicode: use character code if unicode mapping fails
  use-gid-for-unknown-unicode: use glyph index if unicode mapping fails
  accurate-bboxes: calculate char bboxes from the outlines
  accurate-ascenders: calculate ascender/descender from font glyphs
  accurate-side-bearings: expand char bboxes to completely include width of glyphs
  collect-styles: attempt to detect text features (fake bold, strikeout, underlined etc)
  clip: do not include text that is completely clipped
  clip-rect=x0:y0:x1:y1 specify clipping rectangle within which to collect content
  structured: collect structure markup
  vectors: include vector bboxes in output
  segment: attempt to segment the page
  table-hunt: hunt for tables within a (segmented) page
  resolution: resolution to render at
```
