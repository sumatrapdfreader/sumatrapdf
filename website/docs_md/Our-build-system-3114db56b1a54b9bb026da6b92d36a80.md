# [Documentation](/docs/) : Our build system

Our build system uses [Premake5](http://premake.github.io/download.html#v5) . I've used alpha11 build.

Premake generates Visual Studio solution in directory vs2017 and vs2015 from premake5.lua file. We checkin generated solution files for convenience.

## Running

You only have to worry about premake if you add new source files, since that requires re-generating Visual Studio solution. To make changes, modify `premake5.lua` and/or `premake5.files.lua` .

Download premake 5 from http://premake.github.io/download.html#v5. I put it in bin directory. Run `bin\premake5 vs2017` and `bin\premake5 vs2015` to re-generate Visual Studio project files.

## Customizing build

Sometimes we want to customize the build with `#ifdef` . We could do it by adding additional configurations, but that can spiral out of control quickly.

Instead we have `src\utils\BuildConfig.h ` file. It's empty by default but you can changed it to add your `#define` customizations.

## How official builds are made

I use [Go](https://golang.org/) program `tools\build\main.go.` You can read it for details of operation.