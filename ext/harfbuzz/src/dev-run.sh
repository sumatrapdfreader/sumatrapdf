#!/bin/bash
# Suggested setup to use the script:
#  (on the root of the project)
#  $ NOCONFIGURE=1 ./autogen.sh && mkdir build && cd build
#  $ ../configure --with-freetype --with-glib --with-gobject --with-cairo
#  $ make -j5 && cd ..
#  $ src/dev-run.sh [FONT-FILE] [TEXT]
#
# Or, using cmake:
#  $ cmake -DHB_CHECK=ON -Bbuild -H. -GNinja && ninja -Cbuild
#  $ src/dev-run.sh [FONT-FILE] [TEXT]
#
# If you want to open the result rendering using a GUI app,
#  $ src/dev-run.sh open [FONT-FILE] [TEXT]
#
# And if you are using iTerm2, you can use the script like this,
#  $ src/dev-run.sh img [FONT-FILE] [TEXT]
#

[ $# = 0 ] && echo Usage: "src/dev-run.sh [FONT-FILE] [TEXT]" && exit
command -v entr >/dev/null 2>&1 || { echo >&2 "This script needs `entr` be installed"; exit 1; }


GDB=gdb
# if gdb doesn't exist, hopefully lldb exist
command -v $GDB >/dev/null 2>&1 || export GDB="lldb"


[ $1 = "open" ] && openimg=1 && shift
OPEN=xdg-open
[ "$(uname)" == "Darwin" ] && OPEN=open


[ $1 = "img" ] && img=1 && shift
# http://iterm2.com/documentation-images.html
osc="\033]"
if [[ $TERM == screen* ]]; then osc="\033Ptmux;\033\033]"; fi
st="\a"
if [[ $TERM == screen* ]]; then st="\a"; fi


tmp=tmp.png
[ -f 'build/build.ninja' ] && CMAKENINJA=TRUE
# or "fswatch -0 . -e build/ -e .git"
find src/ | entr printf '\0' | while read -d ""; do
	clear
	yes = | head -n`tput cols` | tr -d '\n'
	if [[ $CMAKENINJA ]]; then
		ninja -Cbuild hb-shape hb-view && {
			build/hb-shape $@
			if [ $openimg ]; then
				build/hb-view $@ -O png -o $tmp
				$OPEN $tmp
			elif [ $img ]; then
				build/hb-view $@ -O png -o $tmp
				printf "\n${osc}1337;File=;inline=1:`cat $tmp | base64`${st}\n"
			else
				build/hb-view $@
			fi
		}
	else
		make -Cbuild/src -j5 -s lib && {
			build/util/hb-shape $@
			if [ $openimg ]; then
				build/util/hb-view $@ -O png -o $tmp
				$OPEN $tmp
			elif [ $img ]; then
				build/util/hb-view $@ -O png -o $tmp
				printf "\n${osc}1337;File=;inline=1:`cat $tmp | base64`${st}\n"
			else
				build/util/hb-view $@
			fi
		}
	fi
done

read -n 1 -p "[C]heck, [D]ebug, [R]estart, [Q]uit? " answer
case "$answer" in
c|C )
	if [[ $CMAKENINJA ]]; then
		CTEST_OUTPUT_ON_FAILURE=1 CTEST_PARALLEL_LEVEL=5 ninja -Cbuild test
	else
		make -Cbuild -j5 check && .ci/fail.sh
	fi
;;
d|D )
	if [[ $CMAKENINJA ]]; then
		echo "Not supported on cmake builds yet"
	else
		build/libtool --mode=execute $GDB -- build/util/hb-shape $@
	fi
;;
r|R )
	src/dev-run.sh $@
;;
* )
	exit
;;
esac
