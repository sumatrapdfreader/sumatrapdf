package main

import (
	"archive/zip"
	"bytes"
	"compress/flate"
	"fmt"
	"io"
	"io/fs"
	"net/http"
	"os"
	"os/signal"
	"path"
	"path/filepath"
	"strings"
	"syscall"
	"time"
)

// ServerConfig represents all files known to the server
type ServerConfig struct {
	Handlers  []Handler
	CleanURLS bool
	Port      int
}

type HandlerFunc = func(w http.ResponseWriter, r *http.Request)

// Handler represents one or more urls and their content
type Handler interface {
	// returns a handler for this url
	// if nil, doesn't handle this url
	Get(url string) HandlerFunc
	// get all urls handled by this Handler
	// useful for e.g. saving a static copy to disk
	URLS() []string
}

// FileWriter implements http.ResponseWriter interface for writing to a file
type FileWriter struct {
	w io.Writer
}

func (w *FileWriter) Header() http.Header {
	return nil
}

func (w *FileWriter) Write(p []byte) (int, error) {
	return w.w.Write(p)
}

func (w *FileWriter) WriteHeader(statusCode int) {
	// no-op
}

type FileHandler struct {
	// Path on disk for this file
	Path string
	// list of urls that this file matches
	URL []string
}

func serveFile(w http.ResponseWriter, r *http.Request, path string) {
	if r == nil {
		d := readFileMust(path)
		_, err := w.Write(d)
		must(err)
		return
	}
	http.ServeFile(w, r, path)
}

func serveContent(w http.ResponseWriter, r *http.Request, d []byte, path string) {
	if r == nil {
		_, err := w.Write(d)
		must(err)
		return
	}
	content := bytes.NewReader(d)
	http.ServeContent(w, r, path, time.Now(), content)
}

func (h *FileHandler) Get(url string) HandlerFunc {
	for _, u := range h.URL {
		// urls are case-insensitive
		if strings.EqualFold(u, url) {
			return func(w http.ResponseWriter, r *http.Request) {
				serveFile(w, r, h.Path)
			}
		}
	}
	return nil
}

func (h *FileHandler) URLS() []string {
	return h.URL
}

func NewFileHandler(path string, url string, urls ...string) *FileHandler {
	// early detection of problems
	panicIf(!fileExists(path), "file '%s' doesn't exist", path)
	res := &FileHandler{
		Path: path,
		URL:  []string{url},
	}
	res.URL = append(res.URL, urls...)
	return res
}

type DirHandler struct {
	Dir       string
	URLPrefix string

	URL   []string
	paths []string // same order as URL
}

func (h *DirHandler) Get(url string) func(w http.ResponseWriter, r *http.Request) {
	for i, u := range h.URL {
		// urls are case-insensitive
		if strings.EqualFold(u, url) {
			return func(w http.ResponseWriter, r *http.Request) {
				serveFile(w, r, h.paths[i])
			}
		}
	}
	return nil
}

func (h *DirHandler) URLS() []string {
	return h.URL
}

func getURLSForFiles(startDir string, urlPrefix string, acceptFile func(string) bool) (urls []string, paths []string) {
	filepath.WalkDir(startDir, func(filePath string, d fs.DirEntry, err error) error {
		if err != nil {
			return nil
		}
		if !d.Type().IsRegular() {
			return nil
		}
		if acceptFile != nil && !acceptFile(filePath) {
			return nil
		}
		dir := strings.TrimPrefix(filePath, startDir)
		dir = filepath.ToSlash(dir)
		dir = strings.TrimPrefix(dir, "/")
		uri := path.Join(urlPrefix, dir)
		//logf("getURLSForFiles: dir: '%s'\n", dir)
		urls = append(urls, uri)
		paths = append(paths, filePath)
		return nil
	})
	return
}

func NewDirHandler(dir string, urlPrefix string, acceptFile func(string) bool) *DirHandler {
	urls, paths := getURLSForFiles(dir, urlPrefix, acceptFile)
	return &DirHandler{
		Dir:       dir,
		URLPrefix: urlPrefix,
		URL:       urls,
		paths:     paths,
	}
}

type DynamicHandler struct {
	matches func(string) func(http.ResponseWriter, *http.Request)
	urls    func() []string
}

func (h *DynamicHandler) Get(uri string) func(http.ResponseWriter, *http.Request) {
	return h.matches(uri)
}

func (h *DynamicHandler) URLS() []string {
	return h.urls()
}

func NewDynamicHandler(matches func(string) func(http.ResponseWriter, *http.Request), urls func() []string) *DynamicHandler {
	return &DynamicHandler{
		matches: matches,
		urls:    urls,
	}
}

type InMemoryFilesHandler struct {
	files map[string][]byte
}

func (h *InMemoryFilesHandler) Get(uri string) func(http.ResponseWriter, *http.Request) {
	for path, d := range h.files {
		if strings.EqualFold(path, uri) {
			return func(w http.ResponseWriter, r *http.Request) {
				serveContent(w, r, d, uri)
			}
		}
	}
	return nil
}

func (h *InMemoryFilesHandler) URLS() []string {
	var urls []string
	for path := range h.files {
		urls = append(urls, path)
	}
	return urls
}

func NewInMemoryFilesHandler(files map[string][]byte) *InMemoryFilesHandler {
	for path, d := range files {
		newp := strings.Replace(path, "\\", "/", -1)
		if !strings.HasPrefix(newp, "/") {
			newp = "/" + newp
		}
		newp = "/" + path
		if path == newp {
			continue
		}
		files[newp] = d
		delete(files, path)
	}

	return &InMemoryFilesHandler{
		files: files,
	}
}

func WriteServerFilesToDir(dir string, handlers []Handler) {
	for _, h := range handlers {
		urls := h.URLS()
		for _, uri := range urls {
			path := filepath.Join(dir, uri)
			must(createDirForFile(path))
			f, err := os.Create(path)
			must(err)
			fw := &FileWriter{
				w: f,
			}
			serve := h.Get(uri)
			panicIf(serve == nil, "must have a handler for '%s'", uri)
			serve(fw, nil)
			err = f.Close()
			must(err)
			sizeStr := formatSize(getFileSize(path))
			logf(ctx(), "WriteServerFilesToDir: '%s' of size %s\n", path, sizeStr)
		}
	}
}

func zipWriteContent(zw *zip.Writer, files map[string][]byte) error {
	for name, data := range files {
		fw, err := zw.Create(name)
		if err != nil {
			return err
		}
		_, err = fw.Write(data)
		if err != nil {
			return err
		}
	}
	return zw.Close()
}

func zipCreateFromContent(files map[string][]byte) ([]byte, error) {
	var buf bytes.Buffer
	zw := zip.NewWriter(&buf)
	zw.RegisterCompressor(zip.Deflate, func(out io.Writer) (io.WriteCloser, error) {
		return flate.NewWriter(out, flate.BestCompression)
	})
	err := zipWriteContent(zw, files)
	if err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}

func WriteServerFilesToZip(handlers []Handler) ([]byte, error) {
	files := map[string][]byte{}
	for _, h := range handlers {
		urls := h.URLS()
		for _, uri := range urls {
			path := strings.TrimPrefix(uri, "/")
			var buf bytes.Buffer
			fw := &FileWriter{
				w: &buf,
			}
			serve := h.Get(uri)
			panicIf(serve == nil, "must have a handler for '%s'", uri)
			serve(fw, nil)
			d := buf.Bytes()
			files[path] = d
			sizeStr := formatSize(int64(len(d)))
			logf(ctx(), "WriteServerFilesZip: '%s' of size %s\n", path, sizeStr)
		}
	}
	return zipCreateFromContent(files)
}

// returns function that will wait for SIGTERM signal (e.g. Ctrl-C) and
// shutdown the server
func StartServer(server *ServerConfig) func() {
	panicIf(server == nil, "must provide files")
	httpPort := 8080
	if server.Port != 0 {
		httpPort = server.Port
	}
	httpAddr := fmt.Sprintf(":%d", httpPort)
	if isWindows() {
		httpAddr = "localhost" + httpAddr
	}
	mux := &http.ServeMux{}
	handleAll := func(w http.ResponseWriter, r *http.Request) {
		uri := r.URL.Path
		if strings.HasSuffix(uri, "/") {
			uri += "index.html"
		}
		trySend := func(uri string) bool {
			for _, h := range server.Handlers {
				if send := h.Get(uri); send != nil {
					logf(ctx(), "handleFile: found match for '%s'\n", uri)
					send(w, r)
					return true
				}
			}
			return false
		}
		if trySend(uri) {
			return
		}
		ext := strings.ToLower(filepath.Ext(uri))
		shouldRepeat := server.CleanURLS
		switch ext {
		case ".html", ".js", ".css", ".txt", ".xml":
			shouldRepeat = false
		}
		if shouldRepeat && trySend(uri+".html") {
			return
		}
		gen404Candidates := func(uri string) []string {
			parts := strings.Split(uri, "/")
			n := len(parts)
			for n > 0 {
				n = len(parts) - 1
				if parts[n] != "" {
					break
				}
				parts = parts[:n]
			}
			var res []string
			for len(parts) > 0 {
				s := strings.Join(parts, "/") + "/404.html"
				res = append(res, s)
				parts = parts[:len(parts)-1]
			}
			return res
		}

		// try 404.html
		a := gen404Candidates(uri)
		for _, uri404 := range a {
			if trySend(uri404) {
				logf(ctx(), "handleFile: sent 404 '%s' for '%s'\n", uri404, uri)
				return
			}
		}
		logf(ctx(), "handleFile: no match for '%s'\n", uri)
		http.NotFound(w, r)
	}
	mux.HandleFunc("/", handleAll)
	var handler http.Handler = mux
	httpSrv := &http.Server{
		ReadTimeout:  120 * time.Second,
		WriteTimeout: 120 * time.Second,
		IdleTimeout:  120 * time.Second, // introduced in Go 1.8
		Handler:      handler,
	}
	httpSrv.Addr = httpAddr
	ctx := ctx()
	logf(ctx, "Starting server on http://%s'\n", httpAddr)
	if isWindows() {
		openBrowser(fmt.Sprintf("http://%s", httpAddr))
	}

	chServerClosed := make(chan bool, 1)
	go func() {
		err := httpSrv.ListenAndServe()
		// mute error caused by Shutdown()
		if err == http.ErrServerClosed {
			err = nil
		}
		must(err)
		logf(ctx, "trying to shutdown HTTP server\n")
		chServerClosed <- true
	}()

	return func() {
		c := make(chan os.Signal, 2)
		signal.Notify(c, os.Interrupt /* SIGINT */, syscall.SIGTERM)

		sig := <-c
		logf(ctx, "Got signal %s\n", sig)

		if httpSrv != nil {
			go func() {
				// Shutdown() needs a non-nil context
				_ = httpSrv.Shutdown(ctx)
			}()
			select {
			case <-chServerClosed:
				// do nothing
				logf(ctx, "server shutdown cleanly\n")
			case <-time.After(time.Second * 5):
				// timeout
				logf(ctx, "server killed due to shutdown timeout\n")
			}
		}
	}
}
