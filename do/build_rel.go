package main

import (
	"fmt"
	"path/filepath"
)

func buildRelease() {
	// early exit if missing
	detectSigntoolPath()
	msbuildPath := detectMsbuildPath()

	s := fmt.Sprintf("buidling release version %s", sumatraVersion)
	defer makePrintDuration(s)()
	verifyGitCleanMust()
	verifyOnReleaseBranchMust()
	verifyTranslationsMust()

	//verifyReleaseNotInS3Must(sumatraVersion)
	//verifyReleaseNotInSpaces(sumatraVersion)

	isDaily := false
	setBuildConfig(isDaily)
	defer revertBuildConfig()

	slnPath := filepath.Join("vs2019", "SumatraPDF.sln")

	// we want to sign files inside the installer, so we have to
	runExeLoggedMust(msbuildPath, slnPath, `/t:SumatraPDF;SumatraPDF-dll;PdfFilter;PdfPreview;test_util`, `/p:Configuration=Release;Platform=Win32`, `/m`)
	runTestUtilMust(rel32Dir)
	signFilesMust(rel32Dir)

	runExeLoggedMust(msbuildPath, slnPath, "/t:SumatraPDF;SumatraPDF-dll;PdfFilter;PdfPreview;test_util", "/p:Configuration=Release;Platform=x64", "/m")
	runTestUtilMust(rel64Dir)
	signFilesMust(rel64Dir)

	// TODO: also build ramicro?

	preReleaseVer := getPreReleaseVer()
	nameInZip := fmt.Sprintf("SumatraPDF-prerel-%s-32.exe", preReleaseVer)
	createExeZipWithGoWithNameMust(rel32Dir, nameInZip)
	nameInZip = fmt.Sprintf("SumatraPDF-prerel-%s.exe", preReleaseVer)
	createExeZipWithGoWithNameMust(rel64Dir, nameInZip)

	createPdbZipMust(rel32Dir)
	createPdbZipMust(rel64Dir)

	createPdbLzsaMust(rel32Dir)
	createPdbLzsaMust(rel64Dir)

	createManifestMust()
}
