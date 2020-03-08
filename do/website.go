package main

import (
	"os/exec"

	"github.com/kjk/u"
)

func websiteDeployProd() {
	// using https://github.com/netlify/cli
	cmd := exec.Command("netlify", "deploy", "--prod", "--open", "--dir", "website", "--site", "2963982f-7d39-439c-a7eb-0eb118efbd02")
	u.RunCmdLoggedMust(cmd)
}

func websiteDeployDev() {
	// using https://github.com/netlify/cli
	cmd := exec.Command("netlify", "deploy", "--open", "--dir", "website", "--site", "2963982f-7d39-439c-a7eb-0eb118efbd02")
	u.RunCmdLoggedMust(cmd)
}

func websiteRunLocally() {
	// using https://github.com/netlify/cli
	cmd := exec.Command("netlify", "dev", "--dir", "website")
	u.RunCmdLoggedMust(cmd)
}
