# MuPDF reflow image fitting repro

This bug is present in MuPDF master `9ef7ec2a6`. Regenerate the EPUB from the repository root on Windows:

```console
tar --format zip -cf tests\issue-6007.epub -C tests\issue-6007-data mimetype META-INF OEBPS
```

Reproduce with:

```console
mutool draw -a -W 200 -H 120 -r 72 -o page-%d.png issue-6007.epub
```

Expected: each 100x240 image is scaled down to fit one 200x120 page, producing three pages.

Actual: MuPDF master produces ten pages. Only the first image is scaled down; later images keep their intrinsic height and are clipped across multiple pages because `layout_flow()` calculates `page_h` from the advancing block bounds instead of the fixed page bounds.
