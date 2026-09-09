# CAD / Engineering Drawings

**Available in version 3.7 or later.** PDF only.

CAD and engineering-drawing PDFs often use hairline strokes and mid-gray lines that fade when you zoom out. SumatraPDF can thicken those strokes (zoom-aware minimum width) and darken typical CAD-export grays so the drawing stays readable, closer to Acrobat. The effect is stronger when zoomed out and does nothing when zoomed in. Text geometry is not changed.

Off by default. The Explorer preview pane and the Windows Search filter never apply it.

## Global setting: `EngineeringDrawingEnhance`

In [advanced settings](Advanced-options-settings.md):

```
EngineeringDrawingEnhance = off
```

| Value  | Effect                                             |
| ------ | -------------------------------------------------- |
| `off`  | Never enhance (default)                            |
| `auto` | Enhance only when the PDF looks like a CAD drawing |
| `on`   | Enhance every PDF                                  |

`auto` looks for a PDF/E marker, a CAD authoring tool in the document metadata, or content heuristics on the first pages (hairline vectors, raster screenshots of drawings, WPS-style hairline exports).

### How to change it

- **Advanced Settings** dialog: `Settings` → **Advanced Settings...**, filter `EngineeringDrawingEnhance`, pick `off` / `auto` / `on`, Save. Or Command Palette (`Ctrl + K`), type `adv`, choose **Advanced Settings...**.
- **Command Palette settings:** `Ctrl + K`, type `=EngineeringDrawingEnhance`, Enter, pick a value. Takes effect immediately, no restart.
- **Advanced Options** text file: `Settings` → **Advanced Options...**, edit `EngineeringDrawingEnhance = …`, save.

The setting is global: it applies to every PDF. If a file is already open, reopen it (or use the toggle below) so rendering picks up the new value.

## Per-file toggle

**Toggle Engineering Drawing Enhancement** (`CmdToggleEngineeringDrawingEnhance`) flips enhancement for the **current PDF only**.

- Command Palette (`Ctrl + K`): type `engineering` or `cad` and run **Toggle Engineering Drawing Enhancement**
- No default keyboard shortcut; bind one in [Customize keyboard shortcuts](Customize-keyboard-shortcuts.md)

The first use flips away from whatever is in effect (so with the default `off`, it turns enhancement on). After that it alternates on/off for that document.

This override is **session-only**. It is not saved in `FileStates`. Closing the file (or the tab) restores the global setting. It does not change `EngineeringDrawingEnhance` for other files.

## Global vs per file

|       | Global `EngineeringDrawingEnhance` | Per-file toggle                        |
| ----- | ---------------------------------- | -------------------------------------- |
| Scope | All PDFs                           | Current document                       |
| Saved | Yes, in settings                   | No                                     |
| Wins  | Used unless the file has a toggle  | Beats the global setting for that file |

## See also

- [Advanced options / settings](Advanced-options-settings.md)
- [Commands](Commands.md) (`CmdToggleEngineeringDrawingEnhance`)
- [Command Palette](Command-Palette.md)
