For the development of HarfBuzz, the Microsoft shaping technology, Uniscribe,
as a widely used and tested shaper is used as more-or-less OpenType reference
implementation and that specially is important where OpenType specification
is or wasn't that clear. For having access to Uniscribe on Linux/macOS these
steps are recommended:

1. Install Wine from your favorite package manager.  On Fedora that's `dnf install wine`.

2. And `mingw-w64` compiler.
   With `brew` on macOS, you can have it like `brew install mingw-w64`.
   On Fedora, with `dnf install mingw32-gcc-c++`, or `dnf install mingw64-gcc-c++` for the
   64-bit Windows. Use `apt install g++-mingw-w64` on Debian.

3. See how `.ci/build-win32.sh` uses meson or run that script anyway.

Now you can use hb-shape by `(cd win32build/harfbuzz-win32 && wine hb-shape.exe)`
but if you like to shape with the Microsoft Uniscribe,

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
