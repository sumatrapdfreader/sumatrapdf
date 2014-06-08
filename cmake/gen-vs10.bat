@echo off
if not exist "cmake\vs10" mkdir "cmake\vs10"
cd cmake\vs10

rem this is a work-around cl.exe failing if both TMP and tmp env variables are
rem defined, which might happen due to cygwin. This in turns braks cmake
set TMP=
set TEMP=

cmake -G "Visual Studio 10" ..\..
