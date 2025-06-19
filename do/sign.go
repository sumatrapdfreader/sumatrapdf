package do

import (
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"
)

func runCmdLogged(cmd *exec.Cmd) error {
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	logf("> %s\n", cmd.String())
	return cmd.Run()
}

// https://zabkat.com/blog/code-signing-sha1-armageddon.htm
// signtool sign /n "subject name" /t http://timestamp.comodoca.com/authenticode myInstaller.exe
// signtool sign /n "subject name" /fd sha256 /tr http://timestamp.comodoca.com/rfc3161 /td sha256 /as myInstaller.exe
// signtool args (https://msdn.microsoft.com/en-us/library/windows/desktop/aa387764(v=vs.85).aspx):
//
//	/as          : append signature
//	/n ${name}   : name of the certificate, must be installed in the cert store
//	/fd ${alg}   : specify digest algo, default is sha1, SHA256 is recommended
//	/t ${url}    : timestamp server
//	/tr ${url}   : timestamp rfc 3161 server
//	/td ${alg}   : for /tr, must be after /tr
//	/du ${url}   : URL for expanded description of the signed content
//	/v           : verbose
//	/debug       : show debugging info
func signFiles(dir string, files []string) error {
	signtoolPath := detectSigntoolPathMust()

	desc := "https://www.sumatrapdfreader.org"
	signServer := "http://time.certum.pl/"
	//signServer := "http://timestamp.verisign.com/scripts/timstamp.dll"
	//signServer := "http://timestamp.sectigo.com"

	// retry 3 times because signing might fail due to temorary error
	// ("The specified timestamp server either could not be reached or")
	var err error
	for i := 0; i < 3; i++ {

		// Note: not signing with sha1 for pre-win-7
		// We don't support win7 anymore

		// sign with sha256 for win7+ ater Jan 2016
		args := []string{"sign",
			"/t", signServer,
			"/du", desc,
			"/n", "Open Source Developer, Krzysztof Kowalczyk",
			"/fd", "sha256",
			"/debug",
			"/v",
		}
		args = append(args, files...)
		cmd := exec.Command(signtoolPath, args...)
		cmd.Dir = dir
		err = runCmdLogged(cmd)

		if err == nil {
			return nil
		}
		logf("signFiles: failed with: '%s', will retry in 15 seconds\n", err)
		time.Sleep(time.Second * 15)
	}
	return err
}

func shouldSign(f os.DirEntry) bool {
	if f.IsDir() {
		return false
	}
	ext := filepath.Ext(f.Name())
	return ext == ".exe" || ext == ".msix"
}

func signExesInDir(dir string) error {
	logf("signing exes in '%s'\n", dir)
	files, err := os.ReadDir(dir)
	if err != nil {
		return err
	}
	var exes []string
	for _, f := range files {
		if shouldSign(f) {
			exes = append(exes, f.Name())
		}
	}
	logf("to sign: %s\n", strings.Join(exes, ", "))
	return signFiles(dir, exes)
}
