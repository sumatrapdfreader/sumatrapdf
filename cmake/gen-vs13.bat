@echo off
if not exist "cmake\vs13" mkdir "cmake\vs13"
cd cmake\vs13

cmake -T "v120_xp" -G "Visual Studio 12" ..\..
