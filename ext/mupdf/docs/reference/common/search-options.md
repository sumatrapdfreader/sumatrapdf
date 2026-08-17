# Search Options

The options are specified using an <a href="option-strings.html">option string</a> of key-value pairs.

exact
: Search for the given string exactly as written

regexp
: The search needle is interpreted as a JS regular expression

ignore-case
: During searching, ignore case differences in search needle and page text

ignore-diacritics
: During searching, ignore diacritics differences in search needle and page text

keep-lines
: With this option line endings are kept as ``\n``, otherwise they are transformed into spaces in the page text before searching begins.

keep-paragraphs
: With this option paragraph endings are kept as ``\n``, otherwise they are transformed into spaces in the page text before searching begins.

: Combining ``keep-lines`` with ``keep-paragraphs`` means that lines end in ``\n`` and paragraphs in ``\n\n``.

keep-hyphens
: Without this option hyphens will be removed and lines joined in the page text before searching begins.
