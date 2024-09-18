# LaTeX integration

You can use [DDE commands](DDE-Commands.md) from TeX and LaTeX editors to use Sumatra as a previewer.

## notepad++

Launch SumatraPDF from notepad++ enabling forward and backward search:

```
"C:\Program files\SumatraPDF\SumatraPDF.exe" -forward-search "$(FULL_CURRENT_PATH)" $(CURRENT_LINE) -inverse-search "\"C:/Program Files/Notepad++/notepad++.exe\" \"%f\" -n%l" "$(CURRENT_DIRECTORY)"/"$(NAME_PART)".pdf
```

## TexStudio

Launch SumatraPDF from TeXStudio enabling forward and backward search:

```
"C:/Program Files/SumatraPDF/SumatraPDF.exe" -forward-search "?c:am.tex" @ -inverse-search "\"C:/Program Files (x86)/TeXstudio/texstudio.exe\" \"%%f\" -line %%l" "?am.pdf"
```

## TeXnicCenter

**Detailed instructions**: [https://tex.stackexchange.com/questions/453670/adobe-reader-makes-texniccenter-crash-alternative-sumatra/453731#453731](https://tex.stackexchange.com/questions/453670/adobe-reader-makes-texniccenter-crash-alternative-sumatra/453731#453731)

**Short instructions:**

Configure viewer in [output profiles](https://texniccenter.sourceforge.net/configuration.html#viewer-tab).

- press Alt+F7 (Build > Define Output Profiles)
- for any one of the PDF Profiles e.g. LaTeX > PDF
- for Executable path it should have something like:
  - `C:\Program Files\SumatraPDF\SumatraPDF.exe -inverse-search "\"C:\Program Files (x86)\TeXnicCenter\TeXnicCenter.exe\" /ddecmd \"[goto('%f','%l')]\""`
  - `SumatraPDF.exe` path might be different on your computer
- go back to the editor and using any simple .TeX press `Ctrl + Shift + F5` (Build and view)
- SumatraPDF should have fired up with the compiled PDF
- in SumatraPDF go To `Settings` > `Advanced Options`
- make the following modifications and save the settings file:

```
ReuseInstance = true
ReloadModifiedDocuments = true

InverseSearchCmdLine = "C:\Program Files\TeXnicCenter\TeXnicCenter.exe" /nosplash /ddecmd "[goto('%f', '%l')]"
OR
InverseSearchCmdLine = "C:\Program Files (x86)\TeXnicCenter\TeXnicCenter.exe" /nosplash /ddecmd "[goto('%f', '%l')]"

EnableTeXEnhancements = true
UseTabs = true
```

Now a double click in the PDF should take you back to TeXnicCenter either in an included file or the main file. IF not, check the syntax of the InverseSearchCmdLine = matches YOUR location for TeXnicCenter

Back in the editor press Alt+F7 (Build > Define Output Profiles) and for each of the PDF options select viewer

In the 1Executable path1 section REMOVE any thing after the .exe

In the 1View project's Output1 select `Command line argument` and check it is `"%bm.pdf"`

In Forward search change it to `-forward-search "%Wc" %l "%bm.pdf"`

## Vim

The easiest is to write a Vim function to forward-search and a callback
function that is triggered on backward-search event, i.e. when you double
click somewhere on the pdf.

It is suggested to write such functions in `C:\Users\<your_user_id>\vimfiles\after\ftplugin\tex.vim` file
so that they exists only in `tex` filetypes, but you are free to define them
in your `.vimrc` as well, although the first option is preferred.

### forward-search

The following function works pretty well (note that it is written in Vim9
language):

```
def ForwardSearch()
  var filename_root = expand('%:p:r')
  system($'SumatraPDF.exe -forward-search {filename_root}.tex {line(".")} {filename_root}.pdf')
enddef
```

Feel free to replace `SumatraPDF.exe` with the correct executable filename,
e.g. `SumatraPDFv3-4-5.exe`.

Next, you should map this function to some key, for example you could use the
following.

```
nnoremap <buffer> <F5> <Scriptcmd>ForwardSearch()<cr>
```

Now, `<F5>` will perform a forward-search. Feel free to replace `<F5>` with
the key that you prefer.

### backward-search

Define a global-scope `BackwardSearch` function as it follows:

```
def g:BackwardSearch(line: number, filename: string)
  exe $'buffer {bufnr(fnamemodify(filename, ':.'))}'
  cursor(line, 1)
enddef
```

Next, open `SumatraPDF` and go to _Settings/Options_. Replace the line in the
_Set inverse-search command-line_ box, with the following:

```
vim --servername vim --remote-send ":call BackwardSearch(%l, '%f')<cr>"
```

If you use gvim, then replace `vim` with `gvim` in the above line. The
backwards search should be now enabled.

## WinEdt and Emacs

<!-- See instructions at [https://william.famille-blum.org/blog/static.php?page=static081010-000413](https://william.famille-blum.org/blog/static.php?page=static081010-000413) -->

Emacs:
[https://www.emacswiki.org/emacs/AUCTeX#toc25](https://www.emacswiki.org/emacs/AUCTeX#toc25)
