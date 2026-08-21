# Search Options

The options are specified using an <a href="option-strings.html">option string</a> of key-value pairs.

exact
: Search for the given string exactly as written.

regexp
: The search needle is interpreted as a JS regular expression.

ignore-case
: Ignore case differences in search needle and page text.

ignore-diacritics
: Ignore diacritics differences in search needle and page text.

keep-lines
: Keep line endings as ``\n``, otherwise they are transformed into spaces.

keep-paragraphs
: Keep paragraph endings as ``\n``, otherwise they are transformed into spaces.

: Combining ``keep-lines`` with ``keep-paragraphs`` means that lines end in ``\n`` and paragraphs in ``\n\n``.

keep-hyphens
: Normally hyphenated words at the end of a line are joined together. This option disables that behavior, and searches for hyphenated words as they are.
