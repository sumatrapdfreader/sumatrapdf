# SumatraPDF command-line tools

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

You can run `sumatrapdf-tool.exe <tool>` to perform operations on PDF and other files.

Convert, extract, compress and more.

> `sumatrapdf-tool.exe` is a small console program installed next to `SumatraPDF.exe`. The command-line tools only work after SumatraPDF has been installed (not in the single-file portable build). See [Installation](Installation.md).

## I want to…

| Goal | Tool / doc |
| --- | --- |
| Delete specific pages | `clean` — [Delete pages from PDF](Tool-x-delete-pages-from-pdf.md) |
| Delete the last page without knowing the count | `clean input.pdf out.pdf 1-N-1` (`N` = last page) |
| Extract pages into a new PDF | `clean` — [Extract pages from PDF](Tool-x-extract-pages-from-pdf.md) |
| Merge PDFs | `merge` — [Tool merge](Tool-merge.md) |
| Search text in one or many PDFs | `grep` — [Tool grep](Tool-grep.md) |
| Extract plain text | `draw -tt` or `grep` — [Extract text from PDF](Tool-x-extract-text-from-pdf.md) |
| Extract embedded images | `extract` — [Extract images from PDF](Tool-x-extract-images-from-pdf.md) |
| Compress / rewrite PDF | `clean` — [Compress a PDF](Tool-x-compress-pdf.md) |
| Remove compression streams | `clean` — [Decompress a PDF](Tool-x-decompress-pdf.md) |
| Password-protect a PDF | `clean` — [Encrypt a PDF](Tool-x-encrypt-pdf-with-password.md) |
| Remove password | `clean` — [Decrypt a PDF](Tool-x-decrypt-pdf.md) |
| Convert images / other formats to PDF | `convert` / `draw` — [Tool convert](Tool-convert.md) |
| Inspect PDF structure | `info`, `show`, `pages` — [Tool info](Tool-info.md) |
| Run JavaScript on PDFs | `run` — [Tool run](Tool-run.md) |

The same page-delete operations are also available in the app: `Ctrl + K` → **Delete Pages From PDF**, or context menu **Document → Delete Pages From PDF**.

```
sumatrapdf-tool.exe <command> [options]
  draw    -- convert document
  convert -- convert document (with simpler options)
  audit   -- produce usage stats from PDF files
  bake    -- bake PDF form into static content
  clean   -- rewrite PDF file
  create  -- create PDF document
  extract -- extract font and image resources
  info    -- show information about PDF resources
  merge   -- merge pages from multiple PDF sources into a new PDF
  pages   -- show information about PDF pages
  poster  -- split large PDF page into many tiles
  recolor -- change colorspace of PDF document
  show    -- show internal PDF objects
  trim    -- trim PDF page contents
  grep    -- search for text
  trace   -- trace device calls
  run     -- run a MuPDF JavaScript program
```

Tools:

- [draw](Tool-draw.md)
- [convert](Tool-convert.md)
- [audit](Tool-audit.md)
- [bake](Tool-bake.md)
- [clean](Tool-clean.md)
- [extract](Tool-extract.md)
- [info](Tool-info.md)
- [merge](Tool-merge.md)
- [pages](Tool-pages.md)
- [poster](Tool-poster.md)
- [recolor](Tool-recolor.md)
- [show](Tool-show.md)
- [trim](Tool-trim.md)
- [grep](Tool-grep.md)
- [trace](Tool-trace.md)
- [run](Tool-run.md)
- [run JavaScript examples](Tool-run-javascript-examples.md)
