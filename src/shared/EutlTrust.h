/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

bool EutlCertIsEuTrusted(const u8* der, int derLen);
bool EutlCacheExists();
TempStr EutlCacheInfoTemp();
bool EutlUpdate(Str* errOut);
void EutlRegisterLookup();
