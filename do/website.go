package main

import (
	"os"
	"os/exec"
	"path/filepath"

	"github.com/kjk/u"
)

func cdWebsiteDir() {
	dir := filepath.Join("website")
	err := os.Chdir(dir)
	must(err)
}

func websiteDeployProd() {
	cdWebsiteDir()
	// using https://github.com/netlify/cli
	cmd := exec.Command("netlify", "deploy", "--prod", "--open", "--dir", "www", "--site", "2963982f-7d39-439c-a7eb-0eb118efbd02")
	u.RunCmdLoggedMust(cmd)
}

func websiteDeployDev() {
	cdWebsiteDir()
	// using https://github.com/netlify/cli
	cmd := exec.Command("netlify", "deploy", "--open", "--dir", "www", "--site", "2963982f-7d39-439c-a7eb-0eb118efbd02")
	u.RunCmdLoggedMust(cmd)
}

func websiteRunLocally() {
	cdWebsiteDir()
	// using https://github.com/netlify/cli
	cmd := exec.Command("netlify", "dev", "--dir", "www")
	u.RunCmdLoggedMust(cmd)
}
