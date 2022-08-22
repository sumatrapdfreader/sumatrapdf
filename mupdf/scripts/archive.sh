#!/bin/bash

REV=$(git describe --tags)
O=mupdf-$REV-source

echo git archive $O.tar
git archive --format=tar --prefix=$O/ HEAD > $O.tar

git submodule | while read R P T
do
	M=$(basename $P)
	echo git archive $O.$M.tar

	# Remove bloat from bundled thirdparty libraries.
	case $M in
	harfbuzz)
		git archive --format=tar --remote=$P --prefix=$O/$P/ HEAD \
			| tar --wildcards --delete '*/test/*' \
			> $O.$M.tar
		;;
	leptonica)
		git archive --format=tar --remote=$P --prefix=$O/$P/ HEAD \
			| tar --wildcards --delete '*/prog/*' \
			> $O.$M.tar
		;;
	gumbo-parser)
		git archive --format=tar --remote=$P --prefix=$O/$P/ HEAD \
			| tar --wildcards --delete '*/benchmarks/*' \
			> $O.$M.tar
		;;
	zlib)
		git archive --format=tar --remote=$P --prefix=$O/$P/ HEAD \
			| tar --wildcards --delete '*/contrib/*' \
			> $O.$M.tar
		;;
	lcms2)
		git archive --format=tar --remote=$P --prefix=$O/$P/ HEAD \
			| tar --wildcards --delete '*/testbed/*' --delete '*/plugins/*' \
			> $O.$M.tar
		;;
	extract)
		git archive --format=tar --remote=$P --prefix=$O/$P/ HEAD \
			| tar --wildcards --delete '*/test/*' \
			> $O.$M.tar
		;;
	curl)
		git archive --format=tar --remote=$P --prefix=$O/$P/ HEAD \
			| tar --wildcards --delete '*/tests/*' \
			> $O.$M.tar
		;;
	*)
	git archive --format=tar --remote=$P --prefix=$O/$P/ HEAD > $O.$M.tar
		;;
	esac

	tar Af $O.tar $O.$M.tar
	rm -f $O.$M.tar
done

echo gzip $O.tar
pigz -f -k -11 $O.tar
echo lzip $O.tar
plzip -9 -f -k $O.tar
echo zstd $O.tar
zstd -q -T0 -19 -f -k $O.tar
