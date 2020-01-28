package main

import (
	"fmt"
	"path/filepath"
)

func buildDaily() {
	// early exit if missing
	detectSigntoolPath()
	msbuildPath := detectMsbuildPath()

	preReleaseVer := getPreReleaseVer()
	s := fmt.Sprintf("buidling daily version %s", preReleaseVer)
	defer makePrintDuration(s)()
	verifyGitCleanMust()
	verifyOnMasterBranchMust()

	clean()

	isDaily := true
	setBuildConfig(isDaily)
	defer revertBuildConfig()

	slnPath := filepath.Join("vs2019", "SumatraPDF.sln")
	runExeLoggedMust(msbuildPath, slnPath, "/t:SumatraPDF;SumatraPDF-dll;PdfFilter;PdfPreview;test_util", "/p:Configuration=Release;Platform=x64", "/m")
	runTestUtilMust(rel64Dir)
	signFilesMust(rel64Dir)
	createPdbZipMust(rel64Dir)
	createPdbLzsaMust(rel64Dir)
	{
		nameInZip := fmt.Sprintf("SumatraPDF-prerel-%s.exe", preReleaseVer)
		createExeZipWithGoWithNameMust(rel64Dir, nameInZip)
	}

	createManifestMust()
}
