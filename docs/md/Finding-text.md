# Finding text in documents

Find searches the current document for text. It works in PDF, EPUB, MOBI, and other formats that support text extraction, and opens from the **Find** toolbar button, `Ctrl + F` or **Go To → Find...**.

**Press `Ctrl + F`, type, and matches are found as you type.** A `n / m` counter shows the current match and the total number of matches.

- **Find:** open the find box — `Ctrl + F` or `/`
- **Find next / previous:** step through matches — `F3` / `Shift + F3`
- **Find selection:** use the selected text as the search term — `Ctrl + F3` / `Shift + Ctrl + F3`
- **Floating window:** a movable window with a list of all matches
- **Limit to pages:** search only a page range (floating window)
- **Command line:** open a document and start a search — `-search`

## Start a search

- Click the **Find** button in the toolbar, or use **Go To → Find...**.
- Press `Ctrl + F` or `/`.
- [Command Palette](Command-Palette.md): `Ctrl + K`, then `Find`.

A small find bar appears near the **Find** button. Type your text; matches are found as you type.

## Go to the next or previous match

| Action                              | Shortcut            |
| ----------------------------------- | ------------------- |
| Find next                           | `F3`                |
| Find previous                       | `Shift + F3`        |
| Find next (selection as needle)     | `Ctrl + F3`         |
| Find previous (selection as needle) | `Shift + Ctrl + F3` |

Shortcuts depend on where keyboard focus is:

- **Focus in the document:** `F3` / `Shift + F3` jump to the next / previous match.
- **Focus in the find box:** `Enter` finds the next match, `Shift + Enter` the previous. In the **floating window** these (and the arrow keys) step through the results list with the cursor left in the box; with the **compact bar**, `F3` may not work until you click back in the document.

If `Ctrl + F` behaves oddly (for example replacing the search term with selected text), focus may be split between the find box and the page. Click the document pane, then press `F3`.

If `F3` changes volume instead of finding the next match (many laptops map `F1`–`F12` to volume and brightness unless you hold **Fn**), press **Fn + F3**, or rebind `CmdFindNext` in [custom keyboard shortcuts](Customize-keyboard-shortcuts.md).

All shortcuts are also listed in [Keyboard shortcuts](Keyboard-shortcuts.md) and [Commands](Commands.md).

## See all matches in a floating window

There are two find UIs:

- **Compact bar** (default): the small overlay near the toolbar.
- **Floating window:** a separate, movable and resizable window that also shows a **results list**. It floats over the document so it doesn't cover the page.

Switch between them with the diagonal-arrows button on the right of the find UI (hover it for a tooltip):

| Button                                         | Tooltip            | Action                                     |
| ---------------------------------------------- | ------------------ | ------------------------------------------ |
| arrows pointing **out**, on the compact bar    | _Open in a window_ | Pop the find UI out into a floating window |
| arrows pointing **in**, on the floating window | _Dock to toolbar_  | Dock it back to the compact bar            |

The choice is remembered across launches via the `SearchUIFloating` advanced setting (`true` = floating window). The floating window's position and size are remembered via `SearchUIWindowPos`. See [Advanced settings](Advanced-options-settings.md).

The results list shows every match below the search box, across the whole document, even on large files:

- Each row shows a **snippet** of the surrounding text with the match **highlighted**, plus the **page number** on the right.
- Click a result, or use **Up / Down** (and **Page Up / Page Down**) while the cursor stays in the search box, to jump to that match.
- **Find Next / Previous** (the buttons, `F3` / `Shift + F3`, or `Enter` / `Shift + Enter`) step through the list the same way.
- Typing selects and jumps to the first match automatically.

## Search only some pages

In the floating window, below the search field, **Limit to pages 1-N:** restricts the search to a page range (N is the last page of the document). Leave the box empty to search every page.

- List several ranges: `3,4-6,18-` (page 3, pages 4–6, and page 18 through the end).
- Other forms: `10-25`, `10`, `10-`, `-25`.

The `n / m` counter and the results list follow the same range. The compact find bar always searches the whole document.

## Search from the command line

Open a document and start a search immediately:

```
SumatraPDF.exe -search "needle" document.pdf
```

The leading `-` is required. Adobe Reader-style `/A` also works:

```
SumatraPDF.exe /A "page=1;search=needle" document.pdf
```

See [Command-line arguments](Command-line-arguments.md) for the full `/A` parameter list.

For an already open document, use [DDE](DDE-Commands.md):

```
[Search("C:\path\file.pdf", "needle")]
```

To jump to a page and select a term only if it appears on that page:

```
[GotoPageWord("C:\path\file.pdf", 12, "needle")]
```

## Search across many PDFs

Use the `sumatrapdf-tool grep` command-line tool. See [Tool grep](Tool-grep.md) and [Search text in PDF from command line](Tool-x-search-pdf.md).

## Search the web or translate a selection

To send **selected** text to Google, Bing, DeepL, etc., see [Customize search / translation services](Customize-search-translation-services.md). That is separate from in-document find.

## How search works

- Search is **case-insensitive** by default (German `ß` matches `ss` since 3.7).
- Matching continues onto following pages and **wraps** around to the start of the document.
- The **current** match is highlighted with `FixedPageUI.SelectionColor` (configurable in advanced settings); all other matches use a secondary orange highlight so the active match stands out.
- The find UI **closes when you switch tabs**; each tab gets its own fresh search.

## Tips

- Select a word and press `Ctrl + F3` to find its next occurrence without typing it.
- Use the floating window to see every match with its page number before jumping.
- Set a page range like `10-` to skip front matter.
- If `F3` does nothing, click the document first so it has focus.

## See also

- [FAQ](FAQ.md) — search shortcut troubleshooting
- [Keyboard shortcuts](Keyboard-shortcuts.md)
- [Command-line arguments](Command-line-arguments.md) — `-search`
