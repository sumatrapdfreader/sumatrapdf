package main

import (
	"flag"
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
	"time"
)

type BuildContext struct {
	OutDir string

	CcCmd    string
	CDefines []string
	IncDirs  []string
	CFlags   []string // for C files
	CxxFlags []string // for C++ files

	ArCmd   string
	ArFlags []string

	LinkCmd   string
	LinkFlags []string

	CcOutputs []string

	// must be pointer to preserve idnetity even when we copy BuildContext by value
	Wg *sync.WaitGroup
	Mu *sync.Mutex
}

var (
	echo                   = true // TODO: make them part of BuildContext?
	showTimings            = true // TODO: make them part of BuildContext?
	silentIfSkipping       = true
	flgClean               bool
	flgRelease             bool
	flgReleaseSanitizeAddr bool
	flgReleaseSanitizeMem  bool
	flgClang               bool

	jobLimiter     chan bool
	useConcurrency = true

	gBuildContextMutex     sync.Mutex
	gBuildContextWaitGroup sync.WaitGroup
)

func DefaultBuildContext() BuildContext {
	cc := "gcc"
	if flgClang {
		cc = "clang-5.0"
	}
	ctx := BuildContext{
		CcCmd:     cc,
		CFlags:    []string{"-g", "-O0", "-Wall", "-Werror"},
		CxxFlags:  []string{"-fno-exceptions", "-fno-rtti", "-std=c++1z"},
		CDefines:  []string{"DEBUG"},
		ArCmd:     "ar",
		ArFlags:   []string{},
		LinkCmd:   cc,
		LinkFlags: []string{"-g"},
		IncDirs:   []string{"ext/unarr", "src/utils"},
		OutDir:    "linux_dbg64",
		Wg:        &gBuildContextWaitGroup,
		Mu:        &gBuildContextMutex,
	}
	if !flgClang {
		// only in gcc
		ctx.LinkFlags = append(ctx.LinkFlags, "-static-libstdc++")
	}
	return ctx
}

func DefaultReleaseBuildContext() BuildContext {
	cc := "gcc"
	if flgClang {
		cc = "clang-5.0"
	}
	ctx := BuildContext{
		CcCmd:     cc,
		CFlags:    []string{"-g", "-Os", "-Wall", "-Werror"},
		CxxFlags:  []string{"-fno-exceptions", "-fno-rtti", "-std=c++1z"},
		CDefines:  []string{"NDEBUG"},
		ArCmd:     "ar",
		ArFlags:   []string{},
		LinkCmd:   cc,
		LinkFlags: []string{"-g"},
		IncDirs:   []string{"ext/unarr", "src/utils"},
		OutDir:    "linux_rel64",
		Wg:        &gBuildContextWaitGroup,
		Mu:        &gBuildContextMutex,
	}
	if !flgClang {
		// only in gcc
		ctx.LinkFlags = append(ctx.LinkFlags, "-static-libstdc++")
	}
	return ctx
}

func DefaultReleaseSanitizeAddressBuildContext() BuildContext {
	cc := "gcc"
	if flgClang {
		cc = "clang-5.0"
	}
	ctx := BuildContext{
		CcCmd:     cc,
		CFlags:    []string{"-g", "-Os", "-Wall", "-Werror", "-fsanitize=address", "-fno-omit-frame-pointer"},
		CxxFlags:  []string{"-fno-exceptions", "-fno-rtti", "-std=c++1z"},
		CDefines:  []string{"NDEBUG"},
		ArCmd:     "ar",
		ArFlags:   []string{},
		LinkCmd:   cc,
		LinkFlags: []string{"-g", "-fsanitize=address"},
		IncDirs:   []string{"ext/unarr", "src/utils"},
		OutDir:    "linux_relSanitizeAddr64",
		Wg:        &gBuildContextWaitGroup,
		Mu:        &gBuildContextMutex,
	}
	if flgClang {
		ctx.OutDir = "linux_relClangSanitizeAddr64"
	}
	if !flgClang {
		// only in gcc
		ctx.LinkFlags = append(ctx.LinkFlags, "-static-libstdc++")
	}
	return ctx
}

func DefaultReleaseSanitizeMemoryBuildContext() BuildContext {
	cc := "gcc"
	if flgClang {
		cc = "clang-5.0"
	}
	ctx := BuildContext{
		CcCmd:     cc,
		CFlags:    []string{"-g", "-Os", "-Wall", "-Werror", "-fsanitize=memory", "-fno-omit-frame-pointer"},
		CxxFlags:  []string{"-fno-exceptions", "-fno-rtti", "-std=c++1z"},
		CDefines:  []string{"NDEBUG"},
		ArCmd:     "ar",
		ArFlags:   []string{},
		LinkCmd:   cc,
		LinkFlags: []string{"-g", "-fsanitize=memory"},
		IncDirs:   []string{"ext/unarr", "src/utils"},
		OutDir:    "linux_relSanitizeMem64",
		Wg:        &gBuildContextWaitGroup,
		Mu:        &gBuildContextMutex,
	}
	if flgClang {
		ctx.OutDir = "linux_relClangSanitizeMem64"
		ctx.LinkFlags = append(ctx.LinkFlags, "-lstdc++")
	}
	if !flgClang {
		// only in gcc
		ctx.LinkFlags = append(ctx.LinkFlags, "-static-libstdc++")
	}
	return ctx
}

func dupStrArray(a []string) []string {
	return append([]string{}, a...)
}

// GetCopy creates a deep copy of BuildContext. We must duplicate arrays
// manually, to prevent sharing with source
func (c *BuildContext) GetCopy(wg *sync.WaitGroup) BuildContext {
	res := *c
	res.CDefines = dupStrArray(res.CDefines)
	res.CDefines = dupStrArray(res.CDefines)
	res.IncDirs = dupStrArray(res.IncDirs)
	res.CFlags = dupStrArray(res.CFlags)
	res.CxxFlags = dupStrArray(res.CxxFlags)
	res.ArFlags = dupStrArray(res.ArFlags)
	res.LinkFlags = dupStrArray(res.LinkFlags)
	res.CcOutputs = []string{}
	res.Wg = wg
	return res
}

func (c *BuildContext) Lock() {
	c.Mu.Lock()
}

func (c *BuildContext) Unlock() {
	c.Mu.Unlock()
}

func (c *BuildContext) InOutDir(name string) string {
	return filepath.Join(c.OutDir, name)
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
func needsRebuild(dstPath string, srcPaths ...string) bool {
	dstStat, err := os.Lstat(dstPath)
	// need to rebuild if dstPath doesn't exist
	if err != nil {
		return true
	}
	for _, srcPath := range srcPaths {
		srcStat, err := os.Lstat(srcPath)
		panicIf(err != nil, "")
		isOlder := srcStat.ModTime().After(dstStat.ModTime())
		if isOlder {
			return true
		}
	}
	return false
}

func runCmd(ctx *BuildContext, cmd *exec.Cmd) {
	ctx.Wg.Add(1)
	jobLimiter <- true
	go func() {
		timeStart := time.Now()
		out, err := cmd.CombinedOutput()
		if echo {
			if showTimings {
				fmt.Printf("%s in %s\n", strings.Join(cmd.Args, " "), time.Since(timeStart))
			} else {
				fmt.Printf("%s", strings.Join(cmd.Args, " "))
			}
			if len(out) > 0 {
				fmt.Printf("%s\n", string(out))
			}
		}
		panicIfErr(err)
		<-jobLimiter
		ctx.Wg.Done()
	}()
}

func link(ctx *BuildContext, dstPath string, srcPaths []string) {
	args := dupStrArray(ctx.LinkFlags)
	args = append(args, "-o", dstPath)
	args = append(args, srcPaths...)
	cmd := exec.Command(ctx.LinkCmd, args...)

	createDirForFile(dstPath)
	if !needsRebuild(dstPath, srcPaths...) {
		if echo && !silentIfSkipping {
			fmt.Printf("skipping '%s' because output already exists\n", strings.Join(cmd.Args, " "))
		}
		return
	}

	runCmd(ctx, cmd)
}

func ar(ctx *BuildContext, dstPath string, srcPaths []string) {
	args := append([]string{"cr"}, ctx.ArFlags...)
	args = append(args, dstPath)
	args = append(args, srcPaths...)
	cmd := exec.Command(ctx.ArCmd, args...)

	createDirForFile(dstPath)
	if !needsRebuild(dstPath, srcPaths...) {
		if echo && !silentIfSkipping {
			fmt.Printf("skipping '%s' because output already exists\n", strings.Join(cmd.Args, " "))
		}
		return
	}

	runCmd(ctx, cmd)
}

func isCFile(path string) bool {
	ext := strings.ToLower(filepath.Ext(path))
	return ext == ".c"
}

func isCxxFile(path string) bool {
	ext := strings.ToLower(filepath.Ext(path))
	return ext == ".cpp" || ext == ".cc" || ext == ".cxx"
}

func isCOrCxxFile(path string) bool {
	ext := strings.ToLower(filepath.Ext(path))
	return ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c"
}

func cc(ctx *BuildContext, srcPath string) {
	srcName := filepath.Base(srcPath)
	dstName := replaceExt(srcName, ".o")
	dstPath := filepath.Join(ctx.OutDir, dstName)
	ctx.CcOutputs = append(ctx.CcOutputs, dstPath)

	args := dupStrArray(ctx.CFlags)
	panicIf(!isCOrCxxFile(srcPath), fmt.Sprintf("%s is not C or C++ file", srcPath))
	if isCxxFile(srcPath) {
		args = append(args, ctx.CxxFlags...)
	}

	for _, incDir := range ctx.IncDirs {
		verifyFileExists(incDir)
		args = append(args, "-I"+incDir)
	}
	for _, def := range ctx.CDefines {
		args = append(args, "-D"+def)
	}

	args = append(args, "-c")
	args = append(args, "-o", dstPath, srcPath)
	cmd := exec.Command(ctx.CcCmd, args...)

	if !needsRebuild(dstPath, srcPath) {
		if echo && !silentIfSkipping {
			fmt.Printf("skipping '%s' because output already exists\n", strings.Join(cmd.Args, " "))
		}
		return
	}

	createDirForFile(dstPath)

	runCmd(ctx, cmd)
}

func normalizePath(s string) string {
	// convert to unix path
	s = strings.Replace(s, "\\", "/", -1)
	pathSepStr := string(byte(os.PathSeparator))
	if pathSepStr != "/" {
		s = strings.Replace(s, "/", pathSepStr, -1)
	}
	return s
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

func parseFlags() {
	flag.BoolVar(&flgClean, "clean", false, "if true, do a clean build")
	flag.BoolVar(&flgRelease, "release", false, "if true, make release build")
	flag.BoolVar(&flgReleaseSanitizeMem, "release-sanitize-mem", false, "if true, make release sanitize memory build")
	flag.BoolVar(&flgReleaseSanitizeAddr, "release-sanitize-addr", false, "if true, make release sanitize address build")
	flag.BoolVar(&flgClang, "clang", false, "if true, use clang instead of gcc")
	flag.Parse()
}

func cFilesInDir(dir string) []string {
	dir = normalizePath(dir)
	fileInfos, err := ioutil.ReadDir(dir)
	panicIfErr(err)
	var res []string
	for _, fi := range fileInfos {
		path := filepath.Join(dir, fi.Name())
		if isCFile(path) {
			res = append(res, path)
		}
	}
	return res
}

func unarrMoLzmaFiles() []string {
	files := cFilesInDir("ext/unarr/common")
	files = append(files, cFilesInDir("ext/unarr/rar")...)
	files = append(files, cFilesInDir("ext/unarr/zip")...)
	files = append(files, cFilesInDir("ext/unarr/tar")...)
	files = append(files, cFilesInDir("ext/unarr/_7z")...)
	files = append(files, "ext/unarr/lzmasdk/LzmaDec.c", "ext/bzip2/bzip_all.c")
	files = append(files, filesInDir("ext/unarr/lzmasdk", "CpuArch.c", "Ppmd7.c", "Ppmd7Dec.c", "Ppmd8.c", "Ppmd8Dec.c")...)
	return files
}

func builUnarrArchive(ctx *BuildContext) string {
	archivePath := ctx.InOutDir("unarr.a")

	var localWg sync.WaitGroup
	localCtx := ctx.GetCopy(&localWg)
	localCtx.CDefines = append(localCtx.CDefines, "HAVE_ZLIB", "HAVE_BZIP2", "BZ_NO_STDIO")
	localCtx.CFlags = append(localCtx.CFlags, "-Wno-implicit-function-declaration")
	if !flgClang {
		localCtx.CFlags = append(localCtx.CFlags, "-Wno-unused-but-set-variable")
	}
	localCtx.IncDirs = append(localCtx.IncDirs, "ext/zlib", "ext/bzip2")
	localCtx.OutDir = filepath.Join(normalizePath(localCtx.OutDir), "unarr")

	ccMulti(&localCtx, unarrMoLzmaFiles()...)
	localCtx.Wg.Wait()

	ar(ctx, archivePath, localCtx.CcOutputs)
	return archivePath
}

func zlibFiles() []string {
	return filesInDir("ext/zlib", "adler32.c", "compress.c", "crc32.c", "deflate.c", "inffast.c", "inflate.c", "inftrees.c", "trees.c", "zutil.c", "gzlib.c", "gzread.c", "gzwrite.c", "gzclose.c")
}

func buildZlibArchive(ctx *BuildContext) string {
	archivePath := ctx.InOutDir("zlib.a")

	var localWg sync.WaitGroup
	localCtx := ctx.GetCopy(&localWg)
	localCtx.CFlags = append(localCtx.CFlags, "-Wno-implicit-function-declaration")
	if flgClang {
		localCtx.CFlags = append(localCtx.CFlags, "-Wno-shift-negative-value")
	}
	localCtx.OutDir = filepath.Join(normalizePath(localCtx.OutDir), "zlib")

	ccMulti(&localCtx, zlibFiles()...)
	localCtx.Wg.Wait()

	ar(ctx, archivePath, localCtx.CcOutputs)
	return archivePath
}

func buildTestUnixFiles(ctx *BuildContext) []string {
	var localWg sync.WaitGroup
	localCtx := ctx.GetCopy(&localWg)
	localCtx.OutDir = filepath.Join(normalizePath(localCtx.OutDir), "test_unix_obj")
	files := filesInDir("src/utils", "Archive.cpp", "BaseUtil.cpp", "FileUtil.cpp", "StrSlice.cpp", "StrUtil.cpp", "StrUtil_unix.cpp", "TxtParser.cpp", "UtAssert.cpp")
	ccMulti(&localCtx, files...)
	cc(&localCtx, "tools/test_unix/main.cpp")
	localCtx.Wg.Wait()
	return localCtx.CcOutputs
}

func clean() {
	fileInfos, err := ioutil.ReadDir(".")
	panicIfErr(err)
	for _, fi := range fileInfos {
		if !fi.IsDir() {
			continue
		}
		if strings.HasPrefix(fi.Name(), "linux_") {
			os.RemoveAll(fi.Name())
		}
	}
}

func main() {
	nProcs := runtime.GOMAXPROCS(-1)
	jobLimiter = make(chan bool, nProcs)

	parseFlags()
	fmt.Printf("Building for linux, using %d processors\n", nProcs)
	unitTests()
	if flgClean {
		clean()
	}

	ctx := DefaultBuildContext()
	if flgRelease {
		ctx = DefaultReleaseBuildContext()
	} else if flgReleaseSanitizeMem {
		ctx = DefaultReleaseSanitizeMemoryBuildContext()
	} else if flgReleaseSanitizeAddr {
		ctx = DefaultReleaseSanitizeAddressBuildContext()
	}
	var wg sync.WaitGroup
	ctx.Wg = &wg

	timeStart := time.Now()
	zlibArchive := buildZlibArchive(&ctx)
	unarrAchive := builUnarrArchive(&ctx)
	testUnixFiles := buildTestUnixFiles(&ctx)

	linkInputs := dupStrArray(testUnixFiles)
	linkInputs = append(linkInputs, unarrAchive, zlibArchive)
	dstPath := filepath.Join(ctx.OutDir, "test_unix")
	link(&ctx, dstPath, linkInputs)
	wg.Wait()

	fmt.Printf("completed in %s\n", time.Since(timeStart))
}
