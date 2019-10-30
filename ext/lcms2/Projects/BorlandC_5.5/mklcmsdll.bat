@echo off
echo.
echo This will build the littlecms DLL using Borland C 5.5 compiler.
echo.
echo Press Ctrl-C to abort, or
pause
bcc32 @lcmsdll.lst
if errorlevel 0 ilink32 @lcmsdll.lk
if errorlevel 0 brc32 -fe ..\..\bin\lcms2.dll lcms2.rc
del *.obj
del *.res
echo Done!
								
					
