package main

import (
	"flag"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"time"

	"github.com/kjk/u"
)

var (
	flgNoCleanCheck          bool
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
	panicIfErr(err)
	defer f.Close()

	cmd.Stdout = io.MultiWriter(f, os.Stdout)
	cmd.Stderr = io.MultiWriter(f, os.Stderr)
	fmt.Printf("> %s\n", u.FmtCmdShort(*cmd))
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
	u.CdUpDir("sumatrapdf")
	logf("Current directory: %s\n", u.CurrDirAbsMust())
	timeStart := time.Now()
	defer func() {
		fmt.Printf("Finished in %s\n", time.Since(timeStart))
	}()

	var (
		flgRegenPremake            bool
		flgCIBuild                 bool
		flgUploadCiBuild           bool
		flgBuildLzsa               bool
		flgBuildPreRelease         bool
		flgBuildRaMicroPreRelease  bool
		flgBuildRelease            bool
		flgBuildReleaseFast        bool
		flgBuildRelease32Fast      bool
		flgSmoke                   bool
		flgWc                      bool
		flgDownloadTranslations    bool
		flgRegenerateTranslattions bool
		flgUploadTranslations      bool
		flgClean                   bool
		flgDeleteOldBuilds         bool
		flgCrashes                 bool
		flgCheckAccessKeys         bool
		flgBuildNo                 bool
		flgTriggerPreRel           bool
		flgTriggerRaMicroPreRel    bool
		flgTriggerCodeQL           bool
		flgWebsiteRun              bool
		flgWebsiteDeployProd       bool
		flgWebsiteDeployDev        bool
		flgWebsiteImportNotion     bool
		flgWebsiteImportAndDeploy  bool
		flgNoCache                 bool
		flgClangFormat             bool
		flgCppCheck                bool
		flgCppCheckAll             bool
		flgClangTidy               bool
		flgDiff                    bool
		flgGenStructs              bool
		flgUpdateVer               string
	)

	{
		flag.BoolVar(&flgRegenPremake, "premake", false, "regenerate premake*.lua files")
		flag.BoolVar(&flgCIBuild, "ci", false, "run CI steps")
		flag.BoolVar(&flgUploadCiBuild, "ci-upload", false, "upload the result of ci build to s3 and do spaces")
		flag.BoolVar(&flgSmoke, "smoke", false, "run smoke build (installer for 64bit release)")
		flag.BoolVar(&flgBuildPreRelease, "build-pre-rel", false, "build pre-release")
		flag.BoolVar(&flgBuildRaMicroPreRelease, "build-ramicro-pre-rel", false, "build ramicro pre-release")
		flag.BoolVar(&flgBuildRelease, "build-release", false, "build release")
		flag.BoolVar(&flgBuildReleaseFast, "build-release-fast", false, "build only 64-bit release installer, for testing")
		flag.BoolVar(&flgBuildRelease32Fast, "build-release-32-fast", false, "build only 32-bit release installer, for testing")
		flag.BoolVar(&flgBuildLzsa, "build-lzsa", false, "build MakeLZSA.exe")
		flag.BoolVar(&flgNoCleanCheck, "no-clean-check", false, "allow running if repo has changes (for testing build script)")
		flag.BoolVar(&flgUpload, "upload", false, "upload the build to s3 and do spaces")
		flag.BoolVar(&flgClangFormat, "clang-format", false, "format source files with clang-format")
		flag.BoolVar(&flgWc, "wc", false, "show loc stats (like wc -l)")
		flag.BoolVar(&flgDownloadTranslations, "trans-dl", false, "download translations and re-generate C code")
		flag.BoolVar(&flgRegenerateTranslattions, "trans-regen", false, "regenerate .cpp translations files from strings/translations.txt")
		flag.BoolVar(&flgUploadTranslations, "trans-upload", false, "upload translations to apptranslators.org if changed")
		flag.BoolVar(&flgClean, "clean", false, "clean the build")
		flag.BoolVar(&flgDeleteOldBuilds, "delete-old-builds", false, "delete old builds")
		flag.BoolVar(&flgCrashes, "crashes", false, "see crashes in a web ui")
		flag.BoolVar(&flgCheckAccessKeys, "check-access-keys", false, "check access keys for menu items")
		flag.BoolVar(&flgBuildNo, "build-no", false, "print build number")
		flag.BoolVar(&flgTriggerPreRel, "trigger-pre-rel", false, "trigger pre-release build")
		flag.BoolVar(&flgTriggerRaMicroPreRel, "trigger-ramicro-pre-rel", false, "trigger pre-release build")
		flag.BoolVar(&flgTriggerCodeQL, "trigger-codeql", false, "trigger codeql build")
		flag.BoolVar(&flgWebsiteRun, "website-run", false, "preview website locally")
		flag.BoolVar(&flgWebsiteDeployProd, "website-deploy-prod", false, "deploy website")
		flag.BoolVar(&flgWebsiteDeployDev, "website-deploy-dev", false, "deploy a preview of website")
		flag.BoolVar(&flgWebsiteImportNotion, "website-import-notion", false, "import docs from notion")
		flag.BoolVar(&flgWebsiteImportAndDeploy, "website-import-deploy", false, "import from notion and deploy")
		flag.BoolVar(&flgNoCache, "no-cache", false, "if true, notion import ignores cache")
		flag.BoolVar(&flgCppCheck, "cppcheck", false, "run cppcheck (must be installed)")
		flag.BoolVar(&flgCppCheckAll, "cppcheck-all", false, "run cppcheck with more checks (must be installed)")
		flag.BoolVar(&flgClangTidy, "clang-tidy", false, "run clang-tidy (must be installed)")
		flag.BoolVar(&flgDiff, "diff", false, "preview diff using winmerge")
		flag.BoolVar(&flgGenStructs, "gen-structs", false, "re-generate src/SettingsStructs.h")
		flag.StringVar(&flgUpdateVer, "update-auto-update-ver", "", "update version used for auto-update checks")
		flag.Parse()
	}

	// early check so we don't find it out only after 20 minutes of building
	if flgUpload || flgUploadCiBuild {
		if shouldSignAndUpload() {
			panicIf(!hasSpacesCreds())
			panicIf(!hasS3Creds())
		}
	}

	if flgWebsiteRun {
		websiteRunLocally()
		return
	}

	if flgWebsiteImportNotion {
		websiteImportNotion()
		return
	}

	if flgWebsiteDeployDev {
		websiteDeployDev()
		return
	}

	if flgWebsiteDeployProd {
		websiteDeployProd()
		return
	}

	if flgWebsiteImportAndDeploy {
		websiteImportNotion()
		u.CdUpDir("sumatrapdf")
		websiteDeployProd()
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
		triggerCodeQL()
		return
	}

	if flgTriggerPreRel {
		triggerPreRelBuild()
		triggerRaMicroPreRelBuild()
		return
	}

	if flgTriggerRaMicroPreRel {
		triggerRaMicroPreRelBuild()
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
		previewCrashes()
		return
	}

	if flgCppCheck || flgCppCheckAll {
		runCppCheck(flgCppCheckAll)
		return
	}

	if flgClangTidy {
		runClangTidy()
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
		case githubEventNone, githubEventTypeCodeQL:
			// daily build on push
			buildDaily()
		case githubEventTypeBuildPreRel:
			buildPreRelease()
		case githubEventTypeBuildRaMicroPreRel:
			buildRaMicroPreRelease()
		default:
			panic("unkown value from getGitHubEventType()")
		}
		return
	}

	if flgDeleteOldBuilds {
		fmt.Printf("delete old builds\n")
		minioDeleteOldBuilds()
		s3DeleteOldBuilds()
		return
	}

	// on GitHub Actions the build happens in an earlier step
	if flgUploadCiBuild {
		if shouldSkipUpload() {
			fmt.Printf("Skipping upload\n")
			return
		}
		flgUpload = true
		detectVersions()

		gev := getGitHubEventType()
		switch gev {
		case githubEventNone:
			// daily build on push
			s3UploadBuildMust(buildTypeDaily)
			spacesUploadBuildMust(buildTypeDaily)
		case githubEventTypeBuildPreRel:
			s3UploadBuildMust(buildTypePreRel)
			spacesUploadBuildMust(buildTypePreRel)
		case githubEventTypeBuildRaMicroPreRel:
			spacesUploadBuildMust(buildTypeRaMicro)
		case githubEventTypeCodeQL:
			// do nothing
		default:
			panic("unkown value from getGitHubEventType()")
		}

		minioDeleteOldBuilds()
		s3DeleteOldBuilds()
		return
	}

	if flgBuildRelease {
		failIfNoCertPwd()
		detectVersions()
		buildRelease()
		if flgUpload {
			s3UploadBuildMust(buildTypeRel)
			spacesUploadBuildMust(buildTypeRel)
		}
		return
	}

	if flgBuildReleaseFast {
		failIfNoCertPwd()
		detectVersions()
		buildReleaseFast()
		return
	}

	if flgBuildRelease32Fast {
		failIfNoCertPwd()
		detectVersions()
		buildRelease32Fast()
		return
	}

	if flgBuildPreRelease {
		// make sure we can sign the executables
		failIfNoCertPwd()
		detectVersions()
		buildPreRelease()
		s3UploadBuildMust(buildTypePreRel)
		spacesUploadBuildMust(buildTypePreRel)
		return
	}

	if flgBuildRaMicroPreRelease {
		// make sure we can sign the executables
		failIfNoCertPwd()
		detectVersions()
		buildRaMicroPreRelease()
		//s3UploadBuildMust(buildTypeRaMicro)
		//spacesUploadBuildMust(buildTypeRaMicro)
		return
	}

	if flgUpdateVer != "" {
		updateAutoUpdateVer(flgUpdateVer)
		return
	}

	flag.Usage()
}
