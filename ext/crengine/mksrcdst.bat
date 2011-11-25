call makedocs.bat
del crengine-src.zip
7z a -tzip crengine-src.zip *.txt
7z a -tzip crengine-src.zip *.bat
7z a -tzip crengine-src.zip LICENSE
7z a -tzip crengine-src.zip .project
7z a -tzip crengine-src.zip .cdtproject
7z a -tzip crengine-src.zip .cdtbuild
7z a -tzip crengine-src.zip Makefile
7z a -tzip crengine-src.zip Makefile.am
7z a -tzip crengine-src.zip Makefile.in
7z a -tzip crengine-src.zip Makefile.lbook
7z a -tzip crengine-src.zip settings\*.*
7z a -tzip crengine-src.zip include\*.*
7z a -tzip crengine-src.zip src\*.*
7z a -tzip crengine-src.zip lib\projects\vc6\*.dsp
7z a -tzip crengine-src.zip lib\projects\vc6\*.dsw
7z a -tzip crengine-src.zip lib\projects\codeblocks\*.*
7z a -tzip crengine-src.zip Tools\FontConv\*.h
7z a -tzip crengine-src.zip Tools\FontConv\*.cpp
7z a -tzip crengine-src.zip Tools\FontConv\*.txt
7z a -tzip crengine-src.zip Tools\FontConv\*.rc
7z a -tzip crengine-src.zip Tools\FontConv\*.dsp
7z a -tzip crengine-src.zip Tools\FontConv\*.dsw
7z a -tzip crengine-src.zip Tools\FontConv\*.clw
7z a -tzip crengine-src.zip Tools\FontConv\res\*.*
7z a -tzip crengine-src.zip Tools\Fb2Test\fb2.css
7z a -tzip crengine-src.zip Tools\Fb2Test\*.h
7z a -tzip crengine-src.zip Tools\Fb2Test\*.cpp
7z a -tzip crengine-src.zip Tools\Fb2Test\*.txt
7z a -tzip crengine-src.zip Tools\Fb2Test\*.rc
7z a -tzip crengine-src.zip Tools\Fb2Test\*.res
7z a -tzip crengine-src.zip Tools\Fb2Test\*.ico
7z a -tzip crengine-src.zip Tools\Fb2Test\*.dsp
7z a -tzip crengine-src.zip Tools\Fb2Test\*.dsw
7z a -tzip crengine-src.zip Tools\Fb2Test\*.clw
7z a -tzip crengine-src.zip Tools\Fb2Linux\fb2.css
7z a -tzip crengine-src.zip Tools\Fb2Linux\*.h
7z a -tzip crengine-src.zip Tools\Fb2Linux\*.cpp
7z a -tzip crengine-src.zip Tools\Fb2Linux\*.fb2
7z a -tzip crengine-src.zip Tools\Fb2Linux\makefile
7z a -tzip crengine-src.zip lib\projects\*.dsp
7z a -tzip crengine-src.zip lib\projects\*.dsw
7z a -tzip crengine-src.zip doxygen\*.cfg

del crengine-doc.zip
cd doxygen
7z a -tzip ..\crengine-doc.zip html\*.*
cd ..
