# Hyperlinks in documents

SumatraPDF supports two kinds of links in PDF and other documents: **embedded hyperlinks** stored in the file, and **auto-detected links** it finds in plain text. They behave and are controlled differently.

**Click a link to follow it; press `Alt + Left` to come back.**

- **Embedded hyperlinks:** created by the document author; jump within the file, open another file, or open a URL
- **Auto-detected links:** plain-text URLs, email addresses and DOIs become clickable — disable with `DisableAutoLinks`
- **Navigate back / forward:** `Alt + Left` / `Alt + Right`
- **Restrict links:** `LinkProtocols` in `sumatrapdfrestrict.ini`

## Follow a link

Click it. Embedded hyperlinks are **part of the PDF file** — created by the author in Word, LaTeX, Acrobat, etc. — and work in every standards-compliant viewer. They can:

- jump to another page or destination in the same file
- open another file
- open a URL in the default browser (`http://`, `https://`, `mailto:`, …)

SumatraPDF also turns **plain text** that _looks_ like a URL, email address, or DOI into a clickable link, even when the PDF contains no hyperlink annotation. For example, exporting `www.example.com` as plain text from a word processor creates a clickable link in SumatraPDF but not necessarily in every other viewer. A printed DOI such as `10.1109/WICSA.2015.29` opens as `https://doi.org/10.1109/WICSA.2015.29`.

This follows a long-standing PDF viewer convention (Adobe Reader does something similar).

## Go back after following a link

Internal links (footnotes, table of contents, cross-references) move you within the document. To return:

- `Alt + Left` or `Backspace` — go back in navigation history
- `Alt + Right` or `Shift + Backspace` — go forward

See [Scrolling and zooming](Scrolling-and-zooming.md).

## Disable auto-detected links

Users who treat unexpected links as a security concern often ask about this — see [discussion #5703](https://github.com/sumatrapdfreader/sumatrapdf/discussions/5703).

In [advanced settings](Advanced-options-settings.md) set:

```
DisableAutoLinks = true
```

Save the settings file. Embedded hyperlinks in the PDF file are **not** removed — only automatic detection of URL-like text, email addresses, and plain-text DOIs is disabled.

## Block following links (restricted mode)

In [restricted mode](Configure-for-restricted-use.md), `sumatrapdfrestrict.ini` controls which links open. `LinkProtocols` defaults to `http,https,mailto`.

Disable opening URLs from documents entirely:

```
[Policies]
DiskAccess = 0
```

or narrow allowed protocols:

```
LinkProtocols =
```

An empty `LinkProtocols` value blocks all protocol handlers. This is stricter than `DisableAutoLinks` — it affects embedded links too, not just auto-detection.

See [Configure for restricted use](Configure-for-restricted-use.md) and the [full restrict.ini reference](https://github.com/sumatrapdfreader/sumatrapdf/blob/master/docs/sumatrapdfrestrict.ini).

## Tips

- Use `Alt + Left` to return after jumping to a footnote or cross-reference.
- Use `DisableAutoLinks = true` to stop plain text becoming clickable while keeping the author's links.
- Use an empty `LinkProtocols` in restricted mode to block all links, embedded ones included.

## Reference

### CHM and EPUB

- **CHM** files use HTML links; some `ms-its:` links open topics inside the help file.
- **EPUB** links are HTML anchors; external URLs open in the browser.

## See also

- [FAQ](FAQ.md)
- [Advanced settings](Advanced-options-settings.md) — `DisableAutoLinks`
- [Configure for restricted use](Configure-for-restricted-use.md) — `LinkProtocols`
