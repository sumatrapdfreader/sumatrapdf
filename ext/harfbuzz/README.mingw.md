For the development of HarfBuzz, the Microsoft shaping technology, Uniscribe,
as a widely used and tested shaper is used as more-or-less OpenType reference
implementation and that specially is important where OpenType specification
is or wasn't that clear. For having access to Uniscribe on Linux/macOS these
steps are recommended:

1. Install Wine from your favorite package manager.  On Fedora that's `dnf install wine`.

2. And `mingw-w64` compiler.
   With `brew` on macOS, you can have it like `brew install mingw-w64`.
   On Fedora, with `dnf install mingw32-gcc-c++`, or `dnf install mingw64-gcc-c++` for the
   64-bit Windows.

3. Install cross-compiled dependency packages.  Alternatively see [^1] below.
   On Fedora that would be `dnf install mingw32-glib2 mingw32-cairo mingw32-freetype`
   for 32-bit, or `dnf install mingw64-glib2 mingw64-cairo mingw64-freetype` for 64-bit.

5. `NOCONFIGURE=1 ./autogen.sh && mkdir winbuild && cd winbuild`

6. Run `../mingw32.sh` for 32-bit build, or `../mingw64.sh` for 64-bit.  This configures
   HarfBuzz for cross-compiling.  It enables Uniscribe backend as well.

7. `make`

Now you can use hb-shape using `wine util/hb-shape.exe` but if you like to shape with
the Microsoft Uniscribe,

8. Bring a 32bit version of `usp10.dll` for yourself from `C:\Windows\SysWOW64\usp10.dll` of your
   Windows installation (assuming you have a 64-bit installation, otherwise
   `C:\Windows\System32\usp10.dll`) that it is not a DirectWrite proxy
   ([for more info](https://en.wikipedia.org/wiki/Uniscribe)).
   Rule of thumb, your `usp10.dll` should have a size more than 500kb, otherwise
   it is designed to work with DirectWrite which Wine can't work with its original one.
   You want a Uniscribe from Windows 7 or older.

   Put the DLL in the folder you are going to run the next command,

9. `WINEDLLOVERRIDES="usp10=n" wine util/hb-shape.exe fontname.ttf -u 0061,0062,0063 --shaper=uniscribe`

(`0061,0062,0063` means `abc`, use test/shaping/hb-unicode-decode to generate ones you need)


[^1] Download and put [this](https://drive.google.com/open?id=0B3_fQkxDZZXXbWltRGd5bjVrUDQ)
     in your `~/.local/i686-w64-mingw32`.  Then replace all the instances of
     `/home/behdad/.local/i586-mingw32msvc` and `/home/behdad/.local/i686-w64-mingw32`
     with `<$HOME>/.local/i686-w64-mingw32` on that folder.
     (`<$HOME>` replace it with `/home/XXX` or `/Users/XXX` on macOS)
     You shouldn't replace the instances of those inside binary files.
