@rem some versions of cygwin add cygwin's bin directory to front of %PATH%
@rem For gyp, I need to run Window's Python.
@rem this is a work-around (as long as python is installed to c:\Python27)
@rem TODO: could be more intelligent and detect Python path
@set PP=c:\Python27
%PP%\python gyp/gengyp.py
%PP%\python gyp/fix-vsproj-dirs.py vs/gyp/sizer-vs2010.vcxproj.filters
%PP%\python gyp/fix-vsproj-dirs.py vs/gyp/sizer-vs2012.vcxproj.filters
