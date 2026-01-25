package do

import (
	"archive/zip"
	"compress/gzip"
	"errors"
	"fmt"
	"io/fs"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/kjk/common/u"
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

const (
	// kVSPlatform32    = "Win32"
	// kVSPlatform64    = "x64"
	kVSPlatformArm64 = "ARM64"
)

type Platform struct {
	vsplatform string // ARM64, Win32, x64 - as in Visual Studio platform
	suffix     string // e.g. 32, 64, arm64
	outDir     string // e.g. out/rel32, out/rel64, out/arm64
}

var platformArm64 = &Platform{kVSPlatformArm64, "arm64", filepath.Join("out", "arm64")}
var platform32 = &Platform{"Win32", "32", filepath.Join("out", "rel32")}
var platform64 = &Platform{"x64", "64", filepath.Join("out", "rel64")}

var platforms = []*Platform{
	platformArm64,
	platform32,
	platform64,
}

func getFileNamesWithPrefix(prefix string) [][]string {
	files := [][]string{
		{"SumatraPDF.exe", fmt.Sprintf("%s.exe", prefix)},
		{"SumatraPDF-dll.exe", fmt.Sprintf("%s-install.exe", prefix)},
		{"SumatraPDF.pdb.zip", fmt.Sprintf("%s.pdb.zip", prefix)},
		{"SumatraPDF.pdb.lzsa", fmt.Sprintf("%s.pdb.lzsa", prefix)},
	}
	return files
}

func createPdbLzsa(dir string) error {
	return createLzsaFromFiles("SumatraPDF.pdb.lzsa", dir, pdbFiles)
}

// func copyBuiltFiles(dstDir string, srcDir string, prefix string) {
// 	files := getFileNamesWithPrefix(prefix)
// 	for _, f := range files {
// 		srcName := f[0]
// 		srcPath := filepath.Join(srcDir, srcName)
// 		dstName := f[1]
// 		dstPath := filepath.Join(dstDir, dstName)
// 		must(createDirForFile(dstPath))
// 		if fileExists(srcPath) {
// 			must(copyFile(dstPath, srcPath))
// 		} else {
// 			logf("Skipping copying '%s'\n", srcPath)
// 		}
// 	}
// }

// func copyBuiltManifest(dstDir string, ver string, prefix string) {
// 	srcPath := manifestPath(ver)
// 	dstName := prefix + "-manifest.txt"
// 	dstPath := filepath.Join(dstDir, dstName)
// 	must(copyFile(dstPath, srcPath))
// }

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
	for _, plat := range platforms {
		clearDirPreserveSettings(plat.outDir)
	}
}

func removeReleaseBuilds() {
	for _, plat := range platforms {
		os.RemoveAll(plat.outDir)
	}
	//os.RemoveAll(finalPreRelDir)
}

func runTestUtilMust(dir string) {
	cmd := exec.Command(`.\test_util.exe`)
	cmd.Dir = dir
	runCmdLoggedMust(cmd)
}

func buildLzsa() {
	// early exit if missing
	detectSigntoolPathMust()

	defer makePrintDuration("buildLzsa")()
	cleanPreserveSettings()

	msbuildPath := detectMsbuildPathMust()
	runExeLoggedMust(msbuildPath, `vs2022\MakeLZSA.sln`, `/t:MakeLZSA:Rebuild`, `/p:Configuration=Release;Platform=Win32`, `/m`)

	dir := filepath.Join("out", "rel32")
	files := []string{"MakeLZSA.exe"}
	signFiles(dir, files)
	logf("built and signed '%s'\n", filepath.Join(dir, files[0]))
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

var pdbFiles = []string{"libmupdf.pdb", "SumatraPDF-dll.pdb", "SumatraPDF.pdb"}

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

// manifest is build for pre-release builds and contains information about file sizes
func createManifestMust(manifestPath string) {
	var lines []string
	files := []string{
		"SumatraPDF.exe",
		"SumatraPDF-dll.exe",
		"libmupdf.dll",
		"PdfFilter.dll",
		"PdfPreview.dll",
		"SumatraPDF.pdb.zip",
		"SumatraPDF.pdb.lzsa",
	}
	for _, plat := range platforms {
		dir := plat.outDir
		// 32bit / arm64 are only in daily build
		if !pathExists(dir) {
			continue
		}
		for _, file := range files {
			path := filepath.Join(dir, file)
			size := fileSizeMust(path)
			line := fmt.Sprintf("%s: %d", path, size)
			lines = append(lines, line)
		}
	}
	panicIf(len(lines) == 0, "didn't find any dirs for the manifest")

	s := strings.Join(lines, "\n")
	writeFileCreateDirMust(manifestPath, []byte(s))
}

// func build(config, platform string) {
// 	msbuildPath := detectMsbuildPathMust()
// 	slnPath := filepath.Join("vs2022", "SumatraPDF.sln")

// 	dir := getOutDirForPlatform(platform)

// 	p := fmt.Sprintf(`/p:Configuration=%s;Platform=%s`, config, platform)
// 	runExeLoggedMust(msbuildPath, slnPath, `/t:test_util:Rebuild`, p, `/m`)
// 	// can't run arm binaries in x86 CI
// 	if platform != kPlatformNameArm64 {
// 		runTestUtilMust(dir)
// 	}

// 	runExeLoggedMust(msbuildPath, slnPath, `/t:SumatraPDF:Rebuild;SumatraPDF-dll:Rebuild;PdfFilter:Rebuild;PdfPreview:Rebuild`, p, `/m`)
// 	err := createPdbZip(dir)
// 	must(err)
// 	err = createPdbLzsa(dir)
// 	must(err)
// }

func buildCi() {
	gev := getGitHubEventType()
	switch gev {
	case githubEventPush:
		removeReleaseBuilds()
		genHTMLDocsForApp()
		buildPreRelease(platform64)
	case githubEventTypeCodeQL:
		// code ql is just a regular build, I assume intercepted by
		// by their tooling
		// buildSmoke() runs genHTMLDocsForApp() so no need to do it here
		buildSmoke(false)
	default:
		panic("unkown value from getGitHubEventType()")
	}
}

func detectLlvmPdbutil() string {
	return detectPathMust(vsBasePaths, `VC\Tools\Llvm\bin\llvm-pdbutil.exe`)
}

func runLlvmPdbutilGzipped(pdbPath string, outPath string, args ...string) {
	exePath := detectLlvmPdbutil()
	cmdArgs := append([]string{"pretty"}, args...)
	cmdArgs = append(cmdArgs, pdbPath)
	cmd := exec.Command(exePath, cmdArgs...)
	logf("> %s\n", fmtCmdShort(cmd))
	out, err := cmd.CombinedOutput()
	if err != nil {
		logf("%s failed with '%s', output:\n%s\n", fmtCmdShort(cmd), err, string(out))
		must(err)
	}

	f, err := os.Create(outPath)
	must(err)
	defer f.Close()
	gz := gzip.NewWriter(f)
	_, err = gz.Write(out)
	must(err)
	err = gz.Close()
	must(err)
	logf("wrote %s (%s)\n", outPath, formatSize(int64(len(out))))
}

func uploadPdbBuildArtifacts() {
	pdbPath := filepath.Join("out", "rel64", "SumatraPDF.pdb")
	if !fileExists(pdbPath) {
		logf("uploadPdbBuildArtifacts: '%s' doesn't exist, skipping\n", pdbPath)
		return
	}

	globalsPath := "SumatraPDF-globals.txt.gz"
	classesPath := "SumatraPDF-classes.txt.gz"

	runLlvmPdbutilGzipped(pdbPath, globalsPath, "-globals", "-symbol-order=size")
	runLlvmPdbutilGzipped(pdbPath, classesPath, "-classes")

	ver := getPreReleaseVer()
	sha1 := getGitSha1()
	shortSha1 := sha1[:8]
	dateStr := time.Now().Format("2006-01-02")
	prefix := fmt.Sprintf("software/sumatrapdf-build-artifacts/%s-%s-%s", dateStr, ver, shortSha1)

	remoteGlobals := prefix + ".SumatraPDF-globals.txt.gz"
	remoteClasses := prefix + ".SumatraPDF-classes.txt.gz"

	var wg sync.WaitGroup
	wg.Add(2)
	go func() {
		mc := newMinioR2Client()
		mc.UploadFile(remoteGlobals, globalsPath, true)
		logf("uploaded %s\n", mc.URLForPath(remoteGlobals))
		mc.UploadFile(remoteClasses, classesPath, true)
		logf("uploaded %s\n", mc.URLForPath(remoteClasses))
		wg.Done()
	}()
	go func() {
		mc := newMinioBackblazeClient()
		mc.UploadFile(remoteGlobals, globalsPath, true)
		logf("uploaded %s\n", mc.URLForPath(remoteGlobals))
		mc.UploadFile(remoteClasses, classesPath, true)
		logf("uploaded %s\n", mc.URLForPath(remoteClasses))
		wg.Done()
	}()
	wg.Wait()
}

func ensureManualIsBuilt() {
	// make sure we've built manual
	path := filepath.Join("docs", "manual.dat")
	size := u.FileSize(path)
	panicIf(size < 2*2024, "size of '%s' is %d which indicates we didn't build it", path, size)
}

func buildPreRelease(plat *Platform) {
	ensureManualIsBuilt()
	ver := getPreReleaseVer()
	s := fmt.Sprintf("buidling pre-release version %s", ver)
	defer makePrintDuration(s)()
	setBuildConfigPreRelease()
	defer revertBuildConfig()

	buildAll := func(config, vsplatform string, outDir string) {
		msbuildPath := detectMsbuildPathMust()
		slnPath := filepath.Join("vs2022", "SumatraPDF.sln")

		p := fmt.Sprintf(`/p:Configuration=%s;Platform=%s`, config, vsplatform)
		runExeLoggedMust(msbuildPath, slnPath, `/t:test_util:Rebuild`, p, `/m`)
		// can't run arm binaries in x86 CI
		if vsplatform != kVSPlatformArm64 {
			runTestUtilMust(outDir)
		}

		runExeLoggedMust(msbuildPath, slnPath, `/t:signfile:Rebuild;sizer:Rebuild;PdfFilter:Rebuild;plugin-test:Rebuild;PdfPreview:Rebuild;PdfPreviewTest:Rebuild;SumatraPDF:Rebuild;SumatraPDF-dll:Rebuild`, p, `/m`)
		err := createPdbZip(outDir)
		must(err)
		err = createPdbLzsa(outDir)
		must(err)
	}

	buildAll("Release", plat.vsplatform, plat.outDir)
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
func buildSmoke(sign bool) {
	detectSigntoolPathMust()
	defer makePrintDuration("smoke build")()
	removeReleaseBuilds()
	genHTMLDocsForApp()

	lzsa := absPathMust(filepath.Join("bin", "MakeLZSA.exe"))
	panicIf(!fileExists(lzsa), "file '%s' doesn't exist", lzsa)

	msbuildPath := detectMsbuildPathMust()
	sln := `vs2022\SumatraPDF.sln`
	t := `/t:SumatraPDF-dll:Rebuild;test_util:Rebuild`
	p := `/p:Configuration=Release;Platform=x64`
	runExeLoggedMust(msbuildPath, sln, t, p, `/m`)
	outDir := filepath.Join("out", "rel64")
	runTestUtilMust(outDir)

	{
		cmd := exec.Command(lzsa, "SumatraPDF.pdb.lzsa", "libmupdf.pdb:libmupdf.pdb", "SumatraPDF-dll.pdb:SumatraPDF-dll.pdb")
		cmd.Dir = outDir
		runCmdLoggedMust(cmd)
	}
	if sign {
		files := []string{"SumatraPDF-dll.exe", "libmupdf.dll", "PdfFilter.dll", "PdfPreview.dll"}
		signFiles(outDir, files)
	}
}

func buildTestUtil() {
	msbuildPath := detectMsbuildPathMust()
	slnPath := filepath.Join("vs2022", "SumatraPDF.sln")

	p := `/p:Configuration=Release;Platform=x64`
	runExeLoggedMust(msbuildPath, slnPath, `/t:test_util:Rebuild`, p, `/m`)
}

// build pre-release builds for ci sanity check
// TODO: record if already built in r2 and skip if did
// keep info in /software/sumatrapdf/last-daily-build.txt
func buildCiDaily() {
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
	logf("building unsigned pre-release version %s\n", ver)

	//cleanReleaseBuilds()
	genHTMLDocsForApp()
	ensureManualIsBuilt()

	setBuildConfigPreRelease()
	defer revertBuildConfig()

	printAllBuildDur := makePrintDuration("all builds")
	for _, plat := range platforms {
		platform := plat.vsplatform
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
}

func waitForEnter(s string) {
	// wait for keyboard press
	if s == "" {
		s = "\nPress Enter to continue\n"
	}
	logf(s)
	fmt.Scanln()
}
