# CAD / Engineering Drawings

SumatraPDF can thicken hairline strokes and darken mid-gray lines in CAD and engineering-drawing PDFs, so the drawing stays readable when you zoom out, closer to Acrobat.

**Available in version 3.7 or later.** PDF only.

**Set `EngineeringDrawingEnhance = auto` to enhance only PDFs that look like CAD drawings.** At a glance:

- **Zoom-aware minimum stroke width:** hairlines get thicker when zoomed out; no effect when zoomed in.
- **Darker grays:** typical CAD-export grays are darkened.
- **Global setting:** `EngineeringDrawingEnhance` = `off` / `auto` / `on`.
- **Per-file toggle:** `Toggle Engineering Drawing Enhancement` in [Command Palette](Command-Palette.md), current PDF only.

Text geometry is not changed. Off by default. The Explorer preview pane and the Windows Search filter never apply it.

## Enhance drawings in every PDF

Set `EngineeringDrawingEnhance` in [advanced settings](Advanced-options-settings.md):

```
EngineeringDrawingEnhance = off
```

| Value  | Effect                                             |
| ------ | -------------------------------------------------- |
| `off`  | Never enhance (default)                            |
| `auto` | Enhance only when the PDF looks like a CAD drawing |
| `on`   | Enhance every PDF                                  |

Change it in any of these ways:

- **Advanced Settings** dialog: **Settings → Advanced Settings...**, filter `EngineeringDrawingEnhance`, pick `off` / `auto` / `on`, Save. Or `Ctrl + K`, `Advanced Settings...` in [Command Palette](Command-Palette.md).
- **[Command Palette](Command-Palette.md) settings:** `Ctrl + K`, type `=EngineeringDrawingEnhance`, Enter, pick a value. Takes effect immediately, no restart.
- **Settings file:** **Settings → Open Advanced Settings File...**, edit `EngineeringDrawingEnhance = …`, save.

The setting is global: it applies to every PDF. If a file is already open, reopen it (or use the [per-file toggle](#toggle-enhancement-for-one-file)) so rendering picks up the new value.

## Toggle enhancement for one file

**Toggle Engineering Drawing Enhancement** (`CmdToggleEngineeringDrawingEnhance`) flips enhancement for the **current PDF only**.

- `Ctrl + K`, `Toggle Engineering Drawing Enhancement` in [Command Palette](Command-Palette.md)
- No default keyboard shortcut; bind one in [Customize keyboard shortcuts](Customize-keyboard-shortcuts.md)

The first use flips away from whatever is in effect (so with the default `off`, it turns enhancement on). After that it alternates on/off for that document.

This override is **session-only**. It is not saved in `FileStates`. Closing the file (or the tab) restores the global setting. It does not change `EngineeringDrawingEnhance` for other files.

## Tips

- Use `auto` to leave ordinary PDFs untouched.
- Use the per-file toggle to compare a drawing with and without enhancement.
- Use `=EngineeringDrawingEnhance` in the Command Palette to switch values without a restart.
- Bind a shortcut to `CmdToggleEngineeringDrawingEnhance` if you toggle often.

## How `auto` detects drawings

`auto` looks for a PDF/E marker, a CAD authoring tool in the document metadata, or content heuristics on the first pages (hairline vectors, raster screenshots of drawings, WPS-style hairline exports).

## Global vs per file

|       | Global `EngineeringDrawingEnhance` | Per-file toggle                        |
| ----- | ---------------------------------- | -------------------------------------- |
| Scope | All PDFs                           | Current document                       |
| Saved | Yes, in settings                   | No                                     |
| Wins  | Used unless the file has a toggle  | Beats the global setting for that file |

## See also

- [Advanced settings](Advanced-options-settings.md) — `EngineeringDrawingEnhance`
- [Commands](Commands.md) (`CmdToggleEngineeringDrawingEnhance`) — for rebinding
- [Command Palette](Command-Palette.md) — run the toggle and change settings
