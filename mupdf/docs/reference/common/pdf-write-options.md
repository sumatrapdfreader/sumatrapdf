# PDF Write Options

These are the common options to functions that write PDF files.

decompress
: Decompress all streams (except compress-fonts/images)

compress
: Compress all streams. Bi-level images are compressed with CCITT Fax and generic data is compressed with flate.

compress=flate
: Compress streams with Flate (default).

compress=brotli
: Compress streams with Brotli (WARNING: this is a proposed PDF feature)

compress-fonts
: Compress embedded fonts

compress-images
: Compress images

compress-effort=0|percentage
: Effort spent compressing, 0 is default, 100 is max effort

ascii
: ASCII hex encode binary streams

pretty
: Pretty-print objects with indentation

labels
: Print object labels

linearize
: Optimize for web browsers (no longer supported!)

clean
: Pretty-print graphics commands in content streams

sanitize
: Sanitize graphics commands in content streams

garbage
: Garbage collect unused objects

garbage=compact
: Garbage collect unsued objects, and compact cross reference table

garbage=deduplicate
: Garbage collec unused objects, compact cross reference tables, and remove duplicate objects

incremental
: Write changes as incremental update

objstms
: Use object streams and cross reference streams

appearance=yes|all
: Synthesize just missing, or all, annotation/widget appearance streams

continue-on-error
: Continue saving the document even if there is an error

decrypt
: Write unencrypted document

encrypt=rc4-40|rc4-128|aes-128|aes-256
: Write encrypted document

permissions=NUMBER
: Document permissions to grant when encrypting

user-password=PASSWORD
: Password required to read document

owner-password=PASSWORD
: Password required to edit document

regenerate-id
: Regenerate document id (default yes)
