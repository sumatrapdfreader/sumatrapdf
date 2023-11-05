package main

import (
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
)

var printClangPath bool

func detectClangFormat() string {
	path := detectPath(vsBasePaths, `VC\Tools\Llvm\bin\clang-format.exe`)
	panicIf(!fileExists(path), "didn't find clang-format.exe")
	if !printClangPath {
		logf("clang-format: %s\n", path)
		printClangPath = true
	}
	return path
}

func clangFormatFile(path string) {
	clangFormatPath := detectClangFormat()
	cmd := exec.Command(clangFormatPath, "-i", "-style=file", path)
	runCmdLoggedMust(cmd)
}

func clangFormatFiles() {
	path := detectClangFormat()
	logf("using '%s'\n", path)
	files := []string{
		`src\*.cpp`,
		`src\*.h`,
		`src\mui\*.cpp`,
		`src\mui\*.h`,
		`src\utils\*.cpp`,
		`src\utils\*.h`,
		`src\utils\tests\*.cpp`,
		`src\utils\tests\*.h`,
		`src\wingui\*`,
		`src\uia\*`,
		`src\tools\*`,
		`src\ifilter\*.cpp`,
		`src\iflter\*.h`,
		`src\previewer\*.cpp`,
		`src\previewer\*.h`,
		`ext\CHMLib\*.c`,
		`ext\CHMLib\*.h`,
		`ext\mupdf_load_system_font.c`,
	}
	isWhiteListed := func(s string) bool {
		whitelisted := []string{
			"resource.h",
			"Version.h",
			"TranslationLangs.cpp",
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
	sem := make(chan bool, runtime.NumCPU())
	var wg sync.WaitGroup
	for _, globPattern := range files {
		paths, err := filepath.Glob(globPattern)
		must(err)
		for _, path := range paths {
			if isWhiteListed(path) {
				continue
			}
			sem <- true
			wg.Add(1)
			go func(p string) {
				clangFormatFile(p)
				wg.Done()
				<-sem
			}(path)
		}
	}
	wg.Wait()
	logf("used '%s'\n", path)
}
