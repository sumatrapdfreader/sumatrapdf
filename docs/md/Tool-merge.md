# SumatraPDF merge

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

```
Usage: SumatraPDF merge [-o output.pdf] [-O options] input.pdf [pages] [input2.pdf] [pages2] ...
  -o -    name of PDF file to create
  -O -    comma separated list of output options
  input.pdf       name of input file from which to copy pages
  pages   comma separated list of page numbers and ranges

PDF output options:
  decompress: decompress all streams (except compress-fonts/images)
  compress=yes|flate|brotli: compress all streams, yes defaults to flate
  compress-fonts: compress embedded fonts
  compress-images: compress images
  compress-effort=0|percentage: effort spent compressing, 0 is default, 100 is max effort
  ascii: ASCII hex encode binary streams
  pretty: pretty-print objects with indentation
  labels: print object labels
  clean: pretty-print graphics commands in content streams
  sanitize: sanitize graphics commands in content streams
  garbage: garbage collect unused objects
  or garbage=compact: ... and compact cross reference table
  or garbage=deduplicate: ... and remove duplicate objects
  incremental: write changes as incremental update
  objstms: use object streams and cross reference streams
  appearance=yes|all: synthesize just missing, or all, annotation/widget appearance streams
  continue-on-error: continue saving the document even if there is an error
  decrypt: write unencrypted document
  encrypt=rc4-40|rc4-128|aes-128|aes-256: write encrypted document
  permissions=NUMBER: document permissions to grant when encrypting
  user-password=PASSWORD: password required to read document
  owner-password=PASSWORD: password required to edit document
  regenerate-id: (default yes) regenerate document id
```
