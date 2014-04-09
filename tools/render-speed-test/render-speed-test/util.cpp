#include "stdafx.h"
#include "util.h"

namespace str {

size_t Utf8ToWcharBuf(const char *s, size_t sLen, WCHAR *bufOut, size_t cchBufOutSize)
{
    //CrashIf(0 == cchBufOutSize);
    int cchConverted = MultiByteToWideChar(CP_UTF8, 0, s, (int) sLen, NULL, 0);
    if ((size_t) cchConverted >= cchBufOutSize)
        cchConverted = (int) cchBufOutSize - 1;
    MultiByteToWideChar(CP_UTF8, 0, s, (int) sLen, bufOut, cchConverted);
    bufOut[cchConverted] = '\0';
    return cchConverted;
}

char *DupN(char *s, size_t sLen) {
    auto res = (char*) malloc(sLen + 1);
    if (!res)
        return NULL;
    memcpy(res, s, sLen);
    res[sLen] = 0;
    return res;
}

}

void InitAllCommonControls()
{
    INITCOMMONCONTROLSEX cex = { 0 };
    cex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    cex.dwICC = ICC_WIN95_CLASSES | ICC_DATE_CLASSES | ICC_USEREX_CLASSES | ICC_COOL_CLASSES;
    InitCommonControlsEx(&cex);
}
