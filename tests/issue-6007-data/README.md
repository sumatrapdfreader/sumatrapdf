# MuPDF reflow image fitting repro

This bug was present in MuPDF master `9ef7ec2a6` and fixed upstream in
`b8d245e4e` (bug 709663), which we carry as
`ext/patches/0031-backport-709663-image-page-height.patch`.

Regenerate the EPUB from the repository root on Windows:

```console
tar --format zip -cf tests\issue-6007.epub -C tests\issue-6007-data mimetype META-INF OEBPS
```

Reproduce with:

```console
mutool draw -a -W 200 -H 120 -r 72 -o page-%d.png issue-6007.epub
```

Expected: each 100x240 image is scaled down to fit one 200x120 page, producing three pages.

Actual, without the fix: ten pages. Only the first image is scaled down; later images keep their intrinsic height and are clipped across multiple pages because `layout_flow()` calculates `page_h` from the advancing block bounds instead of the fixed page bounds.
