#!/bin/bash
# Create Makefile for win32 nmake to build fontdump resources.
# Also generate fontdump resources locally.

FONTS="resources/fonts/urw/*.cff resources/fonts/han/*.ttc resources/fonts/droid/*.ttf resources/fonts/noto/*.ttf resources/fonts/noto/*.otf resources/fonts/sil/*.cff"
OUT=scripts/fontdump.nmake.tmp

VC=platform/win32/libresources.vcxproj
VCF=platform/win32/libresources.vcxproj.filters
cp scripts/libresources.vcxproj.template $VC
cp scripts/libresources.vcxproj.filters.template $VCF

echo -e >$OUT "# This is an automatically generated file. Do not edit. */"
echo -e >>$OUT "default: generate"
echo -e >>$OUT "bin2coff.exe: scripts/bin2coff.c"
echo -e >>$OUT "\tcl scripts/bin2coff.c"

mkdir -p build
cc -O2 -o build/bin2coff.exe scripts/bin2coff.c

DIRS=$(dirname $FONTS | sort -u)
for DIR in $DIRS
do
	echo -e >>$OUT "generated/$DIR:"
	echo -e >>$OUT "\tmkdir generated/$DIR"
done

for FILE in $FONTS
do
	NAME=$(echo _binary_$(basename $FILE) | tr '/.-' '___')
	OBJ=$(echo generated/$FILE.obj)
	OBJ64=$(echo generated/$FILE.x64.obj)
	DIR=$(dirname $OBJ)

	echo $OBJ
	mkdir -p $DIR
	./build/bin2coff.exe $FILE $OBJ $NAME
	./build/bin2coff.exe $FILE $OBJ64 $NAME 64bit

	echo -e >>$OUT "generate: $OBJ $OBJ64"
	echo -e >>$OUT "$OBJ: $FILE $DIR bin2coff.exe"
	echo -e >>$OUT "\tbin2coff.exe $FILE $OBJ $NAME"
	echo -e >>$OUT "$OBJ64: $FILE $DIR bin2coff.exe"
	echo -e >>$OUT "\tbin2coff.exe $FILE $OBJ64 $NAME 64bit"

	WINFILE=$(echo $FILE | sed 's,/,\\\\,g')
	WINDIR=$(dirname $FILE | sed 's/resources.//;s,/,\\\\,g')
	case $FILE in
		*.cff)
			sed -i -e '/DUMP:CFF/i <bin2coff__cff_ Include="..\\..\\'$WINFILE'" />' $VC
			sed -i -e '/DUMP:CFF/i <bin2coff__cff_ Include="..\\..\\'$WINFILE'"><Filter>'$WINDIR'</Filter></bin2coff__cff_>' $VCF
			;;
		*.otf)
			sed -i -e '/DUMP:OTF/i <bin2coff__otf_ Include="..\\..\\'$WINFILE'" />' $VC
			sed -i -e '/DUMP:OTF/i <bin2coff__otf_ Include="..\\..\\'$WINFILE'"><Filter>'$WINDIR'</Filter></bin2coff__otf_>' $VCF
			;;
		*.ttc)
			sed -i -e '/DUMP:TTC/i <bin2coff__ttc_ Include="..\\..\\'$WINFILE'" />' $VC
			sed -i -e '/DUMP:TTC/i <bin2coff__ttc_ Include="..\\..\\'$WINFILE'"><Filter>'$WINDIR'</Filter></bin2coff__ttc_>' $VCF
			;;
		*.ttf)
			sed -i -e '/DUMP:TTF/i <bin2coff__ttf_ Include="..\\..\\'$WINFILE'" />' $VC
			sed -i -e '/DUMP:TTF/i <bin2coff__ttf_ Include="..\\..\\'$WINFILE'"><Filter>'$WINDIR'</Filter></bin2coff__ttf_>' $VCF
			;;
	esac
done

tr / \\\\ < $OUT > scripts/fontdump.nmake
rm -f $OUT
