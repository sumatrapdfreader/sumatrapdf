package main

import (
	"flag"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"sync"
	"time"

	"github.com/kjk/u"
)

var (
	flgSkipSign bool
)

func regenPremake() {
	premakePath := filepath.Join("bin", "premake5.exe")
	{
		cmd := exec.Command(premakePath, "vs2019")
		u.RunCmdLoggedMust(cmd)
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
	logf("> %s\n", u.FmtCmdShort(*cmd))
	return cmd.Run()
}

const (
	cppcheckLogFile = "cppcheck.out.txt"
)

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

	// TODO: not sure if adding Windows SDK include  path helps.
	// It takes a lot of time and doesn't seem to provide value
	//winSdkIncludeDir := `C:\Program Files (x86)\Windows Kits\10\Include\10.0.18362.0\um`
	// "-I", winSdkIncludeDir
	// "-D__RPCNDR_H_VERSION__=440"

	args := []string{"--platform=win64", "-DWIN32", "-D_WIN32", "-D_MSC_VER=1800", "-D_M_X64", "-DIFACEMETHODIMP_(x)=x", "-DSTDAPI_(x)=x", "-DPRE_RELEASE_VER=3.3", "-q", "-v"}
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
	}
	args = append(args, "--inline-suppr", "-I", "src", "-I", "src/utils", "src")
	cmd = exec.Command("cppcheck", args...)
	os.Remove(cppcheckLogFile)
	err := runCmdShowProgressAndLog(cmd, cppcheckLogFile)
	must(err)
	logf("\nLogged output to '%s'\n", cppcheckLogFile)
}

type BuildOptions struct {
	sign                      bool
	upload                    bool
	verifyTranslationUpToDate bool
	doCleanCheck              bool
	releaseBuild              bool
}

func ensureCanUpload() {
	panicIf(!hasSpacesCreds())
	panicIf(!hasS3Creds())
}

func ensureBuildOptionsPreRequesites(opts *BuildOptions) {
	logf("upload: %v\n", opts.upload)
	logf("sign: %v\n", opts.sign)
	logf("verifyTranslationUpToDate: %v\n", opts.verifyTranslationUpToDate)

	if opts.upload {
		ensureCanUpload()
	}

	if opts.sign {
		panicIf(!hasCertPwd(), "CERT_PWD env variable is not set")
	}
	if opts.verifyTranslationUpToDate {
		verifyTranslationsMust()
	}
	if opts.doCleanCheck {
		u.PanicIf(!isGitClean(), "git has unsaved changes\n")
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
	if u.DirExists("/opt/buildhome/repo") {
		// on Cloudflare pages build machine
		os.Chdir("/opt/buildhome/repo")
	} else {
		u.CdUpDir("sumatrapdf")
	}
	logf("Current directory: %s\n", u.CurrDirAbsMust())
	timeStart := time.Now()
	defer func() {
		logf("Finished in %s\n", time.Since(timeStart))
	}()

	// ad-hoc flags to be set manually (to show less options)
	var (
		flgGenTranslationsInfoCpp = false
		flgCppCheck               = false
		flgCppCheckAll            = false
		flgClangTidy              = false
		flgClangTidyFix           = false
		flgBuildNo                = false
		flgBuildLzsa              = false
	)

	var (
		flgRegenPremake            bool
		flgUpload                  bool
		flgCIBuild                 bool
		flgUploadCiBuild           bool
		flgBuildPreRelease         bool
		flgBuildRelease            bool
		flgWc                      bool
		flgDownloadTranslations    bool
		flgClean                   bool
		flgCrashes                 bool
		flgCheckAccessKeys         bool
		flgTriggerCodeQL           bool
		flgWebsiteRun              bool
		flgWebsiteDeployCloudflare bool
		flgWebsiteImportNotion     bool
		flgWebsiteBuildCloudflare  bool
		flgNoCache                 bool
		flgClangFormat             bool
		flgDiff                    bool
		flgGenSettings             bool
		flgUpdateVer               string
		flgDrMem                   bool
		flgLogView                 bool
		flgRunTests                bool
		flgSmoke                   bool
		flgFileUpload              string
		flgFilesList               bool
		flgBuildDocs               bool
	)

	{
		flag.StringVar(&flgFileUpload, "file-upload", "", "upload a test file to s3 / spaces")
		flag.BoolVar(&flgFilesList, "files-list", false, "list uploaded files in s3 / spaces")
		flag.BoolVar(&flgRegenPremake, "premake", false, "regenerate premake*.lua files")
		flag.BoolVar(&flgCIBuild, "ci", false, "run CI steps")
		flag.BoolVar(&flgUploadCiBuild, "ci-upload", false, "upload the result of ci build to s3 and do spaces")
		flag.BoolVar(&flgSmoke, "smoke", false, "run smoke build (installer for 64bit release)")
		flag.BoolVar(&flgBuildPreRelease, "build-pre-rel", false, "build pre-release")
		flag.BoolVar(&flgBuildRelease, "build-release", false, "build release")
		//flag.BoolVar(&flgBuildLzsa, "build-lzsa", false, "build MakeLZSA.exe")
		flag.BoolVar(&flgUpload, "upload", false, "upload the build to s3 and do spaces")
		flag.BoolVar(&flgClangFormat, "format", false, "format source files with clang-format")
		flag.BoolVar(&flgWc, "wc", false, "show loc stats (like wc -l)")
		flag.BoolVar(&flgDownloadTranslations, "trans-dl", false, "download translations and re-generate C code")
		//flag.BoolVar(&flgGenTranslationsInfoCpp, "trans-gen-info", false, "generate src/TranslationsInfo.cpp")
		flag.BoolVar(&flgClean, "clean", false, "clean the build (remove out/ files except for settings)")
		flag.BoolVar(&flgCrashes, "crashes", false, "see crashes in a web ui")
		flag.BoolVar(&flgCheckAccessKeys, "check-access-keys", false, "check access keys for menu items")
		//flag.BoolVar(&flgBuildNo, "build-no", false, "print build number")
		flag.BoolVar(&flgTriggerCodeQL, "trigger-codeql", false, "trigger codeql build")
		flag.BoolVar(&flgWebsiteRun, "website-run", false, "preview website locally")
		flag.BoolVar(&flgWebsiteDeployCloudflare, "website-deploy", false, "deploy website to cloudflare")
		flag.BoolVar(&flgWebsiteImportNotion, "website-import", false, "import docs from notion")
		flag.BoolVar(&flgWebsiteBuildCloudflare, "website-build-cf", false, "build the website (download Sumatra files)")
		flag.BoolVar(&flgNoCache, "no-cache", false, "if true, notion import ignores cache")
		//flag.BoolVar(&flgCppCheck, "cppcheck", false, "run cppcheck (must be installed)")
		//flag.BoolVar(&flgCppCheckAll, "cppcheck-all", false, "run cppcheck with more checks (must be installed)")
		//flag.BoolVar(&flgClangTidy, "clang-tidy", false, "run clang-tidy (must be installed)")
		//flag.BoolVar(&flgClangTidyFix, "clang-tidy-fix", false, "run clang-tidy (must be installed)")
		flag.BoolVar(&flgDiff, "diff", false, "preview diff using winmerge")
		flag.BoolVar(&flgGenSettings, "settings-gen", false, "re-generate src/SettingsStructs.h")
		flag.StringVar(&flgUpdateVer, "update-auto-update-ver", "", "update version used for auto-update checks")
		flag.BoolVar(&flgDrMem, "drmem", false, "run drmemory of rel 64")
		flag.BoolVar(&flgLogView, "logview", false, "run logview")
		flag.BoolVar(&flgRunTests, "run-tests", false, "run test_util executable")
		flag.BoolVar(&flgBuildDocs, "build-docs", false, "build epub docs")
		flag.Parse()
	}

	if false {
		detectVersions()
		//buildPreRelease()
		return
	}

	if flgFileUpload != "" {
		fileUpload(flgFileUpload)
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

	if flgBuildPreRelease || flgBuildRelease {
		// only when building locally, not on GitHub CI
		opts.verifyTranslationUpToDate = true
		opts.doCleanCheck = true
	}
	//opts.doCleanCheck = false // for ad-hoc testing
	if flgBuildRelease {
		opts.releaseBuild = true
	}

	ensureBuildOptionsPreRequesites(opts)

	if flgWebsiteRun {
		websiteRunLocally("website")
		return
	}

	if flgWebsiteImportNotion {
		websiteImportNotion()
		return
	}

	if flgBuildDocs {
		buildEpubDocs()
		return
	}

	if flgWebsiteBuildCloudflare {
		websiteBuildCloudflare()
		return
	}

	if flgWebsiteDeployCloudflare {
		websiteDeployCloudlare()
		return
	}

	if flgDiff {
		winmergeDiffPreview()
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
		clean()
		return
	}

	if flgClangFormat {
		clangFormatFiles()
		return
	}

	if flgCrashes {
		downloadCrashesAndGenerateHTML()
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

	if flgDownloadTranslations {
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
		detectVersions()
		buildSmoke()
		return
	}

	if flgBuildNo {
		detectVersions()
		return
	}

	if flgCIBuild {
		detectVersions()
		gev := getGitHubEventType()
		switch gev {
		case githubEventPush:
			currBranch := getCurrentBranchMust()
			if currBranch == "website-cf" {
				logf("skipping build because on branch '%s'\n", currBranch)
				return
			}
			buildPreRelease()
		case githubEventTypeCodeQL:
			// code ql is just a regular build, I assume intercepted by
			// by their tooling
			buildSmoke()
		default:
			panic("unkown value from getGitHubEventType()")
		}
		return
	}

	// on GitHub Actions the build happens in an earlier step
	if flgUploadCiBuild {
		detectVersions()
		gev := getGitHubEventType()
		switch gev {
		case githubEventPush:
			// pre-release build on push
			uploadToStorage(opts, buildTypePreRel)
		case githubEventTypeCodeQL:
			// do nothing
		default:
			panic("unkown value from getGitHubEventType()")
		}
		return
	}

	if flgBuildRelease {
		detectVersions()
		buildRelease()
		uploadToStorage(opts, buildTypeRel)
		return
	}

	if flgBuildPreRelease {
		// make sure we can sign the executables
		detectVersions()
		buildPreRelease()
		uploadToStorage(opts, buildTypePreRel)
		return
	}

	if flgUpdateVer != "" {
		ensureCanUpload()
		updateAutoUpdateVer(flgUpdateVer)
		return
	}

	if flgDrMem {
		buildJustPortableExe(rel64Dir, "Release", "x64")
		//cmd := exec.Command("drmemory.exe", "-light", "-check_leaks", "-possible_leaks", "-count_leaks", "-suppress", "drmem-sup.txt", "--", ".\\out\\rel64\\SumatraPDF.exe")
		cmd := exec.Command("drmemory.exe", "-leaks_only", "-suppress", "drmem-sup.txt", "--", ".\\out\\rel64\\SumatraPDF.exe")
		u.RunCmdLoggedMust(cmd)
		return
	}

	if flgLogView {
		pathSrc := filepath.Join("src", "tools", "logview.cpp")
		dir := filepath.Join("out", "rel64")
		path := filepath.Join(dir, "logview.exe")
		needsRebuild := fileNewerThan(pathSrc, path)
		if needsRebuild {
			buildLogview()
		}
		cmd := exec.Command(".\\logview.exe")
		cmd.Dir = dir
		u.RunCmdLoggedMust(cmd)
		return
	}

	if flgRunTests {
		buildTestUtil()
		dir := filepath.Join("out", "rel64")
		cmd := exec.Command(".\\test_util.exe")
		cmd.Dir = dir
		u.RunCmdLoggedMust(cmd)
		return
	}

	flag.Usage()
}

func uploadToStorage(opts *BuildOptions, buildType string) {
	if !opts.upload {
		logf("Skipping uploadToStorage() because opts.upload = false\n")
		return
	}

	timeStart := time.Now()
	defer func() {
		logf("uploadToStorage of '%s' finished in %s\n", buildType, time.Since(timeStart))
	}()
	var wg sync.WaitGroup
	wg.Add(2)
	go func() {
		s3UploadBuildMust(buildType)
		s3DeleteOldBuilds()
		wg.Done()
	}()

	go func() {
		spacesUploadBuildMust(buildType)
		spacesDeleteOldBuilds()
		wg.Done()
	}()
	wg.Wait()
}
