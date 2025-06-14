package server

import (
	"fmt"
	"sync"

	"github.com/kjk/common/u"
	"github.com/rodrigocfd/windigo/win"
	"github.com/rodrigocfd/windigo/win/co"
)

const kPipeName = "\\\\.\\pipe\\LOCAL\\ArsLexis-Logger"
const kBufSize = 1024 * 64
const PIPE_UNLIMITED_INSTANCES = 255
const INVALID_HANDLE_VALUE = -1

var connNo = 1

var wc sync.WaitGroup

func IsValidHandle(h win.HANDLE) bool {
	invalid := h == 0 || int(h) == INVALID_HANDLE_VALUE
	return !invalid
}

func handlePipe(hPipe win.HPIPE, connNo int) {
	var buf [kBufSize]byte
	var cbBytesRead = 0
	for {
		// TODO: this can return "more bytes needed". I improved things by incrasing kBufSize
		// hPipe.ReadFile() sets n to 0 if error, which is probably a mistake
		n, err := hPipe.ReadFile(buf[:], nil)
		if err != nil {
			s := fmt.Sprintf("ReadFile: failed with '%s', n=%d\n", err, n)
			appendToLastLogs(connNo, s)
			break
		}
		cbBytesRead += int(n)
		s := string(buf[:n])
		appendToLastLogs(connNo, s)
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

func pipeThread() {
	var hPipe win.HPIPE
	var err error
	for {
		hPipe, err = createNamedPipe()
		u.Must(err)
		if !IsValidHandle(win.HANDLE(hPipe)) {
			return
		}
		err = hPipe.ConnectNamedPipe()
		if err != nil {
			logf("client couldn't connect to our pipe\n")
			hPipe.CloseHandle()
			continue
		}
		wc.Add(1)
		go handlePipe(hPipe, connNo)
		connNo++
	}
}
