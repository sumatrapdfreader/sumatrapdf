# LaTeX Integration with WSL

Starting with version 3.7, SumatraPDF can preview LaTeX documents that
you edit and compile inside WSL (Windows Subsystem for Linux). It fully
supports source-to-PDF and PDF-to-source navigation, just as it does in
an all-Windows setup.

## How Previewing Works

When you compile a `.tex` file, most LaTeX tools can also generate a
SyncTeX file (`.synctex` or `.synctex.gz`) alongside the PDF. This file
records which line of the source corresponds to each piece of text in the
PDF. SumatraPDF uses this information to provide:

* **Forward search**: jump from a line in the source file to the
  corresponding location in the PDF.
* **Inverse search**: jump from a location in the PDF back to the
  corresponding line in the source file.

## Forward Search

To trigger forward search from a WSL terminal, run:

```bash
/mnt/c/path/to/SumatraPDF.exe -forward-search "<path to .tex file>" <line> "<path to .pdf file>"
```

You can try this command manually to see how it works. SumatraPDF will
open the PDF (if necessary) and highlight the location corresponding to
the specified source line.

In practice, you will usually configure your editor to run this command
automatically, supplying the file paths and line number for you.

The exact setup depends on your editor. For example, if you use VS Code
with the WSL Remote extension, you will first need a LaTeX language
server. One option is TexLab:

* https://github.com/latex-lsp/texlab-vscode

Once installed, add the following settings to `settings.json`:

```json
{
  "texlab.forwardSearch.executable": "/mnt/c/Users/path/to/SumatraPDF.exe",
  "texlab.forwardSearch.args": [
    "-forward-search",
    "\\\\wsl.localhost\\Ubuntu\\%f",
    "%l",
    "\\\\wsl.localhost\\Ubuntu\\%p"
  ]
}
```

Note that TexLab launches SumatraPDF directly from Windows rather than
through a WSL shell. As a result, file paths must be written using the
`\\wsl.localhost\<DISTRO>\...` form .

After configuring TexLab, open the Command Palette (`Ctrl+Shift+P`) and
run **LaTeX: Forward Search** while editing a `.tex` file. TexLab will
substitute the correct file paths and line number and invoke SumatraPDF
automatically.

## Inverse Search

Inverse search is configured in SumatraPDF. You specify a command that
SumatraPDF will execute whenever you double-click a location in the PDF.

To configure it:

1. Open the Command Palette (`Ctrl+K`).
2. Select **Set Inverse Search Command Line**.
3. Enter the command that should open your editor at a specific file and
   line number.

The command should have the general form:

```text
"executable" [options] [arguments]
```

Whenever you double-click a location in the PDF, SumatraPDF substitutes
the appropriate file and line information and executes the command. In
practice, this is most often used to open (or focus) your editor at the
corresponding source location, but the command can in principle launch
any program that can make use of a file path and line number.

For VS Code with WSL Remote, the following command opens a file at a
specific line:

```
"C:\Users\path\to\Code.exe" --remote wsl+Ubuntu --goto "<path to .tex file>:<line>"
```

It is a good idea to test this command manually first. It should open
(or focus) VS Code, connect to the specified WSL distribution, and
navigate to the requested file and line.

Once it works, configure SumatraPDF to use it by replacing the file path
and line number with `%f` and `%l`:

```
InverseSearchCmdLine = "C:\Users\path\to\Code.exe" --remote wsl+Ubuntu --goto "%f:%l"
```

Whenever you double-click a location in the PDF, SumatraPDF will replace
`%f` and `%l` with the corresponding source file and line number and
execute the command.

In inverse search, SumatraPDF returns file paths in a WSL-compatible
Unix format (for example `/home/project/doc.tex` or `/mnt/project/doc.tex`).
This is an important change in version 3.7:
earlier versions did not correctly handle WSL-style paths found in
SyncTeX data, which often led to broken or unusable links in WSL-based
workflows. With the new behavior, WSL-aware editors can correctly resolve
and open the corresponding source files, as shown above with VS Code’s
remote integration.
