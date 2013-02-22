@rem TODO: figure out why it sometimes calls cygwin's python
@rem @echo %PATH%
python gyp/gengyp.py
python gyp/fix-vsproj-dirs.py vs/gyp/sizer-vs2010.vcxproj.filters
python gyp/fix-vsproj-dirs.py vs/gyp/sizer-vs2012.vcxproj.filters
