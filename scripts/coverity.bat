@rem CALL scripts\vc.bat

IF EXIST "cov-int" RD /q /s "cov-int"
IF EXIST "obj-rel" RD /q /s "obj-rel"
IF EXIST "obj-dbg" RD /q /s "obj-dbg"
IF EXIST "sumatrapdf-cov.lzma" DEL "sumatrapdf-cov.lzma"

@rem this should work but hangs on my (kjk) machine
@rem cov-build --dir cov-int nmake -f makefile.msvc

@rem this must be run from cygwin on my (kjk) machine because I don't have
@rem svn installed
cov-build --dir cov-int "scripts\build-release.bat" -prerelease -noapptrans
tar caf sumatrapdf-cov.lzma cov-int

@echo "now you must upload sumatrapdf-cov.lzma to https://scan.coverity.com/projects/1227"
