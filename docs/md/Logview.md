# Logview

Logview is a tool that helps with debugging SumatraPDF.

When Logview is running, you can see SumatraPDF logs in its window. Logs contain information that can help diagnose issues.

## Download

Download [Logview 0.2](https://files2.sumatrapdfreader.org/software/logview/rel/logview-0.2.exe).

## More info

Logview is a generic logging tool that opens a named pipe `\\.\pipe\LOCAL\ArsLexis-Logger` that any application can open and write to.

SumatraPDF uses it for logging (`log()`, `logf()` functions in `Log.h` and `Log.cpp`).
