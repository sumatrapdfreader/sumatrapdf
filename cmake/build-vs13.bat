@echo off

REM add nasm.exe to the path
SET PATH=%CD%\bin;%PATH%

CALL "%VS120COMNTOOLS%\vsvars32.bat" 2>NUL
IF NOT ERRORLEVEL 1 GOTO VSFOUND
ECHO Visual Studio 2013 doesn't seem to be installed
EXIT /B 1

:VSFOUND

if not exist "cmake\vs13" mkdir "cmake\vs13"
cd cmake\vs13

rem this is a work-around cl.exe failing if both TMP and tmp env variables are
rem defined, which might happen due to cygwin. This in turns braks cmake
set TMP=
set TEMP=

echo running cmake
cmake -G "Visual Studio 13" ..\..
IF NOT ERRORLEVEL 1 GOTO CMAKEOK
ECHO cmake failed
EXIT /B 1

:CMAKEOK

echo runing msbuild
msbuild all.sln /t:Rebuild /p:Configuration=Release
rem msbuild all.sln /t:Build /p:Configuration=Release
