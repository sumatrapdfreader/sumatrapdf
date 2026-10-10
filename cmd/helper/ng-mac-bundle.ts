// Wraps the mac executable in SumatraPDF.app so Finder can offer it in
// "Open With" and make it the default viewer.
//
//   SumatraPDF.app/Contents/Info.plist
//                          /MacOS/SumatraPDF
//                          /Resources/SumatraPDF.icns

import { constants, copyFileSync, existsSync, mkdirSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { basename, join } from "node:path";

const bundleId = "org.sumatrapdfreader.SumatraPDF";
const iconSrc = "src/gfx/appx/SumatraLogo310x310.png";
const iconName = "SumatraPDF.icns";
const iconSizes = [16, 32, 128, 256, 512];

type DocType = { name: string; utis?: string[]; exts?: string[] };

// a type without a system UTI is matched by extension
const docTypes: DocType[] = [
  { name: "PDF Document", utis: ["com.adobe.pdf"] },
  { name: "EPUB Document", utis: ["org.idpf.epub-container"] },
  { name: "Image", utis: ["public.png", "public.jpeg", "public.tiff", "com.compuserve.gif", "com.microsoft.bmp"] },
  { name: "Comic Book", exts: ["cbz", "cbr", "cb7", "cbt"] },
  { name: "DjVu Document", exts: ["djvu", "djv"] },
  { name: "XPS Document", exts: ["xps", "oxps"] },
  { name: "Ebook", exts: ["mobi", "azw", "azw3", "fb2", "fb2z", "pdb", "prc"] },
  { name: "CHM Document", exts: ["chm"] },
  { name: "PostScript Document", exts: ["ps", "eps"] },
  { name: "WebP Image", exts: ["webp"] },
  { name: "AVIF Image", exts: ["avif"] },
  { name: "HEIF Image", exts: ["heic", "heif"] },
  { name: "JPEG XL Image", exts: ["jxl"] },
];

function plistArray(items: string[]): string {
  return `<array>${items.map((s) => `<string>${s}</string>`).join("")}</array>`;
}

function docTypePlist(t: DocType): string {
  const match = t.utis
    ? `<key>LSItemContentTypes</key>${plistArray(t.utis)}`
    : `<key>CFBundleTypeExtensions</key>${plistArray(t.exts!)}`;
  return `    <dict>
      <key>CFBundleTypeName</key><string>${t.name}</string>
      <key>CFBundleTypeRole</key><string>Viewer</string>
      <key>LSHandlerRank</key><string>Alternate</string>
      ${match}
    </dict>`;
}

function infoPlist(exeName: string, version: string): string {
  return `<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>SumatraPDF</string>
  <key>CFBundleDisplayName</key><string>SumatraPDF</string>
  <key>CFBundleIdentifier</key><string>${bundleId}</string>
  <key>CFBundleExecutable</key><string>${exeName}</string>
  <key>CFBundleIconFile</key><string>${iconName}</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
  <key>CFBundleShortVersionString</key><string>${version}</string>
  <key>CFBundleVersion</key><string>${version}</string>
  <key>NSPrincipalClass</key><string>NSApplication</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>CFBundleDocumentTypes</key>
  <array>
${docTypes.map(docTypePlist).join("\n")}
  </array>
</dict>
</plist>
`;
}

function run(cmd: string[]): void {
  const p = Bun.spawnSync(cmd, { stdout: "pipe", stderr: "pipe" });
  if (p.exitCode !== 0) throw new Error(`${cmd.join(" ")} failed:\n${p.stderr.toString()}`);
}

function isNewer(a: string, b: string): boolean {
  return existsSync(b) && statSync(b).mtimeMs >= statSync(a).mtimeMs;
}

// .icns holds each size at 1x and 2x; the 310 px logo is upscaled for 512@2x
function makeIcon(root: string, tmpDir: string, dst: string): void {
  const src = join(root, iconSrc);
  if (isNewer(src, dst)) return;
  const iconset = join(tmpDir, "SumatraPDF.iconset");
  rmSync(iconset, { recursive: true, force: true });
  mkdirSync(iconset, { recursive: true });
  for (const size of iconSizes) {
    for (const scale of [1, 2]) {
      const px = String(size * scale);
      const suffix = scale === 2 ? "@2x" : "";
      run(["sips", "-z", px, px, src, "--out", join(iconset, `icon_${size}x${size}${suffix}.png`)]);
    }
  }
  run(["iconutil", "-c", "icns", iconset, "-o", dst]);
  rmSync(iconset, { recursive: true, force: true });
}

function appVersion(root: string): string {
  const h = readFileSync(join(root, "src/BuildConfig_default.h"), "utf8");
  const m = h.match(/^#define CURR_VERSION\s+([\d.]+)/m);
  if (!m) throw new Error("CURR_VERSION not found in src/BuildConfig_default.h");
  return m[1]!;
}

export function makeMacBundle(root: string, exe: string): string {
  const exeName = basename(exe);
  const outDir = join(exe, "..");
  const app = join(outDir, `${exeName}.app`);
  const macOsDir = join(app, "Contents", "MacOS");
  const resDir = join(app, "Contents", "Resources");
  mkdirSync(macOsDir, { recursive: true });
  mkdirSync(resDir, { recursive: true });

  const bundleExe = join(macOsDir, exeName);
  if (!isNewer(exe, bundleExe)) {
    rmSync(bundleExe, { force: true });
    // a clone on APFS: no second copy of the binary on disk
    copyFileSync(exe, bundleExe, constants.COPYFILE_FICLONE);
  }
  makeIcon(root, outDir, join(resDir, iconName));
  writeFileSync(join(app, "Contents", "Info.plist"), infoPlist(exeName, appVersion(root)));
  // seals Info.plist and the icon into the signature
  run(["codesign", "--force", "--sign", "-", app]);
  return app;
}
