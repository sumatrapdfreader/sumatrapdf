@echo off
for /f "tokens=1-4 eol=/ DELIMS=/ " %%i in ('date /t') do set BUILD=%%l%%j%%k
echo %BUILD%
