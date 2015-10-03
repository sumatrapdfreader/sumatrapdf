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

/*
See .clang-fromat to see the style we're using.
We used to use Mozilla style for the base, but they changed the style a lot
between 3.5 and 3.7. Our style is meant to not differ too much from Mozilla 3.5
style (which we used for a bunch of files already)
TODO: bite the bullet and use PointerAlignment: Right ?
We're not consistent about it but currently Left might be more frequent,
possibly due to being in Mozilla 3.5 style. On the other hand in 3.7 all
styles but llvm use Left


List of clang-format formats in 3.5 and 3.7: https://gist.github.com/kjk/298216da8cb4c665075b

Summary of Mozilla clang-format style changes between clang 3.5 and 3.7

Mozilla 3.5
ConstructorInitializerIndentWidth: 4
Standard:        Cpp03
ContinuationIndentWidth: 4
AllowShortFunctionsOnASingleLine: All
AlwaysBreakTemplateDeclarations: false
BreakBeforeBinaryOperators: false
BreakConstructorInitializersBeforeComma: false
ConstructorInitializerAllOnOneLineOrOnePerLine: true
DerivePointerAlignment: true
BreakBeforeBraces: Attach

Mozilla 3.7
ConstructorInitializerIndentWidth: 2
Standard:        Cpp11
ContinuationIndentWidth: 2
AllowShortFunctionsOnASingleLine: Inline
AlwaysBreakTemplateDeclarations: true
BreakBeforeBinaryOperators: None
ConstructorInitializerAllOnOneLineOrOnePerLine: false
DerivePointerAlignment: false
BreakBeforeBraces: Mozilla

Removed between 3.5 and 3.7:
IndentFunctionDeclarationAfterType: false

New in 3.7:
AlignAfterOpenBracket: true
AlignConsecutiveAssignments: false
AlignOperands:   true
AllowShortCaseLabelsOnASingleLine: false
AlwaysBreakAfterDefinitionReturnType: TopLevel
BinPackArguments: true
MacroBlockBegin: ''
MacroBlockEnd:   ''
ObjCBlockIndentWidth: 2
SpaceAfterCStyleCast: false
SpacesInSquareBrackets: false


Differences between Chromium and Mozilla styles in clang 3.7
Mozilla
AccessModifierOffset: -2
AlignEscapedNewlinesLeft: false
AlwaysBreakAfterDefinitionReturnType: TopLevel
AlwaysBreakBeforeMultilineStrings: false
BinPackParameters: true
BreakBeforeBraces: Mozilla
BreakConstructorInitializersBeforeComma: true
ConstructorInitializerAllOnOneLineOrOnePerLine: false
ConstructorInitializerIndentWidth: 2
ContinuationIndentWidth: 2
Cpp11BracedListStyle: false
KeepEmptyLinesAtTheStartOfBlocks: true
MacroBlockBegin: ''
ObjCSpaceAfterProperty: true
PenaltyBreakBeforeFirstCallParameter: 19
SpacesBeforeTrailingComments: 1
Standard:        Cpp11

Chromium
AccessModifierOffset: -1
AlignEscapedNewlinesLeft: true
AlwaysBreakAfterDefinitionReturnType: None
AlwaysBreakBeforeMultilineStrings: true
BinPackParameters: false
BreakBeforeBraces: Attach
BreakConstructorInitializersBeforeComma: false
ConstructorInitializerAllOnOneLineOrOnePerLine: true
ConstructorInitializerIndentWidth: 4
ContinuationIndentWidth: 4
Cpp11BracedListStyle: true
KeepEmptyLinesAtTheStartOfBlocks: false
MacroBlockBegin: '^IPC_END_MESSAGE_MAP$'
ObjCSpaceAfterProperty: false
PenaltyBreakBeforeFirstCallParameter: 1
SpacesBeforeTrailingComments: 2
Standard:        Auto
*/

const ()

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

func formatFileInDirMust(exePath string, dir, file string) {
	// -style=file means: use .clang-format
	cmd := exec.Command(exePath, "-style=file", "-i", file)
	cmd.Dir = dir
	fmt.Printf("Running: '%s'\n", strings.Join(cmd.Args, " "))
	out, err := cmd.CombinedOutput()
	ifCmdFailed(err, out, cmd)
}

func formatFileMust(exePath string, filePath string) {
	dir := filepath.Dir(filePath)
	file := filepath.Base(filePath)
	formatFileInDirMust(exePath, dir, file)
}

func runInDirMust(exePath string, dir string) {
	files := getSrcFilesMust(dir)
	for _, file := range files {
		formatFileInDirMust(exePath, dir, file)
	}
}

func runOnFilesInDirMust(exePath, dir string, files []string) {
	for _, file := range files {
		formatFileInDirMust(exePath, dir, file)
	}
}

func main() {
	var d string
	exePath, err := exec.LookPath("clang-format")
	fataliferr(err)
	fmt.Printf("exe path: %s\n", exePath)
	verifyClangFormatVersion(exePath)

	runOnFilesInDirMust(exePath, "src", []string{
		"ParseCommandLine.h",
		"ParseCommandLine.cpp",
		//"Print.cpp",
		//"Print.h",
	})

	d = filepath.Join("src", "utils")
	runInDirMust(exePath, d)

	d = filepath.Join("src", "mui")
	runInDirMust(exePath, d)

	d = filepath.Join("src", "wingui")
	runInDirMust(exePath, d)
}
