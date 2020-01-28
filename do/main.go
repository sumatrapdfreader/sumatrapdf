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
	)

	{
		flag.BoolVar(&flgRegenPremake, "premake", false, "regenerate premake*.lua files")
		flag.BoolVar(&flgCIBuild, "ci", false, "run CI steps")
		flag.BoolVar(&flgUploadCiBuild, "ci-upload", false, "upload the result of ci build to s3 and do spaces")
		flag.BoolVar(&flgSmoke, "smoke", false, "run smoke build (installer for 64bit release)")
		flag.BoolVar(&flgBuildPreRelease, "build-pre-rel", false, "build pre-release")
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
		flag.Parse()
	}

	// early check so we don't find it out only after 20 minutes of building
	if flgUpload || flgUploadCiBuild {
		if shouldSignAndUpload() {
			panicIf(!hasSpacesCreds())
			panicIf(!hasS3Creds())
		}
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
			s3UploadPreReleaseMust(buildTypePreRel)
			spacesUploadPreReleaseMust(buildTypePreRel)
			spacesUploadPreReleaseMust(buildTypeRaMicro)
		} else {
			s3UploadPreReleaseMust(buildTypeDaily)
			spacesUploadPreReleaseMust(buildTypeDaily)
		}

		minioDeleteOldBuilds()
		s3DeleteOldBuilds()
		return
	}

	if flgBuildPreRelease {
		if flgUpload {
			// if uploading, make sure we can sign
			failIfNoCertPwd()
		}
		detectVersions()
		buildDaily()
		s3UploadPreReleaseMust(buildTypePreRel)
		spacesUploadPreReleaseMust(buildTypePreRel)
		return
	}

	flag.Usage()
}
