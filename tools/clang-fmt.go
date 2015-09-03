package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
)

const (
	// we used to use Mozilla style for the base, but they really changed the
	// style between 3.5 and 3.7. Chromium style is close to Mozilla and
	// even makes more sense about details like "char* s" vs. "char *"
	clangStyle = `{BasedOnStyle: Chromium, IndentWidth: 4, ColumnLimit: 100, AccessModifierOffset: -2}`
)

func fataliferr(err error) {
	if err != nil {
		fmt.Printf("err: %s\n", err)
		os.Exit(1)
	}
}

func toIntMust(s string) int {
	n, err := strconv.Atoi(s)
	fataliferr(err)
	return n
}

// ensure we have at least version 3.7.0
func verifyClangFormatVersion(exePath string) {
	cmd := exec.Command(exePath, "--version")
	out, err := cmd.Output()
	fataliferr(err)
	// output is in format: clang-format version 3.7.0 (tags/RELEASE_370/final)
	s := strings.TrimSpace(string(out))
	parts := strings.SplitN(s, " ", 4)
	ver := parts[2]
	parts = strings.Split(ver, ".")
	// check version is at lest 3.7.0
	n1 := toIntMust(parts[0])
	n2 := toIntMust(parts[1])
	n3 := 0
	if len(parts) > 2 {
		n3 = toIntMust(parts[2])
	}
	n := n1*100 + n2*10 + n3
	if n < 370 {
		fmt.Printf("Version should be at least 3.7.0 (is %s)\n", ver)
		os.Exit(1)
	}
}

func ifCmdFailed(err error, out []byte, cmd *exec.Cmd) {
	if err == nil {
		return
	}
	s := strings.Join(cmd.Args, " ")
	fmt.Printf("'%s' failed with: '%s', out:\n'%s'\n", s, err, string(out))
	os.Exit(1)
}

func isSrcFile(s string) bool {
	ext := strings.ToLower(filepath.Ext(s))
	switch ext {
	case ".cpp", ".h":
		return true
	}
	return false
}

func getSrcFilesMust(dir string) []string {
	files, err := ioutil.ReadDir(dir)
	fataliferr(err)
	var srcFiles []string
	for _, fi := range files {
		name := fi.Name()
		if isSrcFile(name) {
			srcFiles = append(srcFiles, name)
		}
	}
	return srcFiles
}

func runInDirMust(exePath string, dir string) {
	files := getSrcFilesMust(dir)
	for _, f := range files {
		cmd := exec.Command(exePath, "-style", clangStyle, "-i", f)
		cmd.Dir = dir
		fmt.Printf("Running: '%s'\n", strings.Join(cmd.Args, " "))
		out, err := cmd.CombinedOutput()
		ifCmdFailed(err, out, cmd)
	}
}

func main() {
	var d string
	exePath, err := exec.LookPath("clang-format")
	fataliferr(err)
	fmt.Printf("exe path: %s\n", exePath)
	verifyClangFormatVersion(exePath)

	d = filepath.Join("src", "utils")
	//runInDirMust(exePath, d)

	d = filepath.Join("src", "mui")
	runInDirMust(exePath, d)

	d = filepath.Join("src", "wingui")
	runInDirMust(exePath, d)
}
