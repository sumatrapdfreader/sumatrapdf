# Logview

Logview is a tool that helps in debugging SumatraPDF.

## Download

Download [Logview 0.1](https://files2.sumatrapdfreader.org/software/logview/rel/logview-0.1.exe).

## More info

Logview is a generic logging tools that opens a named pipe `\\.\pipe\LOCAL\ArsLexis-Logger` that any application can open and write to.

SumatraPDF uses it for logging (`log()`, `logf()`, `logfa()` functions in `Log.h` and `Log.cpp`).

This is helpful to see log activity even when not running under a debugger.
