"%ProgramFiles%\Microsoft Visual Studio 9.0\Common7\Tools\vsvars32.bat"
IF ERRORLEVEL 1 GOTO TRYX86
GOTO END

:TRYX86
"%ProgramFiles(x86)%\Microsoft Visual Studio 9.0\Common7\Tools\vsvars32.bat"

:END