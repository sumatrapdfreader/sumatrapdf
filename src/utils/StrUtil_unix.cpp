
#include "BaseUtil.h"

namespace str {

namespace conv {

// tries to convert a string in unknown encoding to utf8, as best
// as it cans
// As an optimization, can return src if the string already is
// valid utf8. Otherwise returns a copy of the string and the
// caller has to free() it
MaybeOwnedData UnknownToUtf8(const std::string_view& str) {
    size_t n = str.size();
    const char* s = str.data();

    if (n < 3) {
        return MaybeOwnedData((char*)s, n, false);
    }

    if (str::StartsWith(s, UTF8_BOM)) {
        return MaybeOwnedData((char*)s, n, false);
    }

    // TODO: UTF16BE_BOM

#if 0
    if (str::StartsWith(s, UTF16_BOM)) {
        s += 2;
        int cchLen = (int)((n - 2) / 2);
        char *str = str::conv::ToUtf8((const WCHAR*)s, cchLen);
        return MaybeOwnedData(str, 0, true);
    }
#endif

    // if s is valid utf8, leave it alone
    const u8* tmp = (const u8*)s;
    if (isLegalUTF8String(&tmp, tmp + n)) {
        return MaybeOwnedData((char*)s, 0, false);
    }
    CrashIf(true);
    return MaybeOwnedData((char*)s, 0, false);
}

} // namespace conv
} // namespace str
