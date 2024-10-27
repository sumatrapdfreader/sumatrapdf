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
 
   
 
    // Get output format
    int format = MessageBox(NULL, L"Choose output format:\nYes for PNG, No for JPEG", L"Output Format", MB_YESNO);
    const WCHAR* outputFormat = (format == IDYES) ? L"png" : L"jpeg";
 
   
}