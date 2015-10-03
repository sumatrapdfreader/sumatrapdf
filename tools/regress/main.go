package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
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
	inFatal     bool
	failedTests []*Test
)

type Test struct {
	// values from parsing test file
	CmdUnparsed    string
	FileSha1Hex    string
	FileURL        string
	ExpectedOutput string

	// computed values
	CmdPath  string
	CmdArgs  []string
	FilePath string
	Error    error
	Output   string
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

func parseTestsMust(path string) []*Test {
	var res []*Test
	d, err := ioutil.ReadFile(path)
	fataliferr(err)
	lines := toTrimmedLines(d)
	// TODO: finish me
	fmt.Printf("%d lines\n", len(lines))
	return res
}

func cmdToStrLong2(cmd *exec.Cmd) string {
	arr := []string{`"` + cmd.Path + `"`}
	arr = append(arr, cmd.Args...)
	return strings.Join(arr, " ")
}

func cmdToStrLong(cmd *exec.Cmd) string {
	return strings.Join(cmd.Args, " ")
}

func isOutputEqual(s1, s2 string) bool {
	// TODO: normalize whitespace
	return s1 != s2
}

func runTest(t *Test) {
	for i, arg := range t.CmdArgs {
		if arg == "$file" {
			t.CmdArgs[i] = t.FilePath
		}
	}
	cmd := exec.Command(t.CmdPath, t.CmdArgs...)
	fmt.Printf("Running: %s\n", cmdToStrLong(cmd))
	res, err := cmd.Output()
	t.Output = string(res)
	if err != nil {
		t.Error = err
		failedTests = append(failedTests, t)
		return
	}
	if !isOutputEqual(t.Output, t.ExpectedOutput) {
		failedTests = append(failedTests, t)
		return
	}
	fmt.Printf("test passed, output: %s\n", res)
}

func dumpFailedTest(t *Test) {
	// TODO: write me
	args := strings.Join(t.CmdArgs, " ")
	fmt.Printf("Test %s %s failed\n", t.CmdPath, args)
	if t.Error != nil {
		fmt.Printf("Reason: process exited with error '%s'\n", t.Error)
		return
	}
	if !isOutputEqual(t.Output, t.ExpectedOutput) {
		fmt.Printf(`
Reason: got output:
-----
%s
-----
expected:
-----
%s
-----
`, t.Output, t.ExpectedOutput)
		return
	}
	fmt.Printf("Internal rror: unknown reason\n")
}

func dumpFailedTests() int {
	if len(failedTests) == 0 {
		fmt.Printf("All tests passed!\n")
		return 0
	}
	for _, t := range failedTests {
		dumpFailedTest(t)
	}
	return len(failedTests)
}

func downloadTestFilesMust(tests []*Test) {
	// TODO: write me
}

func runTests(tests []*Test) {
	for _, test := range tests {
		runTest(test)
	}
}

func main() {
	fmt.Printf("regress\n")
	p := filepath.Join("tools", "regress", "tests.txt")
	tests := parseTestsMust(p)
	downloadTestFilesMust(tests)

	t := &Test{
		CmdPath:        "dbg/SumatraPDF.exe",
		CmdArgs:        []string{"-render", "2", "-zoom", "5", "$file"},
		FilePath:       "89a36816f1ab490d46c0c7a6b34b678f72bf.pdf",
		ExpectedOutput: "rendering page 1 for '89a36816f1ab490d46c0c7a6b34b678f72bf.pdf', zoom: 5.00",
	}
	runTest(t)
	//runTests(tests)
	os.Exit(dumpFailedTests())
}
