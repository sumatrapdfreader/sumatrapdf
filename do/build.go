package main

import (
	"archive/zip"
	"errors"
	"fmt"
	"io/fs"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"time"

	"github.com/kjk/common/u"
)

var (
	pdbFiles = []string{"libmupdf.pdb", "SumatraPDF-dll.pdb", "SumatraPDF.pdb"}
)

var (
	preReleaseVerCached string
	gitSha1Cached       string
	sumatraVersion      string
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
		must(createDirForFile(dstPath))
		if fileExists(srcPath) {
			must(copyFile(dstPath, srcPath))
		} else {
			logf("Skipping copying '%s'\n", srcPath)
		}
	}
}

func copyBuiltManifest(dstDir string, prefix string) {
	srcPath := filepath.Join("out", "artifacts", "manifest.txt")
	dstName := prefix + "-manifest.txt"
	dstPath := filepath.Join(dstDir, dstName)
	must(copyFile(dstPath, srcPath))
}

func extractSumatraVersionMust() string {
	path := filepath.Join("src", "Version.h")
	lines, err := readLinesFromFile(path)
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

func detectVersions() {
	ver := getGitLinearVersionMust()
	preReleaseVerCached = strconv.Itoa(ver)
	gitSha1Cached = getGitSha1Must()
	sumatraVersion = extractSumatraVersionMust()
	logf("preReleaseVer: '%s'\n", preReleaseVerCached)
	logf("gitSha1: '%s'\n", gitSha1Cached)
	logf("sumatraVersion: '%s'\n", sumatraVersion)
}

var (
	nSkipped      = 0
	nDirsDeleted  = 0
	nFilesDeleted = 0
)

func clearDirPreserveSettings(path string) {
	entries2, err := os.ReadDir(path)
	if errors.Is(err, fs.ErrNotExist) {
		return
	}
	must(err)
	for _, e2 := range entries2 {
		name := e2.Name()
		path2 := filepath.Join(path, name)
		// delete everything except those files
		excluded := (name == "sumatrapdfcache") || (name == "SumatraPDF-settings.txt") || strings.Contains(name, "asan")
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

// remove all files and directories under out/ except settings files
func cleanPreserveSettings() {
	entries, err := os.ReadDir("out")
	if err != nil {
		// assuming 'out' doesn't exist, which is fine
		return
	}
	nSkipped = 0
	nDirsDeleted = 0
	nFilesDeleted = 0
	for _, e := range entries {
		path := filepath.Join("out", e.Name())
		if !e.IsDir() {
			os.Remove(path)
			continue
		}
		clearDirPreserveSettings(path)
	}
	logf("clean: skipped %d files, deleted %d dirs and %d files\n", nSkipped, nDirsDeleted, nFilesDeleted)
}

func cleanReleaseBuilds() {
	clearDirPreserveSettings(rel32Dir)
	clearDirPreserveSettings(rel64Dir)
	clearDirPreserveSettings(relArm64Dir)
	clearDirPreserveSettings(finalPreRelDir)
}

func removeReleaseBuilds() {
	os.RemoveAll(rel32Dir)
	os.RemoveAll(rel64Dir)
	os.RemoveAll(relArm64Dir)
	os.RemoveAll(finalPreRelDir)
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
	cleanPreserveSettings()

	msbuildPath := detectMsbuildPathMust()
	runExeLoggedMust(msbuildPath, `vs2022\MakeLZSA.sln`, `/t:MakeLZSA:Rebuild`, `/p:Configuration=Release;Platform=Win32`, `/m`)

	path := filepath.Join("out", "rel32", "MakeLZSA.exe")
	signMust(path)
	logf("build and signed '%s'\n", path)
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
	err := os.WriteFile(buildConfigPath(), []byte(s), 0644)
	must(err)
}

func revertBuildConfig() {
	runExeMust("git", "checkout", buildConfigPath())
}

func addZipDataStore(w *zip.Writer, data []byte, nameInZip string) error {
	fih := &zip.FileHeader{
		Name:   nameInZip,
		Method: zip.Store,
	}
	fw, err := w.CreateHeader(fih)
	if err != nil {
		return err
	}
	_, err = fw.Write(data)
	return err
}

func addZipFileWithName(w *zip.Writer, path, nameInZip string) error {
	fi, err := os.Stat(path)
	if err != nil {
		return err
	}
	fih, err := zip.FileInfoHeader(fi)
	if err != nil {
		return err
	}
	fih.Name = nameInZip
	fih.Method = zip.Deflate
	d, err := os.ReadFile(path)
	if err != nil {
		return err
	}
	fw, err := w.CreateHeader(fih)
	if err != nil {
		return err
	}
	_, err = fw.Write(d)
	return err
	// fw is just a io.Writer so we can't Close() it. It's not necessary as
	// it's implicitly closed by the next Create(), CreateHeader()
	// or Close() call on zip.Writer
}

func addZipFile(w *zip.Writer, path string) error {
	nameInZip := filepath.Base(path)
	return addZipFileWithName(w, path, nameInZip)
}

func createExeZipWithGoWithName(dir, nameInZip string) error {
	zipPath := filepath.Join(dir, "SumatraPDF.zip")
	os.Remove(zipPath) // called multiple times during upload
	f, err := os.Create(zipPath)
	if err != nil {
		return err
	}
	defer func() {
		err = f.Close()
	}()
	zw := zip.NewWriter(f)
	path := filepath.Join(dir, "SumatraPDF.exe")
	err = addZipFileWithName(zw, path, nameInZip)
	if err != nil {
		return err
	}
	err = zw.Close()
	if err != nil {
		return err
	}
	return err
}

// func createExeZipWithPigz(dir string) {
// 	srcFile := "SumatraPDF.exe"
// 	srcPath := filepath.Join(dir, srcFile)
// 	panicIf(!fileExists(srcPath), "file '%s' doesn't exist\n", srcPath)

// 	// this is the file that pigz.exe will create
// 	dstFileTmp := "SumatraPDF.exe.zip"
// 	dstPathTmp := filepath.Join(dir, dstFileTmp)
// 	removeFileMust(dstPathTmp)

// 	// this is the file we want at the end
// 	dstFile := "SumatraPDF.zip"
// 	dstPath := filepath.Join(dir, dstFile)
// 	removeFileMust(dstPath)

// 	wd, err := os.Getwd()
// 	must(err)
// 	pigzExePath := filepath.Join(wd, "bin", "pigz.exe")
// 	panicIf(!fileExists(pigzExePath), "file '%s' doesn't exist\n", pigzExePath)
// 	cmd := exec.Command(pigzExePath, "-11", "--keep", "--zip", srcFile)
// 	// in pigz we don't control the name of the file created inside so
// 	// so when we run pigz the current directory is the same as
// 	// the directory with the file we're compressing
// 	cmd.Dir = dir
// 	runCmdMust(cmd)

// 	panicIf(!fileExists(dstPathTmp), "file '%s' doesn't exist\n", dstPathTmp)
// 	err = os.Rename(dstPathTmp, dstPath)
// 	must(err)
// }

func createPdbZip(dir string) error {
	path := filepath.Join(dir, "SumatraPDF.pdb.zip")
	f, err := os.Create(path)
	if err != nil {
		return err
	}
	defer func() {
		err = f.Close()
	}()
	w := zip.NewWriter(f)

	for _, file := range pdbFiles {
		err = addZipFile(w, filepath.Join(dir, file))
		if err != nil {
			return err
		}
	}

	err = w.Close()
	if err != nil {
		return err
	}
	return err
}

func createLzsaFromFiles(lzsaPath string, dir string, files []string) error {
	args := []string{lzsaPath}
	args = append(args, files...)
	curDir, err := os.Getwd()
	if err != nil {
		return err
	}
	makeLzsaPath := filepath.Join(curDir, "bin", "MakeLZSA.exe")
	cmd := exec.Command(makeLzsaPath, args...)
	cmd.Dir = dir
	runCmdLoggedMust(cmd)
	return nil
}

func createPdbLzsa(dir string) error {
	return createLzsaFromFiles("SumatraPDF.pdb.lzsa", dir, pdbFiles)
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
	var dirs []string
	// 32bit / arm64 are only in daily build
	for _, dir := range []string{rel32Dir, rel64Dir, relArm64Dir} {
		if pathExists(dir) {
			dirs = append(dirs, dir)
		}
	}
	panicIf(len(dirs) == 0, "didn't find any dirs for the manifest")
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
	createDirMust(artifactsDir)
	path := filepath.Join(artifactsDir, "manifest.txt")
	writeFileMust(path, []byte(s))
}

// func listFilesInDir(dir string) {
// 	files, err := os.ReadDir(dir)
// 	panicIfErr(err)
// 	for _, de := range files {
// 		i, err := de.Info()
// 		panicIfErr(err)
// 		logf("%s: %d\n", de.Name(), i.Size())
// 	}
// }

func signFilesMust(dir string) {
	if true {
		logf("signFilesMust: '%s' DISABLED\n", dir)
		return
	}
	logf("signFilesMust: '%s'\n", dir)
	//listFilesInDir(dir)

	if fileExists(filepath.Join(dir, "SumatraPDF.exe")) {
		signMust(filepath.Join(dir, "SumatraPDF.exe"))
	}
	signMust(filepath.Join(dir, "libmupdf.dll"))
	signMust(filepath.Join(dir, "PdfFilter.dll"))
	signMust(filepath.Join(dir, "PdfPreview.dll"))
	signMust(filepath.Join(dir, "SumatraPDF-dll.exe"))
}

const (
	kPlatformIntel32 = "Win32"
	kPlatformIntel64 = "x64"
	kPlatformArm64   = "ARM64"
)

var (
	rel32Dir       = filepath.Join("out", "rel32")
	rel64Dir       = filepath.Join("out", "rel64")
	relArm64Dir    = filepath.Join("out", "arm64")
	finalPreRelDir = filepath.Join("out", "final-prerel")
)

func getBuildDirForPlatform(platform string) string {
	switch platform {
	case kPlatformIntel32:
		return "rel32"
	case kPlatformIntel64:
		return "rel64"
	case kPlatformArm64:
		return "arm64"
	default:
		panicIf(true, "unsupported platform '%s'", platform)
	}
	return ""
}

func getOutDirForPlatform(platform string) string {
	switch platform {
	case kPlatformIntel32:
		return rel32Dir
	case kPlatformIntel64:
		return rel64Dir
	case kPlatformArm64:
		return relArm64Dir
	default:
		panicIf(true, "unsupported platform '%s'", platform)
	}
	return ""
}

func build(config, platform string) {
	msbuildPath := detectMsbuildPathMust()
	slnPath := filepath.Join("vs2022", "SumatraPDF.sln")

	dir := getOutDirForPlatform(platform)

	p := fmt.Sprintf(`/p:Configuration=%s;Platform=%s`, config, platform)
	runExeLoggedMust(msbuildPath, slnPath, `/t:test_util:Rebuild`, p, `/m`)
	// can't run arm binaries in x86 CI
	if platform != kPlatformArm64 {
		runTestUtilMust(dir)
	}

	runExeLoggedMust(msbuildPath, slnPath, `/t:SumatraPDF:Rebuild;SumatraPDF-dll:Rebuild;PdfFilter:Rebuild;PdfPreview:Rebuild`, p, `/m`)
	err := createPdbZip(dir)
	must(err)
	err = createPdbLzsa(dir)
	must(err)
}

// builds more targets, even those not used, to prevent code rot
func buildAll(config, platform string) {
	msbuildPath := detectMsbuildPathMust()
	slnPath := filepath.Join("vs2022", "SumatraPDF.sln")

	dir := getOutDirForPlatform(platform)

	p := fmt.Sprintf(`/p:Configuration=%s;Platform=%s`, config, platform)
	runExeLoggedMust(msbuildPath, slnPath, `/t:test_util:Rebuild`, p, `/m`)
	// can't run arm binaries in x86 CI
	if platform != kPlatformArm64 {
		runTestUtilMust(dir)
	}

	runExeLoggedMust(msbuildPath, slnPath, `/t:signfile:Rebuild;sizer:Rebuild;PdfFilter:Rebuild;plugin-test:Rebuild;PdfPreview:Rebuild;PdfPreviewTest:Rebuild;SumatraPDF:Rebuild;SumatraPDF-dll:Rebuild`, p, `/m`)
	err := createPdbZip(dir)
	must(err)
	err = createPdbLzsa(dir)
	must(err)
}

func getSuffixForPlatform(platform string) string {
	switch platform {
	case kPlatformArm64:
		return "arm64"
	case kPlatformIntel32:
		return "32"
	case kPlatformIntel64:
		return "64"
	}
	panicIf(true, "unrecognized platform '%s'", platform)
	return ""
}

// func buildCiDaily(opts *BuildOptions) {
// 	if opts.upload {
// 		isUploaded := isBuildAlreadyUploaded(newMinioBackblazeClient(), buildTypePreRel)
// 		if isUploaded {
// 			logf("buildCiDaily: skipping build because already built and uploaded")
// 			return
// 		}
// 	}

// 	cleanReleaseBuilds()
// 	genHTMLDocsForApp()
// 	buildPreRelease(kPlatformArm64, false)
// 	buildPreRelease(kPlatformIntel32, false)
// 	buildPreRelease(kPlatformIntel64, false)
// }

func buildCi() {
	gev := getGitHubEventType()
	switch gev {
	case githubEventPush:
		removeReleaseBuilds()
		genHTMLDocsForApp()
		// I'm typically building 64-bit so in ci build 32-bit
		// and build all projects, to find regressions in code
		// I'm not regularly building while developing
		buildPreRelease(kPlatformIntel32, true)
	case githubEventTypeCodeQL:
		// code ql is just a regular build, I assume intercepted by
		// by their tooling
		// buildSmoke() runs genHTMLDocsForApp() so no need to do it here
		buildSmoke()
	default:
		panic("unkown value from getGitHubEventType()")
	}
}

func ensureManualIsBuilt() {
	// make sure we've built manual
	path := filepath.Join("docs", "manual.dat")
	size := u.FileSize(path)
	panicIf(size < 2*2024, "size of '%s' is %d which indicates we didn't build it", path, size)
}

func buildPreRelease(platform string, all bool) {
	// make sure we can sign the executables, early exit if missing
	detectSigntoolPath()

	ensureManualIsBuilt()

	ver := getVerForBuildType(buildTypePreRel)
	s := fmt.Sprintf("buidling pre-release version %s", ver)
	defer makePrintDuration(s)()

	setBuildConfigPreRelease()
	defer revertBuildConfig()

	if all {
		buildAll("Release", platform)
	} else {
		build("Release", platform)
	}

	suffix := getSuffixForPlatform(platform)
	outDir := getOutDirForPlatform(platform)
	nameInZip := fmt.Sprintf("SumatraPDF-prerel-%s-%s.exe", ver, suffix)
	createExeZipWithGoWithName(outDir, nameInZip)

	createManifestMust()

	dstDir := getFinalDirForBuildType(buildTypePreRel)
	prefix := "SumatraPDF-prerel"
	copyBuiltFiles(dstDir, outDir, prefix+"-"+suffix)
	copyBuiltManifest(dstDir, prefix)
}

func buildRelease() {
	// make sure we can sign the executables, early exit if missing
	detectSigntoolPath()
	genHTMLDocsForApp()

	ver := getVerForBuildType(buildTypeRel)
	s := fmt.Sprintf("buidling release version %s", ver)
	defer makePrintDuration(s)()

	verifyBuildNotInStorageMust(newMinioR2Client(), buildTypeRel)
	verifyBuildNotInStorageMust(newMinioBackblazeClient(), buildTypeRel)

	removeReleaseBuilds()
	setBuildConfigRelease()
	defer revertBuildConfig()

	build("Release", kPlatformIntel32)
	nameInZip := fmt.Sprintf("SumatraPDF-%s-32.exe", ver)
	createExeZipWithGoWithName(rel32Dir, nameInZip)

	build("Release", kPlatformIntel64)
	nameInZip = fmt.Sprintf("SumatraPDF-%s-64.exe", ver)
	createExeZipWithGoWithName(rel64Dir, nameInZip)

	build("Release", kPlatformArm64)
	nameInZip = fmt.Sprintf("SumatraPDF-%s-arm64.exe", ver)
	createExeZipWithGoWithName(relArm64Dir, nameInZip)

	createManifestMust()

	dstDir := getFinalDirForBuildType(buildTypeRel)
	prefix := fmt.Sprintf("SumatraPDF-%s", ver)
	copyBuiltFiles(dstDir, rel32Dir, prefix)
	copyBuiltFiles(dstDir, rel64Dir, prefix+"-64")
	copyBuiltFiles(dstDir, relArm64Dir, prefix+"-arm64")
	copyBuiltManifest(dstDir, prefix)
}

func detectVersionsCodeQL() {
	//ver := getGitLinearVersionMust()
	ver := 16648 // we don't have git history in codeql checkout
	preReleaseVerCached = strconv.Itoa(ver)
	gitSha1Cached = getGitSha1Must()
	sumatraVersion = extractSumatraVersionMust()
	logf("preReleaseVer: '%s'\n", preReleaseVerCached)
	logf("gitSha1: '%s'\n", gitSha1Cached)
	logf("sumatraVersion: '%s'\n", sumatraVersion)
}

// build for codeql: just static 64-bit release build
func buildCodeQL() {
	detectVersionsCodeQL()
	//cleanPreserveSettings()
	msbuildPath := detectMsbuildPathMust()
	runExeLoggedMust(msbuildPath, `vs2022\SumatraPDF.sln`, `/t:SumatraPDF:Rebuild`, `/p:Configuration=Release;Platform=x64`, `/m`)
	revertBuildConfig()
}

// smoke build is meant to be run locally to check that we can build everything
// it does full installer build of 64-bit release build
// We don't build other variants for speed. It takes about 5 mins locally
func buildSmoke() {
	detectSigntoolPath()
	defer makePrintDuration("smoke build")()
	removeReleaseBuilds()
	genHTMLDocsForApp()

	lzsa := absPathMust(filepath.Join("bin", "MakeLZSA.exe"))
	panicIf(!fileExists(lzsa), "file '%s' doesn't exist", lzsa)

	msbuildPath := detectMsbuildPathMust()
	runExeLoggedMust(msbuildPath, `vs2022\SumatraPDF.sln`, `/t:SumatraPDF-dll:Rebuild;test_util:Rebuild`, `/p:Configuration=Release;Platform=x64`, `/m`)
	outDir := filepath.Join("out", "rel64")
	runTestUtilMust(outDir)

	{
		cmd := exec.Command(lzsa, "SumatraPDF.pdb.lzsa", "libmupdf.pdb:libmupdf.pdb", "SumatraPDF-dll.pdb:SumatraPDF-dll.pdb")
		cmd.Dir = outDir
		runCmdLoggedMust(cmd)
	}
	signFilesMust(outDir)
}

// func buildJustPortableExe(dir, config, platform string) {
// 	msbuildPath := detectMsbuildPath()
// 	slnPath := filepath.Join("vs2022", "SumatraPDF.sln")

// 	p := fmt.Sprintf(`/p:Configuration=%s;Platform=%s`, config, platform)
// 	runExeLoggedMust(msbuildPath, slnPath, `/t:SumatraPDF`, p, `/m`)
// }

func buildTestUtil() {
	msbuildPath := detectMsbuildPathMust()
	slnPath := filepath.Join("vs2022", "SumatraPDF.sln")

	p := fmt.Sprintf(`/p:Configuration=Release;Platform=%s`, kPlatformIntel64)
	runExeLoggedMust(msbuildPath, slnPath, `/t:test_util:Rebuild`, p, `/m`)
}

const unsignedKeyPrefix = "software/sumatrapdf/prerel-unsigned/"

func unsignedArchivePreRelKey(ver string) string {
	return unsignedKeyPrefix + ver + ".zip"
}

// build pre-release builds and upload unsigned binaries to r2
// TODO: remove old unsigned builds, keep only the last one; do it after we check thie build doesn't exist
// TODO: maybe compress files before uploading using zstd or brotli
func buildCiDaily(signAndUpload bool) {
	if !isGithubMyMasterBranch() {
		logf("buildCiDaily: skipping build because not on master branch\n")
		return
	}
	if !isGitClean(".") {
		logf("buildCiDaily: skipping build because git is not clean\n")
		return
	}

	msbuildPath := detectMsbuildPathMust()

	ver := getPreReleaseVer()
	logf("building and uploading pre-release version %s\n", ver)

	archiveKey := unsignedArchivePreRelKey(ver)
	mc := newMinioR2Client()

	if mc.Exists(archiveKey) {
		logf("buildCiDaily: skipping build because already uploaded (key '%s' exists)\n", archiveKey)
		return
	}

	//cleanReleaseBuilds()
	genHTMLDocsForApp()
	ensureManualIsBuilt()

	setBuildConfigPreRelease()
	defer revertBuildConfig()

	printAllBuildDur := makePrintDuration("all builds")
	for _, platform := range []string{kPlatformIntel32, kPlatformIntel64, kPlatformArm64} {
		printBBuildDur := makePrintDuration(fmt.Sprintf("buidling pre-release %s version %s", platform, ver))
		slnPath := filepath.Join("vs2022", "SumatraPDF.sln")
		p := `/p:Configuration=Release;Platform=` + platform
		// t := `/t:SumatraPDF:Rebuild;SumatraPDF-dll:Rebuild`
		t := `/t:SumatraPDF;SumatraPDF-dll`
		runExeLoggedMust(msbuildPath, slnPath, t, p, `/m`)
		printBBuildDur()
	}
	revertBuildConfig() // can do twice
	printAllBuildDur()

	files := []string{
		"SumatraPDF.exe",
		"SumatraPDF-dll.exe",
		"SumatraPDF.pdb",
		"SumatraPDF-dll.pdb",
		"libmupdf.pdb",
	}
	origSize := int64(0)
	allFiles := []string{}
	for _, platform := range []string{kPlatformIntel32, kPlatformIntel64, kPlatformArm64} {
		buildDir := getBuildDirForPlatform(platform)
		for _, file := range files {
			path := filepath.Join(buildDir, file)
			allFiles = append(allFiles, path)
			path = filepath.Join("out", path)
			sz := u.FileSize(path)
			panicIf(sz < 0)
			origSize += sz
		}
	}
	logf("origSize: %s\n", u.FormatSize(origSize))

	archivePath := filepath.Join("out", "prerel-unsigned-"+ver+".zip")
	os.Remove(archivePath)
	logf("\nCreating %s (%d threads)\n", archivePath, runtime.NumCPU())
	printDur := measureDuration()
	err := creaZipWithCompressFunction(archivePath, "out", allFiles, compressFileWithBr, ".br")
	must(err)
	printDur()
	compressedSize := u.FileSize(archivePath)
	ratio := float64(origSize) / float64(compressedSize)
	logf("compressedSize: %s, ratio: %.2f\n", u.FormatSize(compressedSize), ratio)

	{
		logf("Uploading %s to %s ", archivePath, archiveKey)
		timeStart := time.Now()
		mc.UploadFile(archiveKey, archivePath, true)
		logf("  took %s\n", time.Since(timeStart))
	}
	if !signAndUpload {
		return
	}
	waitForEnter("Press Enter to sign and upload the binaries\n")
	// TODO: implement me
}

func waitForEnter(s string) {
	// wait for keyboard press
	if s == "" {
		s = "\nPress Enter to continue\n"
	}
	logf(s)
	fmt.Scanln()
}
