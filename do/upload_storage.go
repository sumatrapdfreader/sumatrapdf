package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"path"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/kjk/minioutil"
	"github.com/kjk/u"
)

// we delete old daily and pre-release builds. This defines how many most recent
// builds to retain
const nBuildsToRetainPreRel = 5

type BuildType string

const (
	buildTypePreRel BuildType = "prerel"
	buildTypeRel    BuildType = "rel"
)

func getRemotePaths(buildType BuildType) []string {
	if buildType == buildTypePreRel {
		return []string{
			"software/sumatrapdf/sumatralatest.js",
			"software/sumatrapdf/sumpdf-prerelease-latest.txt",
			"software/sumatrapdf/sumpdf-prerelease-update.txt",
		}
	}

	// if buildType == buildTypeDaily {
	// 	return []string{
	// 		"software/sumatrapdf/sumatralatest-daily.js",
	// 		"software/sumatrapdf/sumpdf-daily-latest.txt",
	// 		"software/sumatrapdf/sumpdf-daily-update.txt",
	// 	}
	// }

	if buildType == buildTypeRel {
		return []string{
			"software/sumatrapdf/sumarellatest.js",
			"software/sumatrapdf/release-latest.txt",
			"software/sumatrapdf/release-update.txt",
		}
	}

	panicIf(true, "Unkonwn buildType='%s'", buildType)
	return nil
}

// this returns version to be used in uploaded file names
func getVerForBuildType(buildType BuildType) string {
	switch buildType {
	case buildTypePreRel:
		// this is linear build number like "12223"
		return getPreReleaseVer()
	case buildTypeRel:
		// this is program version like "3.2"
		return sumatraVersion
	}
	panicIf(true, "invalid buildType '%s'", buildType)
	return ""
}

func getRemoteDir(buildType BuildType) string {
	ver := getVerForBuildType(buildType)
	return "software/sumatrapdf/" + string(buildType) + "/" + ver + "/"
}

type DownloadUrls struct {
	installer64   string
	portableExe64 string
	portableZip64 string

	installerArm64   string
	portableExeArm64 string
	portableZipArm64 string

	installer32   string
	portableExe32 string
	portableZip32 string
}

func getDownloadUrlsForPrefix(prefix string, buildType BuildType, ver string) *DownloadUrls {
	// zip is like .exe but can be half the size due to compression
	res := &DownloadUrls{
		installer64:      prefix + "SumatraPDF-${ver}-64-install.exe",
		portableExe64:    prefix + "SumatraPDF-${ver}-64.exe",
		portableZip64:    prefix + "SumatraPDF-${ver}-64.zip",
		installerArm64:   prefix + "SumatraPDF-${ver}-arm64-install.exe",
		portableExeArm64: prefix + "SumatraPDF-${ver}-arm64.exe",
		portableZipArm64: prefix + "SumatraPDF-${ver}-arm64.zip",
		installer32:      prefix + "SumatraPDF-${ver}-install.exe",
		portableExe32:    prefix + "SumatraPDF-${ver}.exe",
		portableZip32:    prefix + "SumatraPDF-${ver}.zip",
	}
	if buildType != buildTypeRel {
		// for pre-release and daily, ${ver} is encoded prefix
		res = &DownloadUrls{
			installer64:      prefix + "SumatraPDF-prerel-64-install.exe",
			portableExe64:    prefix + "SumatraPDF-prerel-64.exe",
			portableZip64:    prefix + "SumatraPDF-prerel-64.zip",
			installerArm64:   prefix + "SumatraPDF-prerel-arm64-install.exe",
			portableExeArm64: prefix + "SumatraPDF-prerel-arm64.exe",
			portableZipArm64: prefix + "SumatraPDF-prerel-arm64.zip",
			installer32:      prefix + "SumatraPDF-prerel-install.exe",
			portableExe32:    prefix + "SumatraPDF-prerel.exe",
			portableZip32:    prefix + "SumatraPDF-prerel.zip",
		}
	}
	rplc := func(s *string) {
		*s = strings.Replace(*s, "${ver}", ver, -1)
		//*s = strings.Replace(*s, "${buildType}", buildType, -1)
	}
	rplc(&res.installer64)
	rplc(&res.portableExe64)
	rplc(&res.portableZip64)
	rplc(&res.installerArm64)
	rplc(&res.portableExeArm64)
	rplc(&res.portableZipArm64)
	rplc(&res.installer32)
	rplc(&res.portableExe32)
	rplc(&res.portableZip32)
	return res
}

func genUpdateTxt(urls *DownloadUrls, ver string) string {
	s := `[SumatraPDF]
Latest: ${ver}
Installer64: ${inst64}
InstallerArm64: ${instArm64}
Installer32: ${inst32}
PortableExe64: ${exe64}
PortableExeArm64: ${exeArm64}
PortableExe32: ${exe32}
PortableZip64: ${zip64}
PortableZipArm64: ${zipArm64}
PortableZip32: ${zip32}
`
	rplc := func(old, new string) {
		s = strings.Replace(s, old, new, -1)
	}
	rplc("${ver}", ver)
	rplc("${inst64}", urls.installer64)
	rplc("${instArm64}", urls.installerArm64)
	rplc("${inst32}", urls.installer32)
	rplc("${exe64}", urls.portableExe64)
	rplc("${exeArm64}", urls.portableExeArm64)
	rplc("${exe32}", urls.portableExe32)
	rplc("${zip64}", urls.portableZip64)
	rplc("${zipArm64}", urls.portableZipArm64)
	rplc("${zip32}", urls.portableZip32)
	return s
}

func testGenUpdateTxt() {
	ver := "14276"
	urls := getDownloadUrlsViaWebsite(buildTypePreRel, ver)
	s := genUpdateTxt(urls, ver)
	fmt.Printf("testGenUpdateTxt:\n%s\n", s)
	os.Exit(0)
}

func getDownloadUrlsViaWebsite(buildType BuildType, ver string) *DownloadUrls {
	prefix := "https://www.sumatrapdfreader.org/dl/" + string(buildType) + "/" + ver + "/"
	return getDownloadUrlsForPrefix(prefix, buildType, ver)
}

func getDownloadUrlsDirectS3(mc *minioutil.Client, buildType BuildType, ver string) *DownloadUrls {
	prefix := mc.URLBase()
	prefix += getRemoteDir(buildType)
	return getDownloadUrlsForPrefix(prefix, buildType, ver)
}

// sumatrapdf/sumatralatest.js
func createSumatraLatestJs(mc *minioutil.Client, buildType BuildType) string {
	var appName string
	switch buildType {
	case buildTypePreRel:
		appName = "SumatraPDF-prerel"
	case buildTypeRel:
		appName = "SumatraPDF"
	default:
		panicIf(true, "invalid buildType '%s'", buildType)
	}

	currDate := time.Now().Format("2006-01-02")
	ver := getVerForBuildType(buildType)

	// old version pointing directly to s3 storage
	//host := strings.TrimSuffix(mc.URLBase(), "/")
	//host + "software/sumatrapdf/" + buildType

	// new version that redirects via www.sumatrapdfreader.org/dl/
	var host string
	switch buildType {
	case buildTypeRel:
		host = "https://www.sumatrapdfreader.org/dl/rel/" + ver
	case buildTypePreRel:
		host = "https://www.sumatrapdfreader.org/dl/prerel/" + ver
	// case buildTypeDaily:
	// 	host = "https://www.sumatrapdfreader.org/dl/daily/" + ver
	default:
		panicIf(true, "unsupported buildType: '%s'", buildType)
	}

	// TODO: use
	// urls := getDownloadUrls(storage, buildType, ver)

	tmplText := `
var sumLatestVer = {{.Ver}};
var sumCommitSha1 = "{{ .Sha1 }}";
var sumBuiltOn = "{{.CurrDate}}";
var sumLatestName = "{{.Prefix}}.exe";

var sumLatestExe         = "{{.Host}}/{{.Prefix}}.exe";
var sumLatestExeZip      = "{{.Host}}/{{.Prefix}}.zip";
var sumLatestPdb         = "{{.Host}}/{{.Prefix}}.pdb.zip";
var sumLatestInstaller   = "{{.Host}}/{{.Prefix}}-install.exe";

var sumLatestExe64       = "{{.Host}}/{{.Prefix}}-64.exe";
var sumLatestExeZip64    = "{{.Host}}/{{.Prefix}}-64.zip";
var sumLatestPdb64       = "{{.Host}}/{{.Prefix}}-64.pdb.zip";
var sumLatestInstaller64 = "{{.Host}}/{{.Prefix}}-64-install.exe";

var sumLatestExeArm64       = "{{.Host}}/{{.Prefix}}-arm64.exe";
var sumLatestExeZipArm64    = "{{.Host}}/{{.Prefix}}-arm64.zip";
var sumLatestPdbArm64       = "{{.Host}}/{{.Prefix}}-arm64.pdb.zip";
var sumLatestInstallerArm64 = "{{.Host}}/{{.Prefix}}-arm64-install.exe";

`
	sha1 := getGitSha1()
	d := map[string]interface{}{
		"Host":     host,
		"Ver":      ver,
		"Sha1":     sha1,
		"CurrDate": currDate,
		"Prefix":   appName + "-" + ver,
	}
	// for prerel / daily, version is in path, not in name
	if buildType == buildTypePreRel {
		d["Prefix"] = appName
	}
	return execTextTemplate(tmplText, d)
}

func getVersionFilesForLatestInfo(mc *minioutil.Client, buildType BuildType) [][]string {
	panicIf(buildType == buildTypeRel)
	remotePaths := getRemotePaths(buildType)
	var res [][]string

	{
		// *latest.js : for the website
		s := createSumatraLatestJs(mc, buildType)
		res = append(res, []string{remotePaths[0], s})
	}

	ver := getVerForBuildType(buildType)
	{
		// *-latest.txt : for older build
		res = append(res, []string{remotePaths[1], ver})
	}

	{
		// *-update.txt : for current builds
		urls := getDownloadUrlsViaWebsite(buildType, ver)
		if false {
			urls = getDownloadUrlsDirectS3(mc, buildType, ver)
		}
		s := genUpdateTxt(urls, ver)
		res = append(res, []string{remotePaths[2], s})
	}

	return res
}

// we shouldn't re-upload files. We upload manifest-${ver}.txt last, so we
// consider a pre-release build already present in s3 if manifest file exists
func isBuildAlreadyUploaded(mc *minioutil.Client, buildType BuildType) bool {
	dirRemote := getRemoteDir(buildType)
	ver := getVerForBuildType(buildType)
	fname := "SumatraPDF-prerel-manifest.txt"
	if buildType == buildTypeRel {
		fname = fmt.Sprintf("SumatraPDF-%s-manifest.txt", ver)
	}
	remotePath := path.Join(dirRemote, fname)
	exists := mc.Exists(remotePath)
	if exists {
		logf("build of type '%s' for ver '%s' already exists because '%s' exists\n", buildType, ver, mc.URLForPath(remotePath))

	}
	return exists
}

func verifyBuildNotInStorageMust(mc *minioutil.Client, buildType BuildType) {
	exists := isBuildAlreadyUploaded(mc, buildType)
	panicIf(exists, "build already exists")
}

func UploadDir(c *minioutil.Client, dirRemote string, dirLocal string, public bool) error {
	files, err := ioutil.ReadDir(dirLocal)
	if err != nil {
		return err
	}
	for _, f := range files {
		fname := f.Name()
		pathLocal := filepath.Join(dirLocal, fname)
		pathRemote := path.Join(dirRemote, fname)
		timeStart := time.Now()
		_, err := c.UploadFile(pathRemote, pathLocal, public)
		if err != nil {
			return fmt.Errorf("upload of '%s' as '%s' failed with '%s'", pathLocal, pathRemote, err)
		}
		uri := c.URLForPath(pathRemote)
		logf("Uploaded %s => %s in %s\n", pathLocal, uri, time.Since(timeStart))
	}
	return nil
}

func getFinalDirForBuildType(buildType BuildType) string {
	var dir string
	switch buildType {
	case buildTypeRel:
		dir = "final-rel"
	case buildTypePreRel:
		dir = "final-prerel"
	// case buildTypeDaily:
	// 	dir = "final-daily"
	default:
		panicIf(true, "invalid buildType '%s'", buildType)
	}
	return filepath.Join("out", dir)
}

// https://kjkpubsf.sfo2.digitaloceanspaces.com/software/sumatrapdf/prerel/1024/SumatraPDF-prerelease-install.exe etc.
func minioUploadBuildMust(mc *minioutil.Client, buildType BuildType) {
	timeStart := time.Now()
	defer func() {
		logf("Uploaded build '%s' to %s in %s\n", buildType, mc.URLBase(), time.Since(timeStart))
	}()

	dirRemote := getRemoteDir(buildType)
	dirLocal := getFinalDirForBuildType(buildType)

	err := UploadDir(mc, dirRemote, dirLocal, true)
	must(err)

	// for release build we don't upload files with version info
	if buildType == buildTypeRel {
		logf("Skipping uploading version for release builds\n")
		return
	}

	uploadBuildUpdateInfoMust := func(buildType BuildType) {
		files := getVersionFilesForLatestInfo(mc, buildType)
		for _, f := range files {
			remotePath := f[0]
			_, err := mc.UploadData(remotePath, []byte(f[1]), true)
			must(err)
			logf("Uploaded `%s'\n", mc.URLForPath(remotePath))
		}
	}

	uploadBuildUpdateInfoMust(buildType)
}

type filesByVer struct {
	ver   int
	files []string
}

func groupFilesByVersion(files []string) []*filesByVer {
	// "software/sumatrapdf/prerel/14028/SumatraPDF-prerel-64.pdb.zip"
	// =>
	// 14028
	extractVersionFromName := func(s string) int {
		parts := strings.Split(s, "/")
		verStr := parts[3]
		ver, err := strconv.Atoi(verStr)
		panicIf(err != nil, "extractVersionFromName: '%s', err='%s'\n", s, err)
		return ver
	}

	m := map[int]*filesByVer{}
	for _, f := range files {
		ver := extractVersionFromName(f)
		i := m[ver]
		if i == nil {
			i = &filesByVer{
				ver: ver,
			}
			m[ver] = i
		}
		i.files = append(i.files, f)
	}
	res := []*filesByVer{}
	for _, v := range m {
		res = append(res, v)
	}
	sort.Slice(res, func(i, j int) bool {
		return res[i].ver > res[j].ver
	})
	return res
}

func minioDeleteOldBuildsPrefix(mc *minioutil.Client, buildType BuildType) {
	nBuildsToRetain := nBuildsToRetainPreRel
	var remoteDir string
	switch buildType {
	case buildTypeRel:
		panicIf(true, "can't delete release builds")
	case buildTypePreRel:
		remoteDir = "software/sumatrapdf/prerel/"
	// case buildTypeDaily:
	// 	remoteDir = "software/sumatrapdf/daily/"
	default:
		panicIf(true, "unsupported buildType: '%s'", buildType)
	}
	objectsCh := mc.ListObjects(remoteDir)
	var keys []string
	for f := range objectsCh {
		keys = append(keys, f.Key)
		//logf("  %s\n", f.Key)
	}

	uri := mc.URLForPath(remoteDir)
	logf("%d files under '%s'\n", len(keys), uri)
	byVer := groupFilesByVersion(keys)
	for i, v := range byVer {
		deleting := (i >= nBuildsToRetain)
		if deleting {
			logf("deleting %d\n", v.ver)
			if true {
				for _, key := range v.files {
					err := mc.Remove(key)
					must(err)
					logf("  deleted %s\n", key)
				}
			}
		} else {
			logf("not deleting %d\n", v.ver)
		}
	}
}

func newMinioBackblazeClient() *minioutil.Client {
	config := &minioutil.Config{
		Bucket:   "kjk-files",
		Endpoint: "s3.us-west-001.backblazeb2.com",
		Access:   b2Access,
		Secret:   b2Secret,
	}
	mc, err := minioutil.New(config)
	must(err)
	return mc
}

func newMinioR2Client() *minioutil.Client {
	config := &minioutil.Config{
		Bucket:   "files",
		Endpoint: "71694ef61795ecbe1bc331d217dbd7a7.r2.cloudflarestorage.com",
		Access:   r2Access,
		Secret:   r2Secret,
	}
	mc, err := minioutil.New(config)
	must(err)
	return mc
}

func uploadToStorage(buildType BuildType) {
	isUploaded := isBuildAlreadyUploaded(newMinioBackblazeClient(), buildType)
	if isUploaded {
		logf("uploadToStorage: skipping upload because already uploaded")
		return
	}

	timeStart := time.Now()
	defer func() {
		logf("uploadToStorage of '%s' finished in %s\n", buildType, time.Since(timeStart))
	}()
	var wg sync.WaitGroup

	wg.Add(1)
	go func() {
		mc := newMinioR2Client()
		minioUploadBuildMust(mc, buildType)
		if buildType != buildTypeRel {
			minioDeleteOldBuildsPrefix(mc, buildType)
		}
		wg.Done()
	}()

	// downloads of pre-release 64-bit installer often fail
	// I suspect cloudflare backblaze proxy is caching 404 responses and 64-bit are hit
	// the most because they are most likely to be downloaded first
	// I'm hoping by delaying uploading https://kjkpubsf.sfo2.digitaloceanspaces.com/software/sumatrapdf/sumatralatest.js
	// (which drives /prelease.html page) is enough for backblaze to make the uploaded files visible to the
	// world (including cloudflare)
	// Alternatively: could do http get against the file until I can see it. Better than arbitrary delay but still
	// no guarantees the files will be visible from other networks
	// if buildType != buildTypeRel {
	// 	logf("uploadToStorage: delay do spaces upload by 5 min to make backblaze files visible to cloudflare proxy\n")
	// 	time.Sleep(time.Minute * 5)
	// }

	wg.Add(1)
	go func() {
		mc := newMinioBackblazeClient()
		minioUploadBuildMust(mc, buildType)
		if buildType != buildTypeRel {
			minioDeleteOldBuildsPrefix(mc, buildType)
		}
		wg.Done()
	}()

	wg.Wait()
}

func uploadLogView() {
	logf("uploadLogView\n")
	ver := extractLogViewVersion()
	path := filepath.Join("tools", "logview-win", "build", "bin", "logview.exe")
	panicIf(!fileExists(path), "file '%s' doesn't exist", path)
	remotePath := "software/logview/rel/" + fmt.Sprintf("logview-%s.exe", ver)
	mc := newMinioBackblazeClient()
	if mc.Exists(remotePath) {
		logf("%s (%s) already uploaded\n", remotePath, mc.URLForPath(remotePath))
		return
	}
	mc.UploadFile(remotePath, path, true)
	sizeStr := u.FmtSizeHuman(getFileSize(path))
	logf("Uploaded %s of size %s as %s\n", path, sizeStr, mc.URLForPath(remotePath))
}
