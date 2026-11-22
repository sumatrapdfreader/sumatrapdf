/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// sha1 of a file's content, valid while its size and modification time are
// those it was hashed at
struct ContentHash {
    u8 sha1[20]{};
    i64 size = -1;
    u64 mtime = 0;
};

constexpr int kSha1Len = 20;

bool ParseSha1Hex(Str hex, u8 out[kSha1Len]);
TempStr Sha1HexTemp(const u8 sha1[kSha1Len]);
bool ParseContentHash(Str s, ContentHash& out);
TempStr FormatContentHashTemp(const ContentHash& h);
void RememberContentHash(Str path, const ContentHash& h);
bool KnownContentHash(Str path, ContentHash& out);
void SaveKnownContentHashes();
void QueueContentHashes(const Vec<i64>& sizes, const Func0& onHashed);
void ForgetContentHashChecks();
void SetContentHashTestHooks(int stallMs, int delayMs, Str match);
TempStr ContentHashStatusTemp();
