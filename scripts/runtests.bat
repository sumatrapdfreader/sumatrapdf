@ECHO OFF
SETLOCAL

REM assume Python is installed in Python27 if it isn't in the path already
FOR %%p IN (python.exe) DO IF "%%~$PATH:p" == "" SET PATH=C:\Python27;%PATH%

REM assumes we're being run from top-level directory as:
REM scripts\runtests.bat
IF NOT EXIST scripts\vc.bat CD %~dp0.. && SET NO_CONSOLE=1

CALL scripts\vc.bat
IF ERRORLEVEL 1 EXIT /B 1

rem work-around cygwin/msdev issue
set tmp=
set temp=

python -u -B scripts\runtests.py %1 %2 %3 %4 %5

REM allow running runtests.bat from Explorer
IF "%NO_CONSOLE%" == "1" PAUSE
