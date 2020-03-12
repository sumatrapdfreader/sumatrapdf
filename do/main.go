package main

import (
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"time"

	"github.com/kjk/u"
)

var (
	flgNoCleanCheck bool
	flgUpload       bool
)

func regenPremake() {
	premakePath := filepath.Join("bin", "premake5.exe")
	{
		cmd := exec.Command(premakePath, "vs2019")
		u.RunCmdLoggedMust(cmd)
	}
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

	// TODO: not sure if adding Windows SDK include  path helps.
	// It takes a lot of time and doesn't seem to provide value
	//winSdkIncludeDir := `C:\Program Files (x86)\Windows Kits\10\Include\10.0.18362.0\um`
	// "-I", winSdkIncludeDir
	// "-D__RPCNDR_H_VERSION__=440"

	if all {
		cmd = exec.Command("cppcheck", "--platform=win64", "-DWIN32", "-D_WIN32", "-D_MSC_VER=1800", "-D_M_X64", "-DIFACEMETHODIMP_(x)=x", "-DSTDAPI_(x)=x", "-q", "-v", "--enable=style", "--suppress=constParameter", "--suppress=cstyleCast", "--suppress=useStlAlgorithm", "--suppress=noExplicitConstructor", "--suppress=variableScope", "--suppress=memsetClassFloat", "--inline-suppr", "-I", "src", "-I", "src/utils", "src")
	} else {
		cmd = exec.Command("cppcheck", "--platform=win64", "-DWIN32", "-D_WIN32", "-D_MSC_VER=1800", "-D_M_X64", "-DIFACEMETHODIMP_(x)=x", "-DSTDAPI_(x)=x", "-q", "-v", "--inline-suppr", "-I", "src", "-I", "src/utils", "src")
	}
	u.RunCmdLoggedMust(cmd)
}

func isOnRepoDispatch() bool {
	v := os.Getenv("GITHUB_EVENT_NAME")
	return v == "repository_dispatch"
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
		flgBuildRelease            bool
		flgBuildReleaseFast        bool
		flgSmoke                   bool
		flgClangFormat             bool
		flgFormat                  bool
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
		flgWebsiteRun              bool
		flgWebsiteDeployProd       bool
		flgWebsiteDeployDev        bool
		flgWebsiteImportNotion     bool
		flgNoCache                 bool
		flgCppCheck                bool
		flgCppCheckAll             bool
		flgDiff                    bool
		flgGenStructs              bool
	)

	{
		flag.BoolVar(&flgRegenPremake, "premake", false, "regenerate premake*.lua files")
		flag.BoolVar(&flgCIBuild, "ci", false, "run CI steps")
		flag.BoolVar(&flgUploadCiBuild, "ci-upload", false, "upload the result of ci build to s3 and do spaces")
		flag.BoolVar(&flgSmoke, "smoke", false, "run smoke build (installer for 64bit release)")
		flag.BoolVar(&flgBuildPreRelease, "build-pre-rel", false, "build pre-release")
		flag.BoolVar(&flgBuildRelease, "build-release", false, "build release")
		flag.BoolVar(&flgBuildReleaseFast, "build-release-fast", false, "build a fast subset of relese for testing")
		flag.BoolVar(&flgBuildLzsa, "build-lzsa", false, "build MakeLZSA.exe")
		flag.BoolVar(&flgNoCleanCheck, "no-clean-check", false, "allow running if repo has changes (for testing build script)")
		flag.BoolVar(&flgUpload, "upload", false, "upload the build to s3 and do spaces")
		flag.BoolVar(&flgClangFormat, "clang-format", false, "format source files with clang-format")
		flag.BoolVar(&flgClangFormat, "format", false, "format source files with clang-format")
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
		flag.BoolVar(&flgWebsiteRun, "website-run", false, "preview website locally")
		flag.BoolVar(&flgWebsiteDeployProd, "website-deploy-prod", false, "deploy website")
		flag.BoolVar(&flgWebsiteDeployDev, "website-deploy-dev", false, "deploy a preview of website")
		flag.BoolVar(&flgWebsiteImportNotion, "website-import-notion", false, "import docs from notion")
		flag.BoolVar(&flgNoCache, "no-cache", false, "if true, notion import ignores cache")
		flag.BoolVar(&flgCppCheck, "cppcheck", false, "run cppcheck (must be installed)")
		flag.BoolVar(&flgCppCheckAll, "cppcheck-all", false, "run cppcheck with more checks (must be installed)")
		flag.BoolVar(&flgDiff, "diff", false, "preview diff using winmerge")
		flag.BoolVar(&flgGenStructs, "gen-structs", false, "re-generate src/SettingsStructs.h")
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

	if flgTriggerPreRel {
		triggerPreRelBuild()
		return
	}

	if flgClean {
		clean()
		return
	}

	if flgClangFormat || flgFormat {
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
		// ci build does the same thing as pre-release
		if shouldSignAndUpload() {
			failIfNoCertPwd()
		}
		detectVersions()
		if isOnRepoDispatch() {
			buildPreRelease()
			return
		}
		buildDaily()
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

		if isOnRepoDispatch() {
			s3UploadBuildMust(buildTypePreRel)
			spacesUploadBuildMust(buildTypePreRel)
			spacesUploadBuildMust(buildTypeRaMicro)
		} else {
			s3UploadBuildMust(buildTypeDaily)
			spacesUploadBuildMust(buildTypeDaily)
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

	if flgBuildPreRelease {
		// make sure we can sign the executables
		failIfNoCertPwd()
		detectVersions()
		buildPreRelease()
		s3UploadBuildMust(buildTypePreRel)
		spacesUploadBuildMust(buildTypePreRel)
		return
	}

	flag.Usage()
}
