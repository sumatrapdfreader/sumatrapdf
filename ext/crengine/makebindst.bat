del crengine-bin.zip

cd Tools\Fb2Test\Release
7z a -tzip ..\..\..\crengine-bin.zip Fb2Test.exe
cd ..\..\..

cd Tools\FontConv\Release
7z a -tzip ..\..\..\crengine-bin.zip LFntConv.exe
cd ..\..\..

cd Tools\Fb2Test
7z a -tzip ..\..\crengine-bin.zip *.lbf
7z a -tzip ..\..\crengine-bin.zip example.fb2
7z a -tzip ..\..\crengine-bin.zip fb2.css
cd ..\..
