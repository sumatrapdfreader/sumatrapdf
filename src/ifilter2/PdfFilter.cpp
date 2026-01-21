/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/FileUtil.h"

#include "FilterBase.h"
#include "RegistrySearchFilter.h"
#include "PdfFilter.h"

#include "utils/Log.h"

// Protocol constants for named pipe IFilter - must match SumatraStartup.cpp
constexpr u32 kIFilterRequestMagic = 0x49464C54;  // "IFLT" - IFilter Request
constexpr u32 kIFilterResponseMagic = 0x49464C52; // "IFLR" - IFilter Response
constexpr u32 kIFilterProtocolVersion = 1;
constexpr DWORD kPipeTimeoutMs = 30000; // 30 second timeout

void FreeExtractedData(IFilterExtractedData* data) {
    if (!data) {
        return;
    }
    str::Free(data->author);
    str::Free(data->title);
    str::Free(data->date);
    if (data->pageTexts) {
        for (int i = 0; i < data->pageCount; i++) {
            str::Free(data->pageTexts[i]);
        }
        free(data->pageTexts);
    }
    delete data;
}

// Generate a unique pipe name using a GUID
static char* GenerateUniquePipeName() {
    GUID guid;
    if (FAILED(CoCreateGuid(&guid))) {
        return nullptr;
    }
    return str::Format("\\\\.\\pipe\\LOCAL\\SumatraPDF-IFilter-%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X",
                       guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                       guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
}

// Create the named pipe (DLL is the server)
static HANDLE CreateIFilterPipe(const char* pipeName) {
    HANDLE hPipe = CreateNamedPipeA(pipeName, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                    PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                    1,         // maxInstances - only one client
                                    64 * 1024, // outBufferSize
                                    64 * 1024, // inBufferSize
                                    kPipeTimeoutMs,
                                    nullptr); // security attributes
    if (hPipe == INVALID_HANDLE_VALUE) {
        logf("CreateIFilterPipe: CreateNamedPipeA failed with error %d\n", (int)GetLastError());
    }
    return hPipe;
}

// Launch SumatraPDF.exe with -ifilter-pipe argument
static HANDLE LaunchSumatraForIFilter(const char* pipeName) {
    // Find SumatraPDF.exe in the same directory as the DLL
    TempStr exePath = GetPathInExeDirTemp("SumatraPDF.exe");
    if (!file::Exists(exePath)) {
        logf("LaunchSumatraForIFilter: SumatraPDF.exe not found at '%s'\n", exePath);
        return nullptr;
    }

    TempStr cmdLine = str::FormatTemp("\"%s\" -ifilter-pipe %s", exePath, pipeName);
    logf("LaunchSumatraForIFilter: launching '%s'\n", cmdLine);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessA(exePath, cmdLine, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok) {
        logf("LaunchSumatraForIFilter: CreateProcessA failed with error %d\n", (int)GetLastError());
        return nullptr;
    }

    CloseHandle(pi.hThread);
    return pi.hProcess;
}

// Send IFilter request through the pipe
static bool SendIFilterRequest(HANDLE hPipe, IFilterFileType fileType, const ByteSlice& data) {
    DWORD bytesWritten = 0;

    // Write header: magic(4) + version(4) + fileType(4) + dataSize(4) = 16 bytes
    u32 magic = kIFilterRequestMagic;
    u32 version = kIFilterProtocolVersion;
    u32 ft = (u32)fileType;
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

// Helper to read a length-prefixed string from pipe
static char* ReadLenPrefixedString(HANDLE hPipe) {
    DWORD bytesRead = 0;
    u32 len = 0;
    if (!ReadFile(hPipe, &len, 4, &bytesRead, nullptr) || bytesRead != 4) {
        return nullptr;
    }
    if (len == 0) {
        return nullptr;
    }
    char* s = AllocArray<char>(len + 1);
    if (!s) {
        return nullptr;
    }
    DWORD totalRead = 0;
    while (totalRead < len) {
        DWORD toRead = len - totalRead;
        if (!ReadFile(hPipe, s + totalRead, toRead, &bytesRead, nullptr) || bytesRead == 0) {
            free(s);
            return nullptr;
        }
        totalRead += bytesRead;
    }
    s[len] = '\0';
    return s;
}

// Helper to read a length-prefixed UTF-16 string from pipe
static WCHAR* ReadLenPrefixedWString(HANDLE hPipe) {
    DWORD bytesRead = 0;
    u32 len = 0; // length in bytes
    if (!ReadFile(hPipe, &len, 4, &bytesRead, nullptr) || bytesRead != 4) {
        return nullptr;
    }
    if (len == 0) {
        return nullptr;
    }
    size_t charCount = len / sizeof(WCHAR);
    WCHAR* ws = AllocArray<WCHAR>(charCount + 1);
    if (!ws) {
        return nullptr;
    }
    DWORD totalRead = 0;
    while (totalRead < len) {
        DWORD toRead = len - totalRead;
        if (!ReadFile(hPipe, (u8*)ws + totalRead, toRead, &bytesRead, nullptr) || bytesRead == 0) {
            free(ws);
            return nullptr;
        }
        totalRead += bytesRead;
    }
    ws[charCount] = L'\0';
    return ws;
}

// Receive IFilter response from the pipe
static IFilterExtractedData* ReceiveIFilterResponse(HANDLE hPipe) {
    DWORD bytesRead = 0;

    // Read response header: magic(4) + status(4) + pageCount(4) = 12 bytes
    u32 magic = 0, status = 0, pageCount = 0;

    if (!ReadFile(hPipe, &magic, 4, &bytesRead, nullptr) || bytesRead != 4 || magic != kIFilterResponseMagic) {
        logf("ReceiveIFilterResponse: invalid magic 0x%08x\n", magic);
        return nullptr;
    }
    if (!ReadFile(hPipe, &status, 4, &bytesRead, nullptr) || bytesRead != 4) {
        logf("ReceiveIFilterResponse: failed to read status\n");
        return nullptr;
    }
    if (status != 0) {
        logf("ReceiveIFilterResponse: server returned error status %d\n", status);
        return nullptr;
    }
    if (!ReadFile(hPipe, &pageCount, 4, &bytesRead, nullptr) || bytesRead != 4) {
        logf("ReceiveIFilterResponse: failed to read pageCount\n");
        return nullptr;
    }

    logf("ReceiveIFilterResponse: status=%d, pageCount=%d\n", status, pageCount);

    IFilterExtractedData* data = new IFilterExtractedData();
    data->pageCount = pageCount;

    // Read metadata
    data->author = ReadLenPrefixedString(hPipe);
    data->title = ReadLenPrefixedString(hPipe);
    data->date = ReadLenPrefixedString(hPipe);

    logf("ReceiveIFilterResponse: author='%s', title='%s'\n", data->author ? data->author : "(null)",
         data->title ? data->title : "(null)");

    // Read page texts
    if (pageCount > 0) {
        data->pageTexts = AllocArray<WCHAR*>(pageCount);
        if (data->pageTexts) {
            for (u32 i = 0; i < pageCount; i++) {
                data->pageTexts[i] = ReadLenPrefixedWString(hPipe);
            }
        }
    }

    return data;
}

// Extract data via pipe communication with SumatraPDF.exe
IFilterExtractedData* ExtractDataViaPipe(IFilterFileType fileType, const ByteSlice& fileData) {
    logf("ExtractDataViaPipe: fileType=%d, dataSize=%d\n", (int)fileType, (int)fileData.size());

    // Generate unique pipe name
    char* pipeName = GenerateUniquePipeName();
    if (!pipeName) {
        logf("ExtractDataViaPipe: failed to generate pipe name\n");
        return nullptr;
    }

    logf("ExtractDataViaPipe: pipe name '%s'\n", pipeName);

    // Create named pipe (we are the server)
    HANDLE hPipe = CreateIFilterPipe(pipeName);
    if (hPipe == INVALID_HANDLE_VALUE) {
        logf("ExtractDataViaPipe: failed to create pipe\n");
        str::Free(pipeName);
        return nullptr;
    }

    // Launch SumatraPDF.exe
    HANDLE hProcess = LaunchSumatraForIFilter(pipeName);
    if (!hProcess) {
        logf("ExtractDataViaPipe: failed to launch SumatraPDF\n");
        CloseHandle(hPipe);
        str::Free(pipeName);
        return nullptr;
    }

    str::Free(pipeName);

    IFilterExtractedData* result = nullptr;

    // Wait for client to connect (with timeout)
    OVERLAPPED ov{};
    ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent) {
        logf("ExtractDataViaPipe: failed to create event\n");
        goto cleanup;
    }

    if (!ConnectNamedPipe(hPipe, &ov)) {
        DWORD err = GetLastError();
        if (err == ERROR_PIPE_CONNECTED) {
            // Client already connected, that's fine
        } else if (err == ERROR_IO_PENDING) {
            // Wait for connection with timeout
            DWORD waitResult = WaitForSingleObject(ov.hEvent, kPipeTimeoutMs);
            if (waitResult != WAIT_OBJECT_0) {
                logf("ExtractDataViaPipe: pipe connection timed out or failed\n");
                CloseHandle(ov.hEvent);
                goto cleanup;
            }
        } else {
            logf("ExtractDataViaPipe: ConnectNamedPipe failed with error %d\n", (int)err);
            CloseHandle(ov.hEvent);
            goto cleanup;
        }
    }

    CloseHandle(ov.hEvent);

    logf("ExtractDataViaPipe: client connected\n");

    // Send request
    if (!SendIFilterRequest(hPipe, fileType, fileData)) {
        logf("ExtractDataViaPipe: failed to send request\n");
        goto cleanup;
    }

    logf("ExtractDataViaPipe: request sent\n");

    // Receive response
    result = ReceiveIFilterResponse(hPipe);
    if (result) {
        logf("ExtractDataViaPipe: received data\n");
    } else {
        logf("ExtractDataViaPipe: failed to receive response\n");
    }

cleanup:
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);

    // Terminate the process if still running
    TerminateProcess(hProcess, 0);
    CloseHandle(hProcess);

    return result;
}

VOID PdfFilter::CleanUp() {
    logf("PdfFilter::Cleanup()\n");
    if (m_extractedData) {
        FreeExtractedData(m_extractedData);
        m_extractedData = nullptr;
    }
    m_state = PdfFilterState::End;
}

HRESULT PdfFilter::OnInit() {
    logf("PdfFilter::OnInit()\n");
    CleanUp();

    // Load content of PDF document into a seekable stream
    HRESULT res;
    ByteSlice data = GetDataFromStream(m_pStream, &res);
    if (data.empty()) {
        return res;
    }

    // Extract data via pipe communication with SumatraPDF.exe
    m_extractedData = ExtractDataViaPipe(IFilterFileType::PDF, data);
    data.Free();

    if (!m_extractedData) {
        return E_FAIL;
    }

    m_state = PdfFilterState::Start;
    m_iPageNo = 0;
    return S_OK;
}

// copied from SumatraProperties.cpp
static bool PdfDateParse(const char* pdfDate, SYSTEMTIME* timeOut) {
    ZeroMemory(timeOut, sizeof(SYSTEMTIME));
    // "D:" at the beginning is optional
    if (str::StartsWith(pdfDate, "D:")) {
        pdfDate += 2;
    }
    return str::Parse(pdfDate,
                      "%4d%2d%2d"
                      "%2d%2d%2d",
                      &timeOut->wYear, &timeOut->wMonth, &timeOut->wDay, &timeOut->wHour, &timeOut->wMinute,
                      &timeOut->wSecond) != nullptr;
}

// Start, Author, Title, Date, Content, End

static const char* PdfFilterStateToStr(PdfFilterState state) {
    const char* res = seqstrings::IdxToStr(kPdfFilterStateStrs, (int)state);
    return res ? res : "uknown";
}

HRESULT PdfFilter::GetNextChunkValue(ChunkValue& chunkValue) {
    const char* stateStr = PdfFilterStateToStr(m_state);
    logf("PdfFilter::GetNextChunkValue(), state: %s (%d)\n", stateStr, (int)m_state);

    if (!m_extractedData) {
        return FILTER_E_END_OF_CHUNKS;
    }

    WCHAR* ws = nullptr;
    switch (m_state) {
        case PdfFilterState::Start:
            m_state = PdfFilterState::Author;
            chunkValue.SetTextValue(PKEY_PerceivedType, L"document");
            return S_OK;

        case PdfFilterState::Author:
            m_state = PdfFilterState::Title;
            if (!str::IsEmpty(m_extractedData->author)) {
                ws = ToWStr(m_extractedData->author);
                chunkValue.SetTextValue(PKEY_Author, ws);
                return S_OK;
            }

            [[fallthrough]];

        case PdfFilterState::Title:
            m_state = PdfFilterState::Date;
            if (!str::IsEmpty(m_extractedData->title)) {
                ws = ToWStr(m_extractedData->title);
                chunkValue.SetTextValue(PKEY_Title, ws);
                return S_OK;
            }

            [[fallthrough]];

        case PdfFilterState::Date:
            m_state = PdfFilterState::Content;
            if (!str::IsEmpty(m_extractedData->date)) {
                SYSTEMTIME systime;
                FILETIME filetime;
                if (PdfDateParse(m_extractedData->date, &systime) && SystemTimeToFileTime(&systime, &filetime)) {
                    chunkValue.SetFileTimeValue(PKEY_ItemDate, filetime);
                    return S_OK;
                }
            }

            [[fallthrough]];

        case PdfFilterState::Content:
            while (++m_iPageNo <= m_extractedData->pageCount) {
                WCHAR* pageText = m_extractedData->pageTexts ? m_extractedData->pageTexts[m_iPageNo - 1] : nullptr;
                if (str::IsEmpty(pageText)) {
                    continue;
                }
                chunkValue.SetTextValue(PKEY_Search_Contents, pageText, CHUNK_TEXT);
                return S_OK;
            }
            m_state = PdfFilterState::End;

            [[fallthrough]];

        case PdfFilterState::End:
        default:
            return FILTER_E_END_OF_CHUNKS;
    }
}
