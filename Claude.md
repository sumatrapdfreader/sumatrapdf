This is a C++ program for Windows, using mostly win32 windows API functions

We don't use STL but our own string / helper / container functions implemented in src\utils directory

Assume that Visual Studio command-line tools are available in the PATH environment variable (cl.exe, msbuild.exe etc.)

Our code is in src/ directory. External dependencies are in ext/ directory and mupdf\ directory

To build run build.ts with bun

This crates ./out/dbg64/SumatraPDF.exe executable

To debug run: `raddbg.exe --auto_run --quit_after_success ./out/dbg64/SumatraPDF.exe

After making a change to .cpp, .c or .h file (and before running build.ts), run clang-format on those files to reformat them in place

When commiting changes also commit src/scratch.txt (if changed)
