## SumatraPDF Reader

SumatraPDF is a multi-format (PDF, EPUB, MOBI, FB2, CHM, XPS, DjVu) reader
for Windows under (A)GPLv3 license, with some code under BSD license (see
AUTHORS).

More information:
* [main website](http://www.sumatrapdfreader.org) with downloads and documentation
* [wiki with more docs](https://www.notion.so/SumatraPDF-documentation-fed36a5624d443fe9f7be0e410ecd715)

To compile you need Visual Studio 2017 or 2015. [Free Community edition](https://www.visualstudio.com/vs/community/) works.

To get the code:
* `git clone --recursive git@github.com:sumatrapdfreader/sumatrapdf.git`

Note: we use git submodules, so `--recursive` option is important.

If you've already checked out without `--recursive` option, you can fix it with:
* `git submodule init`
* `git submodule update`

Open:
* `vs2017/SumatraPDF.sln` when using Visual Studio 2017
* `vs2015/SumatraPDF.sln` when using Visual Studio 2015

[![Build status](https://ci.appveyor.com/api/projects/status/tesjtgmpy26uf8p7?svg=true)](https://ci.appveyor.com/project/kjk/sumatrapdf)
