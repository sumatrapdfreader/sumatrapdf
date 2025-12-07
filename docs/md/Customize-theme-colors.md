# Customize theme (colors)

**Available in version 3.6 or later.**

You can change colors of SumatraPDF UI by creating a custom theme using `Themes` [advanced setting](Advanced-options-settings.md).

To create a theme:
- Navigate to `Settings` / `Advanced Options...` menu (or `Ctrl + K` Command Palette, type `adv` to narrow down and select `Advanced Options...` command)
- This will open `SumatraPDF-settings.txt` text file using your default text editor
- Scroll down to the bottom to find `Themes` array and add new shortcut definitions

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
	[
		Name = Solarized Dark
		TextColor = #839496
		BackgroundColor = #002b36
		ControlBackgroundColor = #073642
		LinkColor = #268bd2
		ColorizeControls = true
	]
	[
		Name = Dracula
		TextColor = #f8f8f2
		BackgroundColor = #282a36
		ControlBackgroundColor = #44475a
		LinkColor = #8be9fd
		ColorizeControls = true
	]
	[
		Name = Nebula
		TextColor = #CBE3E7
		BackgroundColor = #100E23
		ControlBackgroundColor = #1E1C31
		LinkColor = #91DDFF
		ColorizeControls = true
	]
	[
		Name = Greeny
		TextColor = #FDD085
		BackgroundColor = #4F6232
		ControlBackgroundColor = #1E3304
		LinkColor = #A2E53B
		ColorizeControls = true
	]
	[
		Name = Choco
		TextColor = #D7AD62
		BackgroundColor = #2A1104
		ControlBackgroundColor = #172736
		LinkColor = #E8CD12
		ColorizeControls = true
	]
	[
		Name = Purpy
		TextColor = #E2C3C3
		BackgroundColor = #20222A
		ControlBackgroundColor = #1E0126
		LinkColor = #EFF0B8
		ColorizeControls = true
	]
]
```

The above will provide you with a collection of themes: `My Dark Theme`, `Solarized Dark`, `Dracula` and so on.

Meaning of the parameters:
- `TextColor` and `BackgroundColor` are for main window color and color of text.
- If you use `I` (`CmdInvertColors`) they will also be used to replace white background / black text color when renderign PDF/ePub documents.
- `ControlBackgroundColor` is for background of Windows controls (buttons, window frame, menus, list controls etc.).
- `LinkColor` is a color for links. Typically it's blue.
- `ColorizeControls` should be `true`. If `false` we won't try to change colors of standard windows controls (menu, toolbar, buttons etc.) so a lot of UI will not respect theme colors.

Now, once you save the text file above, there are three main ways to choose a theme that you had created:
1. By changing the value of `Theme = ` in the `SumatraPDF-settings.txt` which we accessed above (e.g. `Theme = Solarized Dark`). Or,
2. By launching the Command Palette (`Ctrl + K`), and then typing `theme` (afterwards, select the desired theme from the list). Or,
3. By navigating to `Settings` / `Theme` and then choosing a theme.
