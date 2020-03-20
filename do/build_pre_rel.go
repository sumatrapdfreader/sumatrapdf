package main

import (
	"fmt"
	"path/filepath"
)

func buildPreRelease() {
	detectSigntoolPath() // early exit if missing

	ver := getVerForBuildType(buildTypePreRel)
	s := fmt.Sprintf("buidling pre-release version %s", ver)
	defer makePrintDuration(s)()

	verifyGitCleanMust()
	verifyOnMasterBranchMust()
	verifyTranslationsMust()

	clean()
	setBuildConfigPreRelease()
	defer revertBuildConfig()

	build(rel32Dir, "Release", "Win32")
	nameInZip := fmt.Sprintf("SumatraPDF-prerel-%s-32.exe", ver)
	createExeZipWithGoWithNameMust(rel32Dir, nameInZip)

	build(rel64Dir, "Release", "x64")
	nameInZip = fmt.Sprintf("SumatraPDF-prerel-%s-64.exe", ver)
	createExeZipWithGoWithNameMust(rel64Dir, nameInZip)

	build(rel64RaDir, "Release", "x64_ramicro")
	nameInZip = fmt.Sprintf("RAMicroPDFViewer-prerel-%s.exe", ver)
	createExeZipWithGoWithNameMust(rel64RaDir, nameInZip)

	createManifestMust()

	dstDir := filepath.Join("out", "final-prerel")
	prefix := fmt.Sprintf("SumatraPDF-prerel-%s", ver)
	copyBuiltFiles(dstDir, rel32Dir, prefix)
	copyBuiltFiles(dstDir, rel64Dir, prefix+"-64")
	copyBuiltManifest(dstDir, prefix)

	// note: manifest won't be for the right files but we don't care
	dstDir = filepath.Join("out", "final-ramicro")
	prefix = fmt.Sprintf("RAMicroPDFViewer-prerel-%s", ver)
	copyBuiltFiles(dstDir, rel64RaDir, prefix+"-64")
	copyBuiltManifest(dstDir, prefix)
}
