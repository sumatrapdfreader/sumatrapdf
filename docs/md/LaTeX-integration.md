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

**Detailed instructions**: [https://tex.stackexchange.com/questions/116981/how-to-configure-texniccenter-2-0-with-sumatra-2013-2016-version](https://tex.stackexchange.com/questions/116981/how-to-configure-texniccenter-2-0-with-sumatra-2013-2016-version)

**Short instructions:**

Configure viewer in [output profiles](https://texniccenter.sourceforge.net/configuration.html#viewer-tab).

- Path of executable: `C:\Program Files\SumatraPDF\SumatraPDF.exe` on (or wherever you've installed SumatraPDF)
- View project's output
    - DDE command: `[Open("%bm.pdf",0,1,1)]`
    - Server: `SUMATRA`
    - Topic: `control`
- Forward Search
    - DDE command: `[ForwardSearch("%bm.pdf","%Wc",%l,0,0,1)]`
    - Server: `SUMATRA`
    - Topic: `control`

Some people reported better results when using `%sbm` instead of `%bm` in the above command.

- Close document before running LaTeX: Do not close
- To enable the inverse search, you also have to append to the LaTeX compiler arguments: `-synctex=1`

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
