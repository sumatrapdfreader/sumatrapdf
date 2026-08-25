# Submit crash report

Please help us fix SumatraPDF crashes.

If you have a reproducible crash:

- download symbols:
  - menu: `Debug` / `Download Symbols`
  - `Ctrl + K` for command palette and `Debug: Download Symbols`
- trigger the crash
- when you see the crash dialog, press `Cancel` to open the crash report in the default text editor
- post the crash log as a gist at [https://gist.github.com/](https://gist.github.com/). For example, press `Ctrl + A` to select all text, `Ctrl + C` to copy it to the clipboard, and then `Ctrl + V` to paste it into the gist
- create a bug report at [https://github.com/sumatrapdfreader/sumatrapdf/issues](https://github.com/sumatrapdfreader/sumatrapdf/issues)
- post a link to the gist in the bug report
- **include the file that caused the crash**
  - attach it to the GitHub issue (put it in a `.zip` file if the file type is not accepted)
  - or, if the file is private, email it to kkowalczyk@gmail.com (and reference the bug number)
  - I can’t stress it enough: if I can’t reproduce a crash myself, I might not be able to fix it
- provide additional information like:
  - what you were doing when the crash happened
  - when the crash happened: when opening a file, changing the view, etc.
  - how you opened the file: drag and drop onto the SumatraPDF window, double-click in the file manager, from the command line, etc.
  - the best information is a set of steps I can do to reproduce the crash
