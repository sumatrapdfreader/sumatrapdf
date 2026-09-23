# Writing user docs (sumatra-website www/docs)

Pattern for feature pages, modeled on https://outliner.tana.inc/learn/features/sidebar.

## Page shape

1. `# Title` — the feature name, as it appears in the UI.
2. **Summary** (1–2 sentences, first thing after the title): what the feature is,
   where it lives in the UI, what you use it for. No version note before it.
3. Version note on its own line after the summary: `**Available in version 3.6 or later.**`
4. **Bold lead sentence** naming the most common use, then an **at a glance** list:
   one bullet per capability, `**Name:** what it does`, with its shortcut.
5. Task sections, in order of how often people need them.
   - Headings are tasks, not nouns: "Add a favorite", "Collapse the sidebar", not "Favorites".
   - Under each, list **every way** to do it: mouse / menu, keyboard shortcut,
     [Command Palette](Command-Palette.md) command, advanced setting, command-line.
6. `## Tips` — 3–6 short, practical bullets ("Use X to ...").
7. Reference sections (settings tables, flags, DDE) near the end.
8. `## See also` — links to related pages with a few words on why.

Short pages (troubleshooting, one-off how-tos) only need 1, 2 and the content.

## Style

- Second person, present tense, imperative steps: "Press `Ctrl + B`".
- Shortcuts in code: `Ctrl + Shift + N`. Menu paths bold: **Settings → Advanced Settings...**.
- Command Palette: `Ctrl + K`, then `Toggle Favorites`. Name the command id (`CmdFavoriteToggle`) once, for rebinding.
- Put the fix right after the problem: "If X, do Y."
- Tips, notes and caveats are one line, prefixed `Tip:` / `Note:`. No paragraphs of background.
- Link the first mention of another feature to its page.
- Don't document bugs or history; say how it works now. Keep `(ver 3.7+)` markers where behavior differs by version.

## Videos

A video goes on its own line, e.g. right after the title:
`:video <youtube link> <r2 link>` (the R2 link is the web version under
`https://files.sumatrapdfreader.org/assets/sumatrapdf/docs/video/`). The website and the
in-app manual (`gen_docs.render.js`) render it as an embedded YouTube player. How to record
and upload one: "Doc videos" in the website's `agents.md`.

## Index

`SumatraPDF-documentation.md` groups pages by task (getting started, reading, ...).
Every page link must stay inside a `:columns` block — the in-app TOC is built from those.
Pages under `## Misc docs` (up to `## Downloads`) are excluded from the in-app manual.
