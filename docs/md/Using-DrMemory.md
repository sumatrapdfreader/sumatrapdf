# Using DrMemory

[DrMemory](https://drmemory.org/) is a tool for detecting memory errors and misuse of Windows APIs.

I tried portable version [1.8.1RC1](https://github.com/DynamoRIO/drmemory/wiki/Downloads) on 32-bit Windows 7 (it doesn't support Windows 10 or 64-bit Windows).

I ran it as:

- `mkdir ..\drmemlogs`
- `..\drmemory\bin\drmemory.exe -logdir ..\drmemlogs -suppress=drmem-sup.txt —   .\rel\SumatraPDF.exe ..\f1.pdf`

There are some apparent false positives. I add suppressions to `drmem-sup.txt` as I find them.

DrMemory visibly slows down the program and also crashes sometimes (e.g. it crashed for me with XPS files).
