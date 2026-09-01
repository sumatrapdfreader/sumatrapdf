# LaTeX integration

SumatraPDF is a common PDF previewer for LaTeX on Windows: it opens quickly, reloads when the PDF is rebuilt, and supports **SyncTeX** so you can jump between source and PDF. [Typst](https://typst.app/) users on Windows use it the same way for live PDF reload (see [Typst](#typst)); Typst does not emit SyncTeX.

For LaTeX edited/compiled **inside WSL**, see [LaTeX integration with WSL](LaTeX-integration-wsl.md). Editors can also drive Sumatra with [DDE commands](DDE-Commands.md); prefer command-line `-forward-search` when possible (simpler and Unicode-friendly).

## SyncTeX: forward and inverse search

Compile with SyncTeX enabled (most tools use `-synctex=1` or an equivalent). That produces a `.synctex` / `.synctex.gz` next to the PDF.

| Direction          | Who starts it    | What happens                                                                                     |
| ------------------ | ---------------- | ------------------------------------------------------------------------------------------------ |
| **Forward search** | Editor → Sumatra | Jump from a `.tex` line to the matching place in the PDF (highlight).                            |
| **Inverse search** | Sumatra → editor | Double-click (or inverse-search click) in the PDF opens the matching source line in your editor. |

Forward search is usually configured **in the editor** (viewer command / external PDF viewer). Inverse search is configured **in Sumatra** via the inverse-search command line (below).

Recommended Sumatra settings (Advanced Options or once via the inverse-search dialog):

```
EnableTeXEnhancements = true
ReuseInstance = true
ReloadModifiedDocuments = true
```

- **EnableTeXEnhancements** — shows TeX-related UI (including inverse-search options) and enables inverse search on click when configured.
- **ReuseInstance** — one Sumatra process reuses the open window when the editor launches the viewer again (avoids many windows).
- **ReloadModifiedDocuments** — reloads the PDF after LaTeX rebuilds it.

## Inverse search command line (`InverseSearchCmdLine`)

**Purpose:** when you inverse-search in the PDF, Sumatra builds a process command line from this pattern and launches your editor at the matching source file and line.

**Where to set it (any one of these):**

1. `Ctrl + K` [command palette](Command-Palette.md) → **Set Inverse Search Command Line** (detects common editors; enables TeX enhancements on OK)
2. **Settings → Options** (when TeX enhancements are on) → inverse-search field
3. Advanced setting `InverseSearchCmdLine` in `SumatraPDF-settings.txt`
4. One-shot via command line: `-inverse-search "<command-line>"` (writes the same setting)

**Placeholders** (replaced by Sumatra when launching the editor):

| Token | Meaning                             |
| ----- | ----------------------------------- |
| `%f`  | Full path of the source `.tex` file |
| `%l`  | Line number (1-based)               |
| `%c`  | Column (when available; often 0)    |

Use quotes around paths that may contain spaces. Adjust executable paths to match your install (Program Files vs Program Files (x86), user installs, portable layouts).

**Example (Notepad++):**

```
"C:\Program Files\Notepad++\notepad++.exe" -n%l "%f"
```

---

## Setup by editor / environment

In each section: (1) configure Sumatra inverse search, (2) configure the editor to open Sumatra and run forward search, (3) compile with SyncTeX.

Paths below use common 64-bit install locations; change them if yours differ.

### TeXstudio

**1. Inverse search (in Sumatra)**

```
"C:\Program Files\TeXstudio\texstudio.exe" "%f" -line %l
```

**2. Viewer / forward search (in TeXstudio)**

- **Options → Configure TeXstudio → Commands** (or **Build**)
- Set **External PDF viewer** / PDF viewer command to something like:

```
"C:/Program Files/SumatraPDF/SumatraPDF.exe" -reuse-instance -forward-search "?c:am.tex" @ "?am.pdf"
```

TeXstudio’s `?c:am.tex`, `@` (line), and `?am.pdf` expand to the current root source, line, and PDF. Older docs used `-inverse-search` on this same command line; setting `InverseSearchCmdLine` once in Sumatra is enough and easier to maintain.

**3. Use it**

- Build with SyncTeX (default in TeXstudio PDF workflows).
- Forward: TeXstudio’s “Go to PDF” / jump to PDF action.
- Inverse: double-click in Sumatra.

### Visual Studio Code / Cursor (LaTeX Workshop)

Install the [LaTeX Workshop](https://marketplace.visualstudio.com/items?itemName=James-Yu.latex-workshop) extension. External-viewer SyncTeX is not officially supported by LaTeX Workshop; this is the setup that works with Sumatra.

**1. Inverse search (in Sumatra)**

Set this once (`Ctrl + K` → **Set Inverse Search Command Line**, or Advanced Options). Overwrite `InverseSearchCmdLine` in place; do not paste it under the “Settings below are not recognized” footer, or Sumatra ignores it.

Prefer the `code` / `cursor` CLI on `PATH`, or a full path to the `.cmd` launcher:

```
"C:\Users\<you>\AppData\Local\Programs\Microsoft VS Code\bin\code.cmd" -r -g "%f:%l"
```

`-r` reuses the existing VS Code window. `-g "%f:%l"` opens file `%f` at line `%l` (lowercase L). Using `.cmd` (not only `Code.exe`) avoids some inverse-search failures on Windows. For Cursor, use the analogous `cursor.cmd` path.

If that still fails, launch via Electron’s CLI:

```
"C:\Users\<you>\AppData\Local\Programs\Microsoft VS Code\Code.exe" "C:\Users\<you>\AppData\Local\Programs\Microsoft VS Code\resources\app\out\cli.js" --ms-enable-electron-run-as-node -r -g "%f:%l"
```

Also set `EnableTeXEnhancements = true` (the inverse-search dialog does this on OK).

**2. External viewer (in VS Code `settings.json`)**

`Ctrl + Shift + P` → **Preferences: Open User Settings (JSON)**. Point `viewer.command` / `synctex.command` at your Sumatra (installer under `C:/Program Files/SumatraPDF/`, or portable under `%LOCALAPPDATA%/SumatraPDF/`).

```json
{
  "latex-workshop.view.pdf.viewer": "external",
  "latex-workshop.view.pdf.external.viewer.command": "C:/Program Files/SumatraPDF/SumatraPDF.exe",
  "latex-workshop.view.pdf.external.viewer.args": ["-reuse-instance", "%PDF%"],
  "latex-workshop.view.pdf.external.synctex.command": "C:/Program Files/SumatraPDF/SumatraPDF.exe",
  "latex-workshop.view.pdf.external.synctex.args": ["-reuse-instance", "-forward-search", "%TEX%", "%LINE%", "%PDF%"]
}
```

`%TEX%`, `%LINE%`, and `%PDF%` are LaTeX Workshop placeholders. Do not add `-inverse-search "..."` here once Sumatra already has `InverseSearchCmdLine`; sending it on every jump can cause issues.

**3. Use it**

- Build so SyncTeX is produced (LaTeX Workshop recipes usually pass `-synctex=1`).
- View PDF: LaTeX Workshop **View LaTeX PDF file in external viewer**.
- Forward: **SyncTeX from cursor** (`Ctrl + Alt + J`).
- Inverse: double-click in Sumatra → VS Code focuses `%f` at line `%l`.

For LaTeX inside WSL Remote, see [LaTeX integration with WSL](LaTeX-integration-wsl.md) (TexLab, not LaTeX Workshop).

### TeXworks

**1. Inverse search (in Sumatra)**

```
"C:\Program Files\MiKTeX\miktex\bin\x64\texworks.exe" -p=%l "%f"
```

(or your TeX Live / standalone TeXworks path)

**2. Viewer (in TeXworks)**

- **Edit → Preferences → Typesetting** (or **Preview**)
- PDF viewer: SumatraPDF, or a custom command:

```
"C:\Program Files\SumatraPDF\SumatraPDF.exe" -reuse-instance "$fullname.pdf"
```

Enable SyncTeX in the typesetting tool arguments (`-synctex=1` on pdfLaTeX/XeLaTeX/LuaLaTeX). TeXworks’ “jump to PDF” uses SyncTeX when the viewer supports it; with Sumatra, forward search is often:

```
"C:\Program Files\SumatraPDF\SumatraPDF.exe" -reuse-instance -forward-search "$fullname.tex" $line "$fullname.pdf"
```

(exact placeholders depend on TeXworks version; use its help for `$fullname` / line variables).

### WinEdt

WinEdt has built-in SumatraPDF integration.

**1. PDF viewer (in WinEdt)**

- **Options → Execution Modes → PDF Viewer**
- Uncheck Auto-detect if needed, choose **SumatraPDF**, or browse to `SumatraPDF.exe`.

WinEdt’s macros typically call Sumatra with `-forward-search` (preferred over older DDE).

**2. Inverse search (in Sumatra)**

```
"C:\Program Files\WinEdt Team\WinEdt\WinEdt.exe" "[Open(|%f|);SelPar(%l,8)]"
```

**3. Use it**

- Compile with SyncTeX enabled in the execution mode.
- Forward: WinEdt’s PDF preview / search commands.
- Inverse: double-click in Sumatra.

### TeXnicCenter

**1. Inverse search (in Sumatra)**

```
"C:\Program Files\TeXnicCenter\TeXnicCenter.exe" /nosplash /ddecmd "[goto('%f', '%l')]"
```

**2. Output profile viewer (in TeXnicCenter)**

- **Build → Define Output Profiles** (`Alt + F7`) → PDF profile → **Viewer**
- **Executable path:** `C:\Program Files\SumatraPDF\SumatraPDF.exe` (path only)
- **View project’s output:** command-line argument `"%bm.pdf"`
- **Forward search:**

```
-reuse-instance -forward-search "%Wc" %l "%bm.pdf"
```

**3. Sumatra**

- `ReuseInstance = true`, `ReloadModifiedDocuments = true`, `EnableTeXEnhancements = true`
- Build and view once, then inverse-search with double-click.

### Texmaker

**1. Inverse search (in Sumatra)**

```
"C:\Program Files\Texmaker\texmaker.exe" "%f" -line %l
```

**2. Viewer (in Texmaker)**

- **Options → Configure Texmaker → Commands**
- **External viewer:**

```
"C:/Program Files/SumatraPDF/SumatraPDF.exe" -reuse-instance %.pdf
```

For SyncTeX forward search (if your Texmaker version supports a dedicated “built-in + external” / jump option), use:

```
"C:/Program Files/SumatraPDF/SumatraPDF.exe" -reuse-instance -forward-search %.tex @ %.pdf
```

(`%.tex` / `%.pdf` / `@` are Texmaker placeholders for root name and line.)

Ensure **PdfLaTeX** (etc.) includes `-synctex=1`.

### Notepad++

Notepad++ is not a full LaTeX IDE; pair it with a build tool (e.g. NppExec + `latexmk`, or an external Makefile).

**1. Inverse search (in Sumatra)**

```
"C:\Program Files\Notepad++\notepad++.exe" -n%l "%f"
```

**2. Open / forward search**

Run Sumatra with something like (NppExec / Run dialog; expand paths to match your macros):

```
"C:\Program Files\SumatraPDF\SumatraPDF.exe" -reuse-instance -forward-search "$(FULL_CURRENT_PATH)" $(CURRENT_LINE) "$(CURRENT_DIRECTORY)\$(NAME_PART).pdf"
```

### Sublime Text (LaTeXTools or similar)

**1. Inverse search (in Sumatra)**

```
"C:\Program Files\Sublime Text\sublime_text.exe" "%f:%l:%c"
```

**2. LaTeXTools**

In LaTeXTools settings, set the Windows viewer to SumatraPDF (or the package’s Sumatra preset). LaTeXTools documents the forward-search invocation; it generally calls Sumatra with `-forward-search` and `-reuse-instance`.

### Vim / gVim

**1. Inverse search (in Sumatra)**

Simple open at line:

```
"C:\Program Files\Vim\vim91\gvim.exe" --remote-silent +%l "%f"
```

Or, with a custom callback (see below), a `--remote-send` form.

**2. Forward search** (example, Vim9 script in `ftplugin/tex.vim`):

```vim
def ForwardSearch()
  var root = expand('%:p:r')
  system($'SumatraPDF.exe -reuse-instance -forward-search {root}.tex {line(".")} {root}.pdf')
enddef
nnoremap <buffer> <F5> <ScriptCmd>ForwardSearch()<CR>
```

Use the full path to `SumatraPDF.exe` if it is not on `PATH`.

**3. Optional callback inverse search**

Define a global function the inverse-search line can call, then set:

```
gvim --servername GVIM --remote-send ":call BackwardSearch(%l, '%f')<CR>"
```

(with a matching `BackwardSearch` implementation in your config).

### Emacs (AUCTeX)

See [EmacsWiki AUCTeX – SumatraPDF](https://www.emacswiki.org/emacs/AUCTeX#toc25) for the maintained recipe.

Typical pieces:

- **Inverse search (Sumatra):**  
  `"C:\...\emacsclientw.exe" -n +%l "%f"`  
  (start an Emacs server first)
- **Forward search:** AUCTeX viewer entry calling  
  `SumatraPDF.exe -reuse-instance -forward-search %b.tex %n %o`  
  (exact expandos depend on your AUCTeX viewer config)

### LyX

On Windows, set the PDF viewer to SumatraPDF under **Tools → Preferences → File Handling → File Formats** (PDF).

For SyncTeX-style jumps, use a PDF viewer converter / view command that launches:

```
SumatraPDF.exe -reuse-instance "$o"
```

and set inverse search in Sumatra to your LyX executable with the arguments LyX documents for SyncTeX inverse search (version-specific). See [LyX + Sumatra forward/inverse search notes](http://joonro.github.io/blog/posts/inverse-and-forward-search-lyx-windows/) for a full walkthrough.

### Typst

[Typst](https://typst.app/) is a markup typesetter (not LaTeX). The Typst project [recommends SumatraPDF on Windows](https://typst.app/open-source/) because Sumatra reloads the PDF while `typst watch` rewrites it and does not lock the file.

The Typst CLI does **not** write SyncTeX (`.synctex` / `.synctex.gz`). Sumatra’s forward search (`-forward-search`) and inverse search (double-click to source) need that file, so they do not work for `.typ` the way they do for LaTeX. For click-to-source, use [Tinymist](https://github.com/Myriad-Dreamin/tinymist)’s built-in preview, not Sumatra.

**1. Sumatra**

```
ReuseInstance = true
ReloadModifiedDocuments = true
```

**2. CLI**

In the project directory:

```
typst watch main.typ
```

Open `main.pdf` in Sumatra (or run `typst compile --open main.typ` once if Sumatra is the default PDF viewer). Each save rebuilds the PDF; Sumatra reloads it.

**3. VS Code / Cursor (Tinymist)**

Install [Tinymist Typst](https://marketplace.visualstudio.com/items?itemName=myriad-dreamin.tinymist). To keep a PDF next to the source for Sumatra:

```json
{
  "tinymist.exportPdf": "onSave"
}
```

Use `"onType"` to rewrite the PDF on every keystroke (heavier). Open that PDF in Sumatra. Inverse-search command line is the same as for [LaTeX Workshop](#visual-studio-code--cursor-latex-workshop) (`code.cmd -r -g "%f:%l"`), but without SyncTeX it will not jump to a line.

**4. Use it**

- Edit `.typ` → compiler or Tinymist writes PDF → Sumatra reloads.
- Source ↔ preview jumps: Tinymist preview, not Sumatra.

---

## Tips and troubleshooting

1. **Always compile with SyncTeX** (`-synctex=1`). Without `.synctex`/`.synctex.gz`, jumps cannot map lines. Typst does not emit SyncTeX; see [Typst](#typst).
2. **Set inverse search once in Sumatra** rather than stuffing a long `-inverse-search "..."` into every editor viewer command.
3. **Paths with spaces** must be quoted; placeholders `%f` / `%l` stay outside or inside quotes as in the examples.
4. **PDF does not reload after build** — set `ReloadModifiedDocuments = true`; avoid locking the PDF (Sumatra does not lock like some readers).
5. **Many Sumatra windows** — set `ReuseInstance = true` and pass `-reuse-instance` from the editor when opening/forward-searching.
6. **Wrong or missing inverse editor** — use **Set Inverse Search Command Line** and pick a detected editor, or paste a command that works from a `cmd` prompt with a sample file and line.
7. **Accented / non-ASCII paths** — use a Unicode-capable editor launch line; prefer UTF-8 locale / editor settings (see also historical notes on inverse search and encoding).
8. **WSL sources** — use [LaTeX integration with WSL](LaTeX-integration-wsl.md).
9. **DDE** — still works for some editors (e.g. TeXnicCenter `goto`); for new setups prefer `-forward-search` and a normal process command for inverse search. See [DDE Commands](DDE-Commands.md).

## Related

- [Command-line arguments](Command-line-arguments.md) (`-forward-search`, `-inverse-search`, forward-search highlight flags)
- [DDE Commands](DDE-Commands.md)
- [LaTeX integration with WSL](LaTeX-integration-wsl.md)
- [Typst](https://typst.app/), [Tinymist](https://github.com/Myriad-Dreamin/tinymist)
- Advanced settings: `InverseSearchCmdLine`, `EnableTeXEnhancements`, `ForwardSearch` highlight options
