@rem This script assumes that gyp is checked out alongside sumatrapdf directory
@rem and it's being called as: gype\gen.bat

@rem cd gyp
@rem --generator-output=..\..\build\32
@rem call ..\..\gyp\gyp.bat -G msvs_version=2013 -f msvs -Icommon.gypi sumatra-all.gyp
@rem cd ..

call ..\gyp\gyp.bat -G msvs_version=2013 -f msvs --depth=. -Igyp\common.gypi gyp\sumatra-all.gyp

@rem TODO: still can't figure out how to make --generator-output work
@rem nasm rules break if the generator-output directory has a different level than gyp directory
@rem call ..\gyp\gyp.bat -G msvs_version=2013 -f msvs --depth=. --generator-output=build32 -Igyp\common.gypi gyp\sumatra.gyp

