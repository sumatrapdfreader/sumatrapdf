package do

import (
	"bytes"
	"os"
	"os/exec"
	"path/filepath"

	"github.com/kjk/u"
)

func copyExeToAppxTempMust(dirDst string) {
	logf("copyExeToAppxTemp")
	appxDir := "AppxTemp"
	exeSrcPath := filepath.Join(dirDst, "SumatraPDF-prerel-64.exe")
	exeDstPath := filepath.Join(appxDir, "SumatraPDF-store.exe")
	os.Remove(exeDstPath) // overwrite
	must(u.CopyFile(exeDstPath, exeSrcPath))
}

func makeAppxTempMust(dirDst string) {
	logf("makeAppxTempMust")
	appxTempDir := "AppxTemp"
	os.RemoveAll(appxTempDir)
	u.DirCopyRecurMust(appxTempDir, "Appx", nil)

	// overwrite version in AppxManifest.xml
	ver := "3.6.0." + getPreReleaseVer()
	d := u.ReadFileMust(filepath.Join("appx", "AppxManifest.xml"))
	d2 := bytes.Replace(d, []byte("{{VERSION}}"), []byte(ver), -1)
	panicIf(bytes.Equal(d, d2), "AppxManifest.xml must contain {{VERSION}} placeholder")
	must(os.MkdirAll("AppxTemp", 0755))
	u.WriteFileMust(filepath.Join(appxTempDir, "AppxManifest.xml"), d2)

	// at this point is unsinged, will be over-written by signed app
	// only so we can test without signing
	copyExeToAppxTempMust(dirDst)
}

func buildAppxMust(dstDir string, sign bool) {
	logf("buildAppxMust")

	makeAppx := detectMakeAppxPathMust()
	appxDir := "AppxTemp"
	msixName := "SumatraPDF.msix"
	msixPath := filepath.Join(dstDir, msixName)
	os.Remove(msixPath)
	// "makeappx" pack /d bin\release\net461 /p MissionPlanner.
	// /d : directory to pack
	// /p : output .msix file path
	// /v : verbose mode
	cmd := exec.Command(makeAppx, "pack", "/v", "/d", appxDir, "/p", msixPath)
	runCmdLoggedMust(cmd)
	if sign {
		must(signFiles(dstDir, []string{msixName}))
	}
}
