@echo off
@echo Cleaning
echo bogus > example\bogus.class
del /Q example\*.class
echo bogus > src\com\artifex\mupdf\fitz\bogus.class
del /Q src\com\artifex\mupdf\fitz\*.class

@echo Building Viewer
javac -classpath src -sourcepath src -source 1.7 -target 1.7 example/Viewer.java

@echo Building JNI classes
javac -sourcepath src -source 1.7 -target 1.7 src/com/artifex/mupdf/fitz/*.java

@echo Importing DLL (built using VS solution)
@copy ..\win32\%1\javaviewerlib.dll mupdf_java.dll /y

@echo Packaging into jar (incomplete as missing manifest)
jar cf mupdf-java-viewer.jar mupdf_java.dll src\com\artifex\mupdf\fitz\*.class example\*.class
