package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	"github.com/kjk/u"
)

func websiteRunLocally(dir string) {
	// using https://github.com/netlify/cli
	cmd := exec.Command("netlify", "dev", "--dir", dir)
	runCmdLoggedMust(cmd)
}

func fileDownload(uri string, dstPath string) error {
	u.CreateDirForFileMust(dstPath)
	d := httpDlMust(uri)
	return ioutil.WriteFile(dstPath, d, 0755)
}

// needed during cloudflare build: download executables to be served from /dl2
func websiteBuildCloudflare() {
	currBranch := getCurrentBranchMust()
	fmt.Printf("websiteBuildCloudflare: branch '%s'\n", currBranch)
	if currBranch != "website-cf" {
		fmt.Printf("Skipping downloading executables because not 'website-cf' branch\n")
		return
	}
	ver := "3.3.3"
	files := []string{
		"SumatraPDF-%VER%-64-install.exe",
		"SumatraPDF-%VER%-64.zip",
		"SumatraPDF-%VER%-install.exe",
		"SumatraPDF-%VER%.zip",
	}
	err := os.MkdirAll(filepath.Join("website", "dl2"), 0755)
	must(err)
	baseURI := "https://kjkpubsf.sfo2.digitaloceanspaces.com/software/sumatrapdf/rel/"
	for _, file := range files {
		fileName := strings.ReplaceAll(file, "%VER%", ver)
		dstPath := filepath.Join("website", "dl2", fileName)
		if u.PathExists(dstPath) {
			fmt.Printf("Skipping downloading because %s already exists\n", dstPath)
			continue
		}
		uri := baseURI + fileName
		fmt.Printf("Downloading %s to %s...", uri, dstPath)
		timeStart := time.Now()
		err = fileDownload(uri, dstPath)
		must(err)
		fmt.Printf("took %s\n", time.Since(timeStart))
	}
}

func websiteDeployCloudlare() {
	u.EnsureGitClean(".")
	{
		cmd := exec.Command("git", "checkout", "website-cf")
		runCmdLoggedMust(cmd)
	}
	{
		cmd := exec.Command("git", "rebase", "master")
		runCmdLoggedMust(cmd)
	}
	{
		cmd := exec.Command("git", "push", "--force")
		runCmdLoggedMust(cmd)
	}
	{
		cmd := exec.Command("git", "checkout", "master")
		runCmdLoggedMust(cmd)
	}
}
