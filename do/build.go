package main

import (
	"archive/zip"
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/kjk/u"
)

var (
	pdbFiles = []string{"libmupdf.pdb", "SumatraPDF-dll.pdb", "SumatraPDF.pdb"}
)

var (
	preReleaseVerCached string
	gitSha1Cached       string
	sumatraVersion      string
	versionCheckVer     string
)

func getGitSha1() string {
	panicIf(gitSha1Cached == "", "must call detectVersions() first")
	return gitSha1Cached
}

func getPreReleaseVer() string {
	panicIf(preReleaseVerCached == "", "must call detectVersions() first")
	return preReleaseVerCached
}

func isNum(s string) bool {
	_, err := strconv.Atoi(s)
	return err == nil
}

// Version must be in format x.y.z
func verifyCorrectVersionMust(ver string) {
	parts := strings.Split(ver, ".")
	panicIf(len(parts) == 0 || len(parts) > 3, "%s is not a valid version number", ver)
	for _, part := range parts {
		panicIf(!isNum(part), "%s is not a valid version number", ver)
	}
}

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
		if fileExists(srcPath) {
			u.CopyFileMust(dstPath, srcPath)
		} else {
			logf("Skipping copying '%s'\n", srcPath)
		}
	}
}

func copyBuiltManifest(dstDir string, prefix string) {
	srcPath := filepath.Join("out", "artifacts", "manifest.txt")
	dstName := prefix + "-manifest.txt"
	dstPath := filepath.Join(dstDir, dstName)
	u.CopyFileMust(dstPath, srcPath)
}

func build(dir, config, platform string) {
	msbuildPath := detectMsbuildPath()
	slnPath := filepath.Join("vs2019", "SumatraPDF.sln")

	p := fmt.Sprintf(`/p:Configuration=%s;Platform=%s`, config, platform)
	runExeLoggedMust(msbuildPath, slnPath, `/t:test_util:Rebuild`, p, `/m`)
	runTestUtilMust(dir)

	runExeLoggedMust(msbuildPath, slnPath, `/t:SumatraPDF:Rebuild;SumatraPDF-dll:Rebuild;PdfFilter:Rebuild;PdfPreview:Rebuild`, p, `/m`)
	signFilesMust(dir)
	createPdbZipMust(dir)
	createPdbLzsaMust(dir)
}

func extractSumatraVersionMust() string {
	path := filepath.Join("src", "Version.h")
	lines, err := u.ReadLinesFromFile(path)
	must(err)
	s := "#define CURR_VERSION "
	for _, l := range lines {
		if strings.HasPrefix(l, s) {
			ver := l[len(s):]
			verifyCorrectVersionMust(ver)
			return ver
		}
	}
	panic(fmt.Sprintf("couldn't extract CURR_VERSION from %s\n", path))
}

// extract version_check_${VER}
// convert from "3_4" => "3.4"
func extractVersionCheckVerPathMust(path string) string {
	lines, err := u.ReadLinesFromFile(path)
	must(err)
	s := "version_check_"
	for _, l := range lines {
		idx := strings.Index(l, s)
		if idx < 0 {
			continue
		}
		l = l[idx+len(s):]
		idx = strings.Index(l, "(")
		if idx <= 0 {
			idx = len(l)
		}
		ver := l[:idx]
		ver = strings.Replace(ver, "_", ".", -1)
		verifyCorrectVersionMust(ver)
		return ver
	}
	panic(fmt.Sprintf("couldn't extract version_check_${VER} from %s\n", path))
}

func extractVersionCheckVerMust() string {
	path := filepath.Join("ext", "mupdf_load_system_font.c")
	ver1 := extractVersionCheckVerPathMust(path)
	path = filepath.Join("src", "libmupdf.def")
	ver2 := extractVersionCheckVerPathMust(path)
	panicIf(ver1 != ver2, "ver1 != ver2 ('%s' != '%s')", ver1, ver2)
	return ver1
}

func detectVersions() {
	ver := getGitLinearVersionMust()
	preReleaseVerCached = strconv.Itoa(ver)
	gitSha1Cached = getGitSha1Must()
	sumatraVersion = extractSumatraVersionMust()
	versionCheckVer = extractVersionCheckVerMust()
	logf("preReleaseVer: '%s'\n", preReleaseVerCached)
	logf("gitSha1: '%s'\n", gitSha1Cached)
	logf("sumatraVersion: '%s'\n", sumatraVersion)
	logf("versionCheckVer: '%s'\n", versionCheckVer)
	parts := strings.Split(versionCheckVer, ".")
	panicIf(len(parts) != 2, "invalid versionCheckVer (%s), must be x.y", versionCheckVer)
	ok := strings.HasPrefix(sumatraVersion, versionCheckVer)
	panicIf(!ok, "invalid versionCheckVer compared to sumatraVersion")
}

// remove all files and directories under out/ except settings files
func clean() {
	entries, err := os.ReadDir("out")
	if err != nil {
		// assuming 'out' doesn't exist, which is fine
		return
	}
	nSkipped := 0
	nDirsDeleted := 0
	nFilesDeleted := 0
	for _, e := range entries {
		path := filepath.Join("out", e.Name())
		if !e.IsDir() {
			os.Remove(path)
			continue
		}
		entries2, err := os.ReadDir(path)
		must(err)
		for _, e2 := range entries2 {
			name := e2.Name()
			path2 := filepath.Join(path, name)
			// delete everything except those files
			excluded := (name == "sumatrapdfcache") || (name == "SumatraPDF-settings.txt")
			if excluded {
				nSkipped++
				continue
			}
			if e2.IsDir() {
				os.RemoveAll(path2)
				nDirsDeleted++
			} else {
				os.Remove(path2)
				nFilesDeleted++
			}
		}
	}
	logf("clean: skipped %d files, deleted %d dirs and %d files\n", nSkipped, nDirsDeleted, nFilesDeleted)
}

func runTestUtilMust(dir string) {
	cmd := exec.Command(`.\test_util.exe`)
	cmd.Dir = dir
	runCmdLoggedMust(cmd)
}

func buildLzsa() {
	// early exit if missing
	detectSigntoolPath()

	defer makePrintDuration("buildLzsa")()
	clean()

	msbuildPath := detectMsbuildPath()
	runExeLoggedMust(msbuildPath, `vs2019\MakeLZSA.sln`, `/t:MakeLZSA:Rebuild`, `/p:Configuration=Release;Platform=Win32`, `/m`)

	path := filepath.Join("out", "rel32", "MakeLZSA.exe")
	signMust(path)
	logf("build and signed '%s'\n", path)
}

// smoke build is meant to be run locally to check that we can build everything
// it does full installer build of 64-bit release build
// We don't build other variants for speed. It takes about 5 mins locally
func buildSmoke() {
	detectSigntoolPath()
	defer makePrintDuration("smoke build")()
	clean()

	lzsa := absPathMust(filepath.Join("bin", "MakeLZSA.exe"))
	panicIf(!fileExists(lzsa), "file '%s' doesn't exist", lzsa)

	msbuildPath := detectMsbuildPath()
	runExeLoggedMust(msbuildPath, `vs2019\SumatraPDF.sln`, `/t:SumatraPDF-dll:Rebuild;test_util:Rebuild`, `/p:Configuration=Release;Platform=x64`, `/m`)
	outDir := filepath.Join("out", "rel64")
	runTestUtilMust(outDir)

	{
		cmd := exec.Command(lzsa, "SumatraPDF.pdb.lzsa", "libmupdf.pdb:libmupdf.pdb", "SumatraPDF-dll.pdb:SumatraPDF-dll.pdb")
		cmd.Dir = outDir
		runCmdLoggedMust(cmd)
	}
	signFilesMust(outDir)
}

func buildConfigPath() string {
	return filepath.Join("src", "utils", "BuildConfig.h")
}

func getBuildConfigCommon() string {
	sha1 := getGitSha1()
	s := fmt.Sprintf("#define GIT_COMMIT_ID %s\n", sha1)
	todayDate := time.Now().Format("2006-01-02")
	s += fmt.Sprintf("#define BUILT_ON %s\n", todayDate)
	return s
}

// writes src/utils/BuildConfig.h to over-ride some of build settings
func setBuildConfigPreRelease() {
	s := getBuildConfigCommon()
	preRelVer := getPreReleaseVer()
	s += fmt.Sprintf("#define PRE_RELEASE_VER %s\n", preRelVer)
	writeFileMust(buildConfigPath(), []byte(s))
}

func setBuildConfigRelease() {
	s := getBuildConfigCommon()
	err := ioutil.WriteFile(buildConfigPath(), []byte(s), 0644)
	must(err)
}

func revertBuildConfig() {
	runExeMust("git", "checkout", buildConfigPath())
}

func addZipFileWithNameMust(w *zip.Writer, path, nameInZip string) {
	fi, err := os.Stat(path)
	must(err)
	fih, err := zip.FileInfoHeader(fi)
	must(err)
	fih.Name = nameInZip
	fih.Method = zip.Deflate
	d, err := ioutil.ReadFile(path)
	must(err)
	fw, err := w.CreateHeader(fih)
	must(err)
	_, err = fw.Write(d)
	must(err)
	// fw is just a io.Writer so we can't Close() it. It's not necessary as
	// it's implicitly closed by the next Create(), CreateHeader()
	// or Close() call on zip.Writer
}

func addZipFileMust(w *zip.Writer, path string) {
	nameInZip := filepath.Base(path)
	addZipFileWithNameMust(w, path, nameInZip)
}

func createExeZipWithGoWithNameMust(dir, nameInZip string) {
	zipPath := filepath.Join(dir, "SumatraPDF.zip")
	os.Remove(zipPath) // called multiple times during upload
	f, err := os.Create(zipPath)
	must(err)
	defer f.Close()
	zw := zip.NewWriter(f)
	path := filepath.Join(dir, "SumatraPDF.exe")
	addZipFileWithNameMust(zw, path, nameInZip)
	err = zw.Close()
	must(err)
}

func createExeZipWithPigz(dir string) {
	srcFile := "SumatraPDF.exe"
	srcPath := filepath.Join(dir, srcFile)
	fatalIf(!fileExists(srcPath), "file '%s' doesn't exist\n", srcPath)

	// this is the file that pigz.exe will create
	dstFileTmp := "SumatraPDF.exe.zip"
	dstPathTmp := filepath.Join(dir, dstFileTmp)
	removeFileMust(dstPathTmp)

	// this is the file we want at the end
	dstFile := "SumatraPDF.zip"
	dstPath := filepath.Join(dir, dstFile)
	removeFileMust(dstPath)

	wd, err := os.Getwd()
	must(err)
	pigzExePath := filepath.Join(wd, "bin", "pigz.exe")
	fatalIf(!fileExists(pigzExePath), "file '%s' doesn't exist\n", pigzExePath)
	cmd := exec.Command(pigzExePath, "-11", "--keep", "--zip", srcFile)
	// in pigz we don't control the name of the file created inside so
	// so when we run pigz the current directory is the same as
	// the directory with the file we're compressing
	cmd.Dir = dir
	u.RunCmdMust(cmd)

	fatalIf(!fileExists(dstPathTmp), "file '%s' doesn't exist\n", dstPathTmp)
	err = os.Rename(dstPathTmp, dstPath)
	must(err)
}

func createPdbZipMust(dir string) {
	path := filepath.Join(dir, "SumatraPDF.pdb.zip")
	f, err := os.Create(path)
	must(err)
	defer f.Close()
	w := zip.NewWriter(f)

	for _, file := range pdbFiles {
		addZipFileMust(w, filepath.Join(dir, file))
	}

	err = w.Close()
	must(err)
}

func createPdbLzsaMust(dir string) {
	args := []string{"SumatraPDF.pdb.lzsa"}
	args = append(args, pdbFiles...)
	curDir, err := os.Getwd()
	must(err)
	makeLzsaPath := filepath.Join(curDir, "bin", "MakeLZSA.exe")
	cmd := exec.Command(makeLzsaPath, args...)
	cmd.Dir = dir
	runCmdLoggedMust(cmd)
}

// manifest is build for pre-release builds and contains information about file sizes
func createManifestMust() {
	var lines []string
	files := []string{
		"SumatraPDF.exe",
		"SumatraPDF.zip",
		"SumatraPDF-dll.exe",
		"libmupdf.dll",
		"PdfFilter.dll",
		"PdfPreview.dll",
		"SumatraPDF.pdb.zip",
		"SumatraPDF.pdb.lzsa",
	}
	dirs := []string{rel32Dir, rel64Dir}
	// in daily build, there's no 32bit build
	if !u.PathExists(rel32Dir) {
		dirs = []string{rel64Dir}
	}
	for _, dir := range dirs {
		for _, file := range files {
			path := filepath.Join(dir, file)
			size := fileSizeMust(path)
			line := fmt.Sprintf("%s: %d", path, size)
			lines = append(lines, line)
		}
	}

	s := strings.Join(lines, "\n")
	artifactsDir := filepath.Join("out", "artifacts")
	u.CreateDirMust(artifactsDir)
	path := filepath.Join(artifactsDir, "manifest.txt")
	writeFileMust(path, []byte(s))
}

// https://lucasg.github.io/2018/01/15/Creating-an-appx-package/
// https://docs.microsoft.com/en-us/windows/win32/appxpkg/make-appx-package--makeappx-exe-#to-create-a-package-using-a-mapping-file
// https://github.com/lucasg/Dependencies/tree/master/DependenciesAppx
// https://docs.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-packaging-dot-net
func makeAppx() {
	appExePath := detectMakeAppxPath()
	logf("makeAppx: '%s'\n", appExePath)
}

func signFilesMust(dir string) {
	if fileExists(filepath.Join(dir, "SumatraPDF.exe")) {
		signMust(filepath.Join(dir, "SumatraPDF.exe"))
	}
	signMust(filepath.Join(dir, "libmupdf.dll"))
	signMust(filepath.Join(dir, "PdfFilter.dll"))
	signMust(filepath.Join(dir, "PdfPreview.dll"))
	signMust(filepath.Join(dir, "SumatraPDF-dll.exe"))
}

func signFilesOptional(dir string) {
	if !hasCertPwd() {
		return
	}
	signFilesMust(dir)
}

func buildPreRelease() {
	detectSigntoolPath() // early exit if missing

	ver := getVerForBuildType(buildTypePreRel)
	s := fmt.Sprintf("buidling pre-release version %s", ver)
	defer makePrintDuration(s)()

	clean()
	setBuildConfigPreRelease()
	defer revertBuildConfig()

	build(rel32Dir, "Release", "Win32")
	nameInZip := fmt.Sprintf("SumatraPDF-prerel-%s-32.exe", ver)
	createExeZipWithGoWithNameMust(rel32Dir, nameInZip)

	build(rel64Dir, "Release", "x64")
	nameInZip = fmt.Sprintf("SumatraPDF-prerel-%s-64.exe", ver)
	createExeZipWithGoWithNameMust(rel64Dir, nameInZip)

	createManifestMust()

	dstDir := filepath.Join("out", "final-prerel")
	prefix := "SumatraPDF-prerel"
	copyBuiltFiles(dstDir, rel32Dir, prefix)
	copyBuiltFiles(dstDir, rel64Dir, prefix+"-64")
	copyBuiltManifest(dstDir, prefix)
}

func buildRelease() {
	detectSigntoolPath() // early exit if missing

	ver := getVerForBuildType(buildTypeRel)
	s := fmt.Sprintf("buidling release version %s", ver)
	defer makePrintDuration(s)()

	verifyBuildNotInS3ShortMust(buildTypeRel)
	verifyBuildNotInSpacesMust(buildTypeRel)

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

func buildJustPortableExe(dir, config, platform string) {
	msbuildPath := detectMsbuildPath()
	slnPath := filepath.Join("vs2019", "SumatraPDF.sln")

	p := fmt.Sprintf(`/p:Configuration=%s;Platform=%s`, config, platform)
	runExeLoggedMust(msbuildPath, slnPath, `/t:SumatraPDF`, p, `/m`)
	signFilesOptional(dir)
}

func buildLogview() {
	msbuildPath := detectMsbuildPath()
	slnPath := filepath.Join("vs2019", "SumatraPDF.sln")

	config := "Release"
	platform := "x64"
	p := fmt.Sprintf(`/p:Configuration=%s;Platform=%s`, config, platform)
	runExeLoggedMust(msbuildPath, slnPath, `/t:logview:Rebuild`, p, `/m`)
}

func buildTestUtil() {
	msbuildPath := detectMsbuildPath()
	slnPath := filepath.Join("vs2019", "SumatraPDF.sln")

	config := "Release"
	platform := "x64"
	p := fmt.Sprintf(`/p:Configuration=%s;Platform=%s`, config, platform)
	runExeLoggedMust(msbuildPath, slnPath, `/t:test_util:Rebuild`, p, `/m`)
}
