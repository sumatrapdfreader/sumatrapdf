/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"

#include "PreviewPipe.h"

#include "utils/Log.h"

// Generate a unique pipe name using a GUID
char* GenerateUniquePipeName() {
    GUID guid;
    if (FAILED(CoCreateGuid(&guid))) {
        return nullptr;
    }
    return str::Format("\\\\.\\pipe\\LOCAL\\SumatraPDF-Preview-%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X",
                       guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                       guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
}

// Create the named pipe (DLL is the server)
HANDLE CreatePreviewPipe(const char* pipeName) {
    HANDLE hPipe = CreateNamedPipeA(pipeName, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                    PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                    1,         // maxInstances - only one client
                                    64 * 1024, // outBufferSize
                                    64 * 1024, // inBufferSize
                                    kPipeTimeoutMs,
                                    nullptr); // security attributes
    if (hPipe == INVALID_HANDLE_VALUE) {
        logf("CreatePreviewPipe: CreateNamedPipeA failed with error %d\n", (int)GetLastError());
    }
    return hPipe;
}

// Launch SumatraPDF.exe with -preview-pipe argument
HANDLE LaunchSumatraForPreview(const char* pipeName) {
    // Find SumatraPDF.exe in the same directory as the DLL
    TempStr exePath = GetPathInExeDirTemp("SumatraPDF.exe");
    if (!file::Exists(exePath)) {
        logf("LaunchSumatraForPreview: SumatraPDF.exe not found at '%s'\n", exePath);
        return nullptr;
    }

    TempStr cmdLine = str::FormatTemp("\"%s\" -preview-pipe %s", exePath, pipeName);
    logf("LaunchSumatraForPreview: launching '%s'\n", cmdLine);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    // Get the directory containing the exe for the working directory
    TempStr exeDir = path::GetDirTemp(exePath);

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessA(exePath, cmdLine, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, exeDir, &si, &pi);
    if (!ok) {
        logf("LaunchSumatraForPreview: CreateProcessA failed with error %d\n", (int)GetLastError());
        return nullptr;
    }

    CloseHandle(pi.hThread);
    return pi.hProcess;
}

// Wait for client connection with timeout
bool WaitForPipeConnection(HANDLE hPipe) {
    OVERLAPPED ov{};
    ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent) {
        return false;
    }

    if (!ConnectNamedPipe(hPipe, &ov)) {
        DWORD err = GetLastError();
        if (err == ERROR_PIPE_CONNECTED) {
            // Already connected
            CloseHandle(ov.hEvent);
            return true;
        } else if (err == ERROR_IO_PENDING) {
            DWORD waitResult = WaitForSingleObject(ov.hEvent, kPipeTimeoutMs);
            CloseHandle(ov.hEvent);
            return waitResult == WAIT_OBJECT_0;
        } else {
            CloseHandle(ov.hEvent);
            return false;
        }
    }

    CloseHandle(ov.hEvent);
    return true;
}

// Send preview request through the pipe (version 1 protocol for thumbnails)
bool SendPreviewRequest(HANDLE hPipe, PreviewFileType fileType, uint cx, const ByteSlice& data) {
    DWORD bytesWritten = 0;

    // Write header: magic(4) + version(4) + fileType(4) + thumbSize(4) + dataSize(4) = 20 bytes
    u32 magic = kPreviewRequestMagic;
    u32 version = kPreviewProtocolVersion;
    u32 ft = (u32)fileType;
    u32 thumbSize = cx;
    u32 dataSize = (u32)data.size();

    if (!WriteFile(hPipe, &magic, 4, &bytesWritten, nullptr) || bytesWritten != 4) {
        return false;
    }
    if (!WriteFile(hPipe, &version, 4, &bytesWritten, nullptr) || bytesWritten != 4) {
        return false;
    }
    if (!WriteFile(hPipe, &ft, 4, &bytesWritten, nullptr) || bytesWritten != 4) {
        return false;
    }
    if (!WriteFile(hPipe, &thumbSize, 4, &bytesWritten, nullptr) || bytesWritten != 4) {
        return false;
    }
    if (!WriteFile(hPipe, &dataSize, 4, &bytesWritten, nullptr) || bytesWritten != 4) {
        return false;
    }

    // Write file data
    DWORD totalWritten = 0;
    while (totalWritten < dataSize) {
        DWORD toWrite = dataSize - totalWritten;
        if (!WriteFile(hPipe, data.data() + totalWritten, toWrite, &bytesWritten, nullptr) || bytesWritten == 0) {
            return false;
        }
        totalWritten += bytesWritten;
    }

    FlushFileBuffers(hPipe);
    return true;
}

// Receive preview response from the pipe and create HBITMAP
HBITMAP ReceivePreviewResponse(HANDLE hPipe) {
    DWORD bytesRead = 0;

    // Read response header: magic(4) + status(4) + width(4) + height(4) + dataLen(4) = 20 bytes
    u32 magic = 0, status = 0, width = 0, height = 0, bmpDataLen = 0;

    if (!ReadFile(hPipe, &magic, 4, &bytesRead, nullptr) || bytesRead != 4 || magic != kPreviewResponseMagic) {
        logf("ReceivePreviewResponse: invalid magic 0x%08x\n", magic);
        return nullptr;
    }
    if (!ReadFile(hPipe, &status, 4, &bytesRead, nullptr) || bytesRead != 4) {
        logf("ReceivePreviewResponse: failed to read status\n");
        return nullptr;
    }
    if (status != 0) {
        logf("ReceivePreviewResponse: server returned error status %d\n", status);
        return nullptr;
    }
    if (!ReadFile(hPipe, &width, 4, &bytesRead, nullptr) || bytesRead != 4) {
        logf("ReceivePreviewResponse: failed to read width\n");
        return nullptr;
    }
    if (!ReadFile(hPipe, &height, 4, &bytesRead, nullptr) || bytesRead != 4) {
        logf("ReceivePreviewResponse: failed to read height\n");
        return nullptr;
    }
    if (!ReadFile(hPipe, &bmpDataLen, 4, &bytesRead, nullptr) || bytesRead != 4) {
        logf("ReceivePreviewResponse: failed to read bmpDataLen\n");
        return nullptr;
    }

    logf("ReceivePreviewResponse: width=%d, height=%d, bmpDataLen=%d\n", width, height, bmpDataLen);

    if (bmpDataLen == 0 || bmpDataLen != width * height * 4) {
        logf("ReceivePreviewResponse: invalid bitmap data length\n");
        return nullptr;
    }

    // Read bitmap data
    u8* bmpData = AllocArray<u8>(bmpDataLen);
    if (!bmpData) {
        logf("ReceivePreviewResponse: failed to allocate %d bytes\n", bmpDataLen);
        return nullptr;
    }

    DWORD totalRead = 0;
    while (totalRead < bmpDataLen) {
        DWORD toRead = bmpDataLen - totalRead;
        if (!ReadFile(hPipe, bmpData + totalRead, toRead, &bytesRead, nullptr) || bytesRead == 0) {
            logf("ReceivePreviewResponse: failed to read bitmap data at offset %d\n", totalRead);
            free(bmpData);
            return nullptr;
        }
        totalRead += bytesRead;
    }

    // Create DIB section from the bitmap data
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = height; // positive = bottom-up DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    u8* dibData = nullptr;
    HBITMAP hBitmap = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, (void**)&dibData, nullptr, 0);
    if (!hBitmap || !dibData) {
        logf("ReceivePreviewResponse: CreateDIBSection failed\n");
        free(bmpData);
        return nullptr;
    }

    // Copy the bitmap data
    memcpy(dibData, bmpData, bmpDataLen);
    free(bmpData);

    return hBitmap;
}
