/*
Copyright (c) 2008-2024 jerome DOT laurens AT u-bourgogne DOT fr

This file is part of the __SyncTeX__ package.

Latest Revision: Thu Mar 21 12:51:01 UTC 2024

See `synctex_parser_readme.md` for more details

## License

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE

Except as contained in this notice, the name of the copyright holder
shall not be used in advertising or otherwise to promote the sale,
use or other dealings in this Software without prior written
authorization from the copyright holder.

## Acknowledgments:

The author received useful remarks from the __pdfTeX__ developers, especially Hahn The Thanh,
and significant help from __XeTeX__ developer Jonathan Kew.

## Nota Bene:

If you include or use a significant part of the __SyncTeX__ package into a software,
I would appreciate to be listed as contributor and see "__SyncTeX__" highlighted.
*/

#ifndef _SYNCTEX_VERSION_H_
#   define _SYNCTEX_VERSION_H_

/* The version of .synctex files contents. */
#   define SYNCTEX_VERSION 1
#   define SYNCTEX_FILE_VERSION SYNCTEX_VERSION


/* The version of the synctex parser library */
#   define SYNCTEX_VERSION_MAJOR 1
#   define SYNCTEX_VERSION_MINOR 31

/* Keep next value in synch with `synctex_parser_version.txt` contents. */
#   define SYNCTEX_VERSION_STRING "1.31"

/* The version of the synctex CLI tool */
#   define SYNCTEX_CLI_VERSION_MAJOR 1
#   define SYNCTEX_CLI_VERSION_MINOR 7
#   define SYNCTEX_CLI_VERSION_STRING "1.7"

/* The version of the synctex support,
 * how synctex is embedded in various TeX engines. */
#   define SYNCTEX_SUPPORT_VERSION_MAJOR 1
#   define SYNCTEX_SUPPORT_VERSION_MINOR 0
#   define SYNCTEX_SUPPORT_VERSION_STRING "1.0"

#endif
