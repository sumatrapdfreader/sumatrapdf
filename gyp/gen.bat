@rem This script assumes that gyp is checked out alongside sumatrapdf directory
@rem and it's being called as: gype\gen.bat
@rem cd gyp
@rem --generator-output=..\..\build\32
@rem call ..\..\gyp\gyp.bat -G msvs_version=2013 -f msvs -Icommon.gypi sumatra.gyp
@rem cd ..

call ..\gyp\gyp.bat -G msvs_version=2013 -f msvs -Igyp\common.gypi gyp\sumatra.gyp
