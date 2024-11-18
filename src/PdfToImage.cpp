#include <windows.h>
#include <string>
#include <sstream>
#include <filesystem>
#include <commdlg.h> // For OPENFILENAME

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

HWND hwndResolutionField, hwndStartPageField, hwndEndPageField; // Handles for the text fields
const WCHAR* g_pdfPath;                                         // Global variable to hold pdfPath

// Helper function to convert std::wstring to std::string
std::string ConvertWideToNarrow(const std::wstring& wideStr) {
    return std::string(wideStr.begin(), wideStr.end());
}

// Helper function to extract the directory from a file path
std::wstring GetDirectoryPath(const std::wstring& filePath) {
    return std::filesystem::path(filePath).parent_path().wstring();
}

// Helper function to get the path to mutool.exe in the same directory as the executable
std::wstring GetMutoolPath() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);
    std::filesystem::path exeDirectory = std::filesystem::path(exePath).parent_path();

    // Check if mutool.exe is in the same directory as the executable
    std::filesystem::path mutoolPath = exeDirectory / L"mutool.exe";
    if (std::filesystem::exists(mutoolPath)) {
        return mutoolPath.wstring();
    }

    // If not found, prompt the user to locate mutool.exe
    MessageBox(NULL, L"mutool.exe not found. Please locate mutool.exe.", L"Error", MB_ICONERROR);
    OPENFILENAME ofn = {0};
    wchar_t szFile[MAX_PATH] = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"Executable Files\0*.exe\0All Files\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Select mutool.exe";
    ofn.Flags = OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        std::filesystem::path selectedPath = szFile;
        if (selectedPath.filename().wstring() == L"mutool.exe") {
            return selectedPath.wstring();
        } else {
            MessageBox(NULL, L"The selected file is not mutool.exe.", L"Error", MB_ICONERROR);
        }
    }
    return L"";
}

// GUI function to show the conversion window with input fields and button
void ShowConversionGUI(const WCHAR* pdfPath) {
    g_pdfPath = pdfPath;

    const wchar_t CLASS_NAME[] = L"ConversionWindowClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"PDF Conversion", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 400,
                               300, NULL, NULL, wc.hInstance, NULL);
    if (hwnd == NULL) {
        return;
    }
    ShowWindow(hwnd, SW_SHOW);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

// Window Procedure to handle messages for the GUI
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hwndButton;

    switch (uMsg) {
        case WM_CREATE:
            CreateWindow(L"STATIC", L"Resolution:", WS_CHILD | WS_VISIBLE, 50, 30, 100, 20, hwnd, NULL,
                         (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
            CreateWindow(L"STATIC", L"Start Page:", WS_CHILD | WS_VISIBLE, 50, 70, 100, 20, hwnd, NULL,
                         (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
            CreateWindow(L"STATIC", L"End Page:", WS_CHILD | WS_VISIBLE, 50, 110, 100, 20, hwnd, NULL,
                         (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

            hwndResolutionField =
                CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 150, 30, 200, 20, hwnd,
                             NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
            hwndStartPageField =
                CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 150, 70, 200, 20, hwnd,
                             NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
            hwndEndPageField =
                CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 150, 110, 200, 20, hwnd,
                             NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

            hwndButton =
                CreateWindow(L"BUTTON", L"Convert", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 150, 150,
                             100, 30, hwnd, (HMENU)1, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == 1) {
                wchar_t resolutionBuffer[256], startPageBuffer[256], endPageBuffer[256];
                GetWindowText(hwndResolutionField, resolutionBuffer, 256);
                GetWindowText(hwndStartPageField, startPageBuffer, 256);
                GetWindowText(hwndEndPageField, endPageBuffer, 256);

                std::string resolution = ConvertWideToNarrow(resolutionBuffer);
                std::string startPage = ConvertWideToNarrow(startPageBuffer);
                std::string endPage = ConvertWideToNarrow(endPageBuffer);
                std::string pdfPathStr = ConvertWideToNarrow(g_pdfPath);
                std::wstring outputDirectoryW = GetDirectoryPath(g_pdfPath);
                std::string outputDirectory = ConvertWideToNarrow(outputDirectoryW);

                std::wstring mutoolPathW = GetMutoolPath();
                if (mutoolPathW.empty()) {
                    MessageBox(hwnd, L"mutool.exe was not found.", L"Error", MB_ICONERROR);
                    return 0;
                }

                std::string mutoolPath = ConvertWideToNarrow(mutoolPathW);
                std::stringstream command;
                command << "\"" << mutoolPath << "\" convert -o \"" << outputDirectory << "\\page%d.png\" -O "
                        << "resolution=" << resolution << " \"" << pdfPathStr << "\" " << startPage << "-" << endPage;

                std::string commandString = command.str();
                MessageBoxA(hwnd, commandString.c_str(), "Command", MB_OK);

                STARTUPINFOA si = {sizeof(si)};
                PROCESS_INFORMATION pi;

                if (!CreateProcessA(NULL, const_cast<char*>(commandString.c_str()), NULL, NULL, FALSE, 0, NULL, NULL,
                                    &si, &pi)) {
                    MessageBoxA(hwnd, "Failed to execute mutool.", "Error", MB_ICONERROR);
                } else {
                    WaitForSingleObject(pi.hProcess, INFINITE);
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                }

                DestroyWindow(hwnd);
                PostQuitMessage(0);
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Usage example
void ConvertPdfToImages(const WCHAR* pdfPath) {
    ShowConversionGUI(pdfPath);
}
