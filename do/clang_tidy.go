package main

import (
	"os"
	"os/exec"
	"path/filepath"
	"strings"
)

/*
https://clang.llvm.org/extra/clang-tidy/checks/list.html
https://codeyarns.com/2019/01/28/how-to-use-clang-tidy/
https://www.reddit.com/r/cpp/comments/ezn21f/which_checks_do_you_use_for_clangtidy/
https://www.reddit.com/r/cpp/comments/5bqkk5/good_clangtidy_files/
https://www.reddit.com/r/cpp/comments/7obg9p/how_do_you_use_clangtidy/
https://github.com/KratosMultiphysics/Kratos/wiki/How-to-use-Clang-Tidy-to-automatically-correct-code
https://sarcasm.github.io/notes/dev/clang-tidy.html
https://www.labri.fr/perso/fleury/posts/programming/using-clang-tidy-and-clang-format.html
*/

/*
.\doit.bat -clang-format
git commit -am "clang-tidy fix some readability-braces-around-statements"
git commit -am "clang-tidy fix some modernize-use-default-member-init"
git commit -am "clang-tidy fix some readability-inconsistent-declaration-parameter-name"


ad-hoc execution:
clang-tidy.exe --checks=-clang-diagnostic-microsoft-goto,-clang-diagnostic-unused-value -extra-arg=-std=c++20 .\src\*.cpp -- -I mupdf/include -I src -I src/utils -I src/wingui -I ext/WDL -DUNICODE -DWIN32 -D_WIN32 -D_CRT_SECURE_NO_WARNINGS -DWINVER=0x0a00 -D_WIN32_WINNT=0x0a00 -DBUILD_TEX_IFILTER -DBUILD_EPUB_IFILTER

ls src\utils\*.cpp | select Name

clang-tidy src/*.cpp -fix -header-filter=src/ -checks="-*,readability-inconsistent-declaration-parameter-name" -extra-arg=-std=c++20 -- -I mupdf/include -I src -I src/utils -I src/wingui -I ext/WDL -I ext/CHMLib/src -I ext/libdjvu -I ext/zlib -I ext/synctex -I ext/unarr -I ext/lzma/C -I ext/libwebp/src -I ext/freetype/include -DUNICODE -DWIN32 -D_WIN32 -D_CRT_SECURE_NO_WARNINGS -DWINVER=0x0a00 -D_WIN32_WINNT=0x0a00 -DBUILD_TEX_IFILTER -DBUILD_EPUB_IFILTER

*/

/*
Done:
src
src/utils
src/wingui
src/mui
src/uia
src/ifilter
src/previewer

Fix warnings:
* clang-analyzer-deadcode.DeadStores
* clang-analyzer-cplusplus.NewDeleteLeaks
* clang-analyzer-unix.Malloc
* clang-analyzer-core.uninitialized.Assign

TODO fixes:
modernize-use-default-member-init
modernize-return-braced-init-list
modernize-raw-string-literal
modernize-pass-by-value
modernize-loop-convert
modernize-deprecated-headers
modernize-concat-nested-namespaces
modernize-avoid-c-arrays
modernize-avoid-bind
modernize-use-override
modernize-use-nullptr
modernize-use-auto
modernize-use-nodiscard
readability-inconsistent-declaration-parameter-name
readability-make-member-function-const
readability-misplaced-array-index
readability-redundant-access-specifiers
readability-redundant-control-flow
readability-redundant-declaration
readability-redundant-function-ptr-dereference
readability-redundant-member-init
readability-redundant-preprocessor
readability-redundant-string-init
readability-redundant-string-cst
readability-string-compare
*/

const clangTidyLogFile = "clangtidy.out.txt"

// TODO: maybe re-enable clang-diagnostic-switch, for now it's too many false positives
func clangTidyFile(path string) {
	args := []string{
		"--checks=-clang-diagnostic-microsoft-goto,-clang-diagnostic-unused-value,-clang-diagnostic-ignored-pragma-optimize,-clang-diagnostic-pragma-pack,-clang-diagnostic-switch",
		"-extra-arg=-std=c++20",
		"", // file
		"--",
		"-I", "mupdf/include",
		"-I", "src",
		"-I", "src/utils",
		"-I", "src/wingui",
		"-I", "ext/WDL",
		"-I", "ext/CHMLib/src",
		"-I", "ext/libdjvu",
		"-I", "ext/zlib",
		"-I", "ext/synctex",
		"-I", "ext/unarr",
		"-I", "ext/lzma/C",
		"-I", "ext/libwebp/src",
		"-I", "ext/freetype/include",

		"-DUNICODE",
		"-DWIN32",
		"-D_WIN32",
		"-D_CRT_SECURE_NO_WARNINGS",
		"-DWINVER=0x0a00",
		"-D_WIN32_WINNT=0x0a00",
		"-DPRE_RELEASE_VER=3.3",
	}
	args[2] = path
	cmd := exec.Command("clang-tidy", args...)
	_ = runCmdShowProgressAndLog(cmd, clangTidyLogFile)
}

func runClangTidy() {
	os.Remove(clangTidyLogFile)
	files := []string{
		`src\*.cpp`,
		`src\*.h`,
		`src\mui\*.cpp`,
		`src\mui\*.h`,
		`src\utils\*.cpp`,
		`src\utils\*.h`,
		`src\utils\tests\*.cpp`,
		`src\utils\tests\*.h`,
		`src\wingui\*.cpp`,
		`src\wingui\*.h`,
		`src\uia\*.cpp`,
		`src\uia\*.h`,
		`src\tools\*.cpp`,
		`src\previewer\*.h`,
		`src\previewer\*.cpp`,
		`src\ifilter\*.h`,
		`src\ifilter\*.cpp`,
		`ext\mupdf_load_system_font.c`,
	}

	isWhiteListed := func(s string) bool {
		whitelisted := []string{
			"resource.h",
			"Version.h",
			"Trans_sumatra_txt.cpp",
			"Trans_installer_txt.cpp",
			"signfile.cpp",
		}
		s = strings.ToLower(s)

		if strings.HasSuffix(s, ".h") {
			return true
		}

		for _, wl := range whitelisted {
			wl = strings.ToLower(wl)
			if strings.Contains(s, wl) {
				logf("Whitelisted '%s'\n", s)
				return true
			}
		}
		return false
	}
	for _, globPattern := range files {
		paths, err := filepath.Glob(globPattern)
		must(err)
		for _, path := range paths {
			if isWhiteListed(path) {
				continue
			}
			clangTidyFile(path)
		}
	}
	logf("\nLogged output to '%s'\n", clangTidyLogFile)
}
