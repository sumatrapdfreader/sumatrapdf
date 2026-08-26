# Editing annotations

**Available in version 3.3 or later.**

You can add and edit annotations in PDF files.

## Highlight text with `a`

The most common annotation is a text highlight: select text and press `a`. This creates a yellow highlight annotation:

![Unsaved Annotation Changes](img/annot-unsaved-changes.png)

Here I highlighted the word `USAGE` and pressed `a`.

## Saving annotations back to PDF file

Notice the **You have unsaved annotations** message in the upper-right corner of the toolbar.

When you close the document (or exit the app) and have unsaved annotations, SumatraPDF will ask if you want to save them:

![Unsaved Annotations Dialog](img/unsaved-annotations-dialog.png)

If you close the dialog or choose `Discard`, annotations will be lost.

`Save changes to existing PDF` will overwrite the PDF with the newly added annotations.

`Save changes to a new PDF` will allow you to save as a new file.

## Edit PDF mode

Turn on **Edit PDF** in the main toolbar to add, select, and change annotations. `Shift + A` creates a highlight (the same as lowercase `a`) and also turns on Edit PDF mode.

The toolbar search box on the right (`filter N annotations`) lists annotations. Focus it to open the dropdown; an icon after the box pops the list into a floating window.

## Other annotations for selected text

When you select text, you can create the following annotations from the selection:

- highlight
- underline
- strike out
- squiggly underline

![Context Menu Annotation From Selection](img/context-menu-annot-from-sel.png)

We also copy the selected text to the clipboard, so you can, for example, use `Ctrl + V` to paste it into the annotation's `Contents` property.

## Other annotation types

You can also create an annotation object at the mouse location:

- text
- free text
- stamp
- caret
- image from the clipboard
- image from a file (`Insert Image...`)

Click a selected shape annotation (including polygon, polyline, and ink) and drag to move it.

To put a picture of your signature (or any other image) on a PDF page:

- **File → Insert Image...**, or
- right-click the page → **Document → Insert Image...**, or
- right-click the page → **Create annotation under cursor → Image From File...**

Pick a PNG (or JPEG, etc.). The image is stamped on the page and you can drag or resize it. Save with **File → Save Annotations to existing PDF** (or save as a new PDF). This is an electronic signature image, not a cryptographic digital signature (`File → Sign Document...`, which uses a certificate from the Windows store or a `.pfx` / `.p12` file). **Sign Document** can also draw a PNG or JPEG inside the digital signature itself, and you can turn off the "Digitally signed by" labels and the other lines.

![Context Menu Annotation Under Cursor](img/context-menu-annot-under-cursor.png)

## Changing an annotation

Click an annotation in Edit PDF mode to select it. A compact property row appears under it (color, opacity, border, font, icon, line endings, Contents). The filter list and floating list also select an annotation when you click a row.

Delete the selected annotation with `Delete`, or with **Delete Annotation** in the floating list.

## Moving annotations

To move an annotation on the page, left-click it and drag it in Edit PDF mode.

## Default colors, size, and opacity

Open **Settings → Advanced Options...** and edit the `Annotations` block:

| Setting                                     | Used for                                       |
| ------------------------------------------- | ---------------------------------------------- |
| `HighlightColor`                            | New highlights (`a`) — default `#ffff00`       |
| `UnderlineColor`                            | Underline annotations                          |
| `SquigglyColor`                             | Squiggly underline                             |
| `StrikeOutColor`                            | Strike-out                                     |
| `FreeTextColor` / `FreeTextBackgroundColor` | Free text annotations                          |
| `FreeTextSize`                              | Default font size for free text (default `12`) |
| `FreeTextBorderWidth`                       | Border width for free text                     |
| `FreeTextOpacity`                           | `0`–`100` percent opacity for free text        |
| `TextIconColor` / `TextIconType`            | Sticky-note style icons                        |
| `DefaultAuthor`                             | Author name written into new annotations       |

Highlight, underline, squiggly and strike-out colors accept `#aarrggbb` so you can set a default opacity: `#80ffff00` is a half-transparent yellow highlight. `#rrggbb` (6 digits) is fully opaque. You can still change one annotation's opacity from the compact property row.

Free-text opacity is the separate `FreeTextOpacity` percent (0–100).

See [Advanced options / settings](Advanced-options-settings.md) for the full list.

### Keyboard shortcuts with custom colors

**Ver 3.6+:** `CmdCreateAnnotHighlight` accepts a color argument. You can bind e.g. green highlights to a key — see [Customize keyboard shortcuts](Customize-keyboard-shortcuts.md).

### Toolbar buttons

Add annotation commands to the toolbar via the `Shortcuts` array — see [Customize toolbar](Customize-toolbar.md). Example commands: `CmdCreateAnnotHighlight`, `CmdCreateAnnotUnderline`, `CmdSaveAnnotations`.

## Saving workflow

| Action                   | Shortcut / command                                                                                                     |
| ------------------------ | ---------------------------------------------------------------------------------------------------------------------- |
| Save annotations to file | `Ctrl + Shift + S` (`CmdSaveAnnotations`)                                                                              |
| Save to a new PDF        | `CmdSaveAnnotationsNewFile` (tab context menu or command palette)                                                      |
| Discard unsaved changes  | `CmdDiscardChanges` (tab context menu **Discard changes**, or `Ctrl + K` command palette) — reloads the file from disk |
| Save when closing        | Prompt dialog — choose existing file, new file, or discard                                                             |

There is **no undo** (`Ctrl + Z`) for annotation edits. Delete an annotation with `Delete` when it is selected, or with **Delete Annotation** in the floating list.

To avoid the save prompt on every close, save explicitly with `Ctrl + Shift + S` before closing. To drop unsaved work without closing the tab, use **Discard Changes** (`CmdDiscardChanges`).

## Annotations from other programs

SumatraPDF can display most standard PDF annotations created in Acrobat, Foxit, etc. Some proprietary annotation types may show only an icon. Free-text presets and appearance vary between editors.

## Missing features

We don't yet support every annotation type or every editing operation other PDF apps offer.

The future will be driven by your feedback. If features are missing or there are better ways of doing things, let us know in [Discussions](https://github.com/sumatrapdfreader/sumatrapdf/discussions).

When providing feedback:

- tell us what
- tell us why. Context is important for prioritizing features. Is the new feature or idea something you absolutely need, or is it just a nice improvement?
- you might be familiar with how other PDF editors work. When referencing feature or UI ideas from other apps, tell us which app you mean. Screenshots are better than words when describing UI ideas.
