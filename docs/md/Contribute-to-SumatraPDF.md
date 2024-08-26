# Contribute to SumatraPDF

SumatraPDF is an open-source, [collaborative](https://github.com/sumatrapdfreader/sumatrapdf/blob/master/AUTHORS) project. We always welcome new developers who want to join the project, contribute code and new features.

## Getting the sources and compiling

You'll need latest Visual Studio. A [free Community edition](https://www.visualstudio.com/vs/community/) will work.

Get the sources from [https://github.com/sumatrapdfreader/sumatrapdf](https://github.com/sumatrapdfreader/sumatrapdf)

Open `vs2022/SumatraPDF.sln` compile and run.

Read more about our [build system](Build-system.md).

## Contribute to SumatraPDF

We use a standard GitHub model:

- fork [https://github.com/sumatrapdfreader/sumatrapdf](https://github.com/sumatrapdfreader/sumatrapdf)
- submit pull request
- I'll review the code, provide the feedback and merge it when it's ready

Before you start working on a significant addition, it's a good idea to first discuss it in [issue tracker](https://github.com/sumatrapdfreader/sumatrapdf/issues).

## Bug reports? Feature requests? Questions?

You can use [issue tracker](https://github.com/sumatrapdfreader/sumatrapdf/issues) for development related topics or [forums](https://www.sumatrapdfreader.org/forum.html) for general topics.

## Info for new developers

Info to orient new developers to Sumatra code base.

You should install Go ([https://golang.org/dl/](https://golang.org/dl/)).

Many tasks are automated with Go program in `do` directory. Run `doit.bat` for easy running of that program. use different cmd-line args to trigger different functions.

To build, use latest Visual Studio and open `vs2022\SumatraPDF.sln` solution. Look at different targets and configurations.

Don't edit the solution directly. To learn how to make changes (add files, change compilation flags etc.) see info about [build system](Build-system.md).

We use [GitHub Actions](https://help.github.com/en/actions) for a CI system. See `.github` directory. Most importantly it builds a 64-bit release version on every checkin to catch build errors and uploads it to storage and makes available via [https://www.sumatrapdfreader.org/prerelease](https://www.sumatrapdfreader.org/prerelease)

Overview of the directories:

- `src` : main Sumatra code
- `mupdf` : library used to parse / render PDF files (from [https://mupdf.com/](https://mupdf.com/))
- `ext` : other third-party libraries (some are needed for mupdf, some for Sumatra code)
- `do` : Go program that automates common tasks. Invoke with `doit.bat`