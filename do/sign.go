package main

import (
	"os"
	"os/exec"
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
func signFilesMust(dir string, files []string) {
	// the sign tool is finicky, so copy the cert to the same dir as
	// the exe we're signing

	// retry 3 times because signing might fail due to temorary error
	// ("The specified timestamp server either could not be reached or")
	var err error
	for i := 0; i < 3; i++ {
		signtoolPath := detectSigntoolPath()

		signServer := "http://time.certum.pl/"
		//signServer := "http://timestamp.verisign.com/scripts/timstamp.dll"
		//signServer := "http://timestamp.sectigo.com"
		//desc := "https://www.sumatrapdfreader.org"

		// TODO: sign with sha1 for pre-win-7. We don't support win7 anymore

		{
			// TODO: add "/du", desc,
			// sign with sha256 for win7+ ater Jan 2016
			args := []string{"sign",
				"/t", signServer,
				"/n", "Open Source Developer, Krzysztof Kowalczyk",
				"/fd", "sha256",
				"/debug",
				"/v",
			}
			args = append(args, files...)
			cmd := exec.Command(signtoolPath, args...)
			cmd.Dir = dir
			err = runCmdLogged(cmd)
		}

		if err == nil {
			return
		}
		time.Sleep(time.Second * 15)
	}
	must(err)
}
