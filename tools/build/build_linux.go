package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"
)

type BuildContext struct {
	Cc       string
	CxxFlags []string
	IncDirs  []string
	OutDir   string

	CcOutputs []string
}

var (
	echo        = true // TODO: make them part of BuildContext?
	showTimings = true // TODO: make them part of BuildContext?
	clean       = true
)

func DefaultBuildContext() BuildContext {
	extUnarrIncludeDir := filepath.Join("ext", "unarr")
	srcUtilsIncludeDir := filepath.Join("src", "utils")
	return BuildContext{
		Cc:       "gcc",
		CxxFlags: []string{"-Wall", "-g", "-fno-exceptions", "-fno-rtti", "-std=c++1z"},
		IncDirs:  []string{extUnarrIncludeDir, srcUtilsIncludeDir},
		OutDir:   filepath.Join("linux_dbg64"),
	}
}

func panicIfErr(err error) {
	if err != nil {
		panic(err.Error())
	}
}

func panicIf(cond bool, msg string) {
	if cond {
		panic(msg)
	}
}

func verifyFileExists(path string) {
	_, err := os.Lstat(path)
	panicIf(err != nil, fmt.Sprintf("file '%s' doesn't exist", path))
}

func replaceExt(s string, newExt string) string {
	ext := filepath.Ext(s)
	n := len(s)
	nExt := len(ext)
	return s[:n-nExt] + newExt
}

func createDirForFile(filePath string) {
	dir := filepath.Dir(filePath)
	err := os.MkdirAll(dir, 0755)
	panicIfErr(err)
}

// for dependency checking: return true if srcPath needs to be rebuild based
// on state of dstPath i.e. dstPath doesn't exist or dstPath is older than srcPath
func needsRebuild(srcPath string, dstPath string) bool {
	dstStat, err := os.Lstat(dstPath)
	// need to rebuild if dstPath doesn't exist
	if err != nil {
		return true
	}
	srcStat, err := os.Lstat(srcPath)
	panicIf(err != nil, "")
	return srcStat.ModTime().After(dstStat.ModTime())
}

func cc(ctx *BuildContext, srcPath string) {
	srcName := filepath.Base(srcPath)
	dstName := replaceExt(srcName, ".o")
	dstPath := filepath.Join(ctx.OutDir, dstName)

	ctx.CcOutputs = append(ctx.CcOutputs, dstPath)

	args := append([]string{}, ctx.CxxFlags...)
	for _, incDir := range ctx.IncDirs {
		verifyFileExists(incDir)
		args = append(args, "-I"+incDir)
	}
	args = append(args, "-c")
	args = append(args, "-o", dstPath, srcPath)
	cmd := exec.Command("gcc", args...)

	skip := !needsRebuild(srcPath, dstPath)
	if skip {
		if echo {
			fmt.Printf("skipping '%s' because output already exists\n", strings.Join(cmd.Args, " "))
		}
		return
	}

	createDirForFile(dstPath)
	if echo && !showTimings {
		fmt.Printf("%s\n", strings.Join(cmd.Args, " "))
	}
	timeStart := time.Now()
	out, err := cmd.CombinedOutput()
	if echo && showTimings {
		fmt.Printf("%s in %s\n", strings.Join(cmd.Args, " "), time.Since(timeStart))
	}
	if echo && len(out) > 0 {
		fmt.Printf("%s\n", string(out))
	}
	panicIfErr(err)
}

func filesInDir(dir string, files ...string) []string {
	var res []string
	for _, file := range files {
		path := filepath.Join(dir, file)
		verifyFileExists(path)
		res = append(res, path)
	}
	return res
}

func unitTests() {
	s := replaceExt("foo.cc", ".o")
	panicIf(s != "foo.o", fmt.Sprintf("expected 'foo.o', got '%s'", s))
	s = replaceExt("bar", ".o")
	panicIf(s != "bar.o", fmt.Sprintf("expected 'bar.o', got '%s'", s))
}

func ccMulti(ctx *BuildContext, srcPaths ...string) {
	for _, srcPath := range srcPaths {
		cc(ctx, srcPath)
	}
}

func main() {
	fmt.Printf("Building for linux\n")
	unitTests()
	if clean {
		os.RemoveAll("linux_dbg64")
		os.RemoveAll("linux_rel64")
	}

	ctx := DefaultBuildContext()
	timeStart := time.Now()
	files := filesInDir(filepath.Join("src", "utils"), "Archive.cpp", "BaseUtil.cpp", "FileUtil.cpp", "StrUtil.cpp", "UtAssert.cpp")
	ccMulti(&ctx, files...)
	cc(&ctx, "tools/test_unix/main.cpp")
	fmt.Printf("completed in %s\n", time.Since(timeStart))
}
