@ECHO OFF

:TRY_VS12
CALL "%VS110COMNTOOLS%\vsvars32.bat" 2>NUL
IF NOT ERRORLEVEL 1 GOTO VSFOUND

:TRY_VS10
CALL "%VS100COMNTOOLS%\vsvars32.bat" 2>NUL
IF NOT ERRORLEVEL 1 GOTO VSFOUND

REM Fail, if no Visual Studio installation has been found
ECHO Visual Studio 2012 or 2010 doesn't seem to be installed
EXIT /B 1

:VSFOUND

REM add our nasm.exe and StripReloc.exe to the path
SET PATH=%CD%\bin;%PATH%

python -u -B scripts/buildbot.py
