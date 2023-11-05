package main

import (
	"os"
	"os/exec"
	"path/filepath"
	"strings"
)

/*
https://clang.llvm.org/extra/clang-tidy/checks/list.html

https://github.com/derceg/explorerplusplus/blob/master/.clang-tidy
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
clang-tidy.exe --checks=-clang-diagnostic-microsoft-goto,-clang-diagnostic-unused-value -extra-arg=-std=c++20 .\src\*.cpp -- -I mupdf/include -I src -I src/utils -I src/wingui -DUNICODE -DWIN32 -D_WIN32 -D_CRT_SECURE_NO_WARNINGS -DWINVER=0x0a00 -D_WIN32_WINNT=0x0a00 -DBUILD_TEX_IFILTER -DBUILD_EPUB_IFILTER

ls src\utils\*.cpp | select Name

clang-tidy src/*.cpp -fix -header-filter=src/ -checks="-*,readability-inconsistent-declaration-parameter-name" -extra-arg=-std=c++20 -- -I mupdf/include -I src -I src/utils -I src/wingui -I ext/CHMLib/src -I ext/libdjvu -I ext/zlib -I ext/synctex -I ext/unarr -I ext/lzma/C -I ext/libwebp/src -I ext/freetype/include -DUNICODE -DWIN32 -D_WIN32 -D_CRT_SECURE_NO_WARNINGS -DWINVER=0x0a00 -D_WIN32_WINNT=0x0a00 -DBUILD_TEX_IFILTER -DBUILD_EPUB_IFILTER
*/

const clangTidyLogFile = "clangtidy.out.txt"

// TODO: maybe re-enable clang-diagnostic-switch, for now it's too many false positives
func clangTidyFile(path string) {
	/*
		checks := []string{
			"-*",
			"bugprone-*",
		}
	*/
	args := []string{
		//"--checks=" + strings.Join(checks, ","),
		"--header-filter=.*",
		"-extra-arg=-std=c++20",
		path,
		"--",
		"-I", "mupdf/include",
		"-I", "src",
		"-I", "src/utils",
		"-I", "src/wingui",
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
	cmd := exec.Command("clang-tidy", args...)
	err := runCmdShowProgressAndLog(cmd, clangTidyLogFile)
	must(err)
}

/*
Done:
readability-make-member-function-const
readability-avoid-const-params-in-decls
modernize-use-override
readability-simplify-boolean-expr : dangerous, removes if (false) / if (true) debug code
modernize-use-nullptr
readability-braces-around-statements
modernize-use-equals-default
readability-inconsistent-declaration-parameter-name
readability-redundant-declaration
readability-redundant-access-specifiers
readability-redundant-control-flow
readability-misplaced-array-index
readability-redundant-member-init
readability-redundant-string-init
modernize-avoid-bind
modernize-use-bool-literals
google-explicit-constructor
modernize-raw-string-literal
bugprone-copy-constructor-init

*/

/*
TODO fixes:
readability-redundant-function-ptr-dereference
readability-redundant-string-cst

bugprone-bool-pointer-implicit-conversion

performance-move-const-arg
modernize-use-default-member-init
modernize-return-braced-init-list
modernize-pass-by-value
modernize-loop-convert
modernize-deprecated-headers
modernize-use-auto
readability-redundant-preprocessor
readability-string-compare

maybe not:
cppcoreguidelines-prefer-member-initializer ??
modernize-concat-nested-namespaces
modernize-avoid-c-arrays
modernize-use-nodiscard
modernize-use-using : needs to figure out how to not run on WinDynCalls.h
*/

func clangTidyFix(path string) {
	args := []string{
		"--checks=-*,modernize-raw-string-literal",
		"-p",
		".",
		"--header-filter=src/",
		"--fix",
		"-extra-arg=-std=c++20",
		path,
		"--",
		"-I", "mupdf/include",
		"-I", "src",
		"-I", "src/utils",
		"-I", "src/wingui",
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
	cmd := exec.Command("clang-tidy", args...)
	err := runCmdShowProgressAndLog(cmd, clangTidyLogFile)
	must(err)
}

func runClangTidy(fix bool) {
	os.Remove(clangTidyLogFile)
	files := []string{
		`src\*.cpp`,
		`src\mui\*.cpp`,
		`src\utils\*.cpp`,
		//`src\utils\tests\*.cpp`,
		`src\wingui\*.cpp`,
		`src\uia\*.cpp`,
		//`src\tools\*.cpp`,
		`src\previewer\*.cpp`,
		`src\ifilter\*.cpp`,
		//`ext\mupdf_load_system_font.c`,
	}

	isWhiteListed := func(s string) bool {
		whitelisted := []string{
			"resource.h",
			"Version.h",
			"TranslationLangs.cpp",
			"signfile.cpp",
			// those fail due to DrawInstr
			"Doc.cpp",
			"EbookController.cpp",
			"EbookControls.cpp",
			"EbookFormatter.cpp",
			"EngineEbook.cpp",
			"HtmlFormatter.cpp",
			"StressTesting.cpp",
			"Tester.cpp",
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
			if fix {
				clangTidyFix(path)
			} else {
				clangTidyFile(path)
			}
		}
	}
	logf("\nLogged output to '%s'\n", clangTidyLogFile)
}
