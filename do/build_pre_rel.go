package main

import (
	"fmt"
	"path/filepath"
)

func buildPreRelease() {
	// early exit if missing
	detectSigntoolPath()
	msbuildPath := detectMsbuildPath()

	preReleaseVer := getPreReleaseVer()
	s := fmt.Sprintf("buidling pre-release version %s", preReleaseVer)
	defer makePrintDuration(s)()
	verifyGitCleanMust()
	verifyOnMasterBranchMust()

	verifyTranslationsMust()
	clean()

	isDaily := false
	setBuildConfig(isDaily)
	defer revertBuildConfig()

	slnPath := filepath.Join("vs2019", "SumatraPDF.sln")

	// we want to sign files inside the installer, so we have to
	runExeLoggedMust(msbuildPath, slnPath, `/t:SumatraPDF;SumatraPDF-dll;PdfFilter;PdfPreview;test_util`, `/p:Configuration=Release;Platform=Win32`, `/m`)
	runTestUtilMust(rel32Dir)
	signFilesMust(rel32Dir)
	createPdbZipMust(rel32Dir)
	createPdbLzsaMust(rel32Dir)
	{
		// TODO: use pigz for release
		nameInZip := fmt.Sprintf("SumatraPDF-prerel-%s-32.exe", preReleaseVer)
		createExeZipWithGoWithNameMust(rel32Dir, nameInZip)
	}

	runExeLoggedMust(msbuildPath, slnPath, "/t:SumatraPDF;SumatraPDF-dll;PdfFilter;PdfPreview;test_util", "/p:Configuration=Release;Platform=x64", "/m")
	runTestUtilMust(rel64Dir)
	signFilesMust(rel64Dir)
	createPdbZipMust(rel64Dir)
	createPdbLzsaMust(rel64Dir)
	{
		nameInZip := fmt.Sprintf("SumatraPDF-prerel-%s.exe", preReleaseVer)
		createExeZipWithGoWithNameMust(rel64Dir, nameInZip)
	}

	runExeLoggedMust(msbuildPath, slnPath, "/t:SumatraPDF;SumatraPDF-dll;PdfFilter;PdfPreview;test_util", "/p:Configuration=Release;Platform=x64_ramicro", "/m")
	signFilesMust(rel64RaDir)
	createPdbZipMust(rel64RaDir)
	createPdbLzsaMust(rel64RaDir)
	{
		nameInZip := fmt.Sprintf("RAMicroPDFViewer-prerel-%s.exe", preReleaseVer)
		createExeZipWithGoWithNameMust(rel64RaDir, nameInZip)
	}

	createManifestMust()
}
