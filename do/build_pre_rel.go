package main

import (
	"fmt"
	"path/filepath"
)

func buildPreRelease() {
	// early exit if missing
	detectSigntoolPath()
	msbuildPath := detectMsbuildPath()

	s := fmt.Sprintf("buidling pre-release version %s", preReleaseVer)
	defer makePrintDuration(s)()
	verifyGitCleanMust()
	verifyOnMasterBranchMust()

	verifyTranslationsMust()
	clean()

	isDaily := false
	setBuildConfig(gitSha1, preReleaseVer, isDaily)
	defer revertBuildConfig()

	slnPath := filepath.Join("vs2019", "SumatraPDF.sln")

	// we want to sign files inside the installer, so we have to
	runExeLoggedMust(msbuildPath, slnPath, `/t:SumatraPDF;SumatraPDF-dll;PdfFilter;PdfPreview;test_util`, `/p:Configuration=Release;Platform=Win32`, `/m`)
	runTestUtilMust(rel32Dir)
	signFilesMust(rel32Dir)

	runExeLoggedMust(msbuildPath, slnPath, "/t:SumatraPDF;SumatraPDF-dll;PdfFilter;PdfPreview;test_util", "/p:Configuration=Release;Platform=x64", "/m")
	runTestUtilMust(rel64Dir)
	signFilesMust(rel64Dir)

	runExeLoggedMust(msbuildPath, slnPath, "/t:SumatraPDF;SumatraPDF-dll;PdfFilter;PdfPreview;test_util", "/p:Configuration=Release;Platform=x64_ramicro", "/m")
	signFilesMust(rel64RaDir)

	// TODO: use pigz for release
	nameInZip := fmt.Sprintf("SumatraPDF-prerel-%s-32.exe", preReleaseVer)
	createExeZipWithGoWithNameMust(rel32Dir, nameInZip)
	nameInZip = fmt.Sprintf("SumatraPDF-prerel-%s.exe", preReleaseVer)
	createExeZipWithGoWithNameMust(rel64Dir, nameInZip)

	nameInZip = fmt.Sprintf("RAMicroPDFViewer-prerel-%s.exe", preReleaseVer)
	createExeZipWithGoWithNameMust(rel64RaDir, nameInZip)

	createPdbZipMust(rel32Dir)
	createPdbZipMust(rel64Dir)
	createPdbZipMust(rel64RaDir)

	createPdbLzsaMust(rel32Dir)
	createPdbLzsaMust(rel64Dir)
	createPdbLzsaMust(rel64RaDir)

	copyArtifacts()
	createManifestMust()
}
