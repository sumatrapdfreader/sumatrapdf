package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
)

/*
A tool to preview changes before checkin.
Uses WinMerge to do the diffing (http://winmerge.org/)
Anohter option that wouldn't require winmerge is to make it a web server,
implement web-based ui and launch the browser.
*/

var (
	gitPath      string
	winMergePath string
	tempDir      string
)

const (
	Modified = iota
	Added
	Deleted
	NotCheckedIn
)

type GitChange struct {
	Type int // Modified, Added etc.
	Path string
}

func printStack() {
	buf := make([]byte, 1024*164)
	n := runtime.Stack(buf, false)
	fmt.Printf("%s", buf[:n])
}

func fatalf(format string, args ...interface{}) {
	fmt.Printf(format, args...)
	printStack()
	os.Exit(1)
}

func fatalif(cond bool, format string, args ...interface{}) {
	if cond {
		fmt.Printf(format, args...)
		printStack()
		os.Exit(1)
	}
}

func fataliferr(err error) {
	if err != nil {
		fatalf("%s\n", err.Error())
	}
}

func toTrimmedLines(d []byte) []string {
	lines := strings.Split(string(d), "\n")
	i := 0
	for _, l := range lines {
		l = strings.TrimSpace(l)
		// remove empty lines
		if len(l) > 0 {
			lines[i] = l
			i++
		}
	}
	return lines[:i]
}

func detectExeMust(name string) string {
	path, err := exec.LookPath(name)
	if err == nil {
		fmt.Printf("'%s' is '%s'\n", name, path)
		return path
	}
	fmt.Printf("Couldn't find '%s'\n", name)
	fataliferr(err)
	// TODO: could also try known locations for WinMerge in $env["ProgramFiles(x86)"]/WinMerge/WinMergeU.exe
	return ""
}

func detectExesMust() {
	gitPath = detectExeMust("git")
	winMergePath = detectExeMust("WinMergeU")
}

func getWinTempDirMust() string {
	dir := os.Getenv("TEMP")
	if dir != "" {
		return dir
	}
	dir = os.Getenv("TMP")
	fatalif(dir == "", "env variable TEMP and TMP are not set\n")
	return dir
}

func createTempDirMust() {
	dir := getWinTempDirMust()
	// we want a stable name so that we can clean up old junk
	tempDir = filepath.Join(dir, "sum-diff-preview")
	err := os.MkdirAll(tempDir, 0755)
	fataliferr(err)
}

func runCmd(exePath string, args ...string) ([]byte, error) {
	cmd := exec.Command(exePath, args...)
	return cmd.Output()
}

func parseGitStatusLineMust(s string) *GitChange {
	c := &GitChange{}
	parts := strings.SplitN(s, " ", 2)
	fatalif(len(parts) != 2, "invalid line: '%s'\n", s)
	switch parts[0] {
	case "M":
		c.Type = Modified
	case "A":
		c.Type = Added
	case "D":
		c.Type = Deleted
	case "??":
		c.Type = NotCheckedIn
	default:
		fatalif(true, "invalid line: '%s'\n", s)
	}
	c.Path = strings.TrimSpace(parts[1])
	return c
}

func parseGitStatusMust(out []byte) []*GitChange {
	var res []*GitChange
	lines := toTrimmedLines(out)
	for _, l := range lines {
		res = append(res, parseGitStatusLineMust(l))
	}
	return res
}

func gitStatusMust() []*GitChange {
	out, err := runCmd(gitPath, "status", "--porcelain")
	fataliferr(err)
	return parseGitStatusMust(out)
}

func main() {
	detectExesMust()
	createTempDirMust()
	fmt.Printf("temp dir: %s\n", tempDir)
	changes := gitStatusMust()
	if len(changes) == 0 {
		fmt.Printf("No changes to preview!")
		os.Exit(0)
	}
	fmt.Printf("%d changes\n", len(changes))
}
