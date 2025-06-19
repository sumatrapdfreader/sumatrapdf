package do

import (
	"archive/zip"
	"bufio"
	"bytes"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"sync"
	"sync/atomic"

	"github.com/andybalholm/brotli"
	"github.com/kjk/common/u"
	"github.com/klauspost/compress/zstd"
	"github.com/ulikunitz/xz/lzma"
)

func compressFileWithBr(path string) ([]byte, error) {
	buf := bytes.Buffer{}
	w := brotli.NewWriterLevel(&buf, brotli.BestCompression)
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	if _, err := io.Copy(w, f); err != nil {
		return nil, err
	}
	if err := w.Close(); err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

func compressWithZstd(path string) ([]byte, error) {
	buf := bytes.Buffer{}
	w, err := zstd.NewWriter(&buf, zstd.WithEncoderLevel(zstd.SpeedBestCompression))
	if err != nil {
		return nil, err
	}
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	if _, err := io.Copy(w, f); err != nil {
		return nil, err
	}
	if err := w.Close(); err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

func compressWithLzma2(path string) ([]byte, error) {
	buf := bytes.Buffer{}
	bw := bufio.NewWriter(&buf)
	w, err := lzma.NewWriter2(bw)
	if err != nil {
		return nil, err
	}
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	if _, err := io.Copy(w, f); err != nil {
		return nil, err
	}
	if err := w.Close(); err != nil {
		return nil, err
	}
	if err := bw.Flush(); err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

func compressWithLzma2Better(path string) ([]byte, error) {
	buf := bytes.Buffer{}
	bw := bufio.NewWriter(&buf)
	var c lzma.Writer2Config
	c.DictCap = (8 * 1024 * 1024) * 16
	if err := c.Verify(); err != nil {
		return nil, err
	}
	w, err := c.NewWriter2(bw)
	if err != nil {
		return nil, err
	}
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	if _, err := io.Copy(w, f); err != nil {
		return nil, err
	}
	if err := w.Close(); err != nil {
		return nil, err
	}
	if err := bw.Flush(); err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

func creaZipWithCompressFunction(zipPath string, dir string, fielPaths []string, compressFunc func(string) ([]byte, error), comprSuffix string) error {
	os.Remove(zipPath)
	w, err := os.Create(zipPath)
	if err != nil {
		return err
	}
	zw := zip.NewWriter(w)
	defer func() {
		err = zw.Close()
	}()
	var wg sync.WaitGroup
	nConcurrent := runtime.NumCPU()
	var perr atomic.Pointer[error]
	sem := make(chan bool, nConcurrent)
	for _, f := range fielPaths {
		path := filepath.Join(dir, f)
		wg.Add(1)
		sem <- true
		go func() {
			data, err := compressFunc(path)
			if err == nil {
				zipPath := filepath.ToSlash(f) + comprSuffix
				err = addZipDataStore(zw, data, zipPath)
			}
			if err != nil {
				perr.Store(&err)
			}
			<-sem
			wg.Done()
		}()
	}
	wg.Wait()
	err = zw.Close()
	if err != nil {
		return err
	}
	errVal := perr.Load()
	if errVal != nil {
		return *errVal
	}
	return err // from defer
}

func createLzsaFromFiles(lzsaPath string, dir string, files []string) error {
	args := []string{lzsaPath}
	args = append(args, files...)
	curDir, err := os.Getwd()
	if err != nil {
		return err
	}
	makeLzsaPath := filepath.Join(curDir, "bin", "MakeLZSA.exe")
	cmd := exec.Command(makeLzsaPath, args...)
	cmd.Dir = dir
	runCmdLoggedMust(cmd)
	return nil
}

func testCompressOneOff() {
	dir := filepath.Join("out", "rel64")
	files := []string{"SumatraPDF.exe", "SumatraPDF-dll.exe", "libmupdf.pdb", "SumatraPDF.pdb", "SumatraPDF-dll.pdb"}
	origSize := int64(0)
	for _, f := range files {
		origSize += u.FileSize(filepath.Join(dir, f))
	}
	logf("origSize: %s\n", u.FormatSize(origSize))

	{
		archivePath := filepath.Join(dir, "rel64.lzma2.better.zip")
		os.Remove(archivePath)
		logf("\nCreating %s (%d threads)\n", archivePath, runtime.NumCPU())
		printDur := measureDuration()
		creaZipWithCompressFunction(archivePath, dir, files, compressWithLzma2Better, ".lzma2")
		printDur()
		compressedSize := u.FileSize(archivePath)
		ratio := float64(origSize) / float64(compressedSize)
		logf("compressedSize: %s, ratio: %.2f\n", u.FormatSize(compressedSize), ratio)
	}
	{
		archivePath := filepath.Join(dir, "rel64.lzma2.zip")
		os.Remove(archivePath)
		logf("\nCreating %s (%d threads)\n", archivePath, runtime.NumCPU())
		printDur := measureDuration()
		creaZipWithCompressFunction(archivePath, dir, files, compressWithLzma2, ".lzma2")
		printDur()
		compressedSize := u.FileSize(archivePath)
		ratio := float64(origSize) / float64(compressedSize)
		logf("compressedSize: %s, ratio: %.2f\n", u.FormatSize(compressedSize), ratio)
	}
	{
		archivePath := filepath.Join(dir, "rel64.br.zip")
		os.Remove(archivePath)
		logf("\nCreating %s (%d threads)\n", archivePath, runtime.NumCPU())
		printDur := measureDuration()
		creaZipWithCompressFunction(archivePath, dir, files, compressFileWithBr, ".br")
		printDur()
		compressedSize := u.FileSize(archivePath)
		ratio := float64(origSize) / float64(compressedSize)
		logf("compressedSize: %s, ratio: %.2f\n", u.FormatSize(compressedSize), ratio)
	}
	{
		archivePath := filepath.Join(dir, "rel64.zstd.zip")
		os.Remove(archivePath)
		logf("\nCreating %s (%d threads)\n", archivePath, runtime.NumCPU())
		printDur := measureDuration()
		creaZipWithCompressFunction(archivePath, dir, files, compressWithZstd, ".zstd")
		printDur()
		compressedSize := u.FileSize(archivePath)
		ratio := float64(origSize) / float64(compressedSize)
		logf("compressedSize: %s, ratio: %.2f\n", u.FormatSize(compressedSize), ratio)
	}
	{
		logf("\nCreating rel64.lzsa using\n")
		printDur := measureDuration()
		archivePath := filepath.Join(dir, "rel64.lzsa")
		os.Remove(archivePath)
		createLzsaFromFiles("rel64.lzsa", dir, files)
		printDur()
		compressedSize := u.FileSize(archivePath)
		ratio := float64(origSize) / float64(compressedSize)
		logf("compressedSize: %s, ratio: %.2f\n", u.FormatSize(compressedSize), ratio)
	}
}
