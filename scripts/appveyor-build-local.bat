@rem this is for testing appveyor-build.bat locally
@rem we need to set up Visual Studio but on appveyor
@rem build machines it's not necessary

CALL scripts\vc.bat
IF ERRORLEVEL 1 EXIT /B 1

CALL scripts\appveyor-build.bat
