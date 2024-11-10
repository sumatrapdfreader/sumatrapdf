#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/FileUtil.h"
#include "PdfToImage.h"
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <stdlib.h>
#include <sstream> // Include this for std::wstringstream
#include <string>
#include <filesystem> // C++17 for file path handling

// Helper function to convert std::wstring to std::string
std::string ConvertWideToNarrow(const std::wstring& wideStr) {
    return std::string(wideStr.begin(), wideStr.end());
}

// Helper function to extract the directory from a file path
std::wstring GetDirectoryPath(const std::wstring& filePath) {
    return std::filesystem::path(filePath).parent_path().wstring();
}

void ConvertPdfToImages(const WCHAR* pdfPath) {
    // Display messages to confirm paths
    MessageBox(NULL, pdfPath, L"Success", MB_OK);
    MessageBox(NULL, L"Conversion complete. Images will be saved in the PDF's directory.", L"Success", MB_OK);

    // Get the directory path where the PDF file is located
    std::wstring outputDirectory = GetDirectoryPath(pdfPath);

    // Construct the output image path
    std::wstringstream command;
    command << L"C:\\Users\\jsani\\Desktop\\mutool.exe convert -o " << outputDirectory
            << L"\\page%d.png -O "
               L"resolution=300 \""
            << pdfPath << L"\" 1-1";

    // Convert the command to a narrow string for system() call
    std::string commandStr = ConvertWideToNarrow(command.str());

    // Execute the command
    system(commandStr.c_str());
}
