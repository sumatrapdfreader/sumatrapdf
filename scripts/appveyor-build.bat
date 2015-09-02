@ECHO OFF
SETLOCAL

REM assumes we're being run from top-level directory as:
REM scripts\appveyor-build.bat

set t0=%time: =0%

REM temporarily build 64-bit releases
GOTO Build64

CALL scripts\vc.bat
IF ERRORLEVEL 1 EXIT /B 1
nmake -f makefile.msvc CFG=rel PLATFORM=X86 SumatraPDF Installer AllSymbols UnitTest
IF ERRORLEVEL 1 EXIT /B 1
obj-rel\test_util.exe
IF ERRORLEVEL 1 EXIT /B 1

EXIT /B

:Build64
CALL "%VS120COMNTOOLS%\..\..\VC\vcvarsall.bat" x86_amd64
IF ERRORLEVEL 1 EXIT /B 1
nmake -f makefile.msvc CFG=rel PLATFORM=X64 EXTCFLAGS="/D STRICT_X64=1 SumatraPDF Installer AllSymbols UnitTest
IF ERRORLEVEL 1 EXIT /B 1
obj-rel64\test_util.exe
IF ERRORLEVEL 1 EXIT /B 1

@rem TODO: build with more platforms, build and run tests etc.

set t=%time: =0%

set /a h=1%t0:~0,2%-100
set /a m=1%t0:~3,2%-100
set /a s=1%t0:~6,2%-100
set /a c=1%t0:~9,2%-100
set /a starttime = %h% * 360000 + %m% * 6000 + 100 * %s% + %c%

rem make t into a scaler in 100ths of a second
set /a h=1%t:~0,2%-100
set /a m=1%t:~3,2%-100
set /a s=1%t:~6,2%-100
set /a c=1%t:~9,2%-100
set /a endtime = %h% * 360000 + %m% * 6000 + 100 * %s% + %c%

rem runtime in 100ths is now just end - start
set /a runtime = %endtime% - %starttime%
set runtime = %s%.%c%

echo Started at %t0%
echo Ran for %runtime%0 ms
