package main

import (
	"bytes"
	"embed"
	"fmt"
	"io/fs"
	"net/http"
	"net/http/httputil"
	"net/url"
	"os"
	"slices"
	"strings"
	"time"

	"github.com/felixge/httpsnoop"
	hutil "github.com/kjk/common/httputil"
	"github.com/kjk/u"
)

var (
	//go:embed dist/*
	distFS embed.FS

	httpSrv     *http.Server
	chPipeReads chan PipeChunk = make(chan PipeChunk, 10000)
)

const httpPort = 17892

func mkFsysDirPublic() fs.FS {
	dir := "public"
	fsys := os.DirFS(dir)
	printFS(fsys)
	logf("mkFsysDirPublic: serving from dir '%s'\n", dir)
	return fsys
}

func mkServeFileOptions(fsys fs.FS) *hutil.ServeFileOptions {
	return &hutil.ServeFileOptions{
		SupportCleanURLS:     true,
		ForceCleanURLS:       true,
		FS:                   fsys,
		DirPrefix:            "dist/",
		LongLivedURLPrefixes: []string{"/assets/"},
		ServeCompressed:      false, // when served via Cloudflare, no need to compress
	}
}

// in dev, proxyHandler redirects assets to vite web server
// in prod, assets must be pre-built in frontend/dist directory
func makeHTTPServer(serveOpts *hutil.ServeFileOptions, proxyHandler *httputil.ReverseProxy) *http.Server {
	panicIf(serveOpts == nil, "must provide serveOpts")

	mainHandler := func(w http.ResponseWriter, r *http.Request) {
		uri := r.URL.Path
		logf("mainHandler: '%s'\n", r.RequestURI)

		switch uri {
		case "/ping", "/ping.txt":
			content := bytes.NewReader([]byte("pong"))
			http.ServeContent(w, r, "foo.txt", time.Time{}, content)
			return
		case "/kill":
			logf("mainHandler: got /kill, exiting\n")
			// TODO: cleaner kill
			os.Exit(0)
			return
		case "/sse":
			logf("mainHandler: got /sse, serving SSE\n")
			w.Header().Set("Content-Type", "text/event-stream")
			w.Header().Set("Cache-Control", "no-cache")
			w.Header().Set("Connection", "keep-alive")

			for v := range chPipeReads {
				logf("sse: '%s'\n", string(v.d))
				_, err := fmt.Fprintf(w, "data: %d %s\n\n", v.connNo, string(v.d))
				if err != nil {
					logf("sse: error writing to response writer: %v\n", err)
					return
				}
				w.(http.Flusher).Flush()
			}
			return
		}

		if strings.HasPrefix(uri, "/event") {
			// logtastic.HandleEvent(w, r)
			return
		}

		tryServeRedirect := func(uri string) bool {
			return false
		}
		if tryServeRedirect(uri) {
			return
		}

		if proxyHandler != nil {
			transformRequestForProxy := func() {
				uris := []string{}
				shouldProxyURI := slices.Contains(uris, uri)
				if !shouldProxyURI {
					return
				}
				newPath := uri + ".html"
				newURI := strings.Replace(r.URL.String(), uri, newPath, 1)
				var err error
				r.URL, err = url.Parse(newURI)
				must(err)
			}

			transformRequestForProxy()
			proxyHandler.ServeHTTP(w, r)
			return
		}

		if hutil.TryServeURLFromFS(w, r, serveOpts) {
			logf("mainHandler: served '%s' via httputil.TryServeFile\n", uri)
			return
		}

		http.NotFound(w, r)
	}

	handlerWithMetrics := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		httpsnoop.CaptureMetrics(http.HandlerFunc(mainHandler), w, r)
		defer func() {
			if p := recover(); p != nil {
				logErrorf("handlerWithMetrics: panicked with with %v\n", p)
				errStr := fmt.Sprintf("Error: %v", p)
				http.Error(w, errStr, http.StatusInternalServerError)
				return
			}
			// logHTTPReq(r, m.Code, m.Written, m.Duration)
			// logtastic.LogHit(r, m.Code, m.Written, m.Duration)
			// axiomLogHTTPReq(ctx(), r, m.Code, int(m.Written), m.Duration)
		}()
	})

	httpSrv := &http.Server{
		ReadTimeout:  120 * time.Second,
		WriteTimeout: 120 * time.Second,
		IdleTimeout:  120 * time.Second,
		Handler:      http.HandlerFunc(handlerWithMetrics),
	}
	httpAddr := fmt.Sprintf(":%d", httpPort)
	if isWinOrMac() {
		httpAddr = "localhost" + httpAddr
	}
	httpSrv.Addr = httpAddr
	return httpSrv
}

func serverListenAndWait(httpSrv *http.Server) func() {
	chServerClosed := make(chan bool, 1)
	go func() {
		err := httpSrv.ListenAndServe()
		// mute error caused by Shutdown()
		if err == http.ErrServerClosed {
			err = nil
		}
		if err == nil {
			logf("HTTP server shutdown gracefully\n")
		} else {
			logf("httpSrv.ListenAndServe error '%s'\n", err)
		}
		chServerClosed <- true
	}()
	return func() {
		waitForSigIntOrKill()

		logf("Got one of the signals. Shutting down http server\n")
		_ = httpSrv.Shutdown(ctx())
		select {
		case <-chServerClosed:
			// do nothing
		case <-time.After(time.Second * 5):
			// timeout
			logf("timed out trying to shut down http server")
		}
		// logf("stopping logtastic\n")
		// logtastic.Stop()
	}
}

func waitUntilServerReady(url string) {
	for range 10 {
		resp, err := http.Get(url)
		if err != nil {
			logf("waitUntilServerReady: error '%s', retrying\n", err)
			time.Sleep(time.Second * 1)
			continue
		}
		if resp.StatusCode == http.StatusOK {
			logf("waitUntilServerReady: server is ready\n")
			return
		}
		logf("waitUntilServerReady: got status code %d, retrying\n", resp.StatusCode)
		time.Sleep(time.Second * 1)
	}
	logf("waitUntilServerReady: giving up after 10 attempts\n")
}

func runServerDev() {
	proxyURLStr := "http://localhost:3047" // asme as in vite.config.js
	logf("running dev server on port %d, proxying to %s", httpPort, proxyURLStr)
	{
		runLoggedInDir(".", "bun", "install")
		closeDev, err := startLoggedInDir(".", "bun", "run", "dev")
		must(err)
		defer closeDev()
	}
	proxyURL, err := url.Parse(proxyURLStr)
	must(err)
	proxyHandler := httputil.NewSingleHostReverseProxy(proxyURL)
	fsys := mkFsysDirPublic()
	serveOpts := mkServeFileOptions(fsys)
	serveOpts.DirPrefix = "./"
	httpSrv = makeHTTPServer(serveOpts, proxyHandler)
	logf("runServerDev(): starting on '%s', dev: %v\n", httpSrv.Addr, true)
	waitFn := serverListenAndWait(httpSrv)

	if isWinOrMac() {
		url := fmt.Sprintf("http://%s/ping.txt", httpSrv.Addr)
		waitUntilServerReady(url)
		u.OpenBrowser("http://" + httpSrv.Addr)
	}
	go pipeThread(chPipeReads)
	waitFn()
}
