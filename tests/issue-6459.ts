// CAJ conversion failures say what went wrong, CmdConvertCajToPDF writes a PDF with
// caj2pdf, a portable copy keeps caj2pdf in its own dir and a 32-bit OS has no CAJ
// support. Uses the fake converter of tests/issue-6455.ts.
//
// Needs a debug build (the overrides are debug-only) and cl.exe.
//
// Run: bun tests/issue-6459.ts [--no-build]

import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { answerDialog, buildFakeConverter, hasCl, isReleaseExe, kIdDownload, pollUntil } from "./issue-6455.ts";
import { type ControlClient, ControlCommand } from "./control.ts";
import { cmdId, ROOT, runStandalone, tmpPath, USE_NG, writeAppdata } from "./util.ts";
import {
  enumWindows,
  getClassName,
  getWindowPid,
  getWindowText,
  isWindowVisible,
  MK_LBUTTON,
  packCoords,
  postMessage,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const kVer = "1.0.0";
const kNoServer = "http://127.0.0.1:9";
const kStatusDllNotFound = 3221225781; // 0xC0000135, what a missing VCRUNTIME140.dll exits with
const kExitCannotConvert = 1;
const kDialogTitle = "Convert CAJ to PDF";
const kAction = "Convert to PDF";
const kCmd = "CmdConvertCajToPDF";

// what a converter update check would have left behind, so nothing downloads
function installConverter(cajDir: string, exe: Buffer): void {
  mkdirSync(join(cajDir, kVer), { recursive: true });
  writeFileSync(join(cajDir, kVer, "caj2pdf.exe"), exe);
  const sha1 = createHash("sha1").update(exe).digest("hex");
  writeFileSync(join(cajDir, "latestver.json"), JSON.stringify({ Ver: kVer, Sha1: sha1, Size: exe.length }));
}

async function visibility(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestCommandVisibility, [kCmd, "menu"]);
  return String(res[1] ?? "");
}

async function clickConvert(client: ControlClient, frame: number, pid: number): Promise<void> {
  sendCommand(frame, cmdId(kCmd));
  let dlg = 0;
  await pollUntil("the convert dialog", () => {
    enumWindows((h) => {
      if (getWindowPid(h) === pid && isWindowVisible(h) && getWindowText(h) === kDialogTitle) {
        dlg = h;
        return false;
      }
      return true;
    });
    return dlg !== 0;
  });
  const btn = (await client.virtTree(getClassName(dlg))).find((r) => r.text === kAction)?.rect;
  if (!btn) {
    throw new Error("issue-6459: no Convert to PDF button");
  }
  const lp = packCoords(btn.x + Math.floor(btn.dx / 2), btn.y + Math.floor(btn.dy / 2));
  postMessage(dlg, WM_MOUSEMOVE, 0, lp);
  postMessage(dlg, WM_LBUTTONDOWN, MK_LBUTTON, lp);
  postMessage(dlg, WM_LBUTTONUP, 0, lp);
}

export async function testit(): Promise<void> {
  // ng's dialogs are gpui and its load errors go to a notification, not the log
  if (process.platform !== "win32" || USE_NG || isReleaseExe() || !hasCl()) {
    console.log("issue-6459: skipped, needs a Windows debug build of the original app and cl.exe");
    return;
  }

  const dir = tmpPath("issue-6459");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const cajDir = join(dir, "caj2pdf");
  const logPath = join(dir, "sumatra.log");
  const pdf = readFileSync(join(ROOT, "ext", "a-zlib", "zlib.3.pdf"));
  const doc = join(dir, "paper.caj");
  writeFileSync(doc, Buffer.concat([Buffer.from("CAJ\0", "latin1"), pdf]));
  const fake = buildFakeConverter(dir);
  const env = { SUMATRAPDF_CAJ2PDF_URL: kNoServer, SUMATRAPDF_CAJ2PDF_DIR: cajDir };
  const readLog = () => (existsSync(logPath) ? readFileSync(logPath, "latin1") : "");

  // opens a file that won't load and returns the reason the tab shows
  const failureReason = async (name: string, opts: { args?: string[]; extraEnv?: object; answer?: boolean }) => {
    const path = join(dir, name);
    if (!existsSync(path)) {
      writeFileSync(path, "CAJ\0not a real document");
    }
    rmSync(logPath, { force: true });
    const launchEnv = { ...env, ...opts.extraEnv };
    const launch = launchControlled([...(opts.args ?? []), "-log-to-file", logPath, path], {
      env: launchEnv,
      saveSettings: !!opts.args,
    });
    const [{ proc, client }] = await Promise.all([
      launch,
      opts.answer ? answerDialog({ button: kIdDownload, dontAsk: false }) : Promise.resolve(),
    ]);
    try {
      let reason = "";
      await pollUntil(`the load error of ${name}`, () => {
        const m = /SetTabLoadError: '[^']*': (.*)/.exec(readLog());
        reason = m?.[1]?.trim() ?? "";
        return reason !== "";
      });
      await client.quit();
      return reason;
    } finally {
      await killAndWait(proc);
    }
  };

  const expectReason = (what: string, reason: string, wanted: string) => {
    if (!reason.includes(wanted)) {
      throw new Error(`issue-6459: ${what}: reason '${reason}', wanted '${wanted}'`);
    }
  };

  try {
    // the converter can't download
    expectReason(
      "failed download",
      await failureReason("download.caj", { answer: true }),
      "Downloading the caj2pdf converter failed",
    );

    installConverter(cajDir, fake);

    // what the exe's exit code means
    writeFileSync(join(dir, "dll.caj"), `CAJ\0EXIT${kStatusDllNotFound}`);
    expectReason("missing dll", await failureReason("dll.caj", {}), "failed to run (error 0xc0000135)");
    writeFileSync(join(dir, "bad.caj"), `CAJ\0EXIT${kExitCannotConvert}`);
    expectReason("bad file", await failureReason("bad.caj", {}), `couldn't convert this file (exit code 1)`);

    // the command runs the converter and opens the result
    {
      const { proc, client, frame } = await launchControlled(["-log-to-file", logPath, doc], { env });
      try {
        await client.waitForRenderIdle();
        const vis = await visibility(client);
        if (!/vis=show/.test(vis)) {
          throw new Error(`issue-6459: the command should show for a CAJ file: ${vis}`);
        }
        await clickConvert(client, frame, proc.pid!);
        const dest = join(dir, "paper.pdf");
        await pollUntil("the converted PDF", () => existsSync(dest) && readLog().includes(`to '${dest}'`));
        if (!readFileSync(dest).equals(pdf)) {
          throw new Error("issue-6459: the PDF isn't what the converter wrote");
        }
        await client.quit();
      } finally {
        await killAndWait(proc);
      }
    }

    // and it's only for CAJ files
    {
      const other = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
      const { proc, client } = await launchControlled([other], { env });
      try {
        await client.waitForRenderIdle();
        const vis = await visibility(client);
        if (!/vis=hide/.test(vis)) {
          throw new Error(`issue-6459: the command should hide for a PDF: ${vis}`);
        }
        await client.quit();
      } finally {
        await killAndWait(proc);
      }
    }

    // a portable copy keeps the converter in its own dir, not in %LOCALAPPDATA%
    {
      const appdata = writeAppdata("issue-6459-portable", "RestoreSession = false\nCheckForUpdates = false");
      rmSync(join(appdata, "caj2pdf"), { recursive: true, force: true });
      installConverter(join(appdata, "caj2pdf"), fake);
      rmSync(logPath, { force: true });
      const { proc, client } = await launchControlled(["-appdata", appdata, "-log-to-file", logPath, doc], {
        env: { SUMATRAPDF_CAJ2PDF_URL: kNoServer },
        saveSettings: true,
      });
      try {
        await client.waitForRenderIdle();
        if ((await client.chapterInfo()).pageCount < 1) {
          throw new Error("issue-6459: portable copy didn't open the document");
        }
        if (!readLog().includes(join(appdata, "caj2pdf", kVer, "caj2pdf.exe"))) {
          throw new Error(`issue-6459: portable copy didn't use its own converter:\n${readLog()}`);
        }
        await client.quit();
      } finally {
        await killAndWait(proc);
        rmSync(join(appdata, "caj2pdf"), { recursive: true, force: true });
      }
    }

    // a 32-bit OS doesn't try: the converter that's there isn't run and nothing asks
    {
      const reason = await failureReason("paper32.caj", { extraEnv: { SUMATRAPDF_CAJ2PDF_32BIT_OS: "1" } });
      expectReason("32-bit OS", reason, "The file format is not supported");
      if (readLog().includes("caj2pdf:")) {
        throw new Error(`issue-6459: 32-bit OS still used caj2pdf:\n${readLog()}`);
      }
    }
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
