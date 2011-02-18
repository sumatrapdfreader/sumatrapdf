@ECHO OFF
SETLOCAL

REM assumes we're being run from mupdf directory as:
REM buildwin.bat [rel]

CALL ..\scripts\vc.bat
IF ERRORLEVEL 1 EXIT /B 1

REM add our nasm.exe to the path
SET PATH=%CD%\..\bin;%PATH%

SET CFG=dbg
IF "%1" == "rel" SET CFG=rel

nmake -f makefile.msvc EXTLIBSDIR=..\ext CFG=%CFG%
