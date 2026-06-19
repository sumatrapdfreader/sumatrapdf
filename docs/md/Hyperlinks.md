# Hyperlinks in documents

SumatraPDF supports two different kinds of "links" in PDF and other documents. They behave differently and are controlled differently.

## Embedded hyperlinks

These are **part of the PDF file** — created by the author in Word, LaTeX, Acrobat, etc. They appear in every standards-compliant viewer. SumatraPDF follows them on click:

- jump to another page or destination in the same file
- open another file
- open a URL in the default browser (`http://`, `https://`, `mailto:`, …)

In [restricted mode](Configure-for-restricted-use.md), launching external URLs can be limited with `LinkProtocols` in `sumatrapdfrestrict.ini` (default: `http,https,mailto`).

## Auto-detected links (plain text)

SumatraPDF also turns **plain text** that *looks* like a URL or email address into a clickable link, even when the PDF contains no hyperlink annotation. For example, exporting `www.example.com` as plain text from a word processor creates a clickable link in SumatraPDF but not necessarily in every other viewer.

This follows a long-standing PDF viewer convention (Adobe Reader does something similar). Users who treat unexpected links as a security concern often ask about disabling it — see [discussion #5703](https://github.com/sumatrapdfreader/sumatrapdf/discussions/5703).

### Disable auto-detected links only

In [advanced settings](Advanced-options-settings.md):

```
DisableAutoLinks = true
```

Save the settings file. Embedded hyperlinks that exist in the PDF file itself are **not** removed — only automatic detection of URL-like text is disabled.

### Restricted mode: block following links

`sumatrapdfrestrict.ini` can disable opening URLs from documents entirely:

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

## Navigating after following a link

Internal links (footnotes, table of contents, cross-references) move you within the document. To return:

- `Alt + Left` or `Backspace` — go back in navigation history
- `Alt + Right` or `Shift + Backspace` — go forward

See [Scrolling and zooming](Scrolling-and-zooming.md).

## CHM and EPUB

- **CHM** files use HTML links; some `ms-its:` links open topics inside the help file.
- **EPUB** links are HTML anchors; external URLs open in the browser.

## See also

- [FAQ](FAQ.md)
- [Advanced options / settings](Advanced-options-settings.md) — `DisableAutoLinks`
- [Configure for restricted use](Configure-for-restricted-use.md) — `LinkProtocols`