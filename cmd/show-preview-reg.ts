import { spawnSync } from "node:child_process";

// Must match src/RegistryPreview.h and src/RegistryPreview.cpp
const kThumbnailProviderClsid = "{e357fccd-a995-4576-b01f-234630154e96}";
const kPreviewHandlerClsid = "{8895b1c6-b41f-4c1c-a562-0d564250836f}";

// Must match src/RegistryPreview.h
const kPreview2Clsids: Record<string, string> = {
  PDF: "{3D3B1846-CC43-42AE-BFF9-D914083C2BA3}",
  XPS: "{D427A82C-6545-4FBE-8E87-030EDB3BE46D}",
  DjVu: "{6689D0D4-1E9C-400A-8BCA-FA6C56B2C3B5}",
  EPUB: "{80C4E4B1-2B0F-40D5-95AF-BE7B57FEA4F9}",
  FB2: "{D5878036-E863-403E-A62C-7B9C7453336A}",
  MOBI: "{42CA907E-BDF5-4A75-994A-E1AEC8A10954}",
  CBX: "{C29D3E2B-8FF6-4033-A4E8-54221D859D74}",
  TGA: "{CB1D63A6-FE5E-4DED-BEA5-3F6AF1A70D08}",
};

const previewers = [
  { name: "PDF", clsid: kPreview2Clsids.PDF, exts: [".pdf"] },
  { name: "CBX", clsid: kPreview2Clsids.CBX, exts: [".cbz", ".cbr", ".cb7", ".cbt"] },
  { name: "TGA", clsid: kPreview2Clsids.TGA, exts: [".tga"] },
  { name: "DjVu", clsid: kPreview2Clsids.DjVu, exts: [".djvu"] },
  { name: "XPS", clsid: kPreview2Clsids.XPS, exts: [".xps", ".oxps"] },
  { name: "EPUB", clsid: kPreview2Clsids.EPUB, exts: [".epub"] },
  { name: "FB2", clsid: kPreview2Clsids.FB2, exts: [".fb2", ".fb2z", ".fbz", ".zfb2", ".fb2.zip"] },
  { name: "MOBI", clsid: kPreview2Clsids.MOBI, exts: [".mobi"] },
];

function regQuery(key: string, valueName?: string): string | null {
  const args = ["query", key];
  if (valueName !== undefined) {
    args.push("/v", valueName);
  } else {
    args.push("/ve"); // default value
  }
  const result = spawnSync("reg", args, { encoding: "utf-8", stdio: ["pipe", "pipe", "pipe"] });
  if (result.status !== 0) {
    return null;
  }
  // parse output: look for REG_SZ line
  const lines = result.stdout.split("\n");
  for (const line of lines) {
    const match = line.match(/REG_SZ\s+(.+)/);
    if (match) {
      return match[1].trim();
    }
  }
  return null;
}

function checkExt(ext: string, expectedClsid: string) {
  const roots = [
    { name: "HKCR", key: `HKEY_CLASSES_ROOT\\${ext}` },
    { name: "HKCU", key: `HKEY_CURRENT_USER\\Software\\Classes\\${ext}` },
    { name: "HKLM", key: `HKEY_LOCAL_MACHINE\\Software\\Classes\\${ext}` },
  ];

  // Check IThumbnailProvider
  console.log(`  IThumbnailProvider (${kThumbnailProviderClsid}):`);
  for (const root of roots) {
    const key = `${root.key}\\shellex\\${kThumbnailProviderClsid}`;
    const value = regQuery(key);
    if (value) {
      const match = value.toLowerCase() === expectedClsid.toLowerCase();
      const status = match ? "OK" : `MISMATCH (expected ${expectedClsid})`;
      console.log(`    ${root.name}: ${value} - ${status}`);
    } else {
      console.log(`    ${root.name}: (not set)`);
    }
  }

  // Check IPreviewHandler
  console.log(`  IPreviewHandler (${kPreviewHandlerClsid}):`);
  for (const root of roots) {
    const key = `${root.key}\\shellex\\${kPreviewHandlerClsid}`;
    const value = regQuery(key);
    if (value) {
      const match = value.toLowerCase() === expectedClsid.toLowerCase();
      const status = match ? "OK" : `MISMATCH (expected ${expectedClsid})`;
      console.log(`    ${root.name}: ${value} - ${status}`);
    } else {
      console.log(`    ${root.name}: (not set)`);
    }
  }
}

function checkClsidRegistration(name: string, clsid: string) {
  const roots = [
    { name: "HKCU", key: `HKEY_CURRENT_USER\\Software\\Classes\\CLSID\\${clsid}` },
    { name: "HKLM", key: `HKEY_LOCAL_MACHINE\\Software\\Classes\\CLSID\\${clsid}` },
  ];

  for (const root of roots) {
    const displayName = regQuery(root.key);
    const dllPath = regQuery(`${root.key}\\InprocServer32`);
    const appId = regQuery(root.key, "AppId");
    if (displayName || dllPath) {
      console.log(`  CLSID ${root.name}: ${displayName || "(no display name)"}`);
      if (dllPath) {
        console.log(`    InprocServer32: ${dllPath}`);
      }
      if (appId) {
        console.log(`    AppId: ${appId}`);
      }
    } else {
      console.log(`  CLSID ${root.name}: (not registered)`);
    }
  }
}

function checkPreviewHandlersKey(clsid: string, name: string) {
  const roots = [
    { name: "HKCU", key: "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers" },
    { name: "HKLM", key: "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers" },
  ];
  for (const root of roots) {
    const value = regQuery(root.key, clsid);
    if (value) {
      console.log(`  PreviewHandlers ${root.name}: ${value}`);
    }
  }
}

function main() {
  console.log("SumatraPDF Preview/Thumbnail Registration Status\n");

  for (const prev of previewers) {
    console.log(`=== ${prev.name} (CLSID: ${prev.clsid}) ===`);
    checkClsidRegistration(prev.name, prev.clsid);
    checkPreviewHandlersKey(prev.clsid, prev.name);
    for (const ext of prev.exts) {
      console.log(`  --- ${ext} ---`);
      checkExt(ext, prev.clsid);
    }
    console.log("");
  }
}

main();
