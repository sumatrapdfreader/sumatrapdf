@echo off
if not exist "cmake\vs13-64" mkdir "cmake\vs13-64"
cd cmake\vs13-64

rem note: no xp support for 64 bit builds
cmake -G "Visual Studio 12 Win64" ..\..
