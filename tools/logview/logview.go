package main

import (
	"fmt"
	"sync"
)

const kPipeName = "\\\\.\\pipe\\SumatraPDFLogger"
const kBufSize = 1024 * 16

var wc sync.WaitGroup

func handlePipe(hPipe HFILE) {
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
	DisconnectNamedPipe(hPipe)
	hPipe.CloseHandle()
	wc.Done()
}

func main() {
	fmt.Printf("Hello! This is a PipeView\n")
	var hPipe HFILE

	for {
		const openMode = PIPE_ACCESS_INBOUND
		const mode = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT
		const maxInstances = PIPE_UNLIMITED_INSTANCES
		hPipe = CreateNamedPipe(kPipeName, openMode, mode, maxInstances, kBufSize, kBufSize, 0)
		if !IsValidHandle(HANDLE(hPipe)) {
			fmt.Printf("couldn't open pipe\n")
			return
		}
		ok := ConnectNamedPipe(hPipe)
		// if !ok {
		// ok = GetLastError() == ERROR_PIPE_CONNECTED
		// }
		if !ok {
			fmt.Printf("client couldn't connect to our pipe\n")
			hPipe.CloseHandle()
			continue
		}
		wc.Add(1)
		go handlePipe(hPipe)
	}
}
