@echo off
if not exist "cmake\vs8" mkdir "cmake\vs8"
cd cmake\vs8

cmake -G "Visual Studio 8 2005" ..\..
