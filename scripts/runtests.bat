@ECHO OFF
SETLOCAL

@set PATH=c:\Python27;%PATH%

REM assumes we're being run from top-level directory as:
REM scripts\runtestsbat

@CALL scripts\vc.bat
IF ERRORLEVEL 1 EXIT /B 1

@rem work-around cygwin/msdev issue
@set tmp=
@set temp=

python -u -B scripts\runtests.py %1 %2 %3 %4 %5
