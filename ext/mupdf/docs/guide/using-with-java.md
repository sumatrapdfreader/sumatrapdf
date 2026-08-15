# Using with Java


There is also a Java library, which uses JNI to provide access to the C
library. The Java classes provide an interface very similar to that available
to Javascript. This Java library also powers the Android versions of MuPDF.

## Android

If you want to build an application for Android, you have several options. You
can base it off one of the existing viewers, or build a new app using the Java
library directly.

See the "Using with Android" section to get started using the MuPDF library for
Android.

## Building

Check out (or download) the MuPDF repository.

The Java bindings are in the platform/java directory.

You can build them using make:

	make java

The resulting shared library are in build/java/release. You need to make sure
the Java runtime can find the JAR archive (with class-path) and the native
library (with java.library.path).

To test the bindings you can use the Java shell:

	$ jshell --class-path=build/java/release -R-Djava.library.path=build/java/release
	jshell> import com.artifex.mupdf.fitz.*
	jshell> var doc = Document.openDocument("pdfref17.pdf")
	jshell> System.out.println(doc.countPages())

## Examples

There are several more examples in the Java directory.

To build and run the example Swing viewer:

	make -C platform/java run
