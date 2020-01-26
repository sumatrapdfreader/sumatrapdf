package main

import (
	"fmt"
	"path/filepath"
)

func buildDaily() {
	// early exit if missing
	detectSigntoolPath()
	msbuildPath := detectMsbuildPath()

	s := fmt.Sprintf("buidling daily version %s", preReleaseVer)
	defer makePrintDuration(s)()
	verifyGitCleanMust()
	verifyOnMasterBranchMust()

	clean()

	isDaily := true
	setBuildConfig(gitSha1, preReleaseVer, isDaily)
	defer revertBuildConfig()

	slnPath := filepath.Join("vs2019", "SumatraPDF.sln")

	runExeLoggedMust(msbuildPath, slnPath, "/t:SumatraPDF;SumatraPDF-dll;PdfFilter;PdfPreview;test_util", "/p:Configuration=Release;Platform=x64", "/m")
	runTestUtilMust(rel64Dir)
	signFilesMust(rel64Dir)

	runExeLoggedMust(msbuildPath, slnPath, "/t:SumatraPDF;SumatraPDF-dll;PdfFilter;PdfPreview;test_util", "/p:Configuration=Release;Platform=x64_ramicro", "/m")
	signFilesMust(rel64RaDir)

	nameInZip := fmt.Sprintf("SumatraPDF-prerel-%s.exe", preReleaseVer)
	createExeZipWithGoWithNameMust(rel64Dir, nameInZip)

	nameInZip = fmt.Sprintf("RAMicroPDFViewer-prerel-%s.exe", preReleaseVer)
	createExeZipWithGoWithNameMust(rel64RaDir, nameInZip)

	createPdbZipMust(rel64Dir)
	createPdbZipMust(rel64RaDir)

	createPdbLzsaMust(rel64Dir)
	createPdbLzsaMust(rel64RaDir)

	copyArtifacts()
	createManifestMust()
}
