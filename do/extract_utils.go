package main

import (
	"io/fs"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"github.com/kjk/common/u"
	"golang.org/x/exp/slices"
)

var runLogged2 = func(cmd *exec.Cmd, panicOnErr bool) string {
	logf("> %s\n", cmd.String())
	cmd.Stderr = os.Stderr
	out, err := cmd.Output()
	logf("Output:\n%s\n", string(out))
	if panicOnErr {
		logIfError(ctx(), err)
		panicIf(panicOnErr && err != nil)
	}
	return string(out)
}

var runLogged = func(cmd *exec.Cmd) string {
	return runLogged2(cmd, true)
}

func printCurrDir() {
	dir, err := filepath.Abs(".")
	must(err)
	logf("dir: %s\n", dir)
	files, err := os.ReadDir(dir)
	must(err)
	for _, de := range files {
		logf("  %s dir: %v\n", de.Name(), de.IsDir())
	}
}

// action code based on https://www.youtube.com/watch?v=dcSy8uCxOfk
func extractUtils(inAction bool) {
	printCurrDir()

	commitMsg := "update from sumatrapdf"
	logf("updateNotepad2: inAction=%v\n", inAction)
	parentDir, err := filepath.Abs("..")
	must(err)
	logf("parentDir: %s\n", parentDir)
	dstDir := filepath.Join(parentDir, "pdfprint")
	logf("notepadDir: %s\n", dstDir)
	if !u.DirExists(dstDir) {
		logf("Directory '%s' doesn't exist\n", dstDir)
		os.Exit(1)
	}
	if !inAction {
		cmd := exec.Command("git", "pull")
		cmd.Dir = dstDir
		runLogged2(cmd, true)
	}

	utilsDir := filepath.Join(dstDir, "utils")
	// remove existing dir
	logf("deleting all files in directory '%s'\n", utilsDir)
	must(os.RemoveAll(utilsDir))
	// recreate dir
	must(os.MkdirAll(utilsDir, 0755))
	srcDir := filepath.Join("src", "utils")
	copyFilesRecurMust(utilsDir, srcDir)

	if inAction {
		{
			cmd := exec.Command("git", "config", "--global", "user.email", "kkowalczyk@gmail.com")
			runLogged(cmd)
		}
		{
			cmd := exec.Command("git", "config", "--global", "user.name", "Krzysztof Kowalczyk")
			runLogged(cmd)
		}
	}

	{
		cmd := exec.Command("git", "add", ".")
		cmd.Dir = dstDir
		runLogged2(cmd, true)
	}
	{
		msg := commitMsg
		if msg == "" {
			msg = "update from sumatrapdf"
		}
		cmd := exec.Command("git", "commit", "-am", msg)
		cmd.Dir = dstDir
		out := runLogged2(cmd, false)
		if strings.Contains(out, "nothing to commit") {
			return
		}
	}

	cmd := exec.Command("git", "push", "origin", "master")
	cmd.Dir = dstDir
	runLogged(cmd)
}

func shouldCopyFile(dir string, de fs.DirEntry) bool {
	name := de.Name()

	bannedSuffixes := []string{".go", ".bat"}
	for _, s := range bannedSuffixes {
		if strings.HasSuffix(name, s) {
			return false
		}
	}

	bannedPrefixes := []string{"yarn", "go."}
	for _, s := range bannedPrefixes {
		if strings.HasPrefix(name, s) {
			return false
		}
	}

	doNotCopy := []string{"tests"}
	if slices.Contains(doNotCopy, name) {
		return false
	}
	return true
}

func copyFilesRecurMust(dstDir, srcDir string) {
	files, err := os.ReadDir(srcDir)
	must(err)

	for _, de := range files {
		if !shouldCopyFile(dstDir, de) {
			continue
		}
		dstPath := filepath.Join(dstDir, de.Name())
		srcPath := filepath.Join(srcDir, de.Name())
		if de.IsDir() {
			copyFilesRecurMust(dstPath, srcPath)
			continue
		}
		copyFileMust(dstPath, srcPath)
	}
}

func copyFileMust(dst, src string) {
	_, err := os.Stat(dst)
	if err == nil {
		logf("destination '%s' already exists, skipping\n")
		return
	}
	logf("copy %s => %s\n", src, dst)
	dstDir := filepath.Dir(dst)
	err = os.MkdirAll(dstDir, 0755)
	must(err)
	d, err := os.ReadFile(src)
	must(err)
	err = os.WriteFile(dst, d, 0644)
	must(err)
}
