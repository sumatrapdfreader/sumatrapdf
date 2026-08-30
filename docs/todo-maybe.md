# epub hyphenation

MuPDF can auto-hyphenate HTML-engine docs (EPUB/MOBI/FB2/HTML) when CSS is
`hyphens: auto` and the document has a language. Default CSS is
`hyphens: manual`; soft hyphens (`U+00AD`) already work without tables.

Sumatra builds MuPDF with `FZ_ENABLE_HYPHEN=0` (`cmd/deps-build-defs.ts`,
vcxproj). Lookup then always returns null and MuPDF warns
`no hyphenation table for lang='en'` (etc.). That is logged; it is not shown
as "Errors in document" (issue #6109). PDFs are unaffected.

Do not enable `FZ_ENABLE_HYPHEN` just to silence the warning.

If we want EPUB auto-hyphenation as a feature:

- `FZ_ENABLE_HYPHEN=1` and `FZ_ENABLE_HYPHEN_ALL=0` (std set includes `en`)
- embed `ext/mupdf/resources/hyphen/hyph-std.zip` (~174 KB). `hyph-all.zip`
  is ~638 KB and is MuPDF's default if `FZ_ENABLE_HYPHEN_ALL` is left at 1
- Sumatra's build does not bin2coff those zips today; flipping the define
  without embedding is a linker error (`_binary_hyph_std_zip`)
- first use per language unpacks the zip and builds a trie (CPU + RAM)
- books with `hyphens: auto` would reflow differently (page breaks vs now)
- languages not in the zip still warn

Upside: better reflow in narrow/justified EPUB columns and long compounds
when the book asks for auto hyphenation.
