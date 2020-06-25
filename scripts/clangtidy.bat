@rem this is for experimenting with clang-tidy
@rem must have clang-tidy in path, e.g. do:
@rem choco install llvm
@rem to install llvm (which includes clang-tidy)

@rem https://chromium.googlesource.com/chromium/src.git/+/master/docs/clang_tidy.md

clang-tidy.exe --checks=-clang-diagnostic-microsoft-goto,-clang-diagnostic-unused-value -extra-arg=-std=c++20 .\src\SumatraStartup.cpp -- -I src -I src/utils -DUNICODE -DWIN32 -D_WIN32 -D_CRT_SECURE_NO_WARNINGS -DWINVER=0x0a00 -D_WIN32_WINNT=0x0a00

