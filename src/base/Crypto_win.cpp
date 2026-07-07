/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Crypto.h"

#include <wincrypt.h>

#ifndef DWORD_MAX
#define DWORD_MAX 0xffffffffUL
#endif

// TODO: could use CryptoNG available starting in Vista
static NO_INLINE void CalcDigestWin(Str d, u8* digest, DWORD digestSize, const WCHAR* provider, DWORD type,
                                    ALG_ID alg) {
    const void* data = d.s;
    int dataSize = d.len;
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BOOL ok = CryptAcquireContextW(&hProv, nullptr, provider, type, CRYPT_VERIFYCONTEXT);
    ReportIf(!ok);
    ok = CryptCreateHash(hProv, alg, 0, 0, &hHash);
    ReportIf(!ok);

#ifdef _WIN64
    for (; dataSize > DWORD_MAX; data = (const BYTE*)data + DWORD_MAX, dataSize -= DWORD_MAX) {
        ok = CryptHashData(hHash, (const BYTE*)data, DWORD_MAX, 0);
        ReportIf(!ok);
    }
#endif
    ok = CryptHashData(hHash, (const BYTE*)data, (DWORD)dataSize, 0);
    ReportIf(!ok);

    DWORD hashLen = 0;
    DWORD argSize = sizeof(DWORD);
    ok = CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&hashLen, &argSize, 0);
    ReportIf(sizeof(DWORD) != argSize);
    ReportIf(!ok);
    if (digestSize != hashLen) {
        ReportIf(digestSize != hashLen);
    }
    ok = CryptGetHashParam(hHash, HP_HASHVAL, digest, &hashLen, 0);
    ReportIf(!ok);
    ReportIf(digestSize != hashLen);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
}

void CalcMD5Digest(Str data, u8 digest[16]) {
    CalcDigestWin(data, digest, 16, MS_DEF_PROV, PROV_RSA_FULL, CALG_MD5);
}

void CalcSHA1Digest(Str data, u8 digest[20]) {
    CalcDigestWin(data, digest, 20, MS_DEF_PROV, PROV_RSA_FULL, CALG_SHA1);
}

void CalcSHA2Digest(Str data, u8 digest[32]) {
    CalcDigestWin(data, digest, 32, MS_ENH_RSA_AES_PROV, PROV_RSA_AES, CALG_SHA_256);
}

static bool ExtractSignature(Str hexSignature, Str& data, ScopedMem<BYTE>& signature, size_t& signatureLen) {
    // verify hexSignature format - must be either
    // * a string starting with "sha1:" followed by the signature (and optionally whitespace and further content)
    // * empty, then the signature must be found on the last line of non-binary data, starting at " Signature sha1:"
    Str hex = hexSignature;
    if (str::StartsWith(hex, "sha1:")) {
        hex = Str(hex.s + 5, hex.len - 5);
    } else if (!hex) {
        if (data.len < 20 || memchr(data.s, 0, data.len)) {
            return false;
        }
        const char* lastLine = data.s + data.len - 1;
        while (lastLine > data.s && *(lastLine - 1) != '\n') {
            lastLine--;
        }
        if (lastLine == data.s || !str::Contains(Str(lastLine), StrL(" Signature sha1:"))) {
            return false;
        }
        data.len = (int)(lastLine - data.s);
        str::Cut(Str(lastLine), StrL(" Signature sha1:"), nullptr, &hex);
    } else {
        return false;
    }

    Vec<BYTE> signatureBytes;
    for (int off = 0; off + 1 < hex.len && !str::IsWs(hex.s[off]); off += 2) {
        unsigned int val;
        if (1 != sscanf_s(hex.s + off, "%02x", &val)) {
            return false;
        }
        signatureBytes.Append((BYTE)val);
    }
    signatureLen = len(signatureBytes);
    signature.Set(signatureBytes.Take());
    return true;
}

bool VerifySHA1Signature(Str data, Str hexSignature, Str pubkey) {
    HCRYPTPROV hProv = 0;
    HCRYPTKEY hPubKey = 0;
    HCRYPTHASH hHash = 0;
    BOOL ok = false;
    ScopedMem<BYTE> signature;
    size_t signatureLen;
    const BYTE* dataPtr = (const BYTE*)data.s;
    size_t dataLen = (size_t)data.len;

#define Check(val) \
    if ((ok = (val)) == FALSE) goto CleanUp
    Check(ExtractSignature(hexSignature, data, signature, signatureLen));
    dataPtr = (const BYTE*)data.s;
    dataLen = (size_t)data.len;
    Check(CryptAcquireContext(&hProv, nullptr, MS_DEF_PROV, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT));
    Check(CryptImportKey(hProv, (const BYTE*)pubkey.s, (DWORD)pubkey.len, 0, 0, &hPubKey));
    Check(CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash));
#ifdef _WIN64
    for (; dataLen > DWORD_MAX; dataPtr += DWORD_MAX, dataLen -= DWORD_MAX) {
        Check(CryptHashData(hHash, dataPtr, DWORD_MAX, 0));
    }
#endif
    Check(dataLen <= DWORD_MAX && (size_t)pubkey.len <= DWORD_MAX && signatureLen <= DWORD_MAX);
    Check(CryptHashData(hHash, dataPtr, (DWORD)dataLen, 0));
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

Str ExtractP7m(Str d) {
    if (len(d) == 0) {
        return {};
    }
    const u8* data = (u8*)d.s;
    DWORD dataLen = (DWORD)d.len;

    HCRYPTMSG hMsg = CryptMsgOpenToDecode(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, 0, 0, nullptr, nullptr);
    if (!hMsg) {
        return {};
    }

    BOOL ok = CryptMsgUpdate(hMsg, data, dataLen, TRUE);
    if (!ok) {
        CryptMsgClose(hMsg);
        return {};
    }

    DWORD cbContent = 0;
    ok = CryptMsgGetParam(hMsg, CMSG_CONTENT_PARAM, 0, nullptr, &cbContent);
    if (!ok || cbContent == 0) {
        CryptMsgClose(hMsg);
        return {};
    }

    u8* content = AllocArray<u8>(cbContent);
    ok = CryptMsgGetParam(hMsg, CMSG_CONTENT_PARAM, 0, content, &cbContent);
    CryptMsgClose(hMsg);
    if (!ok) {
        free(content);
        return {};
    }
    return Str((char*)(content), (int)(cbContent));
}
