@echo off

:TRYVC9
@call vc9.bat
IF ERRORLEVEL 1 GOTO TRYVC8
GOTO HAS_VC

:TRYVC8
@call vc8.bat
IF ERRORLEVEL 1 GOTO VS_NEEDED
GOTO HAS_VC

:HAS_VC
IF EXIST "%ProgramFiles%\NSIS" GOTO HAS_NSIS
IF EXIST "%ProgramFiles(x86)%\NSIS" GOTO HAS_NSIS_X86
GOTO NSIS_NEEDED

:HAS_NSIS_X86
set PATH=%PATH%;%ProgramFiles(x86)%\NSIS
GOTO NEXT

:HAS_NSIS
set PATH=%PATH%;%ProgramFiles%\NSIS
GOTO NEXT

:NEXT
rem check if makensis exists
makensis /version >nul
IF ERRORLEVEL 1 goto NSIS_NEEDED

rem add nasm.exe to the path and verify it exists
set PATH=%PATH%;bin
nasm -v >nul
IF ERRORLEVEL 1 goto NASM_NEEDED

rem check if zip exists
zip -? >nul
IF ERRORLEVEL 1 goto ZIP_NEEDED

:BUILD
python build-release.py %1
goto END

:VS_NEEDED
echo Visual Studio 2008 (vs9) or 2005 (vs8) doesn't seem to be installed
goto END

:NSIS_NEEDED
echo NSIS doesn't seem to installed in %ProgramFiles% or %ProgramFiles(x86)%\NSIS
echo Get it from http://nsis.sourceforge.net/Download
goto END

:NASM_NEEDED
echo nasm.exe doesn't seem to be in the PATH. It's in bin directory
goto END

:ZIP_NEEDED
echo zip.exe doesn't seem to be available in PATH
goto END

:END