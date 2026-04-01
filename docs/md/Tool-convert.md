# SumatraPDF convert

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

```
Usage: SumatraPDF convert [options] file [pages]
  -p -    password

  -b -    use named page box (MediaBox, CropBox, BleedBox, TrimBox, or ArtBox)
  -A -    number of bits of antialiasing (0 to 8)
  -W -    page width for EPUB layout
  -H -    page height for EPUB layout
  -S -    font size for EPUB layout
  -U -    file name of user stylesheet for EPUB layout
  -X      disable document styles for EPUB layout

  -o -    output file name (%d for page number)
  -F -    output format (default inferred from output file name)
                  raster: cbz, png, pnm, pgm, ppm, pam, pbm, pkm.
                  print-raster: pcl, pclm, ps, pwg.
                  vector: pdf, svg.
                  text: html, xhtml, text, stext.
  -O -    comma separated list of options for output format

  pages   comma separated list of page ranges (N=last page)

Raster output options:
  rotate=N: rotate rendered pages N degrees counterclockwise
  resolution=N: set both X and Y resolution in pixels per inch
  x-resolution=N: X resolution of rendered pages in pixels per inch
  y-resolution=N: Y resolution of rendered pages in pixels per inch
  width=N: render pages to fit N pixels wide (ignore resolution option)
  height=N: render pages to fit N pixels tall (ignore resolution option)
  colorspace=(gray|rgb|cmyk): render using specified colorspace
  alpha: render pages with alpha channel and transparent background
  graphics=(aaN|cop|app): set the rasterizer to use
  text=(aaN|cop|app): set the rasterizer to use for text
          aaN=antialias with N bits (0 to 8)
          cop=center of pixel
          app=any part of pixel

PCL output options:
  colorspace=mono: render 1-bit black and white page
  colorspace=rgb: render full color page
  preset=generic|ljet4|dj500|fs600|lj|lj2|lj3|lj3d|lj4|lj4pl|lj4d|lp2563b|oce9050
  spacing=0: No vertical spacing capability
  spacing=1: PCL 3 spacing (<ESC>*p+<n>Y)
  spacing=2: PCL 4 spacing (<ESC>*b<n>Y)
  spacing=3: PCL 5 spacing (<ESC>*b<n>Y and clear seed row)
  mode2: Enable mode 2 graphics compression
  mode3: Enable mode 3 graphics compression
  eog_reset: End of graphics (<ESC>*rB) resets all parameters
  has_duplex: Duplex supported (<ESC>&l<duplex>S)
  has_papersize: Papersize setting supported (<ESC>&l<sizecode>A)
  has_copies: Number of copies supported (<ESC>&l<copies>X)
  is_ljet4pjl: Disable/Enable HP 4PJL model-specific output
  is_oce9050: Disable/Enable Oce 9050 model-specific output

PCLm output options:
  compression=none: No compression (default)
  compression=flate: Flate compression
  strip-height=N: Strip height (default 16)

PWG output options:
  media_class=<string>: set the media_class field
  media_color=<string>: set the media_color field
  media_type=<string>: set the media_type field
  output_type=<string>: set the output_type field
  rendering_intent=<string>: set the rendering_intent field
  page_size_name=<string>: set the page_size_name field
  advance_distance=<int>: set the advance_distance field
  advance_media=<int>: set the advance_media field
  collate=<int>: set the collate field
  cut_media=<int>: set the cut_media field
  duplex=<int>: set the duplex field
  insert_sheet=<int>: set the insert_sheet field
  jog=<int>: set the jog field
  leading_edge=<int>: set the leading_edge field
  manual_feed=<int>: set the manual_feed field
  media_position=<int>: set the media_position field
  media_weight=<int>: set the media_weight field
  mirror_print=<int>: set the mirror_print field
  negative_print=<int>: set the negative_print field
  num_copies=<int>: set the num_copies field
  orientation=<int>: set the orientation field
  output_face_up=<int>: set the output_face_up field
  page_size_x=<int>: set the page_size_x field
  page_size_y=<int>: set the page_size_y field
  separations=<int>: set the separations field
  tray_switch=<int>: set the tray_switch field
  tumble=<int>: set the tumble field
  media_type_num=<int>: set the media_type_num field
  compression=<int>: set the compression field
  row_count=<int>: set the row_count field
  row_feed=<int>: set the row_feed field
  row_step=<int>: set the row_step field

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

SVG output options:
  text=text: Emit text as <text> elements (inaccurate fonts).
  text=path: Emit text as <path> elements (accurate fonts).
  no-reuse-images: Do not reuse images using <symbol> definitions.
  resolution: Resolution to use when rasterizing elements.
```
