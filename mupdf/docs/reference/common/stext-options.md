# Structured Text Options

preserve-images
: Keep images in output

preserve-ligatures
: Do not expand ligatures into constituent characters

preserve-spans
: Do not merge spans on the same line

preserve-whitespace
: Do not convert all whitespace into space characters

inhibit-spaces
: Don't add spaces between gaps in the text

paragraph-break
: Break blocks at paragraph boundaries

dehyphenate
: Attempt to join up hyphenated words

ignore-actualtext
: Do not apply ActualText replacements

use-cid-for-unknown-unicode
: Use character code if unicode mapping fails

use-gid-for-unknown-unicode
: Use glyph index if unicode mapping fails

accurate-bboxes
: Calculate char bboxes from the outlines

accurate-ascenders
: Calculate ascender/descender from font glyphs

accurate-side-bearings
: Expand character bboxes to completely include width of glyphs

collect-styles
: Attempt to detect text features (fake bold, strikeout, underlined etc)

clip
: Do not include text that is completely clipped

clip-rect=x0:y0:x1:y1
: Specify clipping rectangle within which to collect content

structured
: Collect structure markup

vectors
: Include vector bboxes in output

segment
: Attempt to segment the page

table-hunt
: Hunt for tables within a (segmented) page
