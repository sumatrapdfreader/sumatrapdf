@echo off
rem TODO: this doesn't work, looks like things are tripped by paths with spaces in them
rem NMake requires cl.exe to be in path
call scripts\vc.bat

if not exist "cmake\nmake" mkdir "cmake\nmake"
cd cmake\nmake

rem this is a work-around cl.exe failing if both TMP and tmp env variables are
rem defined, which might happen due to cygwin. This in turns braks cmake
set TMP=
set TEMP=

cmake -G "NMake Makefiles" ..\..
