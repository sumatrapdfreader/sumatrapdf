#include "libctiny.h"
#include <windows.h>

extern "C" {

#ifdef _WIN64
#define kDefaultSecurityCookie 0x2B992DDFA23249D6
#else  /* _WIN64 */
#define kDefaultSecurityCookie 0xBB40E64E
#endif  /* _WIN64 */

DWORD_PTR __security_cookie = kDefaultSecurityCookie;

void __fastcall __security_check_cookie(DWORD_PTR) {
    return;

    /* Immediately return if the local cookie is OK. */
    // if (cookie == __security_cookie)
    //    return;

    /* Report the failure */
    // report_failure();
}

};  // extern "C"
