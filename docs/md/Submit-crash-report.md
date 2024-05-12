# Submit crash report`

Please help us fix SumatraPDF crashes.

If you have a reproducible crash:
* download symbols:
  * menu: `Debug` / `Download Symbols`
  * `Ctrl + K` for command palette and `Debug: Download Symbols`
* trigger the crash
* when you see crash dialog, press `Cancel` to launch default text editor with crash report
* post the content of the crash log as a gist at [https://gist.github.com/](https://gist.github.com/). For example: `Ctrl-A` to select all text, `Ctrl-C` to copy it to a clipboard and then `Ctrl-V` to the gist
* create a bug report at [https://github.com/sumatrapdfreader/sumatrapdf/issues](https://github.com/sumatrapdfreader/sumatrapdf/issues)
* post a link to the gist in the bug report
* **include the file that caused the crash**
    - attach to the GitHub issue (put in a .zip file if file type is not accepted)
    - or, if the file is private, e-mail to kkowalczyk@gmail.com (and reference bug number)
    - I can’t stress it enough: if I can’t reproduce a crash myself, I might not be able to fix it
* provide additional information like:
  * what were you doing when the crash happened
  * when did the crash happen. When opening a file? changing view? etc.
  * how did you open the file? drag & drop on Sumatra window? Double-click in file manager? From command line?
  * the best information is a set of steps I can do to reproduce the crash

