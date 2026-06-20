# Editing annotations

**Available in version 3.3 or later.**

You can add / edit annotations in PDF files.

## Highlight text with `a`

Most common annotation is highlighting of text: select text and press `a`. This creates a highlight annotation in yellow color:

![Unsaved Annotation Changes](img/annot-unsaved-changes.png)

Here I highlighted word `USAGE` and pressed `a`.

## Saving annotations back to PDF file

Notice in toolbar (upper right) we show: You have unsaved annotations.

When you close the document (or exit the app) and have unsaved annotations, SumatraPDF will ask if you want to save them:

![Unsaved Annotations Dialog](img/unsaved-annotations-dialog.png)

If you close the dialog or choose `Discard`, annotations will be lost.

`Save changes to existing PDF` will over-write the PDF with newly added annotations.

`Save changes to a new PDF` will allow you to save as a new file.

## Open annotation editor

If you press `A` (upper case i.e. `Shift-a`) we will create a highlight annotation (same as lower case `a`) but also open annotation editor window.

Another way to open annotation editor is to use context menu (right mouse click or `Shift-F10`) and use `Edit Annotations`:

![Annotation Context Menu](img/annot-context-menu.png)

## Other annotations for selected text

When you selected a text, you can create the following annotations from the selection:

- highlight
- underline
- strike out
- squiggly underline

![Context Menu Annotation From Selection](img/context-menu-annot-from-sel.png)

We also copy selected text to a clipboard so that e.g. you can use `Ctrl-V` to paste it into `Contents` property of the annotation.

## Other annotation types

You can also create annotation object at your mouse location:

- text
- free text
- stamp
- caret

![Context Menu Annotation Under Cursor](img/context-menu-annot-under-cursor.png)

## Annotation editor

All those commands will open annotation editor:

![Annotation Editor](img/annotation-editor.png)

Here we have a text annotation in the editor.

You can delete annotations, change color, text and other attributes.

## Select annotation from page in editor

When you have many annotations on the page, it's hard to locate the desired annotation in the editor list.

To select an annotation in the editor, place the mouse cursor over an annotation in the page, right click for context menu and use `Select Annotation in Editor`.

![Context Menu Select Annotation In Editor](img/context-menu-select-in-editor.png)

## Moving annotations

To move an annotation on the page, left click with mouse and drag to new location.

## Default colors, size, and opacity

Open **Settings → Advanced Options...** and edit the `Annotations` block:

| Setting | Used for |
| --- | --- |
| `HighlightColor` | New highlights (`a`) — default `#ffff00` |
| `UnderlineColor` | Underline annotations |
| `SquigglyColor` | Squiggly underline |
| `StrikeOutColor` | Strike-out |
| `FreeTextColor` / `FreeTextBackgroundColor` | Free text annotations |
| `FreeTextSize` | Default font size for free text (default `12`) |
| `FreeTextBorderWidth` | Border width for free text |
| `FreeTextOpacity` | `0`–`100` percent opacity for free text |
| `TextIconColor` / `TextIconType` | Sticky-note style icons |
| `DefaultAuthor` | Author name written into new annotations |

Highlight/underline/strikeout opacity is changed **per annotation** in the annotation editor, not via a global default.

See [Advanced options / settings](Advanced-options-settings.md) for the full list.

### Keyboard shortcuts with custom colors

**Ver 3.6+:** `CmdCreateAnnotHighlight` accepts a color argument. You can bind e.g. green highlights to a key — see [Customize keyboard shortcuts](Customize-keyboard-shortcuts.md).

### Toolbar buttons

Add annotation commands to the toolbar via the `Shortcuts` array — see [Customize toolbar](Customize-toolbar.md). Example commands: `CmdCreateAnnotHighlight`, `CmdCreateAnnotUnderline`, `CmdSaveAnnotations`.

## Saving workflow

| Action | Shortcut / command |
| --- | --- |
| Save annotations to file | `Ctrl + Shift + S` (`CmdSaveAnnotations`) |
| Save when closing | Prompt dialog — choose existing file, new file, or discard |

There is **no undo** (`Ctrl + Z`) for annotation edits. Delete an annotation with `Delete` when the cursor is over it, or remove it in the annotation editor.

To avoid the save prompt on every close, save explicitly with `Ctrl + Shift + S` before closing.

## Annotations from other programs

SumatraPDF can display most standard PDF annotations created in Acrobat, Foxit, etc. Some proprietary annotation types may show only an icon. Free-text presets and appearance vary between editors.

## Missing features

We don't yet support every annotation type or every editing operation other PDF apps offer.

The future will be driven by your feedback. If there are features missing or there are better ways of doing things, let me know in [Discussions](https://github.com/sumatrapdfreader/sumatrapdf/discussions)

When providing feedback:

- tell us what
- tell us why. Context is important for prioritizing features. Is new feature / idea something that you absolutely need or is it just a nice improvement
- you might be familiar how other PDF editors work. When referencing feature or UI ideas coming from other apps, tell us which app it is. Screenshots are better than words when describing UI ideas.