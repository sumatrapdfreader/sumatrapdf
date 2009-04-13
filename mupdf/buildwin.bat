@rem this is for Visual Studio 2005 aka vs8
call "%ProgramFiles%\Microsoft Visual Studio 8\Common7\Tools\vsvars32.bat"

@rem this is for Visual Studio 2008 aka vs9
@rem call "%ProgramFiles%\Microsoft Visual Studio 9.0\Common7\Tools\vsvars32.bat"

nmake -f makefile.msvc EXTLIBSDIR=..\sumatrapdf\ CFG=dbg
nmake -f makefile.msvc EXTLIBSDIR=..\sumatrapdf\ CFG=rel
