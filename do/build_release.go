package main

import (
	"fmt"
	"path/filepath"
)

func buildRelease(forUpload bool) {
	detectSigntoolPath() // early exit if missing

	ver := getVerForBuildType(buildTypeRel)
	s := fmt.Sprintf("buidling release version %s", ver)
	defer makePrintDuration(s)()

	if forUpload {
		verifyGitCleanMust()
		verifyOnReleaseBranchMust()
		verifyTranslationsMust()
	}

	verifyBuildNotInS3ShortMust(buildTypeRel)
	verifyBuildNotInSpacesShortMust(buildTypeRel)

	clean()
	setBuildConfigRelease()
	defer revertBuildConfig()

	build(rel32Dir, "Release", "Win32")
	nameInZip := fmt.Sprintf("SumatraPDF-%s-32.exe", ver)
	createExeZipWithGoWithNameMust(rel32Dir, nameInZip)

	build(rel64Dir, "Release", "x64")
	nameInZip = fmt.Sprintf("SumatraPDF-%s-64.exe", ver)
	createExeZipWithGoWithNameMust(rel64Dir, nameInZip)

	createManifestMust()

	dstDir := filepath.Join("out", "final-rel")
	prefix := fmt.Sprintf("SumatraPDF-%s", ver)
	copyBuiltFiles(dstDir, rel32Dir, prefix)
	copyBuiltFiles(dstDir, rel64Dir, prefix+"-64")
	copyBuiltManifest(dstDir, prefix)
}

func buildJustInstaller(dir, config, platform string) {
	msbuildPath := detectMsbuildPath()
	slnPath := filepath.Join("vs2019", "SumatraPDF.sln")

	p := fmt.Sprintf(`/p:Configuration=%s;Platform=%s`, config, platform)
	runExeLoggedMust(msbuildPath, slnPath, `/t:SumatraPDF-dll:Rebuild;PdfFilter:Rebuild;PdfPreview:Rebuild`, p, `/m`)
	signFilesOptional(dir)
}

// a faster release build for testing that only does 64-bit installer
func buildReleaseFast() {
	detectSigntoolPath() // early exit if missing

	ver := getVerForBuildType(buildTypeRel)
	s := fmt.Sprintf("buidling release version %s", ver)
	defer makePrintDuration(s)()

	if !isGitClean() {
		logf("note: unsaved git changes\n")
	}
	//verifyOnReleaseBranchMust()

	//verifyBuildNotInS3ShortMust(buildTypeRel)
	//verifyBuildNotInSpacesShortMust(buildTypeRel)

	clean()
	setBuildConfigRelease()
	defer revertBuildConfig()

	buildJustInstaller(rel64Dir, "Release", "x64")

	dstDir := filepath.Join("out", "final-rel-fast")
	prefix := fmt.Sprintf("SumatraPDF-%s", ver)
	copyBuiltFiles(dstDir, rel64Dir, prefix+"-64")
}

// a faster release build for testing that only does 32-bit installer
func buildRelease32Fast() {
	detectSigntoolPath() // early exit if missing

	ver := getVerForBuildType(buildTypeRel)
	s := fmt.Sprintf("buidling release version %s", ver)
	defer makePrintDuration(s)()

	if !isGitClean() {
		logf("%s", "note: unsaved git changes\n")
	}
	//verifyOnReleaseBranchMust()

	//verifyBuildNotInS3ShortMust(buildTypeRel)
	//verifyBuildNotInSpacesShortMust(buildTypeRel)

	clean()
	setBuildConfigRelease()
	defer revertBuildConfig()

	buildJustInstaller(rel32Dir, "Release", "Win32")

	dstDir := filepath.Join("out", "final-rel32-fast")
	prefix := fmt.Sprintf("SumatraPDF-%s", ver)
	copyBuiltFiles(dstDir, rel32Dir, prefix+"-32")
}
