// Installer options for Explorer previews and thumbnails are separate (issue #6442):
//  - -with-thumbnails checks "thumbnails", -with-preview checks "previews"
//  - -with-preview alone leaves "thumbnails" unchecked
//  - the option checkboxes don't overlap and fit above the button row
//
// Only drives the installer window; it never installs. A checkbox is also
// checked when this machine has that handler registered, so those parts are skipped then.
// Registration itself is covered by PreviewHandlersTest in src/tests/Sumatra_ut.cpp.
//
// Run: bun tests/issue-6442.ts [--no-build]
// Needs the regular (non-static) exe: SumatraPDF-static embeds no installer payload.

import { mkdirSync, rmSync } from "node:fs";
import { basename } from "node:path";
import { IS_MAC } from "./host.ts";
import { buttons, checkLayout, showOptions } from "./installer-desktop-shortcut.ts";
import { EXE, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait } from "./win-automation.ts";
import { postMessage, sendMessage, sleep, waitForTopWindow, WM_CLOSE } from "./winapi.ts";

const BM_GETCHECK = 0x00f0;
const BST_CHECKED = 1;
const kPreviewIid = "{8895b1c6-b41f-4c1c-a562-0d564250836f}";
const kThumbnailIid = "{e357fccd-a995-4576-b01f-234630154e96}";
const kPdfPreviewClsid = "{3D3B1846-CC43-42AE-BFF9-D914083C2BA3}";

type Options = { preview: boolean; thumbnails: boolean };

// our handler for .pdf is registered on this machine
function isOurPdfHandler(iid: string): boolean {
  const p = Bun.spawnSync(["reg", "query", `HKCR\\.pdf\\shellex\\${iid}`, "/ve"]);
  return p.stdout.toString().toUpperCase().includes(kPdfPreviewClsid);
}

async function installerOptions(installDir: string, extra: string[]): Promise<Options> {
  const proc = Bun.spawn([EXE, "-for-testing", "-lang", "en", "-install", "-d", installDir, ...extra], {
    stdout: "ignore",
    stderr: "ignore",
  });
  try {
    const frame = await waitForTopWindow(proc.pid!, "SUMATRA_PDF_INSTALLER_FRAME", 12000);
    if (!frame) {
      throw new Error("issue-6442: installer window did not appear");
    }
    await sleep(500);
    const checks = await showOptions(frame);
    const opts = buttons(frame).find((b) => b.text.endsWith("Options"));
    if (!opts) {
      throw new Error("issue-6442: no Options button");
    }
    checkLayout(frame, checks, opts.r);
    const isChecked = (label: string): boolean => {
      const c = checks.find((c) => c.text.includes(label));
      if (!c) {
        throw new Error(`issue-6442: no '${label}' option in ${JSON.stringify(checks.map((c) => c.text))}`);
      }
      return Number(sendMessage(c.hwnd, BM_GETCHECK, 0, 0)) === BST_CHECKED;
    };
    const res = { preview: isChecked("show previews"), thumbnails: isChecked("show thumbnails") };
    postMessage(frame, WM_CLOSE, 0, 0);
    return res;
  } finally {
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  if (IS_MAC || USE_NG) {
    console.log("SKIP issue-6442: ng's installer doesn't register Explorer handlers");
    return;
  }
  if (/static/i.test(basename(EXE))) {
    console.log("skip issue-6442: static exe has no installer payload");
    return;
  }
  const installDir = tmpPath("issue-6442");
  rmSync(installDir, { recursive: true, force: true });
  mkdirSync(installDir, { recursive: true });

  const thumbs = await installerOptions(installDir, ["-with-thumbnails"]);
  if (!thumbs.thumbnails) {
    throw new Error("issue-6442: -with-thumbnails left the thumbnails option unchecked");
  }
  if (!thumbs.preview && isOurPdfHandler(kPreviewIid)) {
    throw new Error("issue-6442: the previews option is unchecked though our preview handler is registered");
  }

  const preview = await installerOptions(installDir, ["-with-preview"]);
  if (!preview.preview) {
    throw new Error("issue-6442: -with-preview left the previews option unchecked");
  }
  if (isOurPdfHandler(kThumbnailIid)) {
    console.log("issue-6442: skip -with-preview alone check, this machine has our thumbnail handler");
    return;
  }
  if (preview.thumbnails) {
    throw new Error("issue-6442: -with-preview checked the thumbnails option");
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
