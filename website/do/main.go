package main

import (
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"

	"github.com/kjk/u"
)

func logf(format string, args ...interface{}) {
	s := format
	if len(args) > 0 {
		s = fmt.Sprintf(format, args...)
	}
	fmt.Print(s)
}

func regenWebsite() {
	exeName := "sumatra_website.exe"
	// build the website generator
	{
		cmd := exec.Command("go", "build", "-o", exeName)
		u.RunCmdLoggedMust(cmd)
	}
	// generate files
	{
		cmd := exec.Command("./" + exeName)
		u.RunCmdLoggedMust(cmd)
	}
	os.Remove(exeName)
}

func runLocal() {
	regenWebsite()

	// run caddy to preview the website, on localhost:9000
	{
		cmd := exec.Command("caddy", "-log", "stdout")
		cmd.Stdout = os.Stdout
		err := cmd.Start()
		u.PanicIfErr(err)
		u.OpenBrowser("http://localhost:9000")
		cmd.Wait()
	}
}

func deployProd() {
	logf("deploying to netlify in production\n")

	regenWebsite()

	// using https://github.com/netlify/cli
	cmd := exec.Command("netlify", "deploy", "--prod", "--dir", "www", "--site", "2963982f-7d39-439c-a7eb-0eb118efbd02")
	u.RunCmdLoggedMust(cmd)
}

func importNotion() {
	// TODO: move that code here
	cmd := exec.Command("go", "run", ".")
	cmd.Dir = filepath.Join("cmd", "import_notion_docs")
	u.RunCmdLoggedMust(cmd)
}

func main() {
	u.CdUpDir("website")
	logf("dir: '%s'\n", u.CurrDirAbsMust())

	var (
		flgRun          bool
		flgDeployProd   bool
		flgImportNotion bool
	)
	flag.BoolVar(&flgRun, "run", false, "run webserver locally to preview the changes")
	flag.BoolVar(&flgDeployProd, "deploy-prod", false, "deploy to Netlify")
	flag.BoolVar(&flgImportNotion, "import-notion", false, "import notion as docs")
	flag.Parse()

	if flgRun {
		runLocal()
		return
	}

	if flgDeployProd {
		deployProd()
		return
	}

	if flgImportNotion {
		importNotion()
		return
	}
	flag.Usage()
}
