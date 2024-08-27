# Scrolling and navigating

There are many ways to navigate around the document.

## Scrolling with keyboard

`Up`, `Down`, `Left`, `Right` refers to arrow keys.

* `k`, `j`, `h`, `l` : scroll up / down / left / right
* `Up`, `Down` : scroll up / down
* `n`, `Left` : go to next page (aligns top of page with top of window)
* `p`, `Right` : go to previous page (aligns top of page with top of window)
* `Shift + Down`, `Shift + Up` : scroll forward / backward by a page
* `Space`, `Shift + Space` : scroll forward / backward by a page
* `Home`, `End` : go to first / last page
* `g`, `Ctrl + g` : go to page (text field in toolbar or dialog if toolbar not shown)

## Scrolling with mouse and touch pad

* click on scrollbar to scroll up or down by page
* `Shift` + click on scrollbar : scrolls to that position
* scroll up / down with mouse scroll wheel or touch pad scrolling gesture
* press `Alt` while scrolling : scrolls faster (by half page instead of by line)
* mouse over scrollbar : scrolls faster (by half page instead of by line)

# Zooming and changing view

## With keyboard

* `+`, `-` : zoom in / out
* `Ctrl + +`, `Ctrl + -` : zoom in / out
* `Ctrl + y` : dialog to set custom zoom level (between 8.3% and 6400%)
* `c` : toggle continuous view
* `Ctrl + 0` : set zoom to fit whole page (or pages in multi-column view)
* `Ctrl + 1` : set 100% zoom
* `Ctrl + 2` : set zoom to fit width of page (or pages in multi-column view)
* `Ctrl + 3` : set zoom to fit content (like fit whole page but we auto-remove borders)
* `Ctrl + 6` : single page view i.e. single column
* `Ctrl + 7` : facing view i.e. 2 columns (pages)
* `Ctrl + 8` : 2 columns (pages) but offset by one page

## With mouse

* `Ctrl` + mouse scroll wheel : zoom in / out
* `Ctrl` + touch pad scroll gesture : zoom in / out
* pinch zoom gesture on touch screen

## Navigating history

Certain actions add navigation point. You can go back and forward in the history of navigation points (similar to browser back button) with:
* `Alt + Left`, `Backspace` : go back in history
* `Alt + Right`, `Shift + Backspace` : go forward in history

Actions that add navigation point:
* explicitly going to a page (`g`, `Ctrl + g`)
* clicking on a links within documents
* going to a page via Bookmarks tree view
* navigating via favorites (`Ctrl + b`)

## Navigating between tabs

* `Ctrl + Tab` : next tab
* `Ctrl + Shift + Tab` : previous tab
* `Ctrl + Page Down` : next tab
* `Ctrl + Page Up` : previous tab

## Moving tabs

**v.3.6+**

* `Ctrl + Shift + Page Down` : move tab right
* `Ctrl + Shift + Page Up` : move tab left

## Navigating between files

* `Shift + Control + Right` : go to next file in current folder
* `Shift + Control + Left` : go to previous file in current folder

# Related commands

You can [assign your own keyboard shortcuts](Customizing-keyboard-shortcuts.md). Here are related commands:

* `CmdScrollUp`, `CmdScrollDown`
* `CmdScrollLeft`, `CmdScrollRight`
* `CmdScrollUpHalfPage`, `CmdScrollDownHalfPage`
* `CmdScrollLeftPage`, `CmdScrollRightPage`
* `CmdScrollDownPage`, `CmdScrollUpPage`
* `CmdGoToNextPage`, `CmdGoToPrevPage`
* `CmdGoToFirstPage`, `CmdGoToLastPage`
* `CmdNavigateBack`, `CmdNavigateForward`
* `CmdOpenNextFileInFolder`, `CmdOpenPrevFileInFolder`

Tab commands:

* `CmdNextTab`, `CmdPrevTab`, `CmdNextTabSmart`, `CmdPrevTabSmart`

Zooming and view commands:

* `CmdZoomIn`, `CmdZoomOut`
* `CmdZoomCustom`
* `CmdZoomFitPage`
* `CmdZoomActualSize`
* `CmdZoomFitWidth`
* `CmdZoomFitContent`
