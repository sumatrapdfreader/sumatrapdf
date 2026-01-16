# Darkmodelib – Win32 Library for Dark Mode Support

[![Build status](https://img.shields.io/github/actions/workflow/status/ozone10/darkmodelib/build_win.yml?logo=Github)](https://github.com/ozone10/darkmodelib/actions)
[![Latest release](https://img.shields.io/github/v/release/ozone10/darkmodelib?include_prereleases)](https://github.com/ozone10/darkmodelib/releases/latest)
[![License](https://img.shields.io/github/license/ozone10/darkmodelib)](https://www.mozilla.org/en-US/MPL/2.0/)
[![License-MIT](https://img.shields.io/badge/license-MIT-green)](./LICENSE-MIT.md)
[![PayPal.me](https://img.shields.io/badge/PayPal-me-00457C?&logo=paypal&logoColor=white&maxAge=2592000)](https://paypal.me/ozone10/)
[![ko-fi.com](https://img.shields.io/badge/Ko--fi-Buy_Me_a_Tea-F16061?logo=ko-fi&logoColor=white&maxAge=2592000)](https://ko-fi.com/ozone10/)
---

Darkmodelib is a C++ library that brings modern visual features, such as dark mode, custom color schemes, and Windows 11's Mica material, to applications built with the Win32 API. It provides support for the most common controls and is designed to simplify integration into legacy applications, making it easier to modernize their look and feel without a complete rewrite.

* * *

<p align="center">
  <img src="https://i.imgur.com/CCJ5txa.png">
</p>

* * *

<details>
  <summary>Gallery</summary>

  <p align="center">
    <img src="https://i.imgur.com/XH9Egmz.png">
  </p>

  <p align="center">
    <img src="https://i.imgur.com/0NMYNBh.png">
  </p>

</details>

## Features

- Native dark mode support for classic Win32 applications
- Custom colors support
- Simplify Mica material and other Windows 11's visual features integration on Windows 11
- Automatic theme switching based on system light/dark mode settings
- Support for common Win32 controls (buttons, edit controls, list views, tree views, ...)

## Compatibility

Darkmodelib works best with Windows 10 version 1809 and later, and mainly targets Windows 11, while maintaining fallback compatibility as far as Windows Vista.

## Used by

- Parts of the code have been backported to [Notepad++](https://github.com/notepad-plus-plus/notepad-plus-plus/blob/master/PowerEditor/src/NppDarkMode.cpp)
- [7-Zip](https://github.com/ozone10/7zip-Dark7zip) and its fork [7-Zip-zstd](https://github.com/ozone10/7zip-Dark7zip/tree/7z-zstd)
- [SumatraPDF](https://github.com/sumatrapdfreader/sumatrapdf)
- [WinMerge](https://github.com/WinMerge/winmerge)

## License

Copyright (c) 2025 ozone10  
Darkmodelib is licensed under the Mozilla Public License, version 2.0, with some code under MIT.  
For more information check the header of files.
