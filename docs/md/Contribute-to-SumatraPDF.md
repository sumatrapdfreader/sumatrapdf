# Contribute to SumatraPDF

SumatraPDF is an open-source, [collaborative](https://github.com/sumatrapdfreader/sumatrapdf/blob/master/AUTHORS) project. We always welcome new developers who want to join the project and contribute code or new features.

## Getting the sources and compiling

You'll need the latest Visual Studio. The [free Community edition](https://www.visualstudio.com/vs/community/) will work.

Get the sources from [GitHub](https://github.com/sumatrapdfreader/sumatrapdf).

Open `vs2022/SumatraPDF.sln`, then compile and run.

Read more about our [build system](Build-system.md).

## Contribute to SumatraPDF

We use a standard GitHub model:

- fork [https://github.com/sumatrapdfreader/sumatrapdf](https://github.com/sumatrapdfreader/sumatrapdf)
- submit a pull request
- we'll review the code, provide feedback, and merge it when it's ready

Before you start working on a significant addition, it's a good idea to discuss it in the [issue tracker](https://github.com/sumatrapdfreader/sumatrapdf/issues) first.

## Bug reports? Feature requests? Questions?

You can use the [issue tracker](https://github.com/sumatrapdfreader/sumatrapdf/issues) for development-related topics or the [forums](https://www.sumatrapdfreader.org/forum.html) for general topics.

## Info for new developers

Information to orient new developers to the SumatraPDF codebase.

You should install [bun](https://bun.com/).

Many tasks are automated with Bun scripts in the `cmd/` directory.

To build, use Visual Studio 2022 and open the `vs2022\SumatraPDF.sln` solution. Look at the different targets and configurations.

Don't edit the solution directly. To learn how to make changes (add files, change compilation flags, etc.), see the information about the [build system](Build-system.md).

We use [GitHub Actions](https://help.github.com/en/actions) as our CI system. See the `.github` directory. Most importantly, it builds a 64-bit release version on every check-in to catch build errors.

Overview of the directories:

- `src` : main SumatraPDF code
- `ext` : third-party libraries, including `ext/mupdf` (from [https://mupdf.com/](https://mupdf.com/))
- `cmd` : Bun scripts that automate common tasks
