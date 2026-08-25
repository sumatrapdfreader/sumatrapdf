# Customize theme (colors)

**Available in version 3.6 or later.**

You can change the colors of the SumatraPDF UI by creating a custom theme with the [`Themes` advanced setting](Advanced-options-settings.md).

To create a theme:

- Navigate to the `Settings` / `Advanced Options...` menu (or open the Command Palette with `Ctrl + K`, type `adv` to narrow the results, and select the `Advanced Options...` command)
- This opens the `SumatraPDF-settings.txt` file in your default text editor
- Scroll to the bottom, find the `Themes` array, and add new theme definitions

Example of customization:

```
Themes [
    [
        Name = My Dark Theme
        TextColor = #bac9d0
        BackgroundColor = #263238
        ControlBackgroundColor = #263238
        LinkColor = #8aa3b0
        DisabledTextColor = #6b7c85
        DarkerTextColor = #8aa3b0
        HotBackgroundColor = #324047
        EdgeColor = #37474f
        HotEdgeColor = #546e7a
        DisabledEdgeColor = #1e272c
        ErrorBackgroundColor = #5c2b2b
        NotificationBackgroundColor = #2e3c43
        NotificationHighlightColor = #455a64
        NotificationHighlightTextColor = #eceff1
        ColorizeControls = true
    ]
    [
        Name = Dracula
        TextColor = #f8f8f2
        BackgroundColor = #282a36
        ControlBackgroundColor = #44475a
        LinkColor = #8be9fd
        DisabledTextColor = #6272a4
        DarkerTextColor = #6272a4
        HotBackgroundColor = #565a73
        EdgeColor = #6272a4
        HotEdgeColor = #bd93f9
        DisabledEdgeColor = #343746
        ErrorBackgroundColor = #ff5555
        NotificationBackgroundColor = #343746
        NotificationHighlightColor = #bd93f9
        NotificationHighlightTextColor = #f8f8f2
        ColorizeControls = true
    ]
]
```

The above will provide you with custom themes such as `My Dark Theme` and `Dracula`. Built-in themes already ship with these colors filled in, including Solarized, Dracula, **One Dark**, **Monokai**, **Nord**, **GitHub Dark**, **Catppuccin Mocha**, **Tokyo Night**, **Gruvbox**, **Night Owl**, **Ayu**, and **Palenight**.

Meaning of the parameters:

**Required / base colors**

- `TextColor` and `BackgroundColor` set the main window's text and background colors.
- `ControlBackgroundColor` sets the background of Windows controls (buttons, window frame, menus, list controls, etc.).
- `LinkColor` sets the color of links. It is typically blue.
- `ColorizeControls` should be `true`. If it is `false`, we won't try to change the colors of standard Windows controls (menus, toolbars, buttons, etc.), so much of the UI will not respect the theme colors.

## Document page colors (`DocumentColorsFollowTheme`)

UI themes only recolor chrome (menus, tabs, toolbars). Separately, **document pages** (PDF, XPS, DjVu, EPUB, MOBI, FB2, comics, images, and other MuPDF-rendered formats) can follow the theme via the advanced setting **`DocumentColorsFollowTheme`**:

| Value               | Effect                                                                                                                                                                                                                                                                                                                                             |
| ------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **`off`** (default) | Keep the document’s own page colors.                                                                                                                                                                                                                                                                                                               |
| **`smart`**         | Recolor text and page background using `FixedPageUI.TextColor` / `FixedPageUI.BackgroundColor` when set, otherwise the current theme’s text/background. **Photos and other images stay as in the file** — preferred for dark reading. Reflowable MuPDF formats (EPUB, HTML, FB2, MOBI) do this with CSS rather than by recoloring the page bitmap. |
| **`legacy`**        | Also recolor images on PDF, XPS, and DjVu (older invert / colorize behavior). Reflowable MuPDF formats still use CSS, so images stay original.                                                                                                                                                                                                     |

How to change it:

- **Settings → Theme…** or **Settings → Make Document Colors Follow Theme** (command palette / `CmdSetDocumentColorsFollowTheme`) for a drop-down including **`legacy`**.
- Advanced settings: `DocumentColorsFollowTheme = off` / `smart` / `legacy`.

`Shift + I` (`CmdInvertColors`) is a different thing: it swaps the page colors for the rest of the session, whatever `DocumentColorsFollowTheme` and `FixedPageUI` are set to, and is not saved to the settings file.

This is independent of `Theme = ...`. You can use a dark UI theme with `DocumentColorsFollowTheme = off` (original white PDF pages) or a light UI with `smart` page recoloring.

**Optional UI colors** (leave empty to derive from the base colors; set them when a warm or tinted `TextColor` would make derived disabled/hover colors look wrong — e.g. Dracula’s near-white foreground)

- `DisabledTextColor` — grayed / disabled labels and buttons
- `DarkerTextColor` — secondary / muted text
- `HotBackgroundColor` — hovered control background
- `EdgeColor` / `HotEdgeColor` / `DisabledEdgeColor` — control borders
- `ErrorBackgroundColor` — error surfaces
- `NotificationBackgroundColor` / `NotificationHighlightColor` / `NotificationHighlightTextColor` — in-app notification tips

After you save the settings file, there are three main ways to choose a theme that you created:

1. Change the value of `Theme = ` in `SumatraPDF-settings.txt` (e.g. `Theme = Solarized Dark`).
2. Open the Command Palette (`Ctrl + K`), type `theme`, and select the desired theme from the list.
3. Navigate to `Settings` / `Theme` and choose a theme.
