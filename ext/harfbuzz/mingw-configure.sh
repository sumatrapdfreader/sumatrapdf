#!/bin/sh

case $1 in
	i686 | x86_64) ;;
	*) echo "Usage: $0 i686|x86_64" >&2; exit 1 ;;
esac

target=$1-w64-mingw32
shift

exec "$(dirname "$0")"/configure \
	--build=`../config.guess` \
	--host=$target \
	--prefix=$HOME/.local/$target \
	CC= \
	CXX= \
	CPP= \
	LD= \
	CFLAGS="-static-libgcc" \
	CXXFLAGS="-static-libgcc -static-libstdc++" \
	CPPFLAGS="-I$HOME/.local/$target/include" \
	LDFLAGS=-L$HOME/.local/$target/lib \
	PKG_CONFIG_LIBDIR=$HOME/.local/$target/lib/pkgconfig:/usr/$target/sys-root/mingw/lib/pkgconfig/ \
	PKG_CONFIG_PATH=$HOME/.local/$target/share/pkgconfig:/usr/$target/sys-root/mingw/share/pkgconfig/ \
	PATH=$HOME/.local/$target/bin:/usr/$target/sys-root/mingw/bin:/usr/$target/bin:$PATH \
	--without-icu \
	--with-uniscribe \
	"$@"
