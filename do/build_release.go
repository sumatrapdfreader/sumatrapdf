package main

import (
	"fmt"
	"path/filepath"

	"github.com/kjk/u"
)

func getFileNamesWithPrefix(prefix string) [][]string {
	files := [][]string{
		{"SumatraPDF.exe", fmt.Sprintf("%s.exe", prefix)},
		{"SumatraPDF.zip", fmt.Sprintf("%s.zip", prefix)},
		{"SumatraPDF-dll.exe", fmt.Sprintf("%s-install.exe", prefix)},
		{"SumatraPDF.pdb.zip", fmt.Sprintf("%s.pdb.zip", prefix)},
		{"SumatraPDF.pdb.lzsa", fmt.Sprintf("%s.pdb.lzsa", prefix)},
	}
	return files
}

func copyBuiltFiles(dstDir string, srcDir string, prefix string) {
	files := getFileNamesWithPrefix(prefix)
	for _, f := range files {
		srcName := f[0]
		srcPath := filepath.Join(srcDir, srcName)
		dstName := f[1]
		dstPath := filepath.Join(dstDir, dstName)
		u.CreateDirForFileMust(dstPath)
		u.CopyFileMust(dstPath, srcPath)
	}
}

func copyBuiltManifest(dstDir string, prefix string) {
	srcPath := filepath.Join(artifactsDir, "manifest.txt")
	dstName := prefix + "-manifest.txt"
	dstPath := filepath.Join(dstDir, dstName)
	u.CopyFileMust(dstPath, srcPath)
}

func copyBuiltFilesAll(dstDir string, prefix string) {
	copyBuiltFiles(dstDir, rel32Dir, prefix)
	copyBuiltFiles(dstDir, rel64Dir, prefix+"-64")
	copyBuiltManifest(dstDir, prefix)
}

func copyBuiltFiles64(dstDir string, prefix string) {
	copyBuiltFiles(dstDir, rel64Dir, prefix+"-64")
	copyBuiltManifest(dstDir, prefix)
}

func copyBuiltFilesRa64(dstDir string, prefix string) {
	copyBuiltFiles(dstDir, rel64RaDir, prefix+"-64")
	copyBuiltManifest(dstDir, prefix)
}

func build(dir, config, platform string) {
	msbuildPath := detectMsbuildPath()
	slnPath := filepath.Join("vs2019", "SumatraPDF.sln")

	p := fmt.Sprintf(`/p:Configuration=%s;Platform=%s`, config, platform)
	runExeLoggedMust(msbuildPath, slnPath, `/t:test_util`, p, `/m`)
	runTestUtilMust(dir)

	runExeLoggedMust(msbuildPath, slnPath, `/t:SumatraPDF;SumatraPDF-dll;PdfFilter;PdfPreview`, p, `/m`)
	signFilesMust(dir)
	createPdbZipMust(dir)
	createPdbLzsaMust(dir)
}

func build32() {
	build(rel32Dir, "Release", "Win32")
}

func build64() {
	build(rel64Dir, "Release", "x64")
}

func buildRa64() {
	build(rel64RaDir, "Release", "x64_ramicro")
}

func buildRelease() {
	detectSigntoolPath() // early exit if missing

	ver := getVerForBuildType(buildTypeRel)
	s := fmt.Sprintf("buidling release version %s", ver)
	defer makePrintDuration(s)()

	verifyGitCleanMust()
	verifyOnReleaseBranchMust()
	verifyTranslationsMust()

	verifyBuildNotInS3ShortMust(buildTypeRel)
	verifyBuildNotInSpacesShortMust(buildTypeRel)

	clean()
	setBuildConfigRelease()
	defer revertBuildConfig()

	build32()
	nameInZip := fmt.Sprintf("SumatraPDF-%s-32.exe", ver)
	createExeZipWithGoWithNameMust(rel32Dir, nameInZip)

	build64()
	nameInZip = fmt.Sprintf("SumatraPDF-%s-64.exe", ver)
	createExeZipWithGoWithNameMust(rel64Dir, nameInZip)

	createManifestMust()

	dstDir := filepath.Join("out", "final-rel")
	prefix := fmt.Sprintf("SumatraPDF-%s", ver)
	copyBuiltFilesAll(dstDir, prefix)
}
