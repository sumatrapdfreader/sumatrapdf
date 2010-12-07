@rem this is for Visual Studio 2005 aka vs8
@rem call "%ProgramFiles%\Microsoft Visual Studio 8\Common7\Tools\vsvars32.bat"

@rem this is for Visual Studio 2008 aka vs9
call "%ProgramFiles%\Microsoft Visual Studio 9.0\Common7\Tools\vsvars32.bat"

@rem add nasm.exe to the path and verify it exists
@set PATH=%PATH%;..\src\bin
@nasm -v >nul
@IF ERRORLEVEL 1 goto NASM_NEEDED

nmake -f makefile.msvc EXTLIBSDIR=..\ext CFG=dbg
@rem nmake -f makefile.msvc EXTLIBSDIR=..\ext CFG=rel
@goto END

:NASM_NEEDED
echo nasm.exe doesn't seem to be in the PATH. It's in bin directory
goto END

:END
