# Finding text in documents

SumatraPDF can search for text in PDF, EPUB, MOBI, and other formats that support text extraction.

## Open the find box

| Action | Shortcut |
| --- | --- |
| Find | `Ctrl + F` |
| Find (alternate) | `/` |

The find toolbar appears at the bottom of the window.

## Find next / previous

| Action | Shortcut |
| --- | --- |
| Find next | `F3` |
| Find previous | `Shift + F3` |
| Find next (selection as needle) | `Ctrl + F3` |
| Find previous (selection as needle) | `Shift + Ctrl + F3` |

All shortcuts are also listed in [Keyboard shortcuts](Keyboard-shortcuts.md) and [Commands](Commands.md).

### Focus matters

Shortcuts behave differently depending on where keyboard focus is:

- **Focus in the document** — `F3` / `Shift + F3` jump to the next / previous match.
- **Focus in the find box** — `Enter` usually finds the next match; `Shift + Enter` the previous. `F3` may not work until you click back in the document.

If `Ctrl + F` behaves oddly (for example replacing the search term with selected text), you may have focus split between the find box and the page. Click the document pane, then press `F3`.

### Laptop keyboards

On many laptops the `F1`–`F12` keys control volume and brightness unless you hold **Fn**. If `F3` changes volume instead of finding the next match, use **Fn + F3**, or rebind `CmdFindNext` in [custom keyboard shortcuts](Customize-keyboard-shortcuts.md).

## How search works

- Search is **case-insensitive** by default (German `ß` matches `ss` since 3.7).
- Matching continues onto following pages and **wraps** around to the start of the document.
- Matches are highlighted with `FixedPageUI.SelectionColor` (configurable in advanced settings).

## Command Palette

`Ctrl + K` and type `find` to run the Find command without using the keyboard shortcut.

## Command-line and automation

Open a document and start a search immediately:

```
SumatraPDF.exe -search "needle" document.pdf
```

For an already open document, use [DDE](DDE-Commands.md):

```
[Search("C:\path\file.pdf", "needle")]
```

To jump to a page and select a term only if it appears on that page:

```
[GotoPageWord("C:\path\file.pdf", 12, "needle")]
```

## Search across many PDFs (command-line tool)

Use `sumatrapdf-tool grep` — see [Tool grep](Tool-grep.md) and [Search text in PDF from command line](Tool-x-search-pdf.md).

## Web search / translation of selection

To send **selected** text to Google, Bing, DeepL, etc., see [Customize search / translation services](Customize-search-translation-services.md). That is separate from in-document find.

## See also

- [FAQ](FAQ.md) — search shortcut troubleshooting
- [Keyboard shortcuts](Keyboard-shortcuts.md)
- [Command-line arguments](Command-line-arguments.md) — `-search`