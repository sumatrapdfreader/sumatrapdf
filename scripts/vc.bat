@ECHO OFF

REM append ..\bin to PATH to make nasm.exe available
FOR %%p IN (nasm.exe) DO IF "%%~$PATH:p" == "" SET PATH=%PATH%;%~dp0..\bin

CALL "%VS120COMNTOOLS%\vsvars32.bat" 2>NUL
IF ERRORLEVEL 1 GOTO NO_VS

IF NOT "%ProgramFiles(x86)%"=="" GOTO XP_WIN64
IF NOT "%PROGRAMFILES%"=="" GOTO XP_WIN32
GOTO NO_VS

:XP_WIN64
ECHO Setting up VS 2013 with XP toolkit in WIN 64
SET "INCLUDE=%ProgramFiles(x86)%\Microsoft SDKs\Windows\7.1A\Include;%INCLUDE%"
SET "PATH=%ProgramFiles(x86)%\Microsoft SDKs\Windows\7.1A\Bin;%PATH%"
SET "LIB=%ProgramFiles(x86)%\Microsoft SDKs\Windows\7.1A\Lib;%LIB%"
@REM REM TODO: for 64bit it should be:
@REM REM set LIB=%ProgramFiles(x86)%\Microsoft SDKs\Windows\7.1A\Lib\x64;%LIB%
GOTO OK

:XP_WIN32
ECHO setting up VS 2013 with XP toolkit in WIN 32
SET "INCLUDE=%PROGRAMFILES%\Microsoft SDKs\Windows\7.1A\Include;%INCLUDE%"
SET "PATH=%PROGRAMFILES%\Microsoft SDKs\Windows\7.1A\Bin;%PATH%"
SET "LIB=%PROGRAMFILES%\Microsoft SDKs\Windows\7.1A\Lib;%LIB%"
@REM REM TODO: for 64bit it should be:
@REM REM set LIB=%PROGRAMFILES%\Microsoft SDKs\Windows\7.1A\Lib\x64;%LIB%
GOTO OK

:NO_VS
ECHO Visual Studio 2013 doesn't seem to be installed
EXIT /B 1

:OK
