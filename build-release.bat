@echo off

:TRYVC9
@call scripts\vc9.bat
IF ERRORLEVEL 1 GOTO TRYVC8
GOTO HAS_VC

:TRYVC8
@call scripts\vc8.bat
IF ERRORLEVEL 1 GOTO VS_NEEDED
GOTO HAS_VC

:HAS_VC
rem add nasm.exe to the path and verify it exists
set PATH=%PATH%;%CD%\bin
nasm -v >nul
IF ERRORLEVEL 1 goto NASM_NEEDED

:BUILD
python scripts\build-release.py %1
goto END

:VS_NEEDED
echo Visual Studio 2008 (vs9) or 2005 (vs8) doesn't seem to be installed
goto END

:NASM_NEEDED
echo nasm.exe doesn't seem to be in the PATH. It's in bin directory
goto END

:END