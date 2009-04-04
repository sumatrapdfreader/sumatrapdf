@call c:\kjk\src\kjkpriv\scripts\vc9.bat
nmake -f makefile.msvc EXTLIBSDIR=..\sumatrapdf\ CFG=dbg
nmake -f makefile.msvc EXTLIBSDIR=..\sumatrapdf\ CFG=rel
