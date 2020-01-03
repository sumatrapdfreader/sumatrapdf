package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	"github.com/kjk/u"
)

func runCmdLogged(cmd *exec.Cmd) error {
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	fmt.Printf("> %s\n", cmd)
	return cmd.Run()
}

func hasCertPwd() bool {
	return strings.TrimSpace(os.Getenv("CERT_PWD")) != ""
}

func failIfNoCertPwd() {
	panicIf(!hasCertPwd(), "CERT_PWD env variable is not set")
}

// http://zabkat.com/blog/code-signing-sha1-armageddon.htm
// signtool sign /n "subject name" /t http://timestamp.comodoca.com/authenticode myInstaller.exe
// signtool sign /n "subject name" /fd sha256 /tr http://timestamp.comodoca.com/rfc3161 /td sha256 /as myInstaller.exe
// signtool args (https://msdn.microsoft.com/en-us/library/windows/desktop/aa387764(v=vs.85).aspx):
//   /as          : append signature
//   /fd ${alg}   : specify digest algo, default is sha1
//   /t ${url}    : timestamp server
//   /tr ${url}   : timestamp rfc 3161 server
//   /td ${alg}   : for /tr, must be after /tr
//   /du ${url}   : URL for expanded description of the signed content.
func signMust(path string) {
	// the sign tool is finicky, so copy the cert to the same dir as
	// the exe we're signing

	var err error
	// signing might fail due to temorary error ("The specified timestamp server either could not be reached or")
	// so retry
	for i := 0; i < 3; i++ {
		certPwd := os.Getenv("CERT_PWD")
		if certPwd == "" {
			// to make it easy on others, skip signing if
			if !shouldSignAndUpload() {
				logf("skipped signing of '%s' because CERT_PWD not set\n", path)
				return
			}
			panic("my repo but no CERT_PWD")
		}

		signtoolPath := detectSigntoolPath()
		fileDir := filepath.Dir(path)
		fileName := filepath.Base(path)
		certSrc := filepath.Join("scripts", "cert.pfx")
		certDest := filepath.Join(fileDir, "cert.pfx")
		u.CopyFileMust(certDest, certSrc)
		{
			// sign with sha1 for pre-win-7
			cmd := exec.Command(signtoolPath, "sign", "/t", "http://timestamp.verisign.com/scripts/timstamp.dll",
				"/du", "http://www.sumatrapdfreader.org", "/f", "cert.pfx",
				"/p", certPwd, fileName)
			cmd.Dir = fileDir
			err = runCmdLogged(cmd)
		}

		if err == nil {
			// double-sign with sha2 for win7+ ater Jan 2016
			cmd := exec.Command(signtoolPath, "sign", "/fd", "sha256", "/tr", "http://timestamp.comodoca.com/rfc3161",
				"/td", "sha256", "/du", "http://www.sumatrapdfreader.org", "/f", "cert.pfx",
				"/p", certPwd, "/as", fileName)
			cmd.Dir = fileDir
			err = runCmdLogged(cmd)
		}
		if err == nil {
			return
		}
		time.Sleep(time.Second * 15)
	}
	u.Must(err)
}
