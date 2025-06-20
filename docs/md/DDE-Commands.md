# DDE Commands

You can control SumatraPDF with [DDE commands.](https://docs.microsoft.com/en-us/windows/win32/dataxchg/dynamic-data-exchange) 

They are mostly used to use SumatraPDF as a [preview tool from e.g. LaTeX editors](LaTeX-integration.md) that generate PDF files.

## Format of DDE comands

Single DDE command: `[Command(parameter1, parameter2, ..., )]`

Multiple DDE commands: `[Command1(parameter1, parameter2, ..., )][Command2(...)][...]`

## Sending DDE commands

You can either use windows api by sending DDE commands to server `SUMATRA` and topic `control`. See [this code](https://github.com/sumatrapdfreader/sumatrapdf/blob/master/src/utils/WinUtil.cpp#L2437) for example of sending DDE command.

Or you can use `-dde` command-line argument to `SumatraPDF.exe` e.g. `SumatraPDF.exe -dde "[SetView(\"c:\\file.pdf\",\"continuous\",-3)]"`.

Notice escaping of DDE command string: `"` and `\` with `\`.

## List of DDE commands:

### Named commands

*Ver 3.5+**: you can send all [named commands](Commands.md) as DDE:

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
    - `zoom level` : either a zoom factor between 8 and 6400 (in percent) or one of -1 (Fit Page), -2 (Fit Width) or -3 (Fit Content)
    - `scrollX, scrollY` : PDF document (user) coordinates of the point to be visible in the top-left of the window
- example: `[SetView("c:\file.pdf","continuous",-3)]`
- note: the pdf file must already be opened