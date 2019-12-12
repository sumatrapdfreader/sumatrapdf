package main

import (
	"os/exec"
	"path/filepath"
	"strings"

	"github.com/kjk/u"
)

var (
	clangFormatPath string
)

func detectClangFormat() string {
	if clangFormatPath != "" {
		return clangFormatPath
	}

	path, err := exec.LookPath("clang-format.exe")
	if err == nil {
		clangFormatPath = path
		return path
	}
	path = `c:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\Llvm\bin\clang-format.exe`
	if u.FileExists(path) {
		clangFormatPath = path
		return path
	}
	// TODO: c:\Users\kjk\.vscode\extensions\ms-vscode.cpptools-0.26.1\LLVM\bin\clang-format.exe
	panic("didn't find clang-format.exe")
}

func clangFormatFile(path string) {
	cmd := exec.Command(clangFormatPath, "-i", "-style=file", path)
	u.RunCmdLoggedMust(cmd)
}

func clangFormatFiles() {
	detectClangFormat()
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
		`src\tools\*.cpp`,
		`src\tools\*.h`,
	}
	isWhiteListed := func(s string) bool {
		whitelisted := []string{
			"resource.h",
			"Version.h",
			"Trans_sumatra_txt.cpp",
			"Trans_installer_txt.cpp",
		}
		s = strings.ToLower(s)
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
			clangFormatFile(path)
		}
	}
}
