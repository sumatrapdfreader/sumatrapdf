#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/FileUtil.h"
#include "PdfToImage.h"
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>

void ConvertPdfToImages(const WCHAR* pdfPath) {
    // // Get output format
    // int format = MessageBox(NULL, L"Choose output format:\nYes for PNG, No for JPEG", L"Output Format", MB_YESNO);
    // const WCHAR* outputFormat = (format == IDYES) ? L"png" : L"jpeg";

    // // Extract directory from pdfPath
    // WCHAR dirPath[MAX_PATH];
    // wcscpy_s(dirPath, MAX_PATH, pdfPath);
    // PathRemoveFileSpecW(dirPath);

    const WCHAR* thepath = L"C:\\Users\\Sainath\\Desktop\\Ser 517 pdf project\\sumatrapdf\\src\\Lab2.pdf";

    // Get output format
    int format = MessageBox(NULL, L"Choose output format:\nYes for PNG, No for JPEG", L"Output Format", MB_YESNO);
    const WCHAR* outputFormat = (format == IDYES) ? L"png" : L"jpeg";

    // Extract directory from pdfPath
    WCHAR dirPath[MAX_PATH];
    wcscpy_s(dirPath, MAX_PATH, thepath);  // Use thepath instead of pdfPath
    PathRemoveFileSpecW(dirPath);

    // Construct command
    WCHAR command[MAX_PATH * 2];
    swprintf_s(command, _countof(command),
        L"mupdf-1.24.0-windows>mutool.exe draw -o page-.png -r 300 Lab2.pdf");

    // Execute command
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION pi;
    if (CreateProcess(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        MessageBox(NULL, L"Conversion complete. Images saved in the same folder as the PDF.", L"Success", MB_OK);
    } else {
        DWORD error = GetLastError();
        WCHAR errorMsg[256];
        swprintf_s(errorMsg, _countof(errorMsg), L"Failed to start conversion process. Error code: %d", error);
        MessageBox(NULL, errorMsg, L"Error", MB_OK | MB_ICONERROR);
    }
}