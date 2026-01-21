/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Pipe communication protocol for preview/thumbnail generation
// Used by PdfPreview2.dll (server) and SumatraPDF.exe (client)

#pragma once

// File type enum for preview pipe protocol
enum class PreviewFileType : u32 {
    PDF = 1,
    DjVu = 2,
    EPUB = 3,
    FB2 = 4,
    MOBI = 5,
    CBX = 6,
    TGA = 7
};

// Protocol constants for named pipe preview - must match SumatraStartup.cpp
constexpr u32 kPreviewRequestMagic = 0x53505657;  // "SPVW" - SumatraPDF Preview
constexpr u32 kPreviewResponseMagic = 0x53505652; // "SPVR" - SumatraPDF Preview Response
constexpr u32 kPreviewProtocolVersion = 1;        // One-shot thumbnail mode
constexpr u32 kPreviewProtocolVersion2 = 2;       // Session-based preview mode
constexpr DWORD kPipeTimeoutMs = 30000;           // 30 second timeout for pipe operations

// Commands for protocol version 2 (session-based)
enum class PreviewCmd : u32 {
    Init = 1,       // Initialize with file data, returns page count
    GetPageBox = 2, // Get page dimensions
    Render = 3,     // Render a page
    Shutdown = 255, // Close session
};

// Pipe session for version 2 protocol
class PreviewPipeSession {
  public:
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    HANDLE hProcess = nullptr;
    int pageCount = 0;

    ~PreviewPipeSession() {
        Close();
    }

    bool IsConnected() const {
        return hPipe != INVALID_HANDLE_VALUE;
    }

    void Close() {
        if (hPipe != INVALID_HANDLE_VALUE) {
            // Send shutdown command
            DWORD bytesWritten = 0;
            u32 magic = kPreviewRequestMagic;
            u32 version = kPreviewProtocolVersion2;
            u32 cmd = (u32)PreviewCmd::Shutdown;
            WriteFile(hPipe, &magic, 4, &bytesWritten, nullptr);
            WriteFile(hPipe, &version, 4, &bytesWritten, nullptr);
            WriteFile(hPipe, &cmd, 4, &bytesWritten, nullptr);
            FlushFileBuffers(hPipe);
            DisconnectNamedPipe(hPipe);
            CloseHandle(hPipe);
            hPipe = INVALID_HANDLE_VALUE;
        }
        if (hProcess) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
            hProcess = nullptr;
        }
        pageCount = 0;
    }

    // Send GetPageBox command and return page dimensions (in points at zoom 1.0)
    RectF GetPageBox(int pageNo) {
        if (!IsConnected()) {
            return RectF();
        }

        DWORD bytesWritten = 0, bytesRead = 0;

        // Send command header + pageNo
        u32 magic = kPreviewRequestMagic;
        u32 version = kPreviewProtocolVersion2;
        u32 cmd = (u32)PreviewCmd::GetPageBox;
        u32 pn = (u32)pageNo;

        WriteFile(hPipe, &magic, 4, &bytesWritten, nullptr);
        WriteFile(hPipe, &version, 4, &bytesWritten, nullptr);
        WriteFile(hPipe, &cmd, 4, &bytesWritten, nullptr);
        WriteFile(hPipe, &pn, 4, &bytesWritten, nullptr);
        FlushFileBuffers(hPipe);

        // Read response: magic(4) + status(4) + widthBits(4) + heightBits(4)
        u32 respMagic = 0, status = 0, widthBits = 0, heightBits = 0;
        if (!ReadFile(hPipe, &respMagic, 4, &bytesRead, nullptr) || bytesRead != 4 ||
            respMagic != kPreviewResponseMagic) {
            return RectF();
        }
        if (!ReadFile(hPipe, &status, 4, &bytesRead, nullptr) || bytesRead != 4 || status != 0) {
            return RectF();
        }
        if (!ReadFile(hPipe, &widthBits, 4, &bytesRead, nullptr) || bytesRead != 4) {
            return RectF();
        }
        if (!ReadFile(hPipe, &heightBits, 4, &bytesRead, nullptr) || bytesRead != 4) {
            return RectF();
        }

        float width, height;
        memcpy(&width, &widthBits, 4);
        memcpy(&height, &heightBits, 4);

        return RectF(0, 0, width, height);
    }

    // Send Render command and return bitmap (caller owns the returned HBITMAP)
    HBITMAP RenderPage(int pageNo, float zoom, int targetWidth, int targetHeight) {
        if (!IsConnected()) {
            return nullptr;
        }

        DWORD bytesWritten = 0, bytesRead = 0;

        // Send command header + render params
        u32 magic = kPreviewRequestMagic;
        u32 version = kPreviewProtocolVersion2;
        u32 cmd = (u32)PreviewCmd::Render;
        u32 pn = (u32)pageNo;
        u32 zoomBits;
        memcpy(&zoomBits, &zoom, 4);
        u32 tw = (u32)targetWidth;
        u32 th = (u32)targetHeight;

        WriteFile(hPipe, &magic, 4, &bytesWritten, nullptr);
        WriteFile(hPipe, &version, 4, &bytesWritten, nullptr);
        WriteFile(hPipe, &cmd, 4, &bytesWritten, nullptr);
        WriteFile(hPipe, &pn, 4, &bytesWritten, nullptr);
        WriteFile(hPipe, &zoomBits, 4, &bytesWritten, nullptr);
        WriteFile(hPipe, &tw, 4, &bytesWritten, nullptr);
        WriteFile(hPipe, &th, 4, &bytesWritten, nullptr);
        FlushFileBuffers(hPipe);

        // Read response header: magic(4) + status(4) + width(4) + height(4) + dataLen(4)
        u32 respMagic = 0, status = 0, width = 0, height = 0, bmpDataLen = 0;
        if (!ReadFile(hPipe, &respMagic, 4, &bytesRead, nullptr) || bytesRead != 4 ||
            respMagic != kPreviewResponseMagic) {
            return nullptr;
        }
        if (!ReadFile(hPipe, &status, 4, &bytesRead, nullptr) || bytesRead != 4 || status != 0) {
            return nullptr;
        }
        if (!ReadFile(hPipe, &width, 4, &bytesRead, nullptr) || bytesRead != 4) {
            return nullptr;
        }
        if (!ReadFile(hPipe, &height, 4, &bytesRead, nullptr) || bytesRead != 4) {
            return nullptr;
        }
        if (!ReadFile(hPipe, &bmpDataLen, 4, &bytesRead, nullptr) || bytesRead != 4) {
            return nullptr;
        }

        if (bmpDataLen == 0 || bmpDataLen != width * height * 4) {
            return nullptr;
        }

        // Read bitmap data
        u8* bmpData = AllocArray<u8>(bmpDataLen);
        if (!bmpData) {
            return nullptr;
        }

        DWORD totalRead = 0;
        while (totalRead < bmpDataLen) {
            DWORD toRead = bmpDataLen - totalRead;
            if (!ReadFile(hPipe, bmpData + totalRead, toRead, &bytesRead, nullptr) || bytesRead == 0) {
                free(bmpData);
                return nullptr;
            }
            totalRead += bytesRead;
        }

        // Create DIB section
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
            free(bmpData);
            return nullptr;
        }

        memcpy(dibData, bmpData, bmpDataLen);
        free(bmpData);

        return hBitmap;
    }
};

// Utility functions for pipe operations

// Generate a unique pipe name using a GUID
char* GenerateUniquePipeName();

// Create the named pipe (caller is the server)
HANDLE CreatePreviewPipe(const char* pipeName);

// Launch SumatraPDF.exe with -preview-pipe argument
HANDLE LaunchSumatraForPreview(const char* pipeName);

// Wait for client connection with timeout
bool WaitForPipeConnection(HANDLE hPipe);

// Send preview request through the pipe (version 1 protocol for thumbnails)
bool SendPreviewRequest(HANDLE hPipe, PreviewFileType fileType, uint cx, const ByteSlice& data);

// Receive preview response from the pipe and create HBITMAP
HBITMAP ReceivePreviewResponse(HANDLE hPipe);
