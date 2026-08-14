# Scrolling and navigating

There are many ways to navigate around the document.

## Scrolling with keyboard

`Up`, `Down`, `Left`, `Right` refers to arrow keys.

- `k`, `j`, `h`, `l` : scroll up / down / left / right
- `Up`, `Down` : scroll up / down
- `n`, `Left` : go to next page (aligns top of page with top of window)
- `p`, `Right` : go to previous page (aligns top of page with top of window)
- `Shift + Down`, `Shift + Up` : scroll forward / backward by a page
- `Space`, `Shift + Space` : scroll forward / backward by a page
- `Home`, `End` : go to first / last page
- `g`, `Ctrl + g` : go to page (text field in toolbar or dialog if toolbar not shown)

## Scrolling with mouse and touch pad

- click on scrollbar to scroll up or down by page
- `Shift` + click on scrollbar : scrolls to that position
- scroll up / down with mouse scroll wheel or touch pad scrolling gesture
- press `Alt` while scrolling : scrolls faster (by half page instead of by line)
- mouse over scrollbar : scrolls faster (by half page instead of by line)

# Zooming and changing view

## With keyboard

- `+`, `-` : zoom in / out
- `Ctrl + +`, `Ctrl + -` : zoom in / out
- `Ctrl + y` : dialog to set custom zoom level (between 8.3% and 6400%)
- `c` : toggle continuous view
- `Ctrl + 0` : set zoom to fit whole page (or pages in multi-column view)
- `Ctrl + 1` : set 100% zoom
- `Ctrl + 2` : set zoom to fit width of page (or pages in multi-column view)
- `Ctrl + 3` : set zoom to fit content (like fit whole page but we auto-remove borders)
- Fit Height (View / Zoom menu or command palette): scale so the page fills the window height (may scroll horizontally)
- `Ctrl + 6` : single page view i.e. single column
- `Ctrl + 7` : facing view i.e. 2 columns (pages)
- `Ctrl + 8` : 2 columns (pages) but offset by one page

For comic books and manga (right-to-left reading, double-page spreads, `LimitToWindowWidth`), see [Comics and manga](Comics-and-manga.md).

## With mouse

- `Ctrl` + mouse scroll wheel : zoom in / out
- `Ctrl` + touch pad scroll gesture : zoom in / out
- pinch zoom gesture on touch screen
- with `ClickEdgeToTurnPage = true` (advanced setting): click the left fifth of the page area for the previous page, the right fifth for the next page (sides reverse in manga mode)

Also mention in comics doc briefly.

## Zoom levels

Zooming in and out steps through a fixed list of zoom levels. This is the built-in list, and also the value of the `ZoomLevels` [advanced setting](Advanced-options-settings.md) that reproduces it:

```
ZoomLevels = 8.33 12.5 18 25 33.33 50 66.67 75 100 125 150 200 300 400 600 800 1000 1200 1600 2000 2400 3200 4800 6400
```

Setting `ZoomLevels` replaces the built-in list rather than adding to it, so the way to change the steps is to copy the line above and edit it. Fit Page, Fit Width and Fit Content are always available regardless of the list.

**Ver 3.7+:** the largest level in the list is also the highest zoom that can be set at all, in the Custom Zoom dialog (`Ctrl + y`) and everywhere else. So to zoom further than 6400% — into a large map, say, where the detail is in the file but the old limit hid it — add the levels you want on the end:

```
ZoomLevels = 8.33 12.5 18 25 33.33 50 66.67 75 100 125 150 200 300 400 600 800 1000 1200 1600 2000 2400 3200 4800 6400 12800 25600 51200 102400
```

Levels up to 1000000 (10000x) are accepted; anything larger is ignored. How far a particular document can be zoomed also depends on its size: all of its pages are laid out on a single canvas measured in pixels, so a long document stops zooming in earlier than a short one does.

## Navigating history

Certain actions add navigation point. You can go back and forward in the history of navigation points (similar to browser back button) with:

- `Alt + Left`, `Backspace` : go back in history
- `Alt + Right`, `Shift + Backspace` : go forward in history

After following an internal hyperlink (footnote, TOC entry), use `Alt + Left` to go back. See [Hyperlinks](Hyperlinks.md).

Actions that add navigation point:

- explicitly going to a page (`g`, `Ctrl + g`)
- clicking on links within documents
- going to a page via Bookmarks tree view
- navigating via favorites (`Ctrl + b`)

## Navigating between tabs

- `Ctrl + Tab` / `Ctrl + Shift + Tab` : next / previous tab. **Ver 3.6+:** **Smart Tab Switch** (tab list while Ctrl is held). To get pre-3.6 immediate strip-order switching on those keys, [rebind](Customize-keyboard-shortcuts.md) them to `CmdNextTab` / `CmdPrevTab` — see [Tabs and windows](Tabs-and-windows.md#restore-pre-36-ctrltab-no-switcher-popup)
- `Ctrl + Page Down` : next tab (strip order, no popup)
- `Ctrl + Page Up` : previous tab (strip order, no popup)

## Moving tabs

**v.3.6+**

- `Ctrl + Shift + Page Down` : move tab right
- `Ctrl + Shift + Page Up` : move tab left

## Navigating between files

- `Shift + Control + Right` : go to next file in current folder
- `Shift + Control + Left` : go to previous file in current folder

# Related commands

You can [assign your own keyboard shortcuts](Customize-keyboard-shortcuts.md). Here are related commands:

- `CmdScrollUp`, `CmdScrollDown`
- `CmdScrollLeft`, `CmdScrollRight`
- `CmdScrollUpHalfPage`, `CmdScrollDownHalfPage`
- `CmdScrollLeftPage`, `CmdScrollRightPage`
- `CmdScrollDownPage`, `CmdScrollUpPage`
- `CmdGoToNextPage`, `CmdGoToPrevPage`
- `CmdGoToFirstPage`, `CmdGoToLastPage`
- `CmdNavigateBack`, `CmdNavigateForward`
- `CmdOpenNextFileInFolder`, `CmdOpenPrevFileInFolder`

Tab commands:

- `CmdNextTab`, `CmdPrevTab`, `CmdNextTabSmart`, `CmdPrevTabSmart`

Zooming and view commands:

- `CmdZoomIn`, `CmdZoomOut`
- `CmdZoomCustom`
- `CmdZoomFitPage`
- `CmdZoomActualSize`
- `CmdZoomFitWidth`
- `CmdZoomFitContent`
