# DDE Commands

You can control SumatraPDF with [DDE commands.](https://docs.microsoft.com/en-us/windows/win32/dataxchg/dynamic-data-exchange) 

They are mostly used to use SumatraPDF as a [preview tool from e.g. LaTeX editors](LaTeX-integration.md) that generate PDF files.

## Format of DDE commands

Single DDE command: `[Command(parameter1, parameter2, ..., )]`

Multiple DDE commands: `[Command1(parameter1, parameter2, ..., )][Command2(...)][...]`

## Sending DDE commands

You can either use windows api by sending DDE commands to server `SUMATRA` and topic `control`. See [this code](https://github.com/sumatrapdfreader/sumatrapdf/blob/master/src/utils/WinUtil.cpp#L2437) for example of sending DDE command.

Or you can use `-dde` command-line argument to `SumatraPDF.exe` e.g. `SumatraPDF.exe -dde "[SetView(\"c:\\file.pdf\",\"continuous\",-3)]"`.

Notice escaping of DDE command string: `"` and `\` with `\`.

## List of DDE commands:

### Named commands

**Ver 3.5+**: you can send all [named commands](Commands.md) as DDE:

- format `[<command_id>]` e.g. `[CmdClose]`

### Open file

- format: `[Open("<filePath>"[,<newWindow>,<focus>,<forceRefresh>])]`
- arguments:
    - if `newWindow` is 1 then a new window is created even if the file is already open
    - if `focus` is 1 then the focus is set to the window
    - if `forceRefresh` is 1 the command forces the refresh of the file window if already open (useful for files opened over network that don't get file-change notifications)".
- example: `[Open("c:\file.pdf",1,1,0)]`

### Forward-search

- format: `[ForwardSearch(["<pdffilepath>",]"<sourcefilepath>",<line>,<column>[,<newwindow>,<setfocus>])]`
- arguments:
    - `pdffilepath` : path to the PDF document (if this path is omitted and the document isn't already open, SumatraPDF won't open it for you)
    - `column` : this parameter is for future use (just always pass 0)
    - `newwindow` : 1 to open the document in a new window (even if the file is already opened)
    - `focus` : 1 to set focus to SumatraPDF's window.
- examples
    - `[ForwardSearch("c:\file.pdf","c:\folder\source.tex",298,0)]`
    - `[ForwardSearch("c:\folder\source.tex",298,0,0,1)]`

### Jump to named destination command

- format: `[GotoNamedDest("<pdffilepath>","<destination name>")]`
- example: `[GotoNamedDest("c:\file.pdf", "chapter.1")]`
- note: the pdf file must be already opened

### Go to page

- format: `[GotoPage("<pdffilepath>",<page number>)]`
- example: `[GotoPage("c:\file.pdf", 37)]`
- note: the pdf file must be already opened.

### Search

Search the document for a term and select/scroll to the first match. Like the Find box, the search continues onto following pages and wraps around to the start.

- format: `[Search("<pdffilepath>","<search-term>")]`
- example: `[Search("c:\file.pdf", "needle")]`
- note: the pdf file must be already opened.

### Go to page and word

**Ver 3.7+**

Go to a specific page and select the search term **only if it is found on that page** (unlike `Search`, which keeps searching following pages and wraps around). If the term is not on that page, it stays on the page and selects nothing. Useful for making a shortcut to a precise location.

- format: `[GotoPageWord("<pdffilepath>",<page number>,"<search-term>")]`
- example: `[GotoPageWord("c:\file.pdf", 12, "green")]`
- note: the pdf file must be already opened.

### Set view settings

- format: `[SetView("<pdffilepath>","<view mode>",<zoom level>[,<scrollX>,<scrollY>])]`
- arguments:
    - `view mode`:
        - `"single page"`
        - `"facing"`
        - `"book view"`
        - `"continuous"`
        - `"continuous facing"`
        - `"continuous book view"`
    - `zoom level` : either a zoom factor between 8 and 6400 (in percent) or one of -1 (Fit Page), -2 (Fit Width) or -3 (Fit Content). Use `0` to keep the current zoom unchanged — useful when scrolling with the scroll arguments, since re-applying a Fit zoom on every call re-fits the page and would reset the scroll position
    - `scrollX, scrollY` : PDF document (user) coordinates of the point to be visible in the top-left of the window
- example: `[SetView("c:\file.pdf","continuous",-3)]`
- note: the pdf file must already be opened

### Get file state

Unlike the commands above (which are sent as DDE *execute* requests), this is a DDE *request* transaction: it returns information about a document.

- format: `[GetFileState("<pdffilepath>")]` or `[GetFileState()]` for the currently active document
- returns multiple `key: value` lines (split the response by `\n`, then each line by the first `:`):

    ```
    path: c:\file.pdf
    page: 1
    pageCount: 6
    zoom: 120
    view: continuous
    sumver: 3.7
    ```

    - `page` : the current page number; `pageCount` : the total number of pages
    - `zoom` : a zoom factor in percent, or -1 (Fit Page), -2 (Fit Width), -3 (Fit Content) — the same convention as `SetView`
    - on error (no such open file) it returns `error: <message>`
- example: `[GetFileState()]`

### Get open files

Also a DDE *request* transaction: returns the full path of every open document (across all windows and tabs), one per line.

- format: `[GetOpenFiles()]`
- returns one file path per line (split the response by `\n`); empty if nothing is open
- example: `[GetOpenFiles()]`