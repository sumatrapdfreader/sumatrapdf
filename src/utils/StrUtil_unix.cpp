
#include "BaseUtil.h"

namespace str {

namespace conv {

// tries to convert a string in unknown encoding to utf8, as best
// as it cans
// As an optimization, can return src if the string already is
// valid utf8. Otherwise returns a copy of the string and the
// caller has to free() it
MaybeOwnedData UnknownToUtf8(const char* s, size_t len) {
    if (0 == len) {
        len = str::Len(s);
    }

    if (len < 3) {
        return MaybeOwnedData((char*)s, len, false);
    }

    if (str::StartsWith(s, UTF8_BOM)) {
        return MaybeOwnedData((char*)s, len, false);
    }

    // TODO: UTF16BE_BOM

#if 0
    if (str::StartsWith(s, UTF16_BOM)) {
        s += 2;
        int cchLen = (int)((len - 2) / 2);
        char *str = str::conv::ToUtf8((const WCHAR*)s, cchLen);
        return MaybeOwnedData(str, 0, true);
    }
#endif

    // if s is valid utf8, leave it alone
    const u8* tmp = (const u8*)s;
    if (isLegalUTF8String(&tmp, tmp + len)) {
        return MaybeOwnedData((char*)s, 0, true);
    }
    CrashIf(true);
        return MaybeOwnedData((char*)s, 0, true);
}

}
}
