# Using OpenCppCoverage

I tried [OpenCppCoverage](https://opencppcoverage.codeplex.com/) (https://github.com/Unity-Technologies/OpenCppCoverage) 0.9.3 on SumatraPDF.

I installed the 64-bit version on Windows 10. I ran `OpenCppCoverage.exe --sources sumatra -- .\dbg64\SumatraPDF.exe`, opened a file, and closed the program.

`--sources` limits reporting to files whose paths match a given pattern. Using `sumatra` for the pattern matches all our source files, assuming the sources were checked out to the `sumatrapdf` directory (because `sumatra` matches `sumatrapdf` in the file path), and skips the sources for C/C++ libraries.

It generated a `CoverageReport-${date}` directory with an HTML report. There is a top-level `index.html` with links to the HTML file for each module (`SumatraPDF.exe` in our case).

They recommend running on debug code.

At the end, it reports generating reports for system DLLs, but it also says it can't get symbols, so it doesn't actually generate anything.

The HTML report isn't great. Another option would be to use the `--export_type=binary` option, which generates a `SumatraPDF.cov` file, and write an HTML generator myself. I assume the format of the `.cov` file is simple.
