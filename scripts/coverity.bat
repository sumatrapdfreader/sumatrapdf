CALL scripts\vc.bat
rmdir /S /Q obj-dbg
cov-build --dir cov-int nmake -f makefile.msvc
