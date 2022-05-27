/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#define kPdfPreviewClsid "{3D3B1846-CC43-42AE-BFF9-D914083C2BA3}"
#define kXpsPreviewClsid "{D427A82C-6545-4FBE-8E87-030EDB3BE46D}"
#define kDjVuPreviewClsid "{6689D0D4-1E9C-400A-8BCA-FA6C56B2C3B5}"
#define kEpubPreviewClsid "{80C4E4B1-2B0F-40D5-95AF-BE7B57FEA4F9}"
#define kFb2PreviewClsid "{D5878036-E863-403E-A62C-7B9C7453336A}"
#define kMobiPreviewClsid "{42CA907E-BDF5-4A75-994A-E1AEC8A10954}"
#define kCbxPreviewClsid "{C29D3E2B-8FF6-4033-A4E8-54221D859D74}"
#define kTgaPreviewClsid "{CB1D63A6-FE5E-4DED-BEA5-3F6AF1A70D08}"

bool InstallPreviewDll(const char* dllPath, bool allUsers);
bool UninstallPreviewDll();
void DisablePreviewInstallExts(const char* cmdLine);
bool IsPreviewInstalled();
