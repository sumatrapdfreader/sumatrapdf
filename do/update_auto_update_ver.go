package main

import (
	"fmt"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/kjk/u"
)

// Format of auto-update file:
/*
[SumatraPDF]
Latest 3.1.2
*/

/*
for 3.1.2 and earlier, we upload:
https://kjkpub.s3.amazonaws.com/sumatrapdf/sumpdf-update.txt
https://kjkpub.s3.amazonaws.com/sumatrapdf/sumpdf-latest.txt

for 3.2 and later, we use
https://www.sumatrapdfreader.org/update-check-rel.txt

This script uploads to s3 and updates website/update-check-rel.txt,
which must be then deployed.
*/

// ver should be in format:
// 3
// 3.1
// 3.1.2
func validateVer(ver string) {
	parts := strings.Split(ver, ".")
	panicIf(len(parts) > 3)
	for _, p := range parts {
		n, err := strconv.Atoi(p)
		panicIfErr(err)
		panicIf(n < 0 || n > 19)
	}
}

func updateAutoUpdateVer(ver string) {
	validateVer(ver)
	// TODO: verify it's bigger than the current vresion
	s := fmt.Sprintf(`[SumatraPDF]
Latest %s
`, ver)
	fmt.Printf("Content of update file:\n%s\n\n", s)
	c := newS3Client()
	{
		remotePath := "sumatrapdf/sumpdf-update.txt"
		err := c.UploadString(remotePath, s, true)
		panicIfErr(err)
	}
	{
		remotePath := "sumatrapdf/sumpdf-latest.txt"
		err := c.UploadString(remotePath, s, true)
		panicIfErr(err)
	}

	path := filepath.Join("website", "update-check-rel.txt")
	u.WriteFileMust(path, []byte(s))
	fmt.Printf("Don't forget to checkin file '%s' and deploy website\n", path)
}
