# __SyncTeX__

This file is part of the __SyncTeX__ package.

[//]: # (Version: 1.22)
[//]: # (Date: Mon May 11 16:05:02 UTC 2020)

## Important

* Changes in version `1.22`

  * Typos fixed in code and printed comments
  * Code cleaning (`static` keyword added)
  * Bug fixed (Missing `va_end`)

* Changes in version `1.21`

	* Remove a `\` character from the name ot 2 test files.
	* Change variable name `class` to `class_` because the former is a reserved keyword in C++.
	* Adding a more portable unused variable management.
	* Various cosmetic changes in the source code, including explicit declaration of `void` in forward function declaration.
	

* Changes in version `1.20`

	One bug fixed in some initialization and different syntax to make some compilers happy.

* Changes in version `1.19`

	A long standing bug affecting only pdftex has been fixed by adding support to pdf forms. As a side effect, the synchronization algorithm gains in accuracy for all the engines, especially from pdf to source. Of course, there is a small cost in both memory footprint and computational time, but none is noticeable.
	
	Some technical changes in the API.
	* more consistent type naming convention
		* `_s` suffix for struct's
		* `_p` suffix for pointers
		* `_t` suffix for standard types
		* `_f` suffix for functions...
		
		thus `synctex_node_t` has become `synctex_node_p`.
		
	* supplemental argument in `synctex_display_query`. Set `page_hint` to the currently displayed pdf page number (0 based). If this info is not available, set `page_hint` to 0, it corresponds to the old behaviour.
	* rename `synctex_next_result` to `synctex_scanner_next_result`
	* additional `synctex_parser_private.h` header where 
	more API are exposed. For an advanced usage only (eg parallel synchronizations).
	* `SYNCTEX_NO_UPDATER` preprocessor macro to deactivate some code useless to frontends.

## What is it ?

The *Synchronization TeXnology* named __SyncTeX__ is a new feature
of recent __TeX__ engines designed by Jérôme Laurens.
It allows to synchronize between input and output, which means to
navigate from the source document to the typeset material and vice versa.
Browse the source code for more information.

## Contents
This package is mainly for developers, it contains the following files:

* `synctex_parser_readme.md` (this file)
* `synctex_parser_version.txt`
* `synctex_parser_utils.c`
* `synctex_parser_utils.h`
* `synctex_parser_local.h`
* `synctex_parser.h`
* `synctex_parser_advanced.h`
* `synctex_parser.c`

The file you are reading contains more informations about the __SyncTeX__ parser history.

## Source

All stable sources are available from [TeXLive repository](http://www.tug.org/svn/texlive/trunk/Build/source/texk/web2c/synctexdir/).

Latest development sources are available from GitHub.

Please notice that due to the complexity of the TeXLive TeX distribution, there might be some delay before synchronization occurs between various repositories.

## Usage

In order to support __SyncTeX__ in a viewer, it is sufficient to include
in the source the files `synctex_parser.c` and `synctex_parser_utils.c`.
The synctex parser usage is described in `synctex_parser.h` header file.

More advanced usage is found in `synctex_parser_advanced.h`

The other files are used by tex engines or by the synctex command line utility:

```
ChangeLog
README.txt
am
man1
man5
synctex-common.h
synctex-convert.sh
synctex-e-mem.ch0
synctex-e-mem.ch1
synctex-e-rec.ch0
synctex-e-rec.ch1
synctex-etex.h
synctex-mem.ch0
synctex-mem.ch1
synctex-mem.ch2
synctex-pdf-rec.ch2
synctex-pdftex.h
synctex-rec.ch0
synctex-rec.ch1
synctex-rec.ch2
synctex-tex.h
synctex-xe-mem.ch2
synctex-xe-rec.ch2
synctex-xe-rec.ch3
synctex-xetex.h
synctex.c
synctex.defines
synctex.h
synctex_main.c
tests
...
```

## Version:

This is version 1, which refers to the synctex output file format.

The files are identified by a build number.
In order to help developers to automatically manage the version and build numbers
and download the parser only when necessary, the `synctex_parser_version.txt`
is an ASCII text file just containing the current version and build numbers eg `1.19`.

## History:

* 1.1: Thu Jul 17 09:28:13 UTC 2008
	- First official version available in TeXLive 2008 DVD.
  Unfortunately, the backwards synchronization is not working properly mainly for ConTeXt users, see below.
* 1.2: Tue Sep  2 10:28:32 UTC 2008
	- Correction for ConTeXt support in the edit query.
  The previous method was assuming that TeX boxes do not overlap,
  which is reasonable for LaTeX but not for ConTeXt.
  This assumption is no longer considered.
* 1.3: Fri Sep  5 09:39:57 UTC 2008
	- Local variable "read" renamed to "already_read" to avoid conflicts.
	- "inline" compiler directive renamed to "SYNCTEX_INLINE" for code support and maintenance
	- _synctex_error cannot be inlined due to variable arguments (thanks Christiaan Hofman)
	- Correction in the display query, extra boundary nodes are used for a more precise forwards synchronization
* 1.4: Fri Sep 12 08:12:34 UTC 2008
	- For an unknown reason, the previous version was not the real 1.3 (as used in iTeXMac2 build 747).
  As a consequence, a crash was observed.
	- Some typos are fixed.
* 1.6: Mon Nov  3 20:20:02 UTC 2008
	- The bug that prevented synchronization with compressed files on windows has been fixed.
	- New interface to allow system specific customization.
	- Note that some APIs have changed.
* 1.8: Mer  8 jul 2009 11:32:38 UTC
Note that version 1.7 was delivered privately.
	- bug fix: synctex was causing a memory leak in pdftex and xetex, thus some processing speed degradation
	- bug fix: the synctex command line tool was broken when updating a .synctex file
	- enhancement: better accuracy of the synchronization process
	- enhancement: the pdf output file and the associated .synctex file no longer need to live in the same directory.
               The new -d option of the synctex command line tool manages this situation.
               This is handy when using something like tex -output-directory=DIR ...
* 1.9: Wed Nov  4 11:52:35 UTC 2009
	- Various typo fixed
	- OutputDebugString replaced by OutputDebugStringA to deliberately disable unicode preprocessing
	- New conditional created because OutputDebugStringA is only available since Windows 2K professional
* 1.10: Sun Jan  10 10:12:32 UTC 2010 
	- Bug fix in synctex_parser.c to solve a synchronization problem with amsmath's gather environment.
  Concerns the synctex tool.
* 1.11: Sun Jan  17 09:12:31 UTC 2010
	- Bug fix in synctex_parser.c, function synctex_node_box_visible_v: 'x' replaced by 'y'.
  Only 3rd party tools are concerned.
* 1.12: Mon Jul 19 21:52:10 UTC 2010
	- Bug fix in synctex_parser.c, function __synctex_open: the io_mode was modified even in case of a non zero return,
causing a void .synctex.gz file to be created even if it was not expected. Reported by Marek Kasik concerning a bug on evince.
* 1.13: Fri Mar 11 07:39:12 UTC 2011
	- Bug fix in synctex_parser.c, better synchronization as suggested by Jan Sundermeyer (near line 3388).
	- Stronger code design in synctex_parser_utils.c, function _synctex_get_name (really neutral behavior).
  Only 3rd party tools are concerned.
* 1.14: Fri Apr 15 19:10:57 UTC 2011
	- taking output_directory into account
	- Replaced FOPEN_WBIN_MODE by FOPEN_W_MODE when opening the text version of the .synctex file.
	- Merging with LuaTeX's version of synctex.c
* 1.15: Fri Jun 10 14:10:17 UTC 2011
This concerns the synctex command line tool and 3rd party developers.
TeX and friends are not concerned by these changes.
	- Bug fixed in _synctex_get_io_mode_name, sometimes the wrong mode was returned
	- Support for LuaTeX convention of './' file prefixing
* 1.16: Tue Jun 14 08:23:30 UTC 2011
This concerns the synctex command line tool and 3rd party developers.
TeX and friends are not concerned by these changes.
	- Better forward search (thanks Jose Alliste)
	- Support for LuaTeX convention of './' file prefixing now for everyone, not only for Windows
* 1.17: Fri Oct 14 08:15:16 UTC 2011
This concerns the synctex command line tool and 3rd party developers.
TeX and friends are not concerned by these changes.
	- synctex_parser.c: cosmetic changes to enhance code readability 
	- Better forward synchronization.
  The problem occurs for example with LaTeX \item command.
  The fact is that this command creates nodes at parse time but these nodes are used only
  after the text material of the \item is displayed on the page. The consequence is that sometimes,
  forward synchronization spots an irrelevant point from the point of view of the editing process.
  This was due to some very basic filtering policy, where a somehow arbitrary choice was made when
  many different possibilities where offered for synchronisation.
  Now, forward synchronization prefers nodes inside an hbox with as many acceptable spots as possible.
  This is achieved with the notion of mean line and node weight.
	- Adding support for the new file naming convention with './'
	    - function `synctex_ignore_leading_dot_slash_in_path replaces` `synctex_ignore_leading_dot_slash`
	    - function `_synctex_is_equivalent_file_name` is more permissive
  Previously, the function synctex_scanner_get_tag would give an answer only when
  the given file name was EXACTLY one of the file names listed in the synctex file.
  The we added some changes accepting for example 'foo.tex' instead of './foo.tex'.
  Now we have an even looser policy for dealing with file names.
  If the given file name does not match exactly one the file names of the synctex file,
  then we try to match the base names. If there is only one match of the base names,
  then it is taken as a match for the whole names.
  The base name is defined as following:
  
      `./foo => foo
      /my///.////foo => foo
      /foo => /foo
      /my//.foo => /my//.foo`
      
* 1.17: Tue Mar 13 10:10:03 UTC 2012
	- minor changes, no version changes
	- syntax man pages are fixed as suggested by M. Shimata
	see mail to tex-live@tug.org titled "syntax.5 has many warnings from groff" and "syntax.1 use invalid macro for mdoc"
	- 1.17: Tue Jan 14 09:55:00 UTC 2014, fixed a segfault, patch from Sebastian Ramacher
	- 1.17: Mon Aug 04, fixed a memory leak
* 1.18: Thu Jun 25 11:36:05 UTC 2015
	- nested sheets now fully supported (does it make sense in TeX)
	- cosmetic changes: uniform indentation
	- suppression of warnings, mainly long/int ones. In short, zlib likes ints when size_t likes longs.
	- CLI synctex tool can build out of TeXLive (modulo appropriate options passed to the compiler)
* 1.19: Mon Jul  3 20:03:56 UTC 2017
	- the nested sheets patch was not a good solution.
  It has been moved from the parser to the engine.
  See the `synctex.c` source file for detailed explanations.
	- there is a new synctex format specification.
  We can see that a `.synctex` file can contain many times
  the same vertical position because many objects belong
  to the same line. When the options read `-synctex=±2` or more,
  a very basic compression algorithm is used:
  if synctex is about to write the same number then it writes
  an `=` sign instead. This saves approximately 10% of the
  synctex output file, either compressed or not.
  The new synctex parser has been updated accordingly.
  Actual tex frontend won't see any difference with the
  TeX engines that include this new feature.
  Frontends with the new parser won't see any difference
  with the older TeX engines.
  Frontends with the new parser will only see a difference
  with new TeX engines if `-synctex=±2` or more is used.

## Acknowledgments:

The author received useful remarks from the __pdfTeX__ developers, especially Hahn The Thanh,
and significant help from __XeTeX__ developer Jonathan Kew.

## Nota Bene:

If you include or use a significant part of the __SyncTeX__ package into a software,
I would appreciate to be listed as contributor and see __SyncTeX__" highlighted.

Copyright (c) 2008-2017 jerome DOT laurens AT u-bourgogne DOT fr

