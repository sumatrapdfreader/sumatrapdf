- `decompress` Decompress all streams (except compress-fonts/images).
- `compress` Compress all streams, and is identical to `compress=flate`.

  or `compress=flate` which compresses all streams using Flate or CCITT Fax compression methods.

  or `compress=brotli` which compresses all streams using Brotli or CCITT Fax compression methods.

- `compress-fonts` Compress embedded fonts.
- `compress-images` Compress images.
- `compression-effort=NUMBER` Desired effort in percent to spend compressing streams. `0` is default effort level, or a number from `1`, the minimum, and `100`, the maximum effort.
- `labels` Print labels for how to reach each object as comments in the output PDF.
- `ascii` ASCII hex encode binary streams.
- `pretty` Pretty-print objects with indentation.
- `linearize` Optimize for web browsers. No longer supported!
- `clean` Pretty-print graphics commands in content streams.
- `sanitize` Sanitize graphics commands in content streams.
- `incremental` Write changes as incremental update.
- `objstms` Use object and cross-reference streams when creating PDF.
- `appearance=yes` Create appearance streams for annotations that miss appearance streams.
- `appearance=all` Recreate appearance streams for all annotations.
- `continue-on-error` Continue saving the document even if there is an error.
- `garbage` Garbage collect unused objects.

   or `garbage=compact` ... and compact cross reference table.

   or `garbage=deduplicate` ... and remove duplicate objects.

- `decrypt` Write unencrypted document.
- `encrypt=none|keep|rc4-40|rc4-128|aes-128|aes-256` Write encrypted document with specified algorithm.
- `permissions=NUMBER` Document permissions to grant when encrypting.
- `user-password=PASSWORD` Password required to read document.
- `owner-password=PASSWORD` Password required to edit document.
- `regenerate-id` Regenerate document id (default yes).
