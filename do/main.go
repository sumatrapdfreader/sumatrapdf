package main

import (
	"flag"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/kjk/common/u"
)

var (
	flgSkipSign bool
)

func regenPremake() {
	premakePath := filepath.Join("bin", "premake5.exe")
	/*
		{
			cmd := exec.Command(premakePath, "vs2019")
			runCmdLoggedMust(cmd)
		}
	*/
	{
		cmd := exec.Command(premakePath, "vs2022")
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
	logf(ctx(), "> %s\n", fmtCmdShort(*cmd))
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
	args = append(args, "--inline-suppr", "-I", "src", "-I", "src/utils", "src")
	cppcheckExe := detectCppcheckExe()
	cmd = exec.Command(cppcheckExe, args...)
	os.Remove(cppcheckLogFile)
	err := runCmdShowProgressAndLog(cmd, cppcheckLogFile)
	must(err)
	logf(ctx(), "\nLogged output to '%s'\n", cppcheckLogFile)
}

type BuildOptions struct {
	sign                      bool
	upload                    bool
	verifyTranslationUpToDate bool
	doCleanCheck              bool
	releaseBuild              bool
}

func ensureAllUploadCreds() {
	panicIf(os.Getenv("R2_ACCESS") == "", "Not uploading to s3 because R2_ACCESS env variable not set\n")
	panicIf(os.Getenv("R2_SECRET") == "", "Not uploading to s3 because R2_SECRET env variable not set\n")
	panicIf(os.Getenv("BB_ACCESS") == "", "Not uploading to backblaze because BB_ACCESS env variable not set\n")
	panicIf(os.Getenv("BB_SECRET") == "", "Not uploading to backblaze because BB_SECRET env variable not set\n")
}

func ensureBuildOptionsPreRequesites(opts *BuildOptions) {
	logf(ctx(), "upload: %v\n", opts.upload)
	logf(ctx(), "sign: %v\n", opts.sign)
	logf(ctx(), "verifyTranslationUpToDate: %v\n", opts.verifyTranslationUpToDate)

	if opts.upload {
		ensureAllUploadCreds()
	}

	if opts.sign {
		panicIf(!hasCertPwd(), "CERT_PWD env variable is not set")
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

	if !opts.sign {
		flgSkipSign = true
	}
}

func main() {
	logf(ctx(), "Current directory: %s\n", currDirAbsMust())
	timeStart := time.Now()
	defer func() {
		logf(ctx(), "Finished in %s\n", time.Since(timeStart))
	}()

	// ad-hoc flags to be set manually (to show less options)
	var (
		flgGenTranslationsInfoCpp = false
		flgCppCheck               = false
		flgCppCheckAll            = false
		flgClangTidy              = false
		flgClangTidyFix           = false
		flgPrintBuildNo           = false
		flgBuildLzsa              = false
		flgFindLargestFilesByExt  = false
	)

	var (
		flgRegenPremake    bool
		flgUpload          bool
		flgCIBuild         bool
		flgCIDailyBuild    bool
		flgUploadCiBuild   bool
		flgBuildPreRelease bool
		flgBuildRelease    bool
		flgWc              bool
		flgTransDownload   bool
		flgClean           bool
		flgCheckAccessKeys bool
		flgTriggerCodeQL   bool
		flgClangFormat     bool
		flgDiff            bool
		flgGenSettings     bool
		flgUpdateVer       string
		flgDrMem           bool
		flgLogView         bool
		flgRunTests        bool
		flgSmoke           bool
		flgFileUpload      string
		flgFilesList       bool
		flgExtractUtils    bool
		flgBuildLogview    bool
		flgBuildNo         int
	)

	{
		flag.StringVar(&flgFileUpload, "file-upload", "", "upload a test file to s3 / spaces")
		flag.BoolVar(&flgFilesList, "files-list", false, "list uploaded files in s3 / spaces")
		flag.BoolVar(&flgRegenPremake, "premake", false, "regenerate premake*.lua files")
		flag.BoolVar(&flgCIBuild, "ci", false, "run CI steps")
		flag.BoolVar(&flgCIDailyBuild, "ci-daily", false, "run CI daily steps")
		flag.BoolVar(&flgUploadCiBuild, "ci-upload", false, "upload the result of ci build to s3 and do spaces")
		flag.BoolVar(&flgSmoke, "smoke", false, "run smoke build (installer for 64bit release)")
		flag.BoolVar(&flgBuildPreRelease, "build-pre-rel", false, "build pre-release")
		flag.BoolVar(&flgBuildRelease, "build-release", false, "build release")
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
		flag.BoolVar(&flgDrMem, "drmem", false, "run drmemory of rel 64")
		flag.BoolVar(&flgLogView, "logview", false, "run logview")
		flag.BoolVar(&flgRunTests, "run-tests", false, "run test_util executable")
		flag.BoolVar(&flgExtractUtils, "extract-utils", false, "extract utils")
		flag.BoolVar(&flgBuildLogview, "build-logview", false, "build logview-win. Use -upload to also upload it to backblaze")
		flag.IntVar(&flgBuildNo, "build-no-info", 0, "print build number info for given build number")
		flag.Parse()
	}

	if flgExtractUtils {
		extractUtils(flgCIBuild)
		return
	}

	detectVersions()

	if false {
		testGenUpdateTxt()
		return
	}

	if false {
		//buildPreRelease()
		return
	}

	if false {
		deleteFilesOneOff()
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
	if flgUploadCiBuild {
		// triggered via -ci-upload from .github workflow file
		// only upload if this is my repo (not a fork)
		// master branch (not work branches) and on push (not pull requests etc.)
		opts.upload = isGithubMyMasterBranch()
	}

	if flgCIBuild {
		// triggered via -ci from .github workflow file
		// only sign if this is my repo (not a fork)
		// master branch (not work branches) and on push (not pull requests etc.)
		opts.sign = isGithubMyMasterBranch()
	}

	if flgUpload {
		// given by me from cmd-line
		opts.sign = true
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

	if flgTransDownload {
		downloadTranslations()
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

	if flgSmoke {
		buildSmoke()
		return
	}

	if flgPrintBuildNo {
		return
	}

	if flgCIDailyBuild {
		buildCiDaily()
		if opts.upload {
			uploadToStorage(buildTypePreRel)
		} else {
			logf(ctx(), "uploadToStorage: skipping because opts.upload = false\n")
		}
		return
	}

	// on GitHub Actions the build happens in an earlier step
	if flgUploadCiBuild {
		// pre-release build on push
		uploadToStorage(buildTypePreRel)
		return
	}

	if flgCIBuild {
		buildCi()
		if opts.upload {
			uploadToStorage(buildTypePreRel)
		} else {
			logf(ctx(), "uploadToStorage: skipping because opts.upload = false\n")
		}
		return
	}

	if flgBuildRelease {
		buildRelease()
		if opts.upload {
			uploadToStorage(buildTypeRel)
		} else {
			logf(ctx(), "uploadToStorage: skipping because opts.upload = false\n")
		}
		return
	}

	// this one is typically for me to build locally, so build all projects
	if flgBuildPreRelease {
		cleanReleaseBuilds()
		buildPreRelease(kPlatformIntel64, true)
		if opts.upload {
			uploadToStorage(buildTypePreRel)
		} else {
			logf(ctx(), "uploadToStorage: skipping because opts.upload = false\n")
		}
		return
	}

	if flgUpdateVer != "" {
		ensureAllUploadCreds()
		updateAutoUpdateVer(flgUpdateVer)
		return
	}

	if flgDrMem {
		buildJustPortableExe(rel64Dir, "Release", kPlatformIntel64)
		//cmd := exec.Command("drmemory.exe", "-light", "-check_leaks", "-possible_leaks", "-count_leaks", "-suppress", "drmem-sup.txt", "--", ".\\out\\rel64\\SumatraPDF.exe")
		cmd := exec.Command("drmemory.exe", "-leaks_only", "-suppress", "drmem-sup.txt", "--", ".\\out\\rel64\\SumatraPDF.exe")
		runCmdLoggedMust(cmd)
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
	cmd := exec.Command("go", "run", `.\tools\logview\`)
	runCmdLoggedMust(cmd)
}

func cmdRunLoggedInDir(dir string, args ...string) {
	cmd := exec.Command(args[0], args[1:]...)
	cmd.Dir = dir
	cmdRunLoggedMust(cmd)
}

var logViewWinDir = filepath.Join("tools", "logview-win")

func buildLogView() {
	ver := extractLogViewVersion()
	logf(ctx(), "biuldLogView: ver: %s\n", ver)
	os.RemoveAll(filepath.Join(logViewWinDir, "build", "bin"))
	//cmdRunLoggedInDir(".", "wails", "build", "-clean", "-f", "-upx")
	cmdRunLoggedInDir(logViewWinDir, "wails", "build", "-clean", "-f", "-upx")

	path := filepath.Join(logViewWinDir, "build", "bin", "logview.exe")
	panicIf(!u.FileExists(path))
	signMust(path)
	logf(ctx(), "\n")
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
	logf(ctx(), "%s: %s\n", path, u.FormatSize(size))
}

func printBuildNoInfo(buildNo int) {
	out := runExeMust("git", "log", "--oneline")
	lines := toTrimmedLines(out)
	// we add 1000 to create a version that is larger than the svn version
	// from the time we used svn
	n := len(lines) - (buildNo - 1000)
	s := lines[n]
	logf(ctx(), "%d: %s\n", buildNo, s)
}
