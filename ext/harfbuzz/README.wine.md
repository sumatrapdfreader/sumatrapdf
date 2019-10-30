For the development of HarfBuzz, the Microsoft shaping technology, Uniscribe,
as a widely used and tested shaper is used as more-or-less OpenType reference
implemenetation and that specially is important where OpenType specification
is or wasn't that clear. For having access to Uniscribe on Linux/macOS these
steps are recommended:

1. Install Wine from your favorite package manager.

2. And `mingw-w64` compiler.
   With `brew` on macOS, you can have it like `brew install mingw-w64`

3. Download and put [this](https://drive.google.com/open?id=0B3_fQkxDZZXXbWltRGd5bjVrUDQ)
   on your `~/.local/i686-w64-mingw32`.

4. Replace all the instances of `/home/behdad/.local/i586-mingw32msvc`
   and `/home/behdad/.local/i686-w64-mingw32` with `<$HOME>/.local/i686-w64-mingw32`
   on that folder. (`<$HOME>` replace it with `/home/XXX` or `/Users/XXX` on macOS)

   Probably you shouldn't replace the ones are inside binaries.

5. `NOCONFIGURE=1 ./autogen.sh && mkdir winbuild && cd winbuild`

6. `../mingw32.sh --with-uniscribe && cd ..`

7. `make -Cwinbuild`

Now you can use hb-shape using `wine winbuild/util/hb-shape.exe` but if you like to
to use the original Uniscribe,

8. Bring a 32bit version of `usp10.dll` for youself from `C:\Windows\SysWOW64\usp10.dll` of your
   Windows installation (asuming you have a 64-bit installation, otherwise `C:\Windows\System32\usp10.dll`)
   that it is not a DirectWrite proxy ([for more info](https://en.wikipedia.org/wiki/Uniscribe)).
   Rule of thumb, your `usp10.dll` should have a size more than 500kb, otherwise
   it is designed to work with DirectWrite which Wine can't work with its original one.

   Put the dll on the folder you are going to run the next command,

9. `WINEDLLOVERRIDES="usp10=n" wine winbuild/util/hb-shape.exe fontname.ttf -u 0061,0062,0063 --shaper=uniscribe`

(`0061,0062,0063` means `abc`, use test/shaping/hb-unicode-decode to generate ones you need)
