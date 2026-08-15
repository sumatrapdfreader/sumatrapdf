#!/bin/bash
# Generate fontdump resources and update visual studio project files.

FONTS="resources/fonts/urw/*.cff resources/fonts/han/*.ttc resources/fonts/droid/*.ttf resources/fonts/noto/*.ttf resources/fonts/noto/*.otf resources/fonts/sil/*.cff"

VC=platform/win32/libresources.vcxproj
VCF=platform/win32/libresources.vcxproj.filters
cp scripts/libresources.vcxproj.template $VC
cp scripts/libresources.vcxproj.filters.template $VCF

mkdir -p build
cc -O2 -o build/bin2coff.exe scripts/bin2coff.c

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
