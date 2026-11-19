[![Build](https://github.com/sumatrapdfreader/sumatrapdf/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/sumatrapdfreader/sumatrapdf/actions/workflows/build.yml)
## SumatraPDF Reader

SumatraPDF is a multi-format (PDF, EPUB, MOBI, CBZ, CBR, FB2, CHM, XPS, DjVu) reader
for Windows under (A)GPLv3 license, with some code under BSD license (see
AUTHORS).

More Information:
* [Website](https://www.sumatrapdfreader.org/free-pdf-reader)
* [Manual](https://www.sumatrapdfreader.org/manual)
* [Developer Information](https://www.sumatrapdfreader.org/docs/Contribute-to-SumatraPDF)

## macOS

SumatraPDF ng (`src/ng/`) is the native macOS app: one window with tabs, a
toolbar, the menu bar, a command palette (`Cmd + K`) and Mac shortcuts
([list](src/ng/README.md#keyboard-shortcuts)). Finder offers it under "Open
With" for PDF, EPUB, DjVu, XPS, CHM, comic books, ebooks and images. A file
that changes on disk reloads by itself; `R` reloads it by hand.

Build and run (needs [bun](https://bun.sh/) and the Xcode command line tools):

```sh
bun cmd/ng-build.ts -rel
open out/mac/rel/SumatraPDF.app
```

Built and tested on Apple Silicon.

### Added in this fork

- **Trackpad.** Pinch zooms around the pointer. A two-finger slide pans in
  both directions at once when the page is wider than the window.
- **Comments on text.** Select text and pick **Add comment** in the selection
  toolbar, the context menu or the command palette (`CmdAddComment`). The
  text is highlighted and its comment editor opens. Hover the highlight to
  read the comment; the card has **Edit** and color swatches. Comments are
  standard PDF highlight annotations, saved with the document.
- **Colors.** Rest the pointer on **Highlight** or **Add comment** to pick a
  color. Each button remembers its own (`HighlightColor`, `CommentColor`).
- **Highlight outline.** The marker of a selected highlight follows its
  lines instead of boxing all of them, and Edit PDF shows no metadata card
  over highlights.
- **AI chat.** A toolbar button (`CmdToggleAIChat`) shows and hides the chat
  panel. Closing the panel removes it and keeps the conversation.

### Not on macOS yet

PDF signing, a print dialog with printer settings, the measuring tool,
dragging a selection into another app, an installer.
