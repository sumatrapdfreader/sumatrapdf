# SumatraPDF clean

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

**Usage:** `SumatraPDF clean [options] input.pdf [output.pdf] [pages]`

See [all-options](#all-options) below.

`clean` rewrites s PDF file. You can:

- compress a PDF
- decompress a PDF
- encrypt / decrypt PDF
- extract / remove pages

## Compress a PDF

`SumatraPDF clean -gggg -e 100 -f -i -t -Z foo.pdf foo-compressed.pdf`

## Decompress a PDF

`SumatraPDF clean -d foo.pdf foo-decompressed.pdf`

## Encrypt a PDF

`SumatraPDF clean -E aes-256 -U pwd foo.pdf foo-encrypted.pdf`

Now to open `foo-encrypted.pdf` the user will have to provide password `pwd`.

## Decrypt a PDF

`SumatraPDF clean -D -p pwd foo-encrypted.pdf foo-decrypted.pdf`

## Extract pages from PDF as PDF

`SumatraPDF clean input.pdf output.pdf 1,3-5`

This creates PDF output.pdf with pages 1,3,4,5 of PDF input.pdf

## Delete a page from PDF

Let's say you have `input.pdf` with 8 pages. To delete a page 4:

`SumatraPDF clean input.pdf output.pdf 1-3,5-N`

`N` represents last page.

### Extract 2nd page

`SumatraPDF draw -o foo-page-2.pdf foo.pdf 2`

### Extract pages 1,2,7,8 into a separate file each

`SumatraPDF draw -o "foo-page-%d.pdf" foo.pdf 1-2,7,8`

### Delete 3rd page

`SumatraPDF draw -o foo-3rd-page-deleted.pdf foo.pdf 1-2,4-8`

## All options

```
Usage: SumatraPDF clean [options] input.pdf [output.pdf] [pages]
  -p -    password
  -g      garbage collect unused objects
  -gg     in addition to -g compact xref table
  -ggg    in addition to -gg merge duplicate objects
  -gggg   in addition to -ggg check streams for duplication
  -D      save file without encryption
  -E -    save file with new encryption (rc4-40, rc4-128, aes-128, or aes-256)
  -O -    owner password (only if encrypting)
  -U -    user password (only if encrypting)
  -P -    permission flags (only if encrypting)
  -a      ascii hex encode binary streams
  -d      decompress streams
  -z      deflate uncompressed streams
  -e -    compression "effort" (0 = default, 1 = min, 100 = max)
  -f      compress font streams
  -i      compress image streams
  -c      clean content streams
  -s      sanitize content streams
  -t      compact object syntax
  -tt     indented object syntax
  -L      write object labels
  -v      vectorize text
  -A      create appearance streams for annotations
  -AA     recreate appearance streams for annotations
  -m      preserve metadata
  -S      subset fonts if possible [EXPERIMENTAL!]
  -Z      use objstms if possible for extra compression
  --{color,gray,bitonal}-{,lossy-,lossless-}image-subsample-method -
          average, bicubic
  --{color,gray,bitonal}-{,lossy-,lossless-}image-subsample-dpi -[,-]
          DPI at which to subsample [+ target dpi]
  --{color,gray,bitonal}-{,lossy-,lossless-}image-recompress-method -[:quality]
          never, same, lossless, jpeg, j2k, fax, jbig2
  --recompress-images-when -
          smaller (default), always
  --structure=keep|drop   Keep or drop the structure tree
  pages   comma separated list of page numbers and ranges
```
