# [Documentation](/docs/) : Use Sumatra as a pre-viewer for LaTeX editors

You can use [DDE commands](/docs/DDE-Commands-43ae651b446b4f09a725f2449150a43d) from TeX and LaTeX editors to use Sumatra as a pre-viewer.

## TeXnicCenter

 **Detailed instructions** : [https://tex.stackexchange.com/questions/116981/how-to-configure-texniccenter-2-0-with-sumatra-2013-2016-version](https://tex.stackexchange.com/questions/116981/how-to-configure-texniccenter-2-0-with-sumatra-2013-2016-version) 

 **Short instructions:** 

Configure viewer in [output profiles](http://texniccenter.sourceforge.net/configuration.html#viewer-tab) .

- Path of executable: `C:\Program Files\SumatraPDF\SumatraPDF.exe` on (or wherever you've installed SumatraPDF)
- View project's output
	- DDE command: ` [Open("%bm.pdf",0,1,1)]` 
	- Server: `SUMATRA` 
	- Topic: `control` 
- Forward Search
	- DDE command: `[ForwardSearch("%bm.pdf","%Wc",%l,0,0,1)]` 
	- Server: `SUMATRA` 
	- Topic: `control` 

Some people reported better results when using `%sbm` instead of `%bm` in the above command.

- Close document before running LaTeX: Do not close
- To enable the inverse search, you also have to append to the LaTeX compiler arguments: `-synctex=1` 

## WinEdt, Emacs and Vim

See instructions at [http://william.famille-blum.org/blog/static.php?page=static081010-000413](http://william.famille-blum.org/blog/static.php?page=static081010-000413) 

Emacs: [https://www.emacswiki.org/emacs/AUCTeX#toc25](https://www.emacswiki.org/emacs/AUCTeX#toc25)