- `decompress` Decompress all streams (except compress-fonts/images).
- `compress` Compress all streams.
- `compress-fonts` Compress embedded fonts.
- `compress-images` Compress images.
- `ascii` ASCII hex encode binary streams.
- `pretty` Pretty-print objects with indentation.
- `linearize` Optimize for web browsers.
- `clean` Pretty-print graphics commands in content streams.
- `sanitize` Sanitize graphics commands in content streams.
- `incremental` Write changes as incremental update.
- `continue-on-error` Continue saving the document even if there is an error.
- `garbage` Garbage collect unused objects.

   or `garbage=compact` ... and compact cross reference table.

   or `garbage=deduplicate` ... and remove duplicate objects.

- `decrypt` Write unencrypted document.
- `encrypt=rc4-40|rc4-128|aes-128|aes-256` Write encrypted document.
- `permissions=NUMBER` Document permissions to grant when encrypting.
- `user-password=PASSWORD` Password required to read document.
- `owner-password=PASSWORD` Password required to edit document.
- `regenerate-id` Regenerate document id (default yes).
