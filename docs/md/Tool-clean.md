# SumatraPDF clean

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

## SumatraPDF clean

Usage: `SumatraPDF clean [options] input.pdf [output.pdf] [pages]`

See [all-options](#all-options) below.

You can use `clean` to:

- compress a PDF
- decompress a PDF
- encrypt / decrypt PDF

## Compress a PDF

`SumatraPDF clean -gggg -e 100 -f -i -t -Z foo.pdf foo-compressed.pdf`

## Decompress a PDF

`SumatraPDF clean -d foo.pdf foo-decompressed.pdf`

## Encrypt a PDF

`SumatraPDF clean -E aes-256 -U pwd foo.pdf foo-encrypted.pdf`

Now to open `foo-encrypted.pdf` the user will have to provide password `pwd`.

## Decrypt a PDF

`SumatraPDF clean -D -p pwd foo-encrypted.pdf foo-decrypted.pdf`

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
