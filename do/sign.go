package main

import (
	"os"
	"os/exec"
	"path/filepath"

	"github.com/kjk/u"
)

func getCertPwd() string {
	certPwd := os.Getenv("CERT_PWD")
	u.PanicIf(certPwd == "", "CERT_PWD env variable must be set")
	return certPwd
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

	certPwd := getCertPwd()
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
		u.RunCmdLoggedMust(cmd)
	}

	{
		// double-sign with sha2 for win7+ ater Jan 2016
		cmd := exec.Command(signtoolPath, "sign", "/fd", "sha256", "/tr", "http://timestamp.comodoca.com/rfc3161",
			"/td", "sha256", "/du", "http://www.sumatrapdfreader.org", "/f", "cert.pfx",
			"/p", certPwd, "/as", fileName)
		cmd.Dir = fileDir
		u.RunCmdLoggedMust(cmd)
	}
}
