package main

import (
	"archive/zip"
	"bufio"
	"bytes"
	"flag"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/andybalholm/brotli"
	"github.com/kjk/common/u"
	"github.com/klauspost/compress/zstd"
	"github.com/ulikunitz/xz/lzma"
)

var (
	r2Access          string
	r2Secret          string
	b2Access          string
	b2Secret          string
	transUploadSecret string
)

func loadSecrets() bool {
	var m map[string]string
	panicIf(!u.IsWinOrMac(), "secretsEnv is empty and running on linux")
	secretsSrcPath := filepath.Join("..", "secrets", "sumatrapdf.env")
	d, err := os.ReadFile(secretsSrcPath)
	if err != nil {
		logf("Failed to read secrets from %s, will try env variables\n", secretsSrcPath)
		return false
	}
	m = u.ParseEnvMust(d)

	getEnv := func(key string, val *string, minLen int) {
		v := strings.TrimSpace(m[key])
		if len(v) < minLen {
			logf("Missing %s, len: %d, wanted: %d\n", key, len(v), minLen)
			return
		}
		*val = v
		// logf("Got %s, '%s'\n", key, v)
		logf("Got %s\n", key)
	}
	getEnv("R2_ACCESS", &r2Access, 8)
	getEnv("R2_SECRET", &r2Secret, 8)
	getEnv("BB_ACCESS", &b2Access, 8)
	getEnv("BB_SECRET", &b2Secret, 8)
	getEnv("TRANS_UPLOAD_SECRET", &transUploadSecret, 4)
	return true
}

func ensureAllUploadCreds() {
	panicIf(r2Access == "", "Not uploading to s3 because R2_ACCESS env variable not set\n")
	panicIf(r2Secret == "", "Not uploading to s3 because R2_SECRET env variable not set\n")
	panicIf(b2Access == "", "Not uploading to backblaze because BB_ACCESS env variable not set\n")
	panicIf(b2Secret == "", "Not uploading to backblaze because BB_SECRET env variable not set\n")
}

func getSecrets() {
	if loadSecrets() {
		return
	}
	r2Access = os.Getenv("R2_ACCESS")
	r2Secret = os.Getenv("R2_SECRET")
	b2Access = os.Getenv("BB_ACCESS")
	b2Secret = os.Getenv("BB_SECRET")
	transUploadSecret = os.Getenv("TRANS_UPLOAD_SECRET")
}

func regenPremake() {
	premakePath := filepath.Join("bin", "premake5.exe")
	{
		cmd := exec.Command(premakePath, "vs2022")
		runCmdLoggedMust(cmd)
	}

	{
		cmd := exec.Command(premakePath, "--with-clang", "vs2022")
		runCmdLoggedMust(cmd)
	}
}

func openForAppend(name string) (*os.File, error) {
	return os.OpenFile(name, os.O_WRONLY|os.O_CREATE|os.O_APPEND, 0666)
}

func runCmdShowProgressAndLog(cmd *exec.Cmd, path string) error {
	f, err := openForAppend(path)
	must(err)
	defer f.Close()

	cmd.Stdout = io.MultiWriter(f, os.Stdout)
	cmd.Stderr = io.MultiWriter(f, os.Stderr)
	logf("> %s\n", fmtCmdShort(*cmd))
	return cmd.Run()
}

const (
	cppcheckLogFile = "cppcheck.out.txt"
)

func detectCppcheckExe() string {
	// TODO: better detection logic
	path := `c:\Program Files\Cppcheck\cppcheck.exe`
	if pathExists(path) {
		return path
	}
	return "cppcheck.exe"
}

func runCppCheck(all bool) {
	// -q : quiet, doesn't print progress report
	// -v : prints more info about the error
	// --platform=win64 : sets platform to 64 bits
	// -DWIN32 -D_WIN32 -D_MSC_VER=1990 : set some defines and speeds up
	//    checking because cppcheck doesn't check all possible combinations
	// --inline-suppr: honor suppression comments in the code like:
	// // cppcheck-suppress <type>
	// ... line with a problem
	var cmd *exec.Cmd

	// TODO: not sure if adding Windows SDK include path helps.
	// It takes a lot of time and doesn't seem to provide value
	//winSdkIncludeDir := `C:\Program Files (x86)\Windows Kits\10\Include\10.0.18362.0\um`
	// "-I", winSdkIncludeDir
	// "-D__RPCNDR_H_VERSION__=440"
	// STDMETHODIMP_(type)=type

	args := []string{"--platform=win64", "-DWIN32", "-D_WIN32", "-D_MSC_VER=1800", "-D_M_X64", "-DIFACEMETHODIMP_(x)=x", "-DSTDMETHODIMP_(x)=x", "-DSTDAPI_(x)=x", "-DPRE_RELEASE_VER=3.4", "-q", "-v"}
	if all {
		args = append(args, "--enable=style")
		args = append(args, "--suppress=constParameter")
		// they are just fine
		args = append(args, "--suppress=cstyleCast")
		// we minimize use of STL
		args = append(args, "--suppress=useStlAlgorithm")
		// trying to make them explicit has cascading side-effects
		args = append(args, "--suppress=noExplicitConstructor")
		args = append(args, "--suppress=variableScope")
		args = append(args, "--suppress=memsetClassFloat")
		// mostly from log() calls
		args = append(args, "--suppress=ignoredReturnValue")
		// complains about: char* x; float* y = (float*)x;
		// all false positives, can't write in a way that doesn't trigger warning
		args = append(args, "--suppress=invalidPointerCast")
		// all false postives, gets confused by MAKEINTRESOURCEW() and wcschr
		// using auto can fix MAKEINTRESOURCEW(), casting wcschr but it's just silly
		args = append(args, "--suppress=AssignmentIntegerToAddress")
		// I often use global variables set at compile time to
		// control paths to take
		args = append(args, "--suppress=knownConditionTrueFalse")
		args = append(args, "--suppress=constParameterPointer")
		args = append(args, "--suppress=constVariablePointer")
		args = append(args, "--suppress=constVariableReference")
		args = append(args, "--suppress=constParameterReference")
		args = append(args, "--suppress=useInitializationList")
		args = append(args, "--suppress=duplInheritedMember")
		args = append(args, "--suppress=unusedStructMember")
		args = append(args, "--suppress=CastIntegerToAddressAtReturn")
		args = append(args, "--suppress=uselessOverride") // false positive
	}
	args = append(args, "--check-level=exhaustive", "--inline-suppr", "-I", "src", "-I", "src/utils", "src")
	cppcheckExe := detectCppcheckExe()
	cmd = exec.Command(cppcheckExe, args...)
	os.Remove(cppcheckLogFile)
	err := runCmdShowProgressAndLog(cmd, cppcheckLogFile)
	must(err)
	logf("\nLogged output to '%s'\n", cppcheckLogFile)
}

type BuildOptions struct {
	upload                    bool
	verifyTranslationUpToDate bool
	doCleanCheck              bool
	releaseBuild              bool
}

func ensureBuildOptionsPreRequesites(opts *BuildOptions) {
	logf("upload: %v\n", opts.upload)
	logf("verifyTranslationUpToDate: %v\n", opts.verifyTranslationUpToDate)

	if opts.upload {
		ensureAllUploadCreds()
	}

	if opts.verifyTranslationUpToDate {
		verifyTranslationsMust()
	}
	if opts.doCleanCheck {
		panicIf(!isGitClean(""), "git has unsaved changes\n")
	}
	if opts.releaseBuild {
		verifyOnReleaseBranchMust()
		os.RemoveAll("out")
	}
}

func main() {
	logf("Current directory: %s\n", currDirAbsMust())
	timeStart := time.Now()
	defer func() {
		logf("Finished in %s\n", time.Since(timeStart))
	}()

	// ad-hoc flags to be set manually (to show less options)
	var (
		flgBuildLzsa              = false
		flgClangTidy              = false
		flgClangTidyFix           = false
		flgCppCheck               = false
		flgCppCheckAll            = false
		flgFindLargestFilesByExt  = false
		flgGenTranslationsInfoCpp = false
		flgPrintBuildNo           = false
	)

	var (
		flgBuildLogview    bool
		flgBuildNo         int
		flgBuildPreRelease bool
		flgBuildRelease    bool
		flgBuildSmoke      bool
		flgBuildCodeQL     bool
		flgCheckAccessKeys bool
		flgCIBuild         bool
		flgCIDailyBuild    bool
		flgClangFormat     bool
		flgClean           bool
		flgDiff            bool
		flgExtractUtils    bool
		flgFilesList       bool
		flgFileUpload      string
		flgGenDocs         bool
		flgGenSettings     bool
		flgGenWebsiteDocs  bool
		flgLogView         bool
		flgRegenPremake    bool
		flgRunTests        bool
		flgTransDownload   bool
		flgTriggerCodeQL   bool
		flgUpdateGoDeps    bool
		flgUpdateVer       string
		flgUpload          bool
		flgWc              bool
	)

	{
		flag.StringVar(&flgFileUpload, "file-upload", "", "upload a test file to s3 / spaces")
		flag.BoolVar(&flgFilesList, "files-list", false, "list uploaded files in s3 / spaces")
		flag.BoolVar(&flgRegenPremake, "premake", false, "regenerate premake*.lua files")
		flag.BoolVar(&flgCIBuild, "ci", false, "run CI steps")
		flag.BoolVar(&flgCIDailyBuild, "ci-daily", false, "run CI daily steps")
		flag.BoolVar(&flgBuildSmoke, "build-smoke", false, "run smoke build (installer for 64bit release)")
		flag.BoolVar(&flgBuildPreRelease, "build-pre-rel", false, "build pre-release")
		flag.BoolVar(&flgBuildRelease, "build-release", false, "build release")
		flag.BoolVar(&flgBuildCodeQL, "build-codeql", false, "build for codeql")
		//flag.BoolVar(&flgBuildLzsa, "build-lzsa", false, "build MakeLZSA.exe")
		flag.BoolVar(&flgUpload, "upload", false, "upload the build to s3 and do spaces")
		flag.BoolVar(&flgClangFormat, "format", false, "format source files with clang-format")
		flag.BoolVar(&flgWc, "wc", false, "show loc stats (like wc -l)")
		flag.BoolVar(&flgTransDownload, "trans-dl", false, "download latest translations to translations/translations.txt")
		//flag.BoolVar(&flgGenTranslationsInfoCpp, "trans-gen-info", false, "generate src/TranslationLangs.cpp")
		flag.BoolVar(&flgClean, "clean", false, "clean the build (remove out/ files except for settings)")
		flag.BoolVar(&flgCheckAccessKeys, "check-access-keys", false, "check access keys for menu items")
		//flag.BoolVar(&flgPrintBuildNo, "build-no", false, "print build number")
		flag.BoolVar(&flgTriggerCodeQL, "trigger-codeql", false, "trigger codeql build")
		flag.BoolVar(&flgCppCheck, "cppcheck", false, "run cppcheck (must be installed)")
		flag.BoolVar(&flgCppCheckAll, "cppcheck-all", false, "run cppcheck with more checks (must be installed)")
		//flag.BoolVar(&flgClangTidy, "clang-tidy", false, "run clang-tidy (must be installed)")
		//flag.BoolVar(&flgClangTidyFix, "clang-tidy-fix", false, "run clang-tidy (must be installed)")
		flag.BoolVar(&flgDiff, "diff", false, "preview diff using winmerge")
		flag.BoolVar(&flgGenSettings, "gen-settings", false, "re-generate src/Settings.h")
		flag.StringVar(&flgUpdateVer, "update-auto-update-ver", "", "update version used for auto-update checks")
		flag.BoolVar(&flgLogView, "logview", false, "run logview")
		flag.BoolVar(&flgRunTests, "run-tests", false, "run test_util executable")
		flag.BoolVar(&flgExtractUtils, "extract-utils", false, "extract utils")
		flag.BoolVar(&flgBuildLogview, "build-logview", false, "build logview-win. Use -upload to also upload it to backblaze")
		flag.IntVar(&flgBuildNo, "build-no-info", 0, "print build number info for given build number")
		flag.BoolVar(&flgUpdateGoDeps, "update-go-deps", false, "update go dependencies")
		flag.BoolVar(&flgGenDocs, "gen-docs", false, "generate html docs in docs/www from markdown in docs/md")
		flag.BoolVar(&flgGenWebsiteDocs, "gen-docs-website", false, "generate html docs in ../sumatra-website repo and check them in")
		flag.Parse()
	}

	if false {
		genHTMLDocsForApp()
		// genTranslationInfoCpp()
		return
	}

	if flgGenDocs {
		genHTMLDocsForApp()
		return
	}

	if flgGenWebsiteDocs {
		genHTMLDocsForWebsite()
		return
	}

	if flgExtractUtils {
		extractUtils(flgCIBuild)
		return
	}

	if flgUpdateGoDeps {
		defer measureDuration()()
		u.UpdateGoDeps("do", true)
		u.UpdateGoDeps(filepath.Join("tools", "regress"), true)
		u.UpdateGoDeps(filepath.Join("tools", "logview-cli"), true)
		u.UpdateGoDeps(filepath.Join("tools", "logview"), true)
		return
	}

	if flgClangFormat {
		clangFormatFiles()
		return
	}

	if flgCppCheck || flgCppCheckAll {
		runCppCheck(flgCppCheckAll)
		return
	}

	if flgClangTidy || flgClangTidyFix {
		runClangTidy(flgClangTidyFix)
		return
	}

	if flgRegenPremake {
		regenPremake()
		return
	}

	if flgCheckAccessKeys {
		checkAccessKeys()
		return
	}

	if flgGenTranslationsInfoCpp {
		genTranslationInfoCpp()
		return
	}

	if flgBuildLzsa {
		buildLzsa()
		return
	}

	if flgBuildCodeQL {
		buildCodeQL()
		return
	}

	getSecrets()

	if flgTransDownload {
		downloadTranslations()
		return
	}

	detectVersions()

	if false {
		testCompressOneOff()
		if false {
			// make them reachable
			testGenUpdateTxt()
			buildPreRelease(kPlatformIntel64, true)
			deleteFilesOneOff()
		}
		return
	}

	if flgBuildNo > 0 {
		printBuildNoInfo(flgBuildNo)
		return
	}

	if flgFindLargestFilesByExt {
		findLargestFileByExt()
		return
	}

	if flgFileUpload != "" {
		fileUpload(flgFileUpload)
		return
	}

	if flgBuildLogview {
		buildLogView()
		if flgUpload {
			uploadLogView()
		}
		return
	}

	if flgFilesList {
		filesList()
		return
	}

	opts := &BuildOptions{}

	if flgUpload {
		// given by me from cmd-line
		opts.upload = true
	}

	if flgBuildRelease {
		// only when building locally, not on GitHub CI
		opts.verifyTranslationUpToDate = true
		opts.doCleanCheck = true
		opts.releaseBuild = true
	}
	//opts.doCleanCheck = false // for ad-hoc testing

	ensureBuildOptionsPreRequesites(opts)

	if flgDiff {
		u.WinmergeDiffPreview()
		return
	}

	if flgGenSettings {
		genAndSaveSettingsStructs()
		return
	}

	if flgWc {
		doLineCount()
		return
	}

	if flgTriggerCodeQL {
		triggerBuildWebHook(githubEventTypeCodeQL)
		return
	}

	if flgClean {
		cleanPreserveSettings()
		return
	}

	if flgBuildSmoke {
		buildSmoke()
		return
	}

	if flgPrintBuildNo {
		return
	}

	if flgCIDailyBuild {
		buildCiDaily()
		return
	}

	if flgCIBuild {
		buildCi()
		if opts.upload {
			uploadToStorage(buildTypePreRel)
		} else {
			logf("uploadToStorage: skipping because opts.upload = false\n")
		}
		return
	}

	if flgBuildRelease {
		buildRelease()
		if opts.upload {
			uploadToStorage(buildTypeRel)
		} else {
			logf("uploadToStorage: skipping because opts.upload = false\n")
		}
		return
	}

	// this one is typically for me to build locally, so build all projects
	// to build less use -build-smoke
	if flgBuildPreRelease {
		cleanReleaseBuilds()
		genHTMLDocsForApp()
		buildPreRelease(kPlatformIntel64, true)
		if opts.upload {
			uploadToStorage(buildTypePreRel)
		} else {
			logf("uploadToStorage: skipping because opts.upload = false\n")
		}
		return
	}

	if flgUpdateVer != "" {
		ensureAllUploadCreds()
		updateAutoUpdateVer(flgUpdateVer)
		return
	}

	if flgLogView {
		logView()
		return
	}

	if flgRunTests {
		buildTestUtil()
		dir := filepath.Join("out", "rel64")
		cmd := exec.Command(".\\test_util.exe")
		cmd.Dir = dir
		runCmdLoggedMust(cmd)
		return
	}

	flag.Usage()
}

func logView() {
	path := filepath.Join(logViewWinDir, "build", "bin", "logview.exe")
	if !u.FileExists(path) {
		logf("'%s' doesn't exist, rebuilding\n", path)
		buildLogView()
	} else {
		logf("'%s' already exist. If you want to re-build:\n", path)
		logf("rm \"%s\"\n", path)
	}
	cmd := exec.Command(path)
	err := cmd.Start()
	must(err)
	logf("Started %s\n", path)
}

func cmdRunLoggedInDir(dir string, args ...string) {
	cmd := exec.Command(args[0], args[1:]...)
	cmd.Dir = dir
	cmdRunLoggedMust(cmd)
}

var logViewWinDir = filepath.Join("tools", "logview")

func buildLogView() {
	ver := extractLogViewVersion()
	logf("biuldLogView: ver: %s\n", ver)
	os.RemoveAll(filepath.Join(logViewWinDir, "build", "bin"))
	//cmdRunLoggedInDir(".", "wails", "build", "-clean", "-f", "-upx")
	cmdRunLoggedInDir(logViewWinDir, "wails", "build", "-clean", "-f", "-upx")

	path := filepath.Join(logViewWinDir, "build", "bin", "logview.exe")
	panicIf(!u.FileExists(path))
	// signMust(path)
	logf("\n")
	printFileSize(path)
}

func extractLogViewVersion() string {
	path := filepath.Join(logViewWinDir, "frontend", "src", "version.js")
	d, err := os.ReadFile(path)
	must(err)
	d = u.NormalizeNewlinesInPlace(d)
	s := string(d)
	// s looks like:
	// export const version = "0.1.2";
	parts := strings.Split(s, "\n")
	s = parts[0]
	parts = strings.Split(s, " ")
	panicIf(len(parts) != 5)
	ver := parts[4] // "0.1.2";
	ver = strings.ReplaceAll(ver, `"`, "")
	ver = strings.ReplaceAll(ver, `;`, "")
	parts = strings.Split(ver, ".")
	panicIf(len(parts) < 2) // must be at least 1.0
	// verify all elements are numbers
	for _, part := range parts {
		n, err := strconv.ParseInt(part, 10, 32)
		panicIf(err != nil)
		panicIf(n > 100)
	}
	return ver
}

func printFileSize(path string) {
	size := u.FileSize(path)
	logf("%s: %s\n", path, u.FormatSize(size))
}

func printBuildNoInfo(buildNo int) {
	out := runExeMust("git", "log", "--oneline")
	lines := toTrimmedLines(out)
	// we add 1000 to create a version that is larger than the svn version
	// from the time we used svn
	n := len(lines) - (buildNo - 1000)
	s := lines[n]
	logf("%d: %s\n", buildNo, s)
}

func compressFileWithBrMust(path string) []byte {
	buf := bytes.Buffer{}
	w := brotli.NewWriterLevel(&buf, brotli.BestCompression)
	f := u.Must2(os.Open(path))
	u.Must2(io.Copy(w, f))
	must(f.Close())
	must(w.Close())
	return buf.Bytes()
}

func compressWithZstdMust(path string) []byte {
	buf := bytes.Buffer{}
	w := u.Must2(zstd.NewWriter(&buf, zstd.WithEncoderLevel(zstd.SpeedBestCompression)))
	f := u.Must2(os.Open(path))
	u.Must2(io.Copy(w, f))
	must(f.Close())
	must(w.Close())
	return buf.Bytes()
}

func compressWithLzma2Must(path string) []byte {
	buf := bytes.Buffer{}
	bw := bufio.NewWriter(&buf)
	w := u.Must2(lzma.NewWriter2(bw))
	f := u.Must2(os.Open(path))
	u.Must2(io.Copy(w, f))
	must(f.Close())
	must(w.Close())
	must(bw.Flush())
	return buf.Bytes()
}

func compressWithLzma2BetterMust(path string) []byte {
	buf := bytes.Buffer{}
	bw := bufio.NewWriter(&buf)
	var c lzma.Writer2Config
	c.DictCap = (8 * 1024 * 1024) * 16
	must(c.Verify())
	w := u.Must2(c.NewWriter2(bw))
	f := u.Must2(os.Open(path))
	u.Must2(io.Copy(w, f))
	must(f.Close())
	must(w.Close())
	must(bw.Flush())
	return buf.Bytes()
}

func creaZipWithCompressFunction(zipPath string, files []string, dir string, compressFunc func(string) []byte, comprSuffix string) {
	os.Remove(zipPath)
	w := u.Must2(os.Create(zipPath))
	zw := zip.NewWriter(w)
	defer zw.Close()
	var wg sync.WaitGroup
	nConcurrent := runtime.NumCPU()
	sem := make(chan bool, nConcurrent)
	for _, f := range files {
		path := filepath.Join(dir, f)
		wg.Add(1)
		sem <- true
		go func() {
			data := compressFunc(path)
			addZipDataStoreMust(zw, data, f+comprSuffix)
			<-sem
			wg.Done()
		}()
	}
	wg.Wait()
	must(zw.Close())
}

func testCompressOneOff() {
	dir := filepath.Join("out", "rel64")
	files := []string{"SumatraPDF.exe", "SumatraPDF-dll.exe", "libmupdf.pdb", "SumatraPDF.pdb", "SumatraPDF-dll.pdb"}
	{
	}
	origSize := int64(0)
	for _, f := range files {
		origSize += u.FileSize(filepath.Join(dir, f))
	}
	logf("origSize: %s\n", u.FormatSize(origSize))

	{
		archivePath := filepath.Join(dir, "rel64.lzma2.better.zip")
		os.Remove(archivePath)
		logf("\nCreating %s (%d threads)\n", archivePath, runtime.NumCPU())
		printDur := measureDuration()
		creaZipWithCompressFunction(archivePath, files, dir, compressWithLzma2BetterMust, ".lzma2")
		printDur()
		compressedSize := u.FileSize(archivePath)
		ratio := float64(origSize) / float64(compressedSize)
		logf("compressedSize: %s, ratio: %.2f\n", u.FormatSize(compressedSize), ratio)
	}
	{
		archivePath := filepath.Join(dir, "rel64.lzma2.zip")
		os.Remove(archivePath)
		logf("\nCreating %s (%d threads)\n", archivePath, runtime.NumCPU())
		printDur := measureDuration()
		creaZipWithCompressFunction(archivePath, files, dir, compressWithLzma2Must, ".lzma2")
		printDur()
		compressedSize := u.FileSize(archivePath)
		ratio := float64(origSize) / float64(compressedSize)
		logf("compressedSize: %s, ratio: %.2f\n", u.FormatSize(compressedSize), ratio)
	}
	{
		archivePath := filepath.Join(dir, "rel64.br.zip")
		os.Remove(archivePath)
		logf("\nCreating %s (%d threads)\n", archivePath, runtime.NumCPU())
		printDur := measureDuration()
		creaZipWithCompressFunction(archivePath, files, dir, compressFileWithBrMust, ".br")
		printDur()
		compressedSize := u.FileSize(archivePath)
		ratio := float64(origSize) / float64(compressedSize)
		logf("compressedSize: %s, ratio: %.2f\n", u.FormatSize(compressedSize), ratio)
	}
	{
		archivePath := filepath.Join(dir, "rel64.zstd.zip")
		os.Remove(archivePath)
		logf("\nCreating %s (%d threads)\n", archivePath, runtime.NumCPU())
		printDur := measureDuration()
		creaZipWithCompressFunction(archivePath, files, dir, compressWithZstdMust, ".zstd")
		printDur()
		compressedSize := u.FileSize(archivePath)
		ratio := float64(origSize) / float64(compressedSize)
		logf("compressedSize: %s, ratio: %.2f\n", u.FormatSize(compressedSize), ratio)
	}
	{
		logf("\nCreating rel64.lzsa using\n")
		printDur := measureDuration()
		archivePath := filepath.Join(dir, "rel64.lzsa")
		os.Remove(archivePath)
		createLzsaFromFiles("rel64.lzsa", files, dir)
		printDur()
		compressedSize := u.FileSize(archivePath)
		ratio := float64(origSize) / float64(compressedSize)
		logf("compressedSize: %s, ratio: %.2f\n", u.FormatSize(compressedSize), ratio)
	}
}
