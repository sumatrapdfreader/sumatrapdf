package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"
	"runtime"
	"strings"
)

/*
The big idea is to start collecting regression tests for bugs we fixed.

Each test is a command we run on a file. A command can be any executable but
for simplicity we'll probably just add more functionality to SumatraPDF.exe.

Test succeeds if the output is same as expected.
*/

var (
	inFatal bool
)

type Test struct {
	Cmd         string
	FileSha1Hex string
	FileURL     string
	StdOut      string
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
		if inFatal {
			os.Exit(1)
		}
		inFatal = true
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

func parseTestsMust(path string) {
	d, err := ioutil.ReadFile(path)
	fataliferr(err)
	lines := toTrimmedLines(d)
	// TODO: finish me
	fmt.Printf("%d lines\n", len(lines))
}

func main() {
	fmt.Printf("regress\n")
	p := filepath.Join("tools", "regress", "tests.txt")
	parseTestsMust(p)
}
