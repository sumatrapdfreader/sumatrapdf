#!/bin/bash
#
# Convert PDF to hybrid HTML output with images and line art rendered to a
# background image, and text overlaid on top as absolutely positioned HTML
# text.

input=$1
out=${2:-out}
fmt=${3:-png}
dpi=${4:-96}

scale=$(expr 72 '*' $dpi / 96)

if test -f "$1"
then
	echo Processing "$input" out=$out fmt=$fmt dpi=$dpi
else
	echo "usage: pdftohtml.sh input.pdf output-stem image-format dpi"
	echo "    example: pdftohtml.sh input.pdf output png 96"
	exit
fi

title=$(basename "$input" | sed 's/.pdf$//')

mutool convert -Oresolution=$dpi -o $out.html "$input"

sed -i -e "/<head>/a<title>$title</title>" $out.html
sed -i -e "/^<div/s/page\([0-9]*\)\" style=\"/page\1\" style=\"background-image:url('$out\1.$fmt');/" $out.html

mutool draw -K -r$dpi -o$out%d.png "$input"

echo Converting to $fmt
for png in $out*.png
do
	xxx=$(basename $png .png).$fmt
	case $fmt in
		png)
			if command -v optipng >/dev/null
			then
				optipng -silent -strip all $png
			fi
		;;
		jpg)
			if command -v mozjpeg >/dev/null
			then
				mozjpeg -outfile $xxx $png
			else
				convert -format $fmt $png $xxx
			fi
		;;
		webp)
			if command -v cwebp >/dev/null
			then
				cwebp -quiet -o $xxx $png
			else
				convert -format $fmt $png $xxx
			fi
		;;
		*)
			convert -format $fmt $png $xxx
		;;
	esac
done
