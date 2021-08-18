/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/CryptoUtil.h"

#ifndef DWORD_MAX
#define DWORD_MAX 0xffffffffUL
#endif

// TODO: could use CryptoNG available starting in Vista
static NO_INLINE void CalcDigestWin(const void* data, size_t dataSize, u8* digest, DWORD digestSize,
                                    const WCHAR* provider, DWORD type, ALG_ID alg) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BOOL ok = CryptAcquireContextW(&hProv, nullptr, provider, type, CRYPT_VERIFYCONTEXT);
    CrashIf(!ok);
    ok = CryptCreateHash(hProv, alg, 0, 0, &hHash);
    CrashIf(!ok);

#ifdef _WIN64
    for (; dataSize > DWORD_MAX; data = (const BYTE*)data + DWORD_MAX, dataSize -= DWORD_MAX) {
        ok = CryptHashData(hHash, (const BYTE*)data, DWORD_MAX, 0);
        CrashIf(!ok);
    }
#endif
    CrashIf(dataSize > DWORD_MAX);
    ok = CryptHashData(hHash, (const BYTE*)data, (DWORD)dataSize, 0);
    CrashIf(!ok);

    DWORD hashLen = 0;
    DWORD argSize = sizeof(DWORD);
    ok = CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&hashLen, &argSize, 0);
    CrashIf(sizeof(DWORD) != argSize);
    CrashIf(!ok);
    CrashIf(digestSize != hashLen);
    ok = CryptGetHashParam(hHash, HP_HASHVAL, digest, &hashLen, 0);
    CrashIf(!ok);
    CrashIf(digestSize != hashLen);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
}

void CalcMD5Digest(const void* data, size_t dataSize, u8 digest[16]) {
    CalcDigestWin(data, dataSize, digest, 16, MS_DEF_PROV, PROV_RSA_FULL, CALG_MD5);
}

void CalcSHA1Digest(const void* data, size_t dataSize, u8 digest[20]) {
    CalcDigestWin(data, dataSize, digest, 20, MS_DEF_PROV, PROV_RSA_FULL, CALG_SHA1);
}

void CalcSHA2Digest(const void* data, size_t dataSize, u8 digest[32]) {
    CalcDigestWin(data, dataSize, digest, 32, MS_ENH_RSA_AES_PROV, PROV_RSA_AES, CALG_SHA_256);
}

static bool ExtractSignature(const char* hexSignature, const void* data, size_t& dataLen, ScopedMem<BYTE>& signature,
                             size_t& signatureLen) {
    // verify hexSignature format - must be either
    // * a string starting with "sha1:" followed by the signature (and optionally whitespace and further content)
    // * nullptr, then the signature must be found on the last line of non-binary data, starting at " Signature sha1:"
    if (str::StartsWith(hexSignature, "sha1:")) {
        hexSignature += 5;
    } else if (!hexSignature) {
        if (dataLen < 20 || memchr(data, 0, dataLen)) {
            return false;
        }
        const char* lastLine = (const char*)data + dataLen - 1;
        while (lastLine > data && *(lastLine - 1) != '\n') {
            lastLine--;
        }
        if (lastLine == data || !str::Find(lastLine, " Signature sha1:")) {
            return false;
        }
        dataLen = lastLine - (const char*)data;
        hexSignature = str::Find(lastLine, " Signature sha1:") + 16;
    } else {
        return false;
    }

    Vec<BYTE> signatureBytes;
    for (const char* c = hexSignature; *c && !str::IsWs(*c); c += 2) {
        unsigned int val;
        if (1 != sscanf_s(c, "%02x", &val)) {
            return false;
        }
        signatureBytes.Append((BYTE)val);
    }
    signatureLen = signatureBytes.size();
    signature.Set(signatureBytes.StealData());
    return true;
}

bool VerifySHA1Signature(const void* data, size_t dataLen, const char* hexSignature, const void* pubkey,
                         size_t pubkeyLen) {
    HCRYPTPROV hProv = 0;
    HCRYPTKEY hPubKey = 0;
    HCRYPTHASH hHash = 0;
    BOOL ok = false;
    ScopedMem<BYTE> signature;
    size_t signatureLen;

#define Check(val)             \
    if ((ok = (val)) == FALSE) \
    goto CleanUp
    Check(ExtractSignature(hexSignature, data, dataLen, signature, signatureLen));
    Check(CryptAcquireContext(&hProv, nullptr, MS_DEF_PROV, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT));
    Check(CryptImportKey(hProv, (const BYTE*)pubkey, (DWORD)pubkeyLen, 0, 0, &hPubKey));
    Check(CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash));
#ifdef _WIN64
    for (; dataLen > DWORD_MAX; data = (const BYTE*)data + DWORD_MAX, dataLen -= DWORD_MAX) {
        Check(CryptHashData(hHash, (const BYTE*)data, DWORD_MAX, 0));
    }
#endif
    Check(dataLen <= DWORD_MAX && pubkeyLen <= DWORD_MAX && signatureLen <= DWORD_MAX);
    Check(CryptHashData(hHash, (const BYTE*)data, (DWORD)dataLen, 0));
    Check(CryptVerifySignature(hHash, signature, (DWORD)signatureLen, hPubKey, nullptr, 0));
#undef Check

CleanUp:
    if (hHash) {
        CryptDestroyHash(hHash);
    }
    if (hProv) {
        CryptReleaseContext(hProv, 0);
    }
    return ok;
}
