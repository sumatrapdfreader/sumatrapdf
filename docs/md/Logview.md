# Logview

Logview is a tool that helps in debugging SumatraPDF.

When LogView is running, you can see SumatraPDF logs in LogView window. Logs show information that can be helpful in diagnosing issues.

## Download

Download [Logview 0.2](https://files2.sumatrapdfreader.org/software/logview/rel/logview-0.2.exe).

## More info

Logview is a generic logging tools that opens a named pipe `\\.\pipe\LOCAL\ArsLexis-Logger` that any application can open and write to.

SumatraPDF uses it for logging (`log()`, `logf()`, `logfa()` functions in `Log.h` and `Log.cpp`).
