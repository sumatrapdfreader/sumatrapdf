@ECHO OFF
SETLOCAL

REM You can add Python to PATH if it's not already there
REM SET PATH=C:\Python;%PATH%

CALL vc.bat
IF ERRORLEVEL 1 EXIT /B 1

nmake -f makefile.msvc
