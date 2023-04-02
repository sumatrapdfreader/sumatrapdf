package main

import (
	"fmt"
	"syscall"
	"unsafe"
)

type HANDLE syscall.Handle
type HFILE HANDLE
type HEVENT HANDLE
type ERROR uint32

const (
	SUCCESS ERROR = 0 // The operation completed successfully.
)

const (
	_INVALID_HANDLE_VALUE = -1
)

// dwOpenMode
const (
	PIPE_ACCESS_INBOUND  = 0x00000001
	PIPE_ACCESS_OUTBOUND = 0x00000002
	PIPE_ACCESS_DUPLEX   = 0x00000003
)

// dwPipeMode
const (
	PIPE_WAIT                  = 0x00000000
	PIPE_NOWAIT                = 0x00000001
	PIPE_READMODE_BYTE         = 0x00000000
	PIPE_READMODE_MESSAGE      = 0x00000002
	PIPE_TYPE_BYTE             = 0x00000000
	PIPE_TYPE_MESSAGE          = 0x00000004
	PIPE_ACCEPT_REMOTE_CLIENTS = 0x00000000
	PIPE_REJECT_REMOTE_CLIENTS = 0x00000008
)

const (
	PIPE_UNLIMITED_INSTANCES = 255
)

// 📑 https://docs.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-overlapped
type OVERLAPPED struct {
	Internal     uintptr
	InternalHigh uintptr
	Pointer      uintptr
	HEvent       HEVENT
}

// Implements error interface.
func (err ERROR) Error() string {
	return err.String()
}

func (err ERROR) String() string {
	return fmt.Sprintf("[%d 0x%02x] %s",
		uint32(err), uint32(err), err.Unwrap().Error())
}

func (err ERROR) Unwrap() error {
	return syscall.Errno(err)
}

var (
	kernel32                = syscall.NewLazyDLL("kernel32.dll")
	CreateNamedPipeWProc    = kernel32.NewProc("CreateNamedPipeW")
	ReadFileProc            = kernel32.NewProc("ReadFile")
	ConnectNamedPipeProc    = kernel32.NewProc("ConnectNamedPipe")
	CloseHandleProc         = kernel32.NewProc("CloseHandle")
	DisconnectNamedPipeProc = kernel32.NewProc("DisconnectNamedPipe")
)

type _StrT struct{}

var Str _StrT

func (_StrT) ToNativePtr(s string) *uint16 {
	pstr, err := syscall.UTF16PtrFromString(s)
	if err != nil {
		panic(fmt.Sprintf("Str.ToNativePtr() failed \"%s\": %s", s, err))
	}
	return pstr
}

// 📑 https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile
func (hFile HFILE) ReadFile(
	buffer []byte, overlapped *OVERLAPPED) (numBytesRead uint32, e error) {

	ret, _, err := syscall.SyscallN(ReadFileProc.Addr(),
		uintptr(hFile), uintptr(unsafe.Pointer(&buffer[0])),
		uintptr(uint32(len(buffer))), uintptr(unsafe.Pointer(&numBytesRead)),
		uintptr(unsafe.Pointer(overlapped)))

	if wErr := ERROR(err); ret == 0 && wErr != SUCCESS {
		numBytesRead, e = 0, wErr
	}
	return
}

func (hFile HFILE) CloseHandle() error {
	ret, _, err := syscall.SyscallN(CloseHandleProc.Addr(),
		uintptr(hFile))
	if ret == 0 {
		return ERROR(err)
	}
	return nil
}

/*
HANDLE CreateNamedPipeW(
  [in]           LPCWSTR               lpName,
  [in]           DWORD                 dwOpenMode,
  [in]           DWORD                 dwPipeMode,
  [in]           DWORD                 nMaxInstances,
  [in]           DWORD                 nOutBufferSize,
  [in]           DWORD                 nInBufferSize,
  [in]           DWORD                 nDefaultTimeOut,
  [in, optional] LPSECURITY_ATTRIBUTES lpSecurityAttributes
);
*/

func CreateNamedPipe(name string, dwOpenMode uint, dwPipeMode uint, nMaxInstances uint, nOutBufferSize uint, nInBufferSize uint, nDefaultTimeOut uint) HFILE {
	ret, _, err := syscall.SyscallN(CreateNamedPipeWProc.Addr(),
		uintptr(unsafe.Pointer(Str.ToNativePtr(name))),
		uintptr(dwOpenMode),
		uintptr(dwPipeMode),
		uintptr(nMaxInstances),
		uintptr(nOutBufferSize),
		uintptr(nInBufferSize),
		uintptr(nDefaultTimeOut),
		uintptr(0))
	if ret == 0 {
		panic(ERROR(err))
	}
	return HFILE(ret)
}

/*
BOOL
WINAPI
ConnectNamedPipe(

	_In_ HANDLE hNamedPipe,
	_Inout_opt_ LPOVERLAPPED lpOverlapped
	);
*/
func ConnectNamedPipe(hNamedPipe HFILE) bool {
	ret, _, err := syscall.SyscallN(ConnectNamedPipeProc.Addr(),
		uintptr(hNamedPipe), uintptr(0))
	if err != 0 {
		return false
	}
	if ret > 0 {
		return true
	}
	return false
}

/*
BOOL
WINAPI
DisconnectNamedPipe(

	_In_ HANDLE hNamedPipe
	);
*/
func DisconnectNamedPipe(hNamedPipe HFILE) bool {
	ret, _, err := syscall.SyscallN(DisconnectNamedPipeProc.Addr(),
		uintptr(hNamedPipe), uintptr(0))
	if err != 0 {
		return false
	}
	if ret > 0 {
		return true
	}
	return false
}

func IsValidHandle(h HANDLE) bool {
	if int(h) == _INVALID_HANDLE_VALUE {
		return false
	}
	return h != 0
}
