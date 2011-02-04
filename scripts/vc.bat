@ECHO OFF

REM Allow to explicitly specify the desired maximun Visual Studio version
IF /I "%1" == "vc10" GOTO TRY_VS10
IF /I "%1" == "vc9" GOTO TRY_VS9
IF /I "%1" == "vc8" GOTO TRY_VS8

REM Try the latest Visual Studio version first, falling successively back to older versions

:TRY_VS10
"%ProgramFiles%\Microsoft Visual Studio 10.0\Common7\Tools\vsvars32.bat" 2>NUL
IF NOT ERRORLEVEL 1 EXIT /B

:TRY_VS9
REM "%VS90COMNTOOLS%vsvars32.bat"
"%ProgramFiles%\Microsoft Visual Studio 9.0\Common7\Tools\vsvars32.bat" 2>NUL
IF NOT ERRORLEVEL 1 EXIT /B

:TRY_VS9_X86
"%ProgramFiles(x86)%\Microsoft Visual Studio 9.0\Common7\Tools\vsvars32.bat" 2>NUL
IF NOT ERRORLEVEL 1 EXIT /B

:TRY_VS8
REM "%VS80COMNTOOLS%vsvars32.bat"
"%ProgramFiles%\Microsoft Visual Studio 8\Common7\Tools\vsvars32.bat" 2>NUL
IF NOT ERRORLEVEL 1 EXIT /B

REM Fail, if no Visual Studio installation has been found
ECHO Visual Studio 2010, 2008 or 2005 doesn't seem to be installed
EXIT /B 1
