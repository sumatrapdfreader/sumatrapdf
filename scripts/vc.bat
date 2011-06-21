@ECHO OFF

REM Allow to explicitly specify the desired Visual Studio version
IF /I "%1" == "vc10" GOTO TRY_VS10
IF /I "%1" == "vc9" GOTO TRY_VS9
IF /I "%1" == "vc8" GOTO TRY_VS8

REM Try Visual Studio 2008 first, as it still supports Windows 2000,
REM otherwise fall back to VS 2005 or use any later version

:TRY_VS9
REM CALL "%VS90COMNTOOLS%vsvars32.bat"
CALL "%ProgramFiles%\Microsoft Visual Studio 9.0\Common7\Tools\vsvars32.bat" 2>NUL
IF NOT ERRORLEVEL 1 EXIT /B

:TRY_VS9_X86
CALL "%ProgramFiles(x86)%\Microsoft Visual Studio 9.0\Common7\Tools\vsvars32.bat" 2>NUL
IF NOT ERRORLEVEL 1 EXIT /B

:TRY_VS8
REM CALL "%VS80COMNTOOLS%vsvars32.bat"
CALL "%ProgramFiles%\Microsoft Visual Studio 8\Common7\Tools\vsvars32.bat" 2>NUL
IF NOT ERRORLEVEL 1 EXIT /B

REM Note: Visual Studio 2010 doesn't support Windows 2000

:TRY_VS10
CALL "%ProgramFiles%\Microsoft Visual Studio 10.0\Common7\Tools\vsvars32.bat" 2>NUL
IF NOT ERRORLEVEL 1 EXIT /B

:TRY_VS10_X86
CALL "%ProgramFiles(x86)%\Microsoft Visual Studio 10.0\Common7\Tools\vsvars32.bat" 2>NUL
IF NOT ERRORLEVEL 1 EXIT /B

REM Fail, if no Visual Studio installation has been found
ECHO Visual Studio 2010, 2008 or 2005 doesn't seem to be installed
EXIT /B 1
