package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"sort"
	"strings"
	"sync"
	"time"
)

/*
To run:
* install Go
 - download and run latest installer http://golang.org/doc/install
 - restart so that PATH changes take place
 - set GOPATH env variable (e.g. to %USERPROFILE%\src\go)
* go run .\tools\buildgo\main.go

Some notes on the insanity that is setting up command-line build for both
32 and 64 bit executables.

Useful references:
https://msdn.microsoft.com/en-us/library/f2ccy3wt.aspx
https://msdn.microsoft.com/en-us/library/x4d2c09s.aspx
http://www.sqlite.org/src/artifact/60dbf6021d3de0a9 -sqlite's win build script

%VS140COMNTOOLS%\vsvars32.bat is how set basic env for 32bit builds for VS 2015
(it's VS120COMNTOOLS for VS 2013).

That sets VSINSTALLDIR env variable which we can use to setup both 32bit and
64bit builds:
%VCINSTALLDIR%\vcvarsall.bat x86_amd64 : 64bit
%VCINSTALLDIR%\vcvarsall.bat x86 : 32bit

If the OS is 64bit, there are also 64bit compilers that can be selected with:
amd64 (for 64bit builds) and amd64_x86 (for 32bit builds). They generate
the exact same code but can compiler bigger programs (can use more memory).

I'm guessing %VS140COMNTOOLS%\vsvars32.bat is the same as %VSINSTALLDIR%\vcvarsall.bat x86.
*/

// Platform represents a 32bit vs 64bit platform
type Platform int

// Config is release, debug etc.
type Config int

const (
	// Platform32Bit describes 32bit build
	Platform32Bit Platform = 1
	// Platform64Bit describes 64bit build
	Platform64Bit Platform = 2

	// ConfigDebug describes debug build
	ConfigDebug Config = 1
	// ConfigRelease describes release build
	ConfigRelease Config = 2
	// ConfigAnalyze describes relase build with /analyze option
	ConfigAnalyze Config = 3
)

// EnvVar describes an environment variable
type EnvVar struct {
	Name string
	Val  string
}

// Args represent arguments to a command to run
type Args struct {
	args []string
}

var (
	cachedVcInstallDir string
	cachedExePaths     map[string]string
	createdDirs        map[string]bool
	fileInfoCache      map[string]os.FileInfo
	alwaysRebuild      bool
	wg                 sync.WaitGroup
	sem                chan bool
	mupdfDir           string
	extDir             string
	zlibDir            string
	freetypeDir        string
	jpegTurboDir       string
	openJpegDir        string
	jbig2Dir           string
	mupdfGenDir        string
	nasmPath           string
	cflagsB            *Args
	cflagsOpt          *Args
	cflags             *Args
	zlibCflags         *Args // TODO: should be cflagsZlib
	freetypeCflags     *Args
	jpegTurboCflags    *Args
	jbig2Cflags        *Args
	openJpegCflags     *Args
	jpegTurboNasmFlags *Args
	mupdfNasmFlags     *Args
	mpudfCflags        *Args
	ldFlags            *Args
	libs               *Args
	ftSrc              []string
	zlibSrc            []string
	jpegTurboSrc       []string
	jpegTurboAsmSrc    []string
	jbig2Src           []string
	openJpegSrc        []string
	mupdfDrawSrc       []string
	mupdfFitzSrc       []string
	mupdfSrc           []string
	muxpsSrc           []string
	mupdfAllSrc        []string
	mudocSrc           []string
	mutoolsSrc         []string
	mutoolAllSrc       []string
	mudrawSrc          []string
)

func (p Platform) is64() bool {
	return p == Platform64Bit
}

func (p Platform) is32() bool {
	return p == Platform32Bit
}

func (a *Args) append(toAppend ...string) *Args {
	return &Args{
		args: strConcat(a.args, toAppend),
	}
}

func NewArgs(args ...string) *Args {
	return &Args{
		args: args,
	}
}

func semEnter() {
	sem <- true
}

func semLeave() {
	<-sem
}

func fatalf(format string, args ...interface{}) {
	fmt.Printf(format, args...)
	os.Exit(1)
}

func pj(elem ...string) string {
	return filepath.Join(elem...)
}

func strConcat(arr1, arr2 []string) []string {
	var res []string
	for _, s := range arr1 {
		res = append(res, s)
	}
	for _, s := range arr2 {
		res = append(res, s)
	}
	return res
}

func replaceExt(path string, newExt string) string {
	ext := filepath.Ext(path)
	return path[0:len(path)-len(ext)] + newExt
}

func fileExists(path string) bool {
	if _, ok := fileInfoCache[path]; !ok {
		fi, err := os.Stat(path)
		if err != nil {
			return false
		}
		fileInfoCache[path] = fi
	}
	fi := fileInfoCache[path]
	return fi.Mode().IsRegular()
}

func createDirCached(dir string) {
	if _, ok := createdDirs[dir]; ok {
		return
	}
	if err := os.MkdirAll(dir, 0644); err != nil {
		fatalf("os.MkdirAll(%s) failed wiht %s\n", dir, err)
	}
}

func getModTime(path string, def time.Time) time.Time {
	if _, ok := fileInfoCache[path]; !ok {
		fi, err := os.Stat(path)
		if err != nil {
			return def
		}
		fileInfoCache[path] = fi
	}
	fi := fileInfoCache[path]
	return fi.ModTime()
}

// maps upper-cased name of env variable to Name/Val
func envToMap(env []string) map[string]*EnvVar {
	res := make(map[string]*EnvVar)
	for _, v := range env {
		if len(v) == 0 {
			continue
		}
		parts := strings.SplitN(v, "=", 2)
		if len(parts) != 2 {

		}
		nameUpper := strings.ToUpper(parts[0])
		res[nameUpper] = &EnvVar{
			Name: parts[0],
			Val:  parts[1],
		}
	}
	return res
}

func getEnvAfterScript(dir, script string) map[string]*EnvVar {
	// TODO: maybe use COMSPEC env variable instead of "cmd.exe" (more robust)
	cmd := exec.Command("cmd.exe", "/c", script+" & set")
	cmd.Dir = dir
	fmt.Printf("Executing: %s in %s\n", cmd.Args, cmd.Dir)
	resBytes, err := cmd.Output()
	if err != nil {
		fmt.Printf("failed with %s\n", err)
		os.Exit(1)
	}
	res := string(resBytes)
	//fmt.Printf("res:\n%s\n", res)
	parts := strings.Split(res, "\n")
	if len(parts) == 1 {
		fmt.Printf("split failed\n")
		fmt.Printf("res:\n%s\n", res)
		os.Exit(1)
	}
	for idx, env := range parts {
		env = strings.TrimSpace(env)
		parts[idx] = env
	}
	return envToMap(parts)
}

func calcEnvAdded(before, after map[string]*EnvVar) map[string]*EnvVar {
	res := make(map[string]*EnvVar)
	for k, afterVal := range after {
		beforeVal := before[k]
		if beforeVal == nil || beforeVal.Val != afterVal.Val {
			res[k] = afterVal
		}
	}
	return res
}

// return value of VCINSTALLDIR env variable after running vsvars32.bat
func getVcInstallDir(toolsDir string) string {
	if cachedVcInstallDir == "" {
		env := getEnvAfterScript(toolsDir, "vsvars32.bat")
		val := env["VCINSTALLDIR"]
		if val == nil {
			fmt.Printf("no 'VCINSTALLDIR' variable in %s\n", env)
			os.Exit(1)
		}
		cachedVcInstallDir = val.Val
	}
	return cachedVcInstallDir
}

func getEnvForVcTools(vcInstallDir, platform string) []string {
	//initialEnv := envToMap(os.Environ())
	afterEnv := getEnvAfterScript(vcInstallDir, "vcvarsall.bat "+platform)
	//return calcEnvAdded(initialEnv, afterEnv)

	var envArr []string
	for _, envVar := range afterEnv {
		v := fmt.Sprintf("%s=%s", envVar.Name, envVar.Val)
		envArr = append(envArr, v)
	}
	return envArr
}

func getEnv32(vcInstallDir string) []string {
	return getEnvForVcTools(vcInstallDir, "x86")
}

func getEnv64(vcInstallDir string) []string {
	return getEnvForVcTools(vcInstallDir, "x86_amd64")
}

func dumpEnv(env map[string]*EnvVar) {
	var keys []string
	for k := range env {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	for _, k := range keys {
		v := env[k]
		fmt.Printf("%s: %s\n", v.Name, v.Val)
	}
}

func getEnv(platform Platform) []string {
	initialEnv := envToMap(os.Environ())
	vs2013 := initialEnv["VS120COMNTOOLS"]
	vs2015 := initialEnv["VS140COMNTOOLS"]
	vsVar := vs2015
	if vsVar == nil {
		vsVar = vs2013
	}
	if vsVar == nil {
		fmt.Printf("VS120COMNTOOLS or VS140COMNTOOLS not set; VS 2013 or 2015 not installed\n")
		os.Exit(1)
	}
	vcInstallDir := getVcInstallDir(vsVar.Val)
	switch platform {
	case Platform32Bit:
		return getEnv32(vcInstallDir)
	case Platform64Bit:
		return getEnv64(vcInstallDir)
	default:
		panic("unknown platform")
	}
}

func getOutDir(platform Platform, config Config) string {
	dir := ""
	switch config {
	case ConfigRelease:
		dir = "rel"
	case ConfigDebug:
		dir = "dbg"
	}
	if platform == Platform64Bit {
		dir += "64"
	}
	return dir
}

// returns true if dst doesn't exist or is older than src or any of the deps
func isOutdated(src, dst string, deps []string) bool {
	if alwaysRebuild {
		return true
	}
	if !fileExists(dst) {
		return true
	}
	dstTime := getModTime(dst, time.Now())
	srcTime := getModTime(src, time.Now())
	if srcTime.Sub(dstTime) > 0 {
		return true
	}
	for _, path := range deps {
		pathTime := getModTime(path, time.Now())
		if srcTime.Sub(pathTime) > 0 {
			return true
		}
	}
	if true {
		fmt.Printf("%s is up to date\n", dst)
	}
	return false
}

func createDirForFileCached(path string) {
	createDirCached(filepath.Dir(path))
}

func lookupInEnvPathUncached(exeName string, env []string) string {
	for _, envVar := range env {
		parts := strings.SplitN(envVar, "=", 2)
		name := strings.ToLower(parts[0])
		if name != "path" {
			continue
		}
		parts = strings.Split(parts[1], ";")
		for _, dir := range parts {
			path := filepath.Join(dir, exeName)
			if fileExists(path) {
				return path
			}
		}
		fatalf("didn't find %s in '%s'\n", exeName, parts[1])
	}
	return ""
}

func lookupInEnvPath(exeName string, env []string) string {
	if _, ok := cachedExePaths[exeName]; !ok {
		cachedExePaths[exeName] = lookupInEnvPathUncached(exeName, env)
		fmt.Printf("found %s as %s\n", exeName, cachedExePaths[exeName])
	}
	return cachedExePaths[exeName]
}

func runExeHelper(exeName string, env []string, args *Args) {
	exePath := lookupInEnvPath(exeName, env)
	cmd := exec.Command(exePath, args.args...)
	cmd.Env = env
	if true {
		args := cmd.Args
		args[0] = exeName
		fmt.Printf("Running %s\n", args)
		args[0] = exePath
	}
	out, err := cmd.CombinedOutput()
	if err != nil {
		fatalf("%s failed with %s, out:\n%s\n", cmd.Args, err, string(out))
	}
}

func runExe(exeName string, env []string, args *Args) {
	semEnter()
	wg.Add(1)
	go func() {
		runExeHelper(exeName, env, args)
		semLeave()
		wg.Done()
	}()
}

func rc(src, dst string, env []string, args *Args) {
	createDirForFileCached(dst)
	extraArgs := []string{
		"/Fo" + dst,
		src,
	}
	args = args.append(extraArgs...)
	runExe("rc.exe", env, args)
}

func cl(src, dst string, env []string, args *Args) {
	if !isOutdated(src, dst, nil) {
		return
	}
	createDirForFileCached(dst)
	extraArgs := []string{
		"/Fo" + dst,
		src,
	}
	args = args.append(extraArgs...)
	runExe("cl.exe", env, args)
}

// given ${dir}/foo.rc, returns ${outDir}/${dir}/foo.rc
func rcOut(src, outDir string) string {
	verifyIsRcFile(src)
	s := filepath.Join(outDir, src)
	return replaceExt(s, ".res")
}

func verifyIsRcFile(path string) {
	s := strings.ToLower(path)
	if strings.HasSuffix(s, ".rc") {
		return
	}
	fatalf("%s should end in '.rc'\n", path)
}

func verifyIsCFile(path string) {
	s := strings.ToLower(path)
	if strings.HasSuffix(s, ".cpp") {
		return
	}
	if strings.HasSuffix(s, ".c") {
		return
	}
	fatalf("%s should end in '.c' or '.cpp'\n", path)
}

func clOut(src, outDir string) string {
	verifyIsCFile(src)
	s := filepath.Join(outDir, src)
	return replaceExt(s, ".obj")
}

func clDir(srcDir string, files []string, outDir string, env []string, args *Args) {
	for _, f := range files {
		src := filepath.Join(srcDir, f)
		dst := clOut(src, outDir)
		cl(src, dst, env, args)
	}
}

func build(platform Platform, config Config) {
	env := getEnv(platform)
	//dumpEnv(env)
	outDir := getOutDir(platform, config)
	createDirCached(outDir)

	rcArgs := []string{
		"/r",
		"/D", "DEBUG",
		"/D", "_DEBUG",
	}
	rcSrc := filepath.Join("src", "SumatraPDF.rc")
	rcDst := rcOut(rcSrc, outDir)
	rc(rcSrc, rcDst, env, &Args{args: rcArgs})

	startArgs := []string{
		"/nologo", "/c",
		"/D", "WIN32",
		"/D", "_WIN32",
		"/D", "WINVER=0x0501",
		"/D", "_WIN32_WINNT=0x0501",
		"/D", "DEBUG",
		"/D", "_DEBUG",
		"/D", "_USING_V110_SDK71_",
		"/GR-",
		"/Zi",
		"/GS",
		"/Gy",
		"/GF",
		"/arch:IA32",
		"/EHs-c-",
		"/MTd",
		"/Od",
		"/RTCs",
		"/RTCu",
		"/WX",
		"/W4",
		"/FS",
		"/wd4100",
		"/wd4127",
		"/wd4189",
		"/wd4428",
		"/wd4324",
		"/wd4458",
		"/wd4838",
		"/wd4800",
		"/Imupdf/include",
		"/Iext/zlib",
		"/Iext/lzma/C",
		"/Iext/libwebp",
		"/Iext/unarr",
		"/Iext/synctex",
		"/Iext/libdjvu",
		"/Iext/CHMLib/src",
		"/Isrc",
		"/Isrc/utils",
		"/Isrc/wingui",
		"/Isrc/mui",
		//fmt.Sprintf("/Fo%s\\sumatrapdf", outDir),
		fmt.Sprintf("/Fd%s\\vc80.pdb", outDir),
	}
	initialClArgs := &Args{
		args: startArgs,
	}
	srcFiles := []string{
		"AppPrefs.cpp",
		"DisplayModel.cpp",
		"CrashHandler.cpp",
		"Favorites.cpp",
		"TextSearch.cpp",
		"SumatraAbout.cpp",
		"SumatraAbout2.cpp",
		"SumatraDialogs.cpp",
		"SumatraProperties.cpp",
		"GlobalPrefs.cpp",
		"PdfSync.cpp",
		"RenderCache.cpp",
		"TextSelection.cpp",
		"WindowInfo.cpp",
		"ParseCOmmandLine.cpp",
		"StressTesting.cpp",
		"AppTools.cpp",
		"AppUtil.cpp",
		"TableOfContents.cpp",
		"Toolbar.cpp",
		"Print.cpp",
		"Notifications.cpp",
		"Selection.cpp",
		"Search.cpp",
		"ExternalViewers.cpp",
		"EbookControls.cpp",
		"EbookController.cpp",
		"Doc.cpp",
		"MuiEbookPageDef.cpp",
		"PagesLayoutDef.cpp",
		"Tester.cpp",
		"Translations.cpp",
		"Trans_sumatra_txt.cpp",
		"Tabs.cpp",
		"FileThumbnails.cpp",
		"FileHistory.cpp",
		"ChmModel.cpp",
		"Caption.cpp",
		"Canvas.cpp",
		"TabInfo.cpp",
	}
	clDir("src", srcFiles, outDir, env, initialClArgs)

	if false {
		regressFiles := []string{
			"Regress.cpp",
		}
		clDir(pj("src", "regress"), regressFiles, outDir, env, initialClArgs)
	}

	srcUtilsFiles := []string{
		"FileUtil.cpp",
		"HttpUtil.cpp",
		"StrUtil.cpp",
		"WinUtil.cpp",
		"GdiPlusUtil.cpp",
		"FileTransactions.cpp",
		"Touch.cpp",
		"TrivialHtmlParser.cpp",
		"HtmlWindow.cpp",
		"DirIter.cpp",
		"BitReader.cpp",
		"HtmlPullParser.cpp",
		"HtmlPrettyPrint.cpp",
		"ThreadUtil.cpp",
		"DebugLog.cpp",
		"DbgHelpDyn.cpp",
		"JsonParser.cpp",
		"TgaReader.cpp",
		"HtmlParserLookup.cpp",
		"ByteOrderDecoder.cpp",
		"CmdLineParser.cpp",
		"UITask.cpp",
		"StrFormat.cpp",
		"Dict.cpp",
		"BaseUtil.cpp",
		"CssParser.cpp",
		"FileWatcher.cpp",
		"CryptoUtil.cpp",
		"StrSlice.cpp",
		"TxtParser.cpp",
		"SerializeTxt.cpp",
		"SquareTreeParser.cpp",
		"SettingsUtil.cpp",
		"WebpReader.cpp",
		"FzImgReader.cpp",
		"ArchUtil.cpp",
		"ZipUtil.cpp",
		"LzmaSimpleArchive.cpp",
		"Dpi.cpp",
	}
	clDir(pj("src", "utils"), srcUtilsFiles, outDir, env, initialClArgs)
}

func initDirs() {
	mupdfDir = "mupdf"
	// we invoke make from inside mupdfDir, so this must be relative to that
	// TODO: fix that
	extDir = pj("..", "ext")
	zlibDir = pj(extDir, "zlib")
	freetypeDir = pj(extDir, "freetype2")
	jpegTurboDir = pj(extDir, "libjpeg-turbo")
	openJpegDir = pj(extDir, "openjpeg")
	jbig2Dir = pj(extDir, "jbig2dec")
	// TODO: this is a build artifact so should go under outDir
	mupdfGenDir = pj(mupdfDir, "generated")
}

func initFlags(platform Platform, config Config) {
	cflagsB = NewArgs("/nologo", "/c")
	if platform.is64() {
		cflagsB.append("/D", "WIN64", "/D", "_WIN64")
	} else {
		cflagsB.append("/D", "WIN32", "/D", "_WIN32")
	}
	if config != ConfigAnalyze {
		// TODO: probably different for vs 2015
		cflagsB = cflagsB.append("/D", "_USING_V110_SDK71_")
	}
	/*
		# /WX  : treat warnings as errors
		# /GR- : disable RTTI
		# /Zi  : enable debug information
		# /GS  : enable security checks
		# /Gy  : separate functions for linker
		# /GF  : enable read-only string pooling
		# /MP  : use muliple processors to speed up compilation
		# Note: /MP not used as it might be causing extreme flakiness on EC2 buildbot
	*/
	// for 64bit don't treat warnings as errors
	// TODO: add an over-ride as STRICT_X64 in makefile
	if !platform.is32() {
		cflagsB = cflagsB.append("/WX")
	}
	cflagsB = cflagsB.append("/GR-", "/Zi", "/GS", "/Gy", "/GF")
	// disable the default /arch:SSE2 for 32-bit builds
	if platform.is32() {
		cflagsB = cflagsB.append("/arch:IA32")
	}
	// /EHs-c- : disable C++ exceptions (generates smaller binaries)
	cflagsB = cflagsB.append("/EHs-c-")
	/*
		# /W4  : bump warnings level from 1 to 4
		# warnings unlikely to be turned off due to false positives from using CrashIf()
		# and various logging functions:
		# 4100 : unreferenced param
		# 4127 : conditional expression is constant
		# 4189 : variable initialized but not referenced
		# warnings that might not make sense to fix:
		# 4428 : universal-character-name encountered in source (prevents using "\u202A" etc.)
		# 4324 : structure was padded due to __declspec(align())
	*/
	cflagsB = cflagsB.append("/W4", "/wd4100", "/wd4127", "/wd4189", "/wd4428", "/wd4324")

	/*
		# Those only need to be disabled in VS 2015
		# 4458 : declaration of '*' hides class member, a new warning in VS 2015
		#        unfortunately it's triggered by SDK 10 Gdiplus headers
		# 4838 : 'conversion from '*' to '*' requires a narrowing conversion
		#        because QITABENT in SDK 10 triggers this
		TODO: only for VS 2015
	*/
	cflagsB = cflagsB.append("/wd4458", "/wd4838")

	/*
		# /Ox  : maximum optimizations
		# /O2  : maximize speed
		# docs say /Ox better, my tests say /O2 better
	*/
	cflagsOpt = cflagsB.append("/O2", "/D", "NDEBUG")

	ldFlags = NewArgs("/nologo", "/DEBUG", "/RELEASE", "/opt:ref", "/opt:icf")
	if platform.is32() {
		ldFlags = ldFlags.append("/MACHINE:X86")
	} else {
		ldFlags = ldFlags.append("/MACHINE:X64")
	}

	if config != ConfigDebug {
		// /GL  : enable link-time code generation
		cflags = cflagsOpt.append("/GL")
		ldFlags = ldFlags.append("/LTCG")
		// /DYNAMICBASE and /NXCOMPAT for better protection against stack overflows
		// http://blogs.msdn.com/vcblog/archive/2009/05/21/dynamicbase-and-nxcompat.aspx
		// We don't use /NXCOMPAT because we need to turn it on/off at runtime
		ldFlags = ldFlags.append("/DYNAMICBASE", "/FIXED:NO")
	} else {
		// /MTd  : statically link debug crt (libcmtd.lib)
		cflagsB = cflagsB.append("/MTd")
		// /RTCs : stack frame runtime checking
		// /RTCu : ununitialized local usage checks
		// /Od   : disable optimizations
		cflags = cflagsB.append("/Od", "/RTCs", "/RTCu")
	}

	if platform.is64() {
		ldFlags = ldFlags.append("/SUBSYSTEM:WINDOWS,5.2")
	} else {
		ldFlags = ldFlags.append("/SUBSYSTEM:WINDOWS,5.1")
	}

	zlibCflags = cflagsOpt.append("/TC", "/wd4131", "/wd4244", "/wd4996", "/I"+zlibDir)

	// TODO:
}

func initSrc() {
	ftSrc = []string{
		"ftbase.c", "ftbbox.c", "ftbitmap.c", "ftgasp.c",
		"ftglyph.c", "ftinit.c", "ftstroke.c", "ftsynth.c",
		"ftsystem.c", "fttype1.c", "ftxf86.c", "cff.c",
		"type1cid.c", "psaux.c", "psnames.c", "smooth.c",
		"sfnt.c", "truetype.c", "type1.c", "raster.c",
		"otvalid.c", "ftotval.c", "pshinter.c", "ftgzip.c",
	}
}

func main() {
	cachedExePaths = make(map[string]string)
	createdDirs = make(map[string]bool)
	fileInfoCache = make(map[string]os.FileInfo)
	initDirs()
	initFlags(Platform64Bit, ConfigRelease)
	initSrc()
	n := runtime.NumCPU()
	fmt.Printf("Using %d goroutines\n", n)
	sem = make(chan bool, n)
	timeStart := time.Now()
	build(Platform32Bit, ConfigRelease)
	wg.Wait()
	fmt.Printf("total time: %s\n", time.Since(timeStart))
}
