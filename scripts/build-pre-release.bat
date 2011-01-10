@rem assumes we're being run from top-level directory as:
@rem scripts\build-pre-release.bat
@call scripts\vc9.bat
IF ERRORLEVEL 1 GOTO VC9_NEEDED
GOTO BUILD

:BUILD
c:\Python26\python scripts\build-pre-release.py
GOTO END

:VC9_NEEDED
@echo Visual Studio 2008 doesn't seem to be installed
GOTO END

:END
