#!/bin/bash

# Call this script from a "Run Script" target in the Xcode project to
# cross compile MuPDF and third party libraries using the regular Makefile.
# Also see "iOS" section in Makerules.

echo Generating cmap and font files
make -C .. generate || exit 1

export OS=ios
export build=$(echo $CONFIGURATION | tr A-Z a-z)

case $ARCHS in
	armv6) ARCHFLAGS="-arch armv6 -mno-thumb" ;;
	armv7) ARCHFLAGS="-arch armv7 -mthumb" ;;
	i386) ARCHFLAGS="-arch i386" ;;
	*) echo "Unknown architecture!"; exit 1 ;;
esac

export CFLAGS="$ARCHFLAGS -isysroot $SDKROOT"
export LDFLAGS="$ARCHFLAGS -isysroot $SDKROOT"
export OUT=build/$build-$OS-$ARCHS

echo Building libraries for $ARCHS.
make -C .. libs || exit 1

echo Assembling final library in $TARGET_BUILD_DIR/.
mkdir -p "$TARGET_BUILD_DIR"
rm -f $TARGET_BUILD_DIR/libLibraries.a
ar cr $TARGET_BUILD_DIR/libLibraries.a ../$OUT/*.o
ranlib $TARGET_BUILD_DIR/libLibraries.a

echo Done.
