# Customize theme (colors)

**Available in version 3.6 or later.**

You can change colors of SumatraPDF UI by creating a custom theme using `Themes` [advanced setting](Advanced-options-settings.md).

To create a theme:

- use `Settings` / `Advanced Options...` menu (or `Ctrl + K` Command Palette, type `adv` to narrow down and select `Advanced Options...` command)
- this opens default .txt editor with advanced settings file
- find `Themes` array and add new shortcut definitions

Example of customization:

```
Themes [
    [
        Name = My Dark Theme
        TextColor = #bac9d0
        BackgroundColor = #263238
        ControlBackgroundColor = #263238
        LinkColor = #8aa3b0
        ColorizeControls = true
    ]
]
```

`TextColor` and `BackgroundColor` are for main window color and color of text.

If you use `I` (`CmdInvertColors`) they will also be used to replace white background / black text color when renderign PDF/ePub documents.

`ControlBackgroundColor` is for background of Windows controls (buttons, window frame, menus, list controls etc.).

`LinkColor` is a color for links. Typically it's blue.

`ColorizeControls` should be `true`. If `false` we won't try to change colors of standard windows controls (menu, toolbar, buttons etc.) so a lot of UI will not respect theme colors.
