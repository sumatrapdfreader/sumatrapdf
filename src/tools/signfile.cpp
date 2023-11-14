/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// signfile produces a cryptographic signature of a given file
// using a (self-signed) certificate from the local store.

// The idea is that an application ships using the certificate's public key
// and is then able to verify whether external data (e.g. an update downloaded
// over a non-secure channel) is from the same source as the application.

// To produce a usable certificate, use the SDK's makecert.exe tool:
// makecert -r -n "CN=SumatraPDF Authority" -cy authority -a sha1 -sv sumatra.pvk sumatra.cer
// makecert -n "CN=SumatraPDF" -ic sumatra.cer -iv sumatra.pvk -a sha1 -sky signature -pe -sr currentuser -ss My
// sumatra-app.cer

#include "utils/BaseUtil.h"
#include "utils/CmdLineArgsIter.h"
#include "utils/CryptoUtil.h"
#include "utils/FileUtil.h"

#define ErrOut(msg, ...) fprintf(stderr, msg "\n", __VA_ARGS__)
#define ErrOut1(msg) fprintf(stderr, "%s", msg "\n")

void _uploadDebugReportIfFunc(bool, char const*){};

void ShowUsage(const char* exeName) {
    ErrOut("Syntax: %s", path::GetBaseNameTemp(exeName));
    ErrOut1("\t[-cert CertName]\t- name of the certificate to use");      // when omitted uses first available
    ErrOut1("\t[-out filename.out]\t- where to save the signature file"); // when omitted uses stdout
    ErrOut1("\t[-comment #]\t\t- comment syntax for signed text files");  // needed when saving the signature into the
                                                                          // signed file
    ErrOut1("\t[-pubkey public.key]\t- where to save the public key");    // usually not needed
    ErrOut1("\tfilename.in"); // usually needed, optional when -pubkey is present
    ErrOut1("");

    HCERTSTORE hStore = CertOpenSystemStore(NULL, L"My");
    CrashAlwaysIf(!hStore);
    bool hasCert = false;
    PCCERT_CONTEXT pCertCtx = nullptr;
    while ((pCertCtx = CertEnumCertificatesInStore(hStore, pCertCtx)) != nullptr) {
        WCHAR name[128];
        DWORD res = CertGetNameString(pCertCtx, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, name, dimof(name));
        if (!res) {
            continue;
        }
        HCRYPTPROV hProv = NULL;
        DWORD keySpec = 0;
        BOOL ok = CryptAcquireCertificatePrivateKey(pCertCtx, 0, nullptr, &hProv, &keySpec, nullptr);
        if (!ok || (keySpec & AT_SIGNATURE) != AT_SIGNATURE)
            continue;
        if (!hasCert) {
            ErrOut1("Available certificates:");
            hasCert = true;
        }
        // fprintf(stderr, "\"%s\"\n", name);
        fprintf(stderr, "%s\n", ToUtf8Temp(name));
    }
    if (!hasCert)
        ErrOut1("Warning: Failed to find a signature certificate in store \"My\"!");
    CertCloseStore(hStore, 0);
}

int main() {
    StrVec args;
    ParseCmdLine(GetCommandLineW(), args);

    const char* filePath = nullptr;
    const char* certName = nullptr;
    const char* signFilePath = nullptr;
    const char* pubkeyPath = nullptr;
    AutoFreeStr inFileCommentSyntax;
    // find certificate
    HCERTSTORE hStore = nullptr;
    PCCERT_CONTEXT pCertCtx = nullptr;
    HCRYPTPROV hProv = NULL;
    HCRYPTKEY hKey = NULL;
    HCRYPTHASH hHash = NULL;
    int errorCode = 2;
    DWORD pubkeyLen = 0;
    DWORD sigLen = 0;

    ScopedMem<BYTE> pubkey;
    AutoFreeStr data;
    AutoFreeStr hexSignature;
    ScopedMem<BYTE> signature;
    BOOL ok;
    const char* sig = nullptr;

#define is_arg(name, var) (str::EqI(args[i], name) && i + 1 < args.Size() && !var)
    for (int i = 1; i < args.Size(); i++) {
        if (is_arg("-cert", certName))
            certName = args.at(++i);
        else if (is_arg("-out", signFilePath))
            signFilePath = args.at(++i);
        else if (is_arg("-pubkey", pubkeyPath))
            pubkeyPath = args.at(++i);
        else if (is_arg("-comment", inFileCommentSyntax)) {
            auto tmp = args.at(++i);
            inFileCommentSyntax.SetCopy(tmp);
        } else if (!filePath)
            filePath = args.at(i);
        else
            goto SyntaxError;
    }
#undef is_arg
    if (!filePath && !pubkeyPath) {
    SyntaxError:
        ShowUsage(args.at(0));
        return 1;
    }

    // find certificate
    hStore = CertOpenSystemStoreW(NULL, L"My");
    CrashAlwaysIf(!hStore);

#define QuitIfNot(cond, msg, ...) \
    if (!(cond)) {                \
        ErrOut(msg, __VA_ARGS__); \
        goto ErrorQuit;           \
    }

    // find a certificate for hash signing
    if (!certName) {
        // find first available certificate (same as in ShowUsage)
        while ((pCertCtx = CertEnumCertificatesInStore(hStore, pCertCtx)) != nullptr) {
            WCHAR name[128];
            DWORD res = CertGetNameString(pCertCtx, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, name, dimof(name));
            if (!res)
                continue;
            DWORD keySpec = 0;
            ok = CryptAcquireCertificatePrivateKey(pCertCtx, 0, nullptr, &hProv, &keySpec, nullptr);
            if (ok && (keySpec & AT_SIGNATURE) == AT_SIGNATURE)
                break;
        }
        QuitIfNot(pCertCtx, "%s", "Error: Failed to find a signature certificate in store \"My\"!");
    } else {
        DWORD keySpec;
        do {
            pCertCtx = CertFindCertificateInStore(hStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0,
                                                  CERT_FIND_SUBJECT_STR, certName, pCertCtx);
            if (!pCertCtx)
                break;
            keySpec = 0;
            ok = CryptAcquireCertificatePrivateKey(pCertCtx, 0, nullptr, &hProv, &keySpec, nullptr);
        } while (!ok || (keySpec & AT_SIGNATURE) != AT_SIGNATURE);
        QuitIfNot(pCertCtx, "Error: Failed to find a signature certificate for \"%s\" in store \"My\"!", certName);
    }

    // extract public key for verficiation and export
    ok = CryptGetUserKey(hProv, AT_SIGNATURE, &hKey);
    QuitIfNot(ok, "%s", "Error: Failed to export the public key!");
    ok = CryptExportKey(hKey, NULL, PUBLICKEYBLOB, 0, nullptr, &pubkeyLen);
    QuitIfNot(ok, "%s", "Error: Failed to export the public key!");
    pubkey.Set(AllocArray<BYTE>(pubkeyLen));
    ok = CryptExportKey(hKey, NULL, PUBLICKEYBLOB, 0, pubkey.Get(), &pubkeyLen);
    QuitIfNot(ok, "%s", "Error: Failed to export the public key!");
    if (pubkeyPath) {
        ByteSlice d(pubkey.Get(), pubkeyLen);
        ok = file::WriteFile(pubkeyPath, d);
        QuitIfNot(ok, "Error: Failed to write the public key to \"%s\"!", pubkeyPath);
        QuitIfNot(filePath, "Wrote the public key to \"%s\", no file to sign.", pubkeyPath);
    }

    // prepare data for signing
    size_t dataLen;
    {
        ByteSlice tmp(file::ReadFile(filePath));
        dataLen = tmp.size();
        data.Set(tmp);
    }
    QuitIfNot(data && dataLen <= UINT_MAX, "Error: Failed to read from \"%s\" (or file is too large)!", filePath);
    ok = !inFileCommentSyntax || (dataLen > 0 && !memchr(data.Get(), 0, dataLen));
    QuitIfNot(ok, "%s", "Error: Can't put signature comment into binary or empty file!");
    if (inFileCommentSyntax) {
        // cut previous signature from file
        char* lastLine = data + dataLen - 1;
        while (lastLine > data.Get() && *(lastLine - 1) != '\n')
            lastLine--;
        const char* lf = str::Find(data, "\r\n") || !str::FindChar(data, '\n') ? "\r\n" : "\n";
        if (lastLine > data && str::StartsWith(lastLine, inFileCommentSyntax.Get()) &&
            str::StartsWith(lastLine + str::Len(inFileCommentSyntax), " Signature sha1:")) {
            strcpy_s(lastLine, 3, lf);
        } else {
            data.Set(str::Format("%s%s", data.Get(), lf));
        }
        dataLen = str::Len(data);
    }

    // sign data
    ok = CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash);
    QuitIfNot(ok, "%s", "Error: Failed to create a SHA-1 hash!");
#ifdef _WIN64
    {
        const BYTE* bytes = (const BYTE*)data.Get();
        size_t bytesLen = dataLen;
        for (; bytesLen > ULONG_MAX; bytes += ULONG_MAX, bytesLen -= ULONG_MAX) {
            ok = ok && CryptHashData(hHash, bytes, ULONG_MAX, 0);
        }
        ok = ok && CryptHashData(hHash, bytes, (ULONG)bytesLen, 0);
    }
#else
    ok = CryptHashData(hHash, (const BYTE*)data.Get(), dataLen, 0);
#endif
    QuitIfNot(ok, "%s", "Error: Failed to calculate the SHA-1 hash!");
    ok = CryptSignHash(hHash, AT_SIGNATURE, nullptr, 0, nullptr, &sigLen);
    QuitIfNot(ok, "%s", "Error: Failed to sign the SHA-1 hash!");
    signature.Set(AllocArray<BYTE>(sigLen));
    ok = CryptSignHash(hHash, AT_SIGNATURE, nullptr, 0, signature.Get(), &sigLen);
    QuitIfNot(ok, "%s", "Error: Failed to sign the SHA-1 hash!");

    // convert signature to ASCII text
    hexSignature.Set(str::MemToHex((const u8*)signature.Get(), sigLen));
    if (inFileCommentSyntax) {
        const char* lf = str::Find(data, "\r\n") || !str::FindChar(data, '\n') ? "\r\n" : "\n";
        data.Set(
            str::Format("%s%s Signature sha1:%s%s", data.Get(), inFileCommentSyntax.Get(), hexSignature.Get(), lf));
        dataLen = str::Len(data);
        hexSignature.SetCopy(data);
    } else {
        hexSignature.Set(str::Format("sha1:%s\r\n", hexSignature.Get()));
    }

    if (!inFileCommentSyntax) {
        sig = hexSignature.Get();
    }
    ok = VerifySHA1Signature(data.Get(), dataLen, sig, pubkey, pubkeyLen);
    QuitIfNot(ok, "%s", "Error: Failed to verify signature!");

    // save/display signature
    if (signFilePath) {
        char* s = hexSignature.Get();
        size_t sLen = str::Len(s);
        ByteSlice d((u8*)s, sLen);
        ok = file::WriteFile(signFilePath, d);
        QuitIfNot(ok, "Error: Failed to write signature to \"%s\"!", signFilePath);
    } else {
        fprintf(stdout, "%s", hexSignature.Get());
    }
    errorCode = 0;

ErrorQuit:
    if (hHash)
        CryptDestroyHash(hHash);
    if (hKey)
        CryptDestroyKey(hKey);
    if (hProv)
        CryptReleaseContext(hProv, 0);
    if (pCertCtx)
        CertFreeCertificateContext(pCertCtx);
    if (hStore)
        CertCloseStore(hStore, 0);

    return errorCode;
}
