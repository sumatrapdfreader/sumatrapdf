package main

import (
	"fmt"
	"io/ioutil"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	"github.com/kjk/common/server"
)

func makeHTTPServer(srv *server.Server) *http.Server {
	panicIf(srv == nil, "must provide srv")
	httpPort := 8080
	if srv.Port != 0 {
		httpPort = srv.Port
	}
	httpAddr := fmt.Sprintf(":%d", httpPort)
	if isWindows() {
		httpAddr = "localhost" + httpAddr
	}

	mainHandler := func(w http.ResponseWriter, r *http.Request) {
		//logf(ctx(), "mainHandler: '%s'\n", r.RequestURI)
		// timeStart := time.Now()
		cw := server.CapturingResponseWriter{ResponseWriter: w}

		tryServeRedirect := func(uri string) bool {
			return server.TryServeBadClient(&cw, r)
		}

		defer func() {
			if p := recover(); p != nil {
				logf(ctx(), "mainHandler: panicked with with %v\n", p)
				http.Error(w, fmt.Sprintf("Error: %v", r), http.StatusInternalServerError)
				//logHTTPReqShort(r, http.StatusInternalServerError, 0, time.Since(timeStart))
				//LogHTTPReq(r, http.StatusInternalServerError, 0, time.Since(timeStart))
			} else {
				//LogHTTPReq(r, cw.StatusCode, cw.Size, time.Since(timeStart))
				//logHTTPReqShort(r, cw.StatusCode, cw.Size, time.Since(timeStart))
			}
		}()
		uri := r.URL.Path

		serve, is404 := srv.FindHandler(uri)
		if serve != nil {
			if is404 {
				if tryServeRedirect(uri) {
					return
				}
			}
			serve(&cw, r)
			return
		}
		http.NotFound(&cw, r)
	}

	httpSrv := &http.Server{
		ReadTimeout:  120 * time.Second,
		WriteTimeout: 120 * time.Second,
		IdleTimeout:  120 * time.Second, // introduced in Go 1.8
		Handler:      http.HandlerFunc(mainHandler),
	}
	httpSrv.Addr = httpAddr
	return httpSrv
}
func websiteRunLocally(dir string) {
	h := server.NewDirHandler(dir, "/", nil)
	srv := &server.Server{
		Handlers:  []server.Handler{h},
		CleanURLS: true,
	}
	httpSrv := makeHTTPServer(srv)
	logf(ctx(), "Starting server on http://%s'\n", httpSrv.Addr)
	if isWindows() {
		openBrowser(fmt.Sprintf("http://%s", httpSrv.Addr))
	}
	err := httpSrv.ListenAndServe()
	logf(ctx(), "runServer: httpSrv.ListenAndServe() returned '%s'\n", err)
}

func fileDownload(uri string, dstPath string) error {
	must(createDirForFile(dstPath))
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
		if pathExists(dstPath) {
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
	panicIf(!isGitClean())
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
