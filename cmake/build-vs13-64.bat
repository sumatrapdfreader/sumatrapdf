@echo off

CALL "%VS120COMNTOOLS%\vsvars32.bat" 2>NUL
IF NOT ERRORLEVEL 1 GOTO VSFOUND
ECHO Visual Studio 2013 doesn't seem to be installed
EXIT /B 1

:VSFOUND

if not exist "cmake\vs13-64" mkdir "cmake\vs13-64"
cd cmake\vs13-64

rem this is a work-around cl.exe failing if both TMP and tmp env variables are
rem defined, which might happen due to cygwin. This in turns braks cmake
set TMP=
set TEMP=

echo running cmake
cmake -G "Visual Studio 12 Win64" ..\..
echo runing msbuild
msbuild all.sln /t:Rebuild /p:Configuration=Release
