package main

import (
	"fmt"
	"sync"

	"github.com/kjk/common/u"
	"github.com/rodrigocfd/windigo/win"
	"github.com/rodrigocfd/windigo/win/co"
)

const kPipeName = `\\.\pipe\LOCAL\ArsLexis-Logger`
const kBufSize = 1024 * 16
const PIPE_UNLIMITED_INSTANCES = 255
const INVALID_HANDLE_VALUE = -1

var wc sync.WaitGroup

func handlePipe(hPipe win.HPIPE) {
	fmt.Printf("handlePipe\n")
	var buf [kBufSize]byte
	var cbBytesRead = 0
	for {
		n, err := hPipe.ReadFile(buf[:], nil)
		if err != nil {
			fmt.Printf("ReadFile: returned %s\n", err)
			break
		}
		cbBytesRead += int(n)
		d := buf[:int(n)]
		fmt.Printf("%s", string(d))
	}
	hPipe.DisconnectNamedPipe()
	hPipe.CloseHandle()
	wc.Done()
}

func createNamedPipe() (win.HPIPE, error) {
	const openMode = co.PIPE_ACCESS_INBOUND
	const mode = co.PIPE_TYPE_MESSAGE | co.PIPE_READMODE_MESSAGE | co.PIPE_WAIT
	const maxInstances = PIPE_UNLIMITED_INSTANCES
	return win.CreateNamedPipe(kPipeName, openMode, mode, maxInstances, kBufSize, kBufSize, 0, nil)

}

func IsValidHandle(h win.HANDLE) bool {
	invalid := h == 0 || int(h) == INVALID_HANDLE_VALUE
	return !invalid
}

func main() {
	fmt.Printf("Logview for SumatraPDF\n")
	var hPipe win.HPIPE
	var err error
	for {
		hPipe, err = createNamedPipe()
		u.Must(err)
		if !IsValidHandle(win.HANDLE(hPipe)) {
			fmt.Printf("couldn't open pipe\n")
			return
		}
		err = hPipe.ConnectNamedPipe()
		if err != nil {
			fmt.Printf("client couldn't connect to our pipe\n")
			hPipe.CloseHandle()
			continue
		}
		wc.Add(1)
		go handlePipe(hPipe)
	}
}
