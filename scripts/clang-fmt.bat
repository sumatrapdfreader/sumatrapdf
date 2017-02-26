@rem This requires clang-format to be installed and in %PATH%
@rem http://llvm.org/releases/download.html
@rem it's part of clang installer

del src\*.bak
del src\*.tmp

for %%f in (src\ParseCommandLine* src\Tests* src\Print*) do (
    clang-format.exe -i -style=file %%f
)

del src\*.bak
del src\*.tmp
