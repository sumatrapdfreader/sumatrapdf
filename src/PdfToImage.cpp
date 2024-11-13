// #include "utils/BaseUtil.h"
// #include "utils/WinUtil.h"
// #include "utils/FileUtil.h"
// #include "PdfToImage.h"
// #include <shellapi.h>
// #include <shlobj.h>
// #include <commdlg.h>
// #include <stdlib.h>
// #include <sstream> // Include this for std::wstringstream
// #include <string>
// #include <filesystem> // C++17 for file path handling

// #include <windows.h>
// #include <string>
// #include <sstream>
// #include <filesystem>


// LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// HWND hwndResolutionField, hwndStartPageField, hwndEndPageField; // Handles for the text fields

// // Helper function to convert std::wstring to std::string
// std::string ConvertWideToNarrow(const std::wstring& wideStr) {
//     return std::string(wideStr.begin(), wideStr.end());
// }

// // Helper function to extract the directory from a file path
// std::wstring GetDirectoryPath(const std::wstring& filePath) {
//     return std::filesystem::path(filePath).parent_path().wstring();
// }


// // GUI function to show the conversion window with a button
// void ShowConversionGUI() {
//     const wchar_t CLASS_NAME[] = L"ConversionWindowClass"; // Use wide string

//     // Define and register a window class
//     WNDCLASS wc = {};
//     wc.lpfnWndProc = WindowProc;
//     wc.hInstance = GetModuleHandle(NULL);
//     wc.lpszClassName = CLASS_NAME;

//     RegisterClass(&wc);

//     // Create the GUI window
//     HWND hwnd = CreateWindowEx(
//         0,
//         CLASS_NAME,
//         L"PDF Conversion",
//         WS_OVERLAPPEDWINDOW,
//         CW_USEDEFAULT, CW_USEDEFAULT, 400, 250,
//         NULL,
//         NULL,
//         wc.hInstance,
//         NULL
//     );

//     if (hwnd == NULL) {
//         return;
//     }

//     ShowWindow(hwnd, SW_SHOW);

//     // Run the message loop
//     MSG msg = {};
//     while (GetMessage(&msg, NULL, 0, 0)) {
//         TranslateMessage(&msg);
//         DispatchMessage(&msg);
//     }
// }

// // Window Procedure to handle messages for the GUI
// LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
//     static HWND hwndButton;

//     switch (uMsg) {
//         case WM_CREATE:
//             // Create text fields for resolution, start page, and end page
//             hwndResolutionField = CreateWindow(
//                 L"EDIT",
//                 L"",
//                 WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
//                 50, 50, 300, 20,
//                 hwnd,
//                 NULL,
//                 (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
//                 NULL
//             );

//             hwndStartPageField = CreateWindow(
//                 L"EDIT",
//                 L"",
//                 WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
//                 50, 90, 300, 20,
//                 hwnd,
//                 NULL,
//                 (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
//                 NULL
//             );

//             hwndEndPageField = CreateWindow(
//                 L"EDIT",
//                 L"",
//                 WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
//                 50, 130, 300, 20,
//                 hwnd,
//                 NULL,
//                 (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
//                 NULL
//             );

//             // Create a button to trigger the action
//             hwndButton = CreateWindow(
//                 L"BUTTON",
//                 L"Convert",
//                 WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
//                 50, 170, 100, 30,
//                 hwnd,
//                 (HMENU)1,
//                 (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
//                 NULL
//             );
//             break;

//         case WM_COMMAND:
//             if (LOWORD(wParam) == 1) {  // Check if the button was clicked
//                 // Retrieve the text from each input field
//                 wchar_t resolutionBuffer[256];
//                 wchar_t startPageBuffer[256];
//                 wchar_t endPageBuffer[256];

//                 GetWindowText(hwndResolutionField, resolutionBuffer, 256);
//                 GetWindowText(hwndStartPageField, startPageBuffer, 256);
//                 GetWindowText(hwndEndPageField, endPageBuffer, 256);

//                 // Convert the retrieved text to strings
//                 std::wstring resolutionW(resolutionBuffer);
//                 std::wstring startPageW(startPageBuffer);
//                 std::wstring endPageW(endPageBuffer);

//                 // Convert to narrow strings (std::string)
//                 std::string resolution = ConvertWideToNarrow(resolutionW);
//                 std::string startPage = ConvertWideToNarrow(startPageW);
//                 std::string endPage = ConvertWideToNarrow(endPageW);

//                 // Construct the command string using the input values
//                 std::stringstream command;
//                 command << "C:\\Users\\dharani.chrinta\\Downloads\\mutool.exe convert -o output\\page%d.png -O "
//                         << "resolution=" << resolution << " \"" << "<pdf_path>"
//                         << "\" " << startPage << "-" << endPage;

//                 // Execute the command (as an example, display in a message box)
//                 std::string commandString = command.str();
//                 MessageBoxA(hwnd, commandString.c_str(), "Command", MB_OK);

//                 // Execute the command
//                 system(commandString.c_str());
//             }
//             break;

//         case WM_DESTROY:
//             PostQuitMessage(0);
//             return 0;
//     }
//     return DefWindowProc(hwnd, uMsg, wParam, lParam);
// }

// void ConvertPdfToImages(const WCHAR* pdfPath) {

//     ShowConversionGUI();

//     // Display messages to confirm paths
//     MessageBox(NULL, pdfPath, L"Success", MB_OK);
//     MessageBox(NULL, L"Conversion complete. Images will be saved in the PDF's directory.", L"Success", MB_OK);

//     // Get the directory path where the PDF file is located
//     std::wstring outputDirectory = GetDirectoryPath(pdfPath);

//     // Construct the output image path
//     std::wstringstream command;
//     command << L"C:\\Users\\dharani.chrinta\\Downloads\\mutool.exe convert -o " << outputDirectory
//             << L"\\page%d.png -O "
//                L"resolution=300 \""
//             << pdfPath << L"\" 1-1";

//     // Convert the command to a narrow string for system() call
//     std::string commandStr = ConvertWideToNarrow(command.str());

//     // Execute the command
//     system(commandStr.c_str());
// }

#include <windows.h>
#include <string>
#include <sstream>
#include <filesystem>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

HWND hwndResolutionField, hwndStartPageField, hwndEndPageField; // Handles for the text fields
const WCHAR* g_pdfPath; // Global variable to hold pdfPath

// Helper function to convert std::wstring to std::string
std::string ConvertWideToNarrow(const std::wstring& wideStr) {
    return std::string(wideStr.begin(), wideStr.end());
}

// Helper function to extract the directory from a file path
std::wstring GetDirectoryPath(const std::wstring& filePath) {
    return std::filesystem::path(filePath).parent_path().wstring();
}

// GUI function to show the conversion window with input fields and button
void ShowConversionGUI(const WCHAR* pdfPath) {
    g_pdfPath = pdfPath; // Store pdfPath in global variable

    const wchar_t CLASS_NAME[] = L"ConversionWindowClass";

    // Define and register a window class
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    // Create the GUI window
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"PDF Conversion",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
        NULL,
        NULL,
        wc.hInstance,
        NULL
    );

    if (hwnd == NULL) {
        return;
    }

    ShowWindow(hwnd, SW_SHOW);

    // Run the message loop
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
            // Create labels for each input field
            CreateWindow(
                L"STATIC",
                L"Resolution:",
                WS_CHILD | WS_VISIBLE,
                50, 30, 100, 20,
                hwnd,
                NULL,
                (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
                NULL
            );

            CreateWindow(
                L"STATIC",
                L"Start Page:",
                WS_CHILD | WS_VISIBLE,
                50, 70, 100, 20,
                hwnd,
                NULL,
                (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
                NULL
            );

            CreateWindow(
                L"STATIC",
                L"End Page:",
                WS_CHILD | WS_VISIBLE,
                50, 110, 100, 20,
                hwnd,
                NULL,
                (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
                NULL
            );

            // Create text fields for resolution, start page, and end page
            hwndResolutionField = CreateWindow(
                L"EDIT",
                L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                150, 30, 200, 20,
                hwnd,
                NULL,
                (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
                NULL
            );

            hwndStartPageField = CreateWindow(
                L"EDIT",
                L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                150, 70, 200, 20,
                hwnd,
                NULL,
                (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
                NULL
            );

            hwndEndPageField = CreateWindow(
                L"EDIT",
                L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                150, 110, 200, 20,
                hwnd,
                NULL,
                (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
                NULL
            );

            // Create a button to trigger the action
            hwndButton = CreateWindow(
                L"BUTTON",
                L"Convert",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                150, 150, 100, 30,
                hwnd,
                (HMENU)1,
                (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
                NULL
            );
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == 1) {  // Check if the button was clicked
                // Retrieve the text from each input field
                wchar_t resolutionBuffer[256];
                wchar_t startPageBuffer[256];
                wchar_t endPageBuffer[256];

                GetWindowText(hwndResolutionField, resolutionBuffer, 256);
                GetWindowText(hwndStartPageField, startPageBuffer, 256);
                GetWindowText(hwndEndPageField, endPageBuffer, 256);

                // Convert the retrieved text to strings
                std::wstring resolutionW(resolutionBuffer);
                std::wstring startPageW(startPageBuffer);
                std::wstring endPageW(endPageBuffer);

                // Convert to narrow strings (std::string)
                std::string resolution = ConvertWideToNarrow(resolutionW);
                std::string startPage = ConvertWideToNarrow(startPageW);
                std::string endPage = ConvertWideToNarrow(endPageW);
                std::string pdfPathStr = ConvertWideToNarrow(g_pdfPath);
                std::wstring outputDirectoryW = GetDirectoryPath(g_pdfPath);
                std::string outputDirectory = ConvertWideToNarrow(outputDirectoryW); // Convert outputDirectory to narrow string

                // Construct the command string using the input values and pdfPath
                std::stringstream command;
                command << "C:\\Users\\dharani.chrinta\\Downloads\\mutool.exe convert -o " << outputDirectory
                        << "\\page%d.png -O "
                        << "resolution=" << resolution << " \"" << pdfPathStr
                        << "\" " << startPage << "-" << endPage;
                // Execute the command (as an example, display in a message box)
                std::string commandString = command.str();
                MessageBoxA(hwnd, commandString.c_str(), "Command", MB_OK);

                // Execute the command
                system(commandString.c_str());

                // Close the GUI window after conversion
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
