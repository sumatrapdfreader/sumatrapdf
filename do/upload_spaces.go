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
	"time"

	"github.com/kjk/u"
)

// we delete old daily and pre-release builds. This defines how many most recent
// builds to retain
const nBuildsToRetainPreRel = 16
const nBuildsToRetainDaily = 64
const nBuildsToRetaininMicro = 32

const (
	buildTypeDaily   = "daily"
	buildTypePreRel  = "prerel"
	buildTypeRel     = "rel"
	buildTypeRaMicro = "ramicro"
)

var (
	rel32Dir   = filepath.Join("out", "rel32")
	rel32XPDir = filepath.Join("out", "rel32_xp")
	rel64Dir   = filepath.Join("out", "rel64")
	rel64RaDir = filepath.Join("out", "rel64ra")
)

func getRemotePaths(buildType string) []string {
	if buildType == buildTypePreRel {
		return []string{
			"software/sumatrapdf/sumatralatest.js",
			"software/sumatrapdf/sumpdf-prerelease-latest.txt",
			"software/sumatrapdf/sumpdf-prerelease-update.txt",
		}
	}

	if buildType == buildTypeDaily {
		return []string{
			"software/sumatrapdf/sumadaily.js",
			"software/sumatrapdf/sumpdf-daily-latest.txt",
			"software/sumatrapdf/sumpdf-daily-update.txt",
		}
	}

	if buildType == buildTypeRaMicro {
		return []string{
			"software/sumatrapdf/ramicrolatest.js",
			"software/sumatrapdf/ramicro-daily-latest.txt",
			"software/sumatrapdf/ramicro-daily-update.txt",
		}
	}

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

func isValidBuildType(buildType string) bool {
	switch buildType {
	case buildTypeDaily, buildTypePreRel, buildTypeRel, buildTypeRaMicro:
		return true
	}
	return false
}

func getRemoteDir(buildType string) string {
	panicIf(!isValidBuildType(buildType), "invalid build type: '%s'", buildType)
	return "software/sumatrapdf/" + buildType + "/"
}

func newMinioClient() *u.MinioClient {
	res := &u.MinioClient{
		StorageKey:    os.Getenv("SPACES_KEY"),
		StorageSecret: os.Getenv("SPACES_SECRET"),
		Bucket:        "kjkpubsf",
		Endpoint:      "sfo2.digitaloceanspaces.com",
	}
	res.EnsureConfigured()
	return res
}

func hasSpacesCreds() bool {
	if os.Getenv("SPACES_KEY") == "" {
		logf("Not uploading to do spaces because SPACES_KEY env variable not set\n")
		return false
	}
	if os.Getenv("SPACES_SECRET") == "" {
		logf("Not uploading to do spaces because SPACES_SECRET env variable not set\n")
		return false
	}
	return true
}

// TODO: add Exists() method to u.MinioClient to keep code closer to s3
func minioExists(c *u.MinioClient, remotePath string) bool {
	_, err := c.StatObject(remotePath)
	return err == nil
}

func minioUploadDir(c *u.MinioClient, dirRemote string, dirLocal string) error {
	files, err := ioutil.ReadDir(dirLocal)
	must(err)
	for _, f := range files {
		fname := f.Name()
		pathLocal := filepath.Join(dirLocal, fname)
		pathRemote := path.Join(dirRemote, fname)
		err := c.UploadFilePublic(pathRemote, pathLocal)
		if err != nil {
			return fmt.Errorf("failed spaces upload '%s' as '%s', err: %s", pathLocal, pathRemote, err)
		}
		logf("Uploaded to spaces: '%s' as '%s'\n", pathLocal, pathRemote)
	}
	return nil
}

func verifyBuildNotInSpacesShortMust(buildType string) {
	dirRemote := getRemoteDir(buildType)
	ver := getVerForBuildType(buildType)
	fname := fmt.Sprintf("SumatraPDF-prerelease-%s-manifest.txt", ver)
	remotePath := path.Join(dirRemote, fname)
	c := newMinioClient()
	fatalIf(minioExists(c, remotePath), "build of type '%s' for ver '%s' already exists in s3 because file '%s' exists\n", buildType, ver, remotePath)
}

// we shouldn't re-upload files. We upload manifest-${ver}.txt last, so we
// consider a pre-release build already present in s3 if manifest file exists
func verifyBuildNotInSpacesMust(c *u.MinioClient, buildType string) {
	if !flgUpload {
		return
	}
	dirRemote := getRemoteDir(buildType)
	dirLocal := getFinalDirForBuildType(buildType)
	files, err := ioutil.ReadDir(dirLocal)
	panicIfErr(err)
	for _, f := range files {
		fname := f.Name()
		remotePath := path.Join(dirRemote, fname)
		fatalIf(minioExists(c, remotePath), "build from dir %s already exists in s3 because file '%s' exists\n", dirLocal, remotePath)
	}
}

func getVersionFilesForLatestInfo(buildType string) [][]string {
	panicIf(buildType == buildTypeRel)
	remotePaths := getRemotePaths(buildType)
	var res [][]string
	s := createSumatraLatestJs(buildType)
	res = append(res, []string{remotePaths[0], s})
	ver := getVerForBuildType(buildType)
	res = append(res, []string{remotePaths[1], ver})
	// TOOD different for ramicro
	s = fmt.Sprintf("[SumatraPDF]\nLatest %s\n", ver)
	res = append(res, []string{remotePaths[2], s})
	return res
}

// https://kjkpubsf.sfo2.digitaloceanspaces.com/software/sumatrapdf/prerel/SumatraPDF-prerelease-1027-install.exe etc.
func spacesUploadBuildMust(buildType string) {
	if shouldSkipUpload() {
		return
	}
	if !hasSpacesCreds() {
		return
	}

	timeStart := time.Now()
	c := newMinioClient()

	dirRemote := getRemoteDir(buildType)
	dirLocal := getFinalDirForBuildType(buildType)
	//verifyBuildNotInSpaces(c, buildType)

	err := minioUploadDir(c, dirRemote, dirLocal)
	panicIfErr(err)

	// for release build we don't upload files with version info
	if buildType == buildTypeRel {
		return
	}

	files := getVersionFilesForLatestInfo(buildType)
	for _, f := range files {
		remotePath := f[0]
		err = c.UploadDataPublic(remotePath, []byte(f[1]))
		panicIfErr(err)
		logf("Uploaded to spaces: '%s'\n", remotePath)
	}

	logf("Uploaded the build to spaces in %s\n", time.Since(timeStart))
}

// "software/sumatrapdf/prerel/SumatraPDF-prerelease-11290-64-install.exe"
// =>
// 11290
func extractVersionFromName(s string) int {
	parts := strings.Split(s, "/")
	name := parts[len(parts)-1]
	// TODO: eventually we'll only need prerel- as prerelease-
	// is older naming
	name = strings.TrimPrefix(name, "SumatraPDF-prerelease-")
	name = strings.TrimPrefix(name, "SumatraPDF-prerel-")

	name = strings.TrimPrefix(name, "RAMicro-prerelease-")
	name = strings.TrimPrefix(name, "RAMicro-prerel-")
	name = strings.TrimPrefix(name, "RAMicroPDFViewer-prerel-")

	// TODO: temporary, for old builds in s3
	name = strings.TrimPrefix(name, "SumatraPDF-prerelase-")
	name = strings.TrimPrefix(name, "manifest-")
	name = strings.TrimPrefix(name, "manifest")
	if name == "" {
		return 0
	}

	parts = strings.Split(name, "-")
	parts = strings.Split(parts[0], ".")
	verStr := parts[0]
	ver, err := strconv.Atoi(verStr)
	if err != nil {
		// TODO: temporary, for builds uploaded with bad names
		//
		return 1
	}
	//panicIf(err != nil, "extractVersionFromName: '%s', err='%s'\n", s, err)
	return ver
}

type filesByVer struct {
	ver   int
	files []string
}

func groupFilesByVersion(files []string) []*filesByVer {
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

func minioDeleteOldBuildsPrefix(buildType string) {
	panicIf(buildType == buildTypeRel, "can't delete release builds")

	nBuildsToRetain := nBuildsToRetainDaily
	if buildType == buildTypePreRel {
		nBuildsToRetain = nBuildsToRetainPreRel
	}
	if buildType == buildTypeRaMicro {
		nBuildsToRetain = nBuildsToRetaininMicro
	}
	remoteDir := getRemoteDir(buildType)

	c := newMinioClient()
	files, err := c.ListRemoteFiles(remoteDir)
	must(err)
	fmt.Printf("%d minio files under '%s'\n", len(files), remoteDir)
	var keys []string
	for _, f := range files {
		keys = append(keys, f.Key)
		//fmt.Printf("key: %s\n", f.Key)
	}
	byVer := groupFilesByVersion(keys)
	for i, v := range byVer {
		deleting := (i >= nBuildsToRetain)
		if deleting {
			fmt.Printf("%d, deleting\n", v.ver)
			for _, fn := range v.files {
				fmt.Printf("  %s deleting\n", fn)
				err := c.Delete(fn)
				must(err)
			}
		} else {
			//fmt.Printf("%d, not deleting\n", v.ver)
			// for _, fn := range v.files {
			// 	fmt.Printf("  %s not deleting\n", fn)
			// }
		}
	}
}

func minioDeleteOldBuilds() {
	minioDeleteOldBuildsPrefix(buildTypePreRel)
	minioDeleteOldBuildsPrefix(buildTypeDaily)
	minioDeleteOldBuildsPrefix(buildTypeRaMicro)
}
