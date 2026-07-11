# Themes & chrome (fork feature F) — analysis and work plan

Status: analyzed 2026-07-11. This is fork feature **F** from
[plus-merge.md](plus-merge.md). Verdict there stands: **port ideas, not diffs** —
master's chrome theming (darkmodelib / `DarkModeSubclass`) evolved far past the
fork's base, so their diffs conflict heavily and are partly redundant.

## What the fork changed

### 1. Replaced the theme roster with 5 opinionated themes

`Light-Warm` (default; "eye-care" paper palette: window `#ebe6da`, control bg
`#f5f1e8`, page bg `#f7f3e8`, text `#333333`), `Light-White`, `System`
(follows the OS light/dark setting), `Dark-Dracula` (`#282A36` bg, `#F8F8F2`
text, `#BD93F9` links), `Dark-Black` (pure black AMOLED, `#EDEDED` text,
`#7AA2F7` links). Same `themesTxt` data mechanism master uses.

Master comparison: master already ships 14 built-in themes (incl. Solarized
pair) **plus user-definable themes** via the `Themes` settings array (ver 3.6),
so the fork's palettes are just data.

### 2. Themes drive page rendering colors (fundamental divergence)

Their `ThemePageRenderColors(bg, respectPdfDocColorMode)`: dark chrome ⇒ pages
invert (feeding their smart dark mode, feature D), Light-Warm ⇒ paper-tinted
pages unless the user customized colors. Supporting predicates
`ThemeUsesDarkChrome()` / `ThemeUsesOriginalPageColors()`.

Master deliberately keeps themes chrome-only; page colors belong to
`FixedPageUI` (`InvertColors`, custom colors). Our feature-D port (86e5cb24c)
already adapted to master's philosophy — keep it that way.

### 3. System theme + one-click light/dark toggle

`CmdToggleLightDarkTheme` flips between the last-used light and dark themes,
remembered in new `LastDarkTheme` / `LastLightTheme` settings. The `System`
pseudo-theme resolves to the preferred light/dark theme from the OS dark-mode
setting and re-resolves when Windows switches modes. Master has neither.

### 4. Windows 11-style caption buttons

New `src/CaptionGlyphs.{cpp,h}` (~255 lines) + `tools/gen_caption_glyphs.py`:
min/max/restore/close glyphs drawn from generated polyline geometry, DPI-aware
icon sizing and layout proportions, relayout on `WM_DPICHANGED`, hover-state
polish. Commit series `1cd0c85d6`→`b0002c393`.

### 5. Dark-chrome fix grab-bag

- `c4018ec06` — dark toolbar separators; **maximized window frame edge** colors
  (new WinDynCalls, DWM border attribute); splitter theming.
- `0e493c658` — remove toolbar separator lines, unify button spacing (partly
  for their extra toolbar buttons).
- `597f0c47d` — reset canvas scrollbar theme when switching dark→light
  (scrollbars stayed dark).
- `81defc78b` — Light-Warm menubar chrome when tabs are disabled.
- `36468333a` — broad polish: menu check marks, tab UI, Favorites, ChmModel,
  ebook renderer colors for dark themes.
- Already handled during the feature-K review: TOC scrollbar mouse-capture
  (`5491f5520`, ported as `a1b40ef1f`); dark TOC scrollbar (`2183086c1`,
  skipped — master's recursive darkmodelib pass covers it).

## Ranked plan for master (each as its own commit)

1. **System theme + light/dark toggle** — implement natively:
   `Theme = System` special value resolved against the OS apps-dark-mode
   setting, re-resolved on `WM_SETTINGCHANGE` (`ImmersiveColorSet`);
   `CmdToggleLightDarkTheme` command; `LastLightTheme` / `LastDarkTheme`
   settings updated whenever the user picks a theme ("is dark" =
   `!IsLightColor(ThemeWindowBackgroundColor())`).
2. **New theme palettes** — add `Light Warm`, `Dark Dracula`, `Dark Black`
   entries to `themesTxt` in `Theme.cpp` (chrome-only; do NOT tint pages —
   that's `FixedPageUI`'s job in master).
3. **Caption glyphs** — port `CaptionGlyphs.{cpp,h}` + wire into master's
   caption button painting (master: `src/Caption.cpp`); keep the generator
   script `tools/gen_caption_glyphs.py`.
4. **Chrome fixes** — verify symptoms on master first; port only what
   reproduces (candidates: scrollbar staying dark after switching to a light
   theme; maximized frame edge color in dark themes). The rest is presumed
   superseded by darkmodelib.

## Gotchas

- Fork's theme code assumes their fixed 5-theme indexes (`kThemeIdx*`) —
  master's theme list is dynamic (built-ins + user themes); never port
  index-based logic.
- Fork's `ThemeUsesDarkChrome()` has no master equivalent; master idiom is
  `!IsLightColor(ThemeWindowBackgroundColor())` (used by features D/H ports).
- Their caption layout code lives in *their* `SumatraPDF.cpp`; master's caption
  painting is in `Caption.cpp` — find the right seam rather than following
  their diff.
