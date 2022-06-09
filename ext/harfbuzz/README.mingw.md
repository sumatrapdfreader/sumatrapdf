For the development of HarfBuzz, the Microsoft shaping technology, Uniscribe,
as a widely used and tested shaper is used as more-or-less OpenType reference
implementation and that specially is important where OpenType specification
is or wasn't that clear. For having access to Uniscribe on Linux/macOS these
steps are recommended:

You want to follow the 32bit instructions. The 64bit equivalents are included
for reference.

1. Install Wine.
   - Fedora: `dnf install wine`.

2. Install `mingw-w64` compiler.
   - Fedora, 32bit: `dnf install mingw32-gcc-c++`
   - Fedora, 64bit: `dnf install mingw64-gcc-c++`
   - Debian: `apt install g++-mingw-w64`
   - Mac: `brew install mingw-w64`

3. If you have drank the `meson` koolaid, look at `.ci/build-win32.sh` to see how to
   invoke `meson` now, or just run that script.  Otherwise, here's how to use the
   old trusty autotools instead:

   a) Install dependencies.
      - Fedora, 32bit: `dnf install mingw32-glib2 mingw32-cairo mingw32-freetype`
      - Fedora, 64bit: `dnf install mingw64-glib2 mingw64-cairo mingw64-freetype`

   b) Configure:
     - `NOCONFIGURE=1 ./autogen.sh && mkdir winbuild && cd winbuild`
     - 32bit: `../mingw-configure.sh i686`
     - 64bit: `../mingw-configure.sh x86_64`

Now you can use `hb-shape` by `(cd win32build/util && wine hb-shape.exe)`
but if you like to shape with the Microsoft Uniscribe:

4. Bring a 32bit version of `usp10.dll` for yourself from `C:\Windows\SysWOW64\usp10.dll` of your
   Windows installation (assuming you have a 64-bit installation, otherwise
   `C:\Windows\System32\usp10.dll`) that it is not a DirectWrite proxy
   ([for more info](https://en.wikipedia.org/wiki/Uniscribe)).
   Rule of thumb, your `usp10.dll` should have a size more than 500kb, otherwise
   it is designed to work with DirectWrite which Wine can't work with its original one.
   You want a Uniscribe from Windows 7 or older.

   Put the DLL in the folder you are going to run the next command,

5. `WINEDLLOVERRIDES="usp10=n" wine hb-shape.exe fontname.ttf -u 0061,0062,0063 --shaper=uniscribe`

(`0061,0062,0063` means `abc`, use test/shaping/hb-unicode-decode to generate ones you need)
