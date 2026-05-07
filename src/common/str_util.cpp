#include "common.h"

// Counters for StrFmt optimization tracking
AtomicInt gStrFmtFirstAlloc = 0;  // Formatted into available space
AtomicInt gStrFmtSecondAlloc = 0; // Needed separate allocation

void WStrCopy(wchar_t* dst, const wchar_t* src, int maxLen) {
    int i = 0;
    while (src[i] && i < maxLen - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

wchar_t ToLowerW(wchar_t c) {
    if (c >= L'A' && c <= L'Z') return c + (L'a' - L'A');
    return c;
}

int WStrFindSubstr(WStr str, WStr substr) {
    if (substr.len == 0) return -1; // Empty search - no highlight
    if (substr.len > str.len) return -1;

    for (int i = 0; i <= str.len - substr.len; i++) {
        bool match = true;
        for (int j = 0; j < substr.len; j++) {
            if (ToLowerW(str.s[i + j]) != ToLowerW(substr.s[j])) {
                match = false;
                break;
            }
        }
        if (match) return i;
    }
    return -1;
}

int WStrCmpNoCase(WStr a, WStr b) {
    int minLen = a.len < b.len ? a.len : b.len;
    for (int i = 0; i < minLen; i++) {
        wchar_t ca = ToLowerW(a.s[i]);
        wchar_t cb = ToLowerW(b.s[i]);
        if (ca != cb) return ca - cb;
    }
    return a.len - b.len;
}

// make zero-terminated copy of a string
Str StrDupTemp(Str s) {
    if (IsEmpty(s)) {
        return s;
    }
    int n = len(s) + 1;
    char* buf = (char*)AllocTemp(n);
    memcpy(buf, s.s, s.len);
    buf[s.len] = 0;
    return Str(buf, s.len);
}

int StrLastIndexOfChar(Str s, char c) {
    for (int i = s.len - 1; i >= 0; i--) {
        if (s.s[i] == c) return i;
    }
    return -1;
}

static wchar_t emptyWideStr[1] = {0};

#if 0
// Convert UTF-8 to UTF-16 (wide string), allocate with gTempAllocator
WStr ToWStrTemp(const char* utf8) {
    if (!utf8 || !utf8[0]) {
        return WStr(&emptyWideStr[0], 0);
    }
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    wchar_t* wide = (wchar_t*)AllocTemp(len * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, len);
    return WStr(wide, len - 1); // Exclude null terminator from length
}
#endif

// Convert UTF-16 to UTF-8
Str ToUtf8(Arena* arena, WStr wide) {
    if (!wide.s || wide.len == 0) {
        return Str();
    }
    // Use explicit length instead of -1 (null-terminated)
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.s, wide.len, nullptr, 0, nullptr, nullptr);
    char* utf8 = (char*)Alloc(arena, len + 1);
    WideCharToMultiByte(CP_UTF8, 0, wide.s, wide.len, utf8, len, nullptr, nullptr);
    utf8[len] = 0;
    return Str(utf8, len);
}

Str ToUtf8Temp(WStr wide) {
    return ToUtf8(GetTempArena(), wide);
}

// Case-insensitive character for ASCII subset
static char ToLowerAscii(char c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

// Convert Str to wide string (optimized - uses known length)
WStr ToWStrTemp(Str s) {
    if (!s.s || s.len == 0) {
        return WStr(&emptyWideStr[0], 0);
    }
    // Use explicit length instead of -1 (null-terminated)
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, s.s, s.len, nullptr, 0);
    wchar_t* wide = (wchar_t*)AllocTemp((wideLen + 1) * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, s.s, s.len, wide, wideLen);
    wide[wideLen] = 0;
    return WStr(wide, wideLen);
}

// Duplicate string with known length (internal helper)
static Str StrDupInternal(Arena* arena, const char* src, int len) {
    if (!src || len <= 0) return Str();
    char* dst = (char*)Alloc(arena, len + 1);
    for (int i = 0; i < len; i++) {
        dst[i] = src[i];
    }
    dst[len] = 0;
    return Str(dst, len);
}

// Duplicate Str
Str StrDup(Arena* arena, Str s) {
    return StrDupInternal(arena, s.s, s.len);
}

// Str equality - compare characters using pointers
__declspec(noinline) bool StrEqRest(const char* a, const char* b, int len) {
    const char* end = a + len;
    while (a < end) {
        if (*a++ != *b++) return false;
    }
    return true;
}

// we hope that a fast path of comparing lengths will be inlined
// and StrEqRest() will not
bool StrEq(Str a, Str b) {
    if (a.len != b.len) return false;
    return StrEqRest(a.s, b.s, a.len);
}

__declspec(noinline) bool WStrEqRest(const wchar_t* a, const wchar_t* b, int len) {
    const wchar_t* end = a + len;
    while (a < end) {
        if (*a++ != *b++) return false;
    }
    return true;
}

// we hope that a fast path of comparing lengths will be inlined
// and WStrEqRest() will not
bool WStrEq(WStr a, WStr b) {
    if (a.len != b.len) return false;
    return WStrEqRest(a.s, b.s, a.len);
}

// Case-insensitive substring search for Str (ASCII case folding)
bool StrContains(Str str, Str substr) {
    if (substr.len == 0) return true; // Empty search matches all
    if (substr.len > str.len) return false;

    for (int i = 0; i <= str.len - substr.len; i++) {
        bool match = true;
        for (int j = 0; j < substr.len; j++) {
            if (ToLowerAscii(str.s[i + j]) != ToLowerAscii(substr.s[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

// Case-insensitive prefix check (ASCII case folding)
bool StrHasPrefixNoCase(Str s, Str prefix) {
    if (prefix.len > s.len) return false;
    for (int i = 0; i < prefix.len; i++) {
        if (ToLowerAscii(s.s[i]) != ToLowerAscii(prefix.s[i])) {
            return false;
        }
    }
    return true;
}

bool StrHasPrefix(Str s, Str prefix) {
    if (prefix.len > s.len) return false;
    char* sc = s.s;
    char* se = sc + prefix.len;
    char* pc = prefix.s;
    while (sc < se) {
        if (*sc++ != *pc++) return false;
    }
    return true;
}

bool StrHasSuffix(Str s, Str suffix) {
    if (suffix.len > s.len) return false;
    char* sc = s.s + s.len - suffix.len;
    char* se = s.s + s.len;
    char* pc = suffix.s;
    while (sc < se) {
        if (*sc++ != *pc++) return false;
    }
    return true;
}

Str StrTrimSuffix(Str s, Str suffix) {
    if (StrHasSuffix(s, suffix)) {
        return Str(s.s, s.len - suffix.len);
    }
    return s;
}

bool IsWhiteSpace(char c) {
    switch (c) {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            return true;
        default:
            return false;
    }
}

Str StrTrimSuffixWhitespace(Str s) {
    while (s.len > 0 && IsWhiteSpace(s.s[s.len - 1])) {
        s.len--;
        s.s[s.len] = 0;
    }
    return s;
}

// Format file size with comma separators, returns Str
Str FormatFileSize(Arena* arena, u64 size) {
    char buf[32];

    if (size == 0) {
        return StrDup(arena, StrL("0"));
    }

    // Convert to string (reversed)
    char temp[32];
    int i = 0;
    while (size > 0 && i < 31) {
        temp[i++] = '0' + (size % 10);
        size /= 10;
    }
    int numDigits = i;

    // Calculate position of first comma (from left)
    int firstCommaAfter = numDigits % 3;
    if (firstCommaAfter == 0) firstCommaAfter = 3;

    // Reverse into buf with comma separators
    int j = 0;
    int digitPos = 0;
    while (i > 0 && j < 31) {
        buf[j++] = temp[--i];
        digitPos++;
        if (digitPos == firstCommaAfter || (digitPos > firstCommaAfter && (digitPos - firstCommaAfter) % 3 == 0)) {
            if (i > 0 && j < 31) {
                buf[j++] = ',';
            }
        }
    }

    return StrDup(arena, Str(buf, j));
}

// Format file size with comma separators directly into wide string buffer
void FormatFileSizeToWstrBuf(u64 size, WStr buf) {
    if (buf.len < 1) return;

    if (size == 0) {
        buf.s[0] = L'0';
        buf.s[1] = 0;
        return;
    }

    // Convert to string (reversed)
    wchar_t temp[32];
    int i = 0;
    while (size > 0 && i < 31) {
        temp[i++] = L'0' + (size % 10);
        size /= 10;
    }
    int numDigits = i;

    // Calculate position of first comma (from left)
    int firstCommaAfter = numDigits % 3;
    if (firstCommaAfter == 0) firstCommaAfter = 3;

    // Reverse into buf with comma separators
    int j = 0;
    int digitPos = 0;
    int maxLen = buf.len - 1; // Leave room for null terminator
    while (i > 0 && j < maxLen) {
        buf.s[j++] = temp[--i];
        digitPos++;
        if (digitPos == firstCommaAfter || (digitPos > firstCommaAfter && (digitPos - firstCommaAfter) % 3 == 0)) {
            if (i > 0 && j < maxLen) {
                buf.s[j++] = L',';
            }
        }
    }
    buf.s[j] = 0;
}

// Format size in human readable form (e.g., "1.23 GB", "456 KB")
// Returns length written (excluding null terminator)
int FormatSizeHumanIntoBuf(u64 size, Str buf) {
    if (buf.len < 2) return 0;

    const u64 GB = 1024ULL * 1024 * 1024;
    const u64 MB = 1024ULL * 1024;
    const u64 KB = 1024ULL;

    const char* suffix;
    u64 divisor;

    if (size >= GB) {
        suffix = " GB";
        divisor = GB;
    } else if (size >= MB) {
        suffix = " MB";
        divisor = MB;
    } else if (size >= KB) {
        suffix = " KB";
        divisor = KB;
    } else {
        // Bytes - just format as integer
        int len = snprintf(buf.s, buf.len, "%llu B", size);
        return len < buf.len ? len : buf.len - 1;
    }

    // Calculate with 2 decimal precision
    u64 whole = size / divisor;
    u64 remainder = size % divisor;
    int frac = (int)((remainder * 100) / divisor);

    int len;
    if (frac == 0) {
        len = snprintf(buf.s, buf.len, "%llu%s", whole, suffix);
    } else if (frac % 10 == 0) {
        len = snprintf(buf.s, buf.len, "%llu.%d%s", whole, frac / 10, suffix);
    } else {
        len = snprintf(buf.s, buf.len, "%llu.%02d%s", whole, frac, suffix);
    }
    return len < buf.len ? len : buf.len - 1;
}

// Wrapper that formats into wide string buffer
void FormatSizeHumanIntoWBuf(u64 size, WStr wbuf) {
    char temp[32];
    int len = FormatSizeHumanIntoBuf(size, Str(temp, 32));

    // Copy to wide buffer
    int maxLen = wbuf.len - 1;
    int i = 0;
    while (i < len && i < maxLen) {
        wbuf.s[i] = (wchar_t)temp[i];
        i++;
    }
    wbuf.s[i] = 0;
}

// Join path components, returns Str allocated with gTempAllocator
Str PathJoinTemp(Str dir, Str name) {
    // Handle ".." - go up one directory
    if (StrEq(name, StrL(".."))) {
        char* result = (char*)AllocTemp(dir.len + 1);
        for (int i = 0; i < dir.len; i++) {
            result[i] = dir.s[i];
        }
        result[dir.len] = 0;

        // Find last backslash
        int lastSlash = -1;
        for (int i = 0; i < dir.len; i++) {
            if (result[i] == '\\') lastSlash = i;
        }
        // Don't go above root (e.g., "C:\")
        int newLen = dir.len;
        if (lastSlash > 2) {
            result[lastSlash] = 0;
            newLen = lastSlash;
        } else if (lastSlash == 2) {
            // Going up from "C:\something" to "C:\"
            result[3] = 0;
            newLen = 3;
        }
        return Str(result, newLen);
    }

    // Strip trailing "/" from name if present
    int nameLen = name.len;
    if (nameLen > 0 && name.s[nameLen - 1] == '/') {
        nameLen--;
    }

    int needsSlash = (dir.len > 0 && dir.s[dir.len - 1] != '\\') ? 1 : 0;
    int totalLen = dir.len + needsSlash + nameLen;

    char* result = (char*)AllocTemp(totalLen + 1);
    int pos = 0;

    // Copy dir
    for (int i = 0; i < dir.len; i++) {
        result[pos++] = dir.s[i];
    }

    // Add backslash if needed
    if (needsSlash) {
        result[pos++] = '\\';
    }

    // Copy name (without trailing /)
    for (int i = 0; i < nameLen; i++) {
        result[pos++] = name.s[i];
    }

    result[pos] = 0;
    return Str(result, pos);
}

// Copy UTF-8 string with max bytes
void StrCopyUtf8(char* dst, const char* src, int maxBytes) {
    int i = 0;
    while (src[i] && i < maxBytes - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

// Format string with allocator
Str StrFmt(Arena* arena, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // Try formatting into available space first (avoids double vsnprintf)
    int availSize = 0;
    char* availBuf = arena ? (char*)arena->GetAvailableSpace(&availSize) : nullptr;

    if (availBuf && availSize > 0) {
        va_list args2;
        va_copy(args2, args);
        int len = vsnprintf(availBuf, availSize, fmt, args2);
        va_end(args2);

        if (len >= 0 && len < availSize) {
            // Fits in available space - commit the allocation
            char* buf = (char*)arena->CommitReserved(availBuf, len + 1);
            AtomicIntInc(&gStrFmtFirstAlloc);
            va_end(args);
            return Str(buf ? buf : availBuf, len);
        }
        // Doesn't fit - fall through to normal allocation with known length
        if (len >= 0) {
            char* buf = (char*)Alloc(arena, len + 1);
            AtomicIntInc(&gStrFmtSecondAlloc);
            vsnprintf(buf, len + 1, fmt, args);
            va_end(args);
            return Str(buf, len);
        }
    }

    // Fallback: determine required size first
    va_list args2;
    va_copy(args2, args);
    int len = vsnprintf(nullptr, 0, fmt, args2);
    va_end(args2);

    if (len < 0) {
        va_end(args);
        return Str();
    }

    // Allocate and format
    AtomicIntInc(&gStrFmtSecondAlloc);
    char* buf = (char*)Alloc(arena, len + 1);
    vsnprintf(buf, len + 1, fmt, args);
    va_end(args);

    return Str(buf, len);
}

static bool IsWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

void SplitStrByWhitespace(Arena* arena, const Str& s, VecStr& vecOut) {
    vecOut.len = 0;
    vecOut.cap = 0;
    vecOut.els = nullptr;

    int i = 0;
    while (i < s.len) {
        // Skip whitespace
        while (i < s.len && IsWhitespace(s.s[i])) {
            i++;
        }
        if (i >= s.len) break;

        // Find end of token
        int start = i;
        while (i < s.len && !IsWhitespace(s.s[i])) {
            i++;
        }

        // Add token (points into original string, no allocation)
        Str token(s.s + start, i - start);
        VecPush(arena, vecOut, token);
    }
}
