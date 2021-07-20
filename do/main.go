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
	flgNoCleanCheck          bool
	flgSmoke                 bool
	flgUpload                bool
	flgSkipTranslationVerify bool
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

	var (
		flgRegenPremake            bool
		flgCIBuild                 bool
		flgUploadCiBuild           bool
		flgBuildLzsa               bool
		flgBuildPreRelease         bool
		flgBuildRelease            bool
		flgBuildRel64Fast          bool
		flgBuildRel32Fast          bool
		flgWc                      bool
		flgDownloadTranslations    bool
		flgRegenerateTranslattions bool
		flgUploadTranslations      bool
		flgClean                   bool
		flgCrashes                 bool
		flgCheckAccessKeys         bool
		flgBuildNo                 bool
		flgTriggerCodeQL           bool
		flgWebsiteRun              bool
		flgWebsiteDeployCloudflare bool
		flgWebsiteImportNotion     bool
		flgWebsiteBuildCloudflare  bool
		flgNoCache                 bool
		flgClangFormat             bool
		flgCppCheck                bool
		flgCppCheckAll             bool
		flgClangTidy               bool
		flgClangTidyFix            bool
		flgDiff                    bool
		flgGenStructs              bool
		flgUpdateVer               string
		flgDrMem                   bool
		flgLogView                 bool
		flgRunTests                bool
	)

	{
		flag.BoolVar(&flgRegenPremake, "premake", false, "regenerate premake*.lua files")
		flag.BoolVar(&flgCIBuild, "ci", false, "run CI steps")
		flag.BoolVar(&flgUploadCiBuild, "ci-upload", false, "upload the result of ci build to s3 and do spaces")
		flag.BoolVar(&flgSmoke, "smoke", false, "run smoke build (installer for 64bit release)")
		flag.BoolVar(&flgBuildPreRelease, "build-pre-rel", false, "build pre-release")
		flag.BoolVar(&flgBuildRelease, "build-release", false, "build release")
		flag.BoolVar(&flgBuildRel64Fast, "build-rel64-fast", false, "build only 64-bit release installer, for testing")
		flag.BoolVar(&flgBuildRel32Fast, "build-rel32-fast", false, "build only 32-bit release installer, for testing")
		flag.BoolVar(&flgBuildLzsa, "build-lzsa", false, "build MakeLZSA.exe")
		flag.BoolVar(&flgNoCleanCheck, "no-clean-check", false, "allow running if repo has changes (for testing build script)")
		flag.BoolVar(&flgUpload, "upload", false, "upload the build to s3 and do spaces")
		flag.BoolVar(&flgClangFormat, "format", false, "format source files with clang-format")
		flag.BoolVar(&flgWc, "wc", false, "show loc stats (like wc -l)")
		flag.BoolVar(&flgDownloadTranslations, "trans-dl", false, "download translations and re-generate C code")
		flag.BoolVar(&flgRegenerateTranslattions, "trans-regen", false, "regenerate .cpp translations files from strings/translations.txt")
		flag.BoolVar(&flgUploadTranslations, "trans-upload", false, "upload translations to apptranslators.org if changed")
		flag.BoolVar(&flgClean, "clean", false, "clean the build (remove out/ files except for settings)")
		flag.BoolVar(&flgCrashes, "crashes", false, "see crashes in a web ui")
		flag.BoolVar(&flgCheckAccessKeys, "check-access-keys", false, "check access keys for menu items")
		flag.BoolVar(&flgBuildNo, "build-no", false, "print build number")
		flag.BoolVar(&flgTriggerCodeQL, "trigger-codeql", false, "trigger codeql build")
		flag.BoolVar(&flgWebsiteRun, "website-run", false, "preview website locally")
		flag.BoolVar(&flgWebsiteDeployCloudflare, "website-deploy", false, "deploy website to cloudflare")
		flag.BoolVar(&flgWebsiteImportNotion, "website-import", false, "import docs from notion")
		flag.BoolVar(&flgWebsiteBuildCloudflare, "website-build-cf", false, "build the website (download Sumatra files)")
		flag.BoolVar(&flgNoCache, "no-cache", false, "if true, notion import ignores cache")
		flag.BoolVar(&flgCppCheck, "cppcheck", false, "run cppcheck (must be installed)")
		flag.BoolVar(&flgCppCheckAll, "cppcheck-all", false, "run cppcheck with more checks (must be installed)")
		flag.BoolVar(&flgClangTidy, "clang-tidy", false, "run clang-tidy (must be installed)")
		flag.BoolVar(&flgClangTidyFix, "clang-tidy-fix", false, "run clang-tidy (must be installed)")
		flag.BoolVar(&flgDiff, "diff", false, "preview diff using winmerge")
		flag.BoolVar(&flgGenStructs, "gen-structs", false, "re-generate src/SettingsStructs.h")
		flag.StringVar(&flgUpdateVer, "update-auto-update-ver", "", "update version used for auto-update checks")
		flag.BoolVar(&flgDrMem, "drmem", false, "run drmemory of rel 64")
		flag.BoolVar(&flgLogView, "logview", false, "run logview")
		flag.BoolVar(&flgRunTests, "run-tests", false, "run test_util executable")
		flag.Parse()
	}

	if false {
		detectVersions()
		//buildPreRelease()
		return
	}

	// early check so we don't find it out only after 20 minutes of building
	if flgUpload || flgUploadCiBuild {
		if shouldSignAndUpload() {
			panicIf(!hasSpacesCreds())
			panicIf(!hasS3Creds())
		}
	}

	if flgWebsiteRun {
		websiteRunLocally("website")
		return
	}

	if flgWebsiteImportNotion {
		websiteImportNotion()
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

	if flgGenStructs {
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
		downloadTranslationsMain()
		return
	}

	if flgRegenerateTranslattions {
		regenerateLangs()
		return
	}

	if flgUploadTranslations {
		uploadStringsIfChanged()
		return
	}

	if flgBuildLzsa {
		buildLzsa()
		return
	}

	if flgSmoke {
		smokeBuild()
		return
	}

	if flgBuildNo {
		detectVersions()
		return
	}

	if flgCIBuild {
		// TODO: temporary
		//dumpEnv()
		//dumpWebHookEventPayload()

		// ci build does the same thing as pre-release
		if shouldSignAndUpload() {
			failIfNoCertPwd()
		}
		flgSkipTranslationVerify = true
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
			buildRelease64Fast()
		default:
			panic("unkown value from getGitHubEventType()")
		}
		return
	}

	// on GitHub Actions the build happens in an earlier step
	if flgUploadCiBuild {
		if shouldSkipUpload() {
			logf("Skipping upload\n")
			return
		}
		flgUpload = true
		detectVersions()

		gev := getGitHubEventType()
		switch gev {
		case githubEventPush:
			// pre-release build on push
			uploadToStorage(buildTypePreRel)
		case githubEventTypeCodeQL:
			// do nothing
		default:
			panic("unkown value from getGitHubEventType()")
		}
		return
	}

	if flgBuildRelease {
		if !flgUpload {
			failIfNoCertPwd()
		} else {
			os.RemoveAll("out")
		}
		detectVersions()
		buildRelease(flgUpload)
		if flgUpload {
			uploadToStorage(buildTypeRel)
		}
		return
	}

	if flgBuildRel64Fast {
		warnIfNoCertPwd()
		detectVersions()
		buildRelease64Fast()
		return
	}

	if flgBuildRel32Fast {
		warnIfNoCertPwd()
		detectVersions()
		buildRelease32Fast()
		return
	}

	if flgBuildPreRelease {
		// make sure we can sign the executables
		failIfNoCertPwd()
		detectVersions()
		buildPreRelease()
		uploadToStorage(buildTypePreRel)
		return
	}

	if flgUpdateVer != "" {
		updateAutoUpdateVer(flgUpdateVer)
		return
	}

	if flgDrMem {
		buildPortableExe64()
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

func uploadToStorage(buildType string) {
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
