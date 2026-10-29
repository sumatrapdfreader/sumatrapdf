// #6439: Convert PDF to Images must not silently overwrite existing files.
//  - the default name skips taken names: doc-<N>.png => doc-<N>.1.png
//  - a name that would overwrite says so in the dialog, the action becomes
//    Overwrite, and Use New Names picks free names
//
// Run: bun tests/issue-6439.ts [--no-build] [-ng]

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath, USE_NG } from "./util.ts";
import {
  enumChildWindows,
  enumWindows,
  getClassName,
  getControlText,
  getWindowPid,
  getWindowText,
  isWindowVisible,
  MK_LBUTTON,
  packCoords,
  postMessage,
  sendText,
  sleep,
  WM_CLOSE,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const kTitle = "Convert PDF to Images";
const kKeep = "not an image, must survive";
const kNgWindow = "converttoimages";
const VK_A = 0x41;
const kNgCtrl = 1;

type Rc = { x: number; y: number; dx: number; dy: number };

// what the test reads and does in either app's dialog
type Dlg = {
  dest(): Promise<string>;
  // the texts and button labels shown, with their rects
  labels(): Promise<Map<string, Rc>>;
  setDest(s: string): Promise<void>;
  click(r: Rc): Promise<void>;
  close(): Promise<void>;
};

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 2 /Kids [3 0 R 4 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] >>",
  ]);
}

async function waitFor<T>(what: string, get: () => Promise<T | null>): Promise<T> {
  const deadline = Date.now() + 5000;
  for (;;) {
    const v = await get();
    if (v !== null) {
      return v;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6439: ${what}`);
    }
    await sleep(50);
  }
}

async function origDialog(client: ControlClient, pid: number): Promise<Dlg> {
  const dlg = await waitFor("the dialog did not open", async () => {
    let found = 0;
    enumWindows((h) => {
      if (getWindowPid(h) === pid && isWindowVisible(h) && getWindowText(h) === kTitle) {
        found = h;
        return false;
      }
      return true;
    });
    return found || null;
  });
  // the destination is the edit that holds an image template
  let edit = 0;
  enumChildWindows(dlg, (h) => {
    if (getClassName(h).toLowerCase() === "edit" && getControlText(h).includes("<N>")) {
      edit = h;
      return false;
    }
    return true;
  });
  if (!edit) {
    throw new Error("issue-6439: no destination edit");
  }
  return {
    dest: async () => getControlText(edit),
    labels: async () => {
      const m = new Map<string, Rc>();
      for (const r of await client.virtTree(getClassName(dlg))) {
        m.set(r.text, r.rect);
      }
      return m;
    },
    setDest: async (s) => sendText(edit, s),
    click: async (r) => {
      const lp = packCoords(r.x + Math.floor(r.dx / 2), r.y + Math.floor(r.dy / 2));
      postMessage(dlg, WM_MOUSEMOVE, 0, lp);
      postMessage(dlg, WM_LBUTTONDOWN, MK_LBUTTON, lp);
      postMessage(dlg, WM_LBUTTONUP, 0, lp);
    },
    close: async () => {
      postMessage(dlg, WM_CLOSE, 0, 0);
    },
  };
}

async function ngTool(client: ControlClient, args: (string | number)[]): Promise<string> {
  const res = await client.request(ControlCommand.TestToolWindow, args);
  return String(res[1] ?? "");
}

const reRect = /rect=(-?\d+),(-?\d+),(\d+),(\d+)/;

function rcOf(line: string): Rc {
  const m = reRect.exec(line)!;
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

async function ngDialog(client: ControlClient): Promise<Dlg> {
  const layout = () => ngTool(client, ["layout", kNgWindow, "", 0, 0, 0, 0]);
  const destLine = async () => (await layout()).split("\n").find((l) => /^node .* value='.*<N>/.test(l)) ?? null;
  await waitFor("the dialog did not open", destLine);
  const input = (kind: string, a: number, b: number) => ngTool(client, ["input", kNgWindow, kind, a, b, 0, 0]);
  const click = async (r: Rc) => {
    await input("click", r.x + Math.floor(r.dx / 2), r.y + Math.floor(r.dy / 2));
  };
  return {
    dest: async () => /value='(.*)'/.exec((await destLine()) ?? "")?.[1] ?? "",
    labels: async () => {
      const m = new Map<string, Rc>();
      for (const l of (await layout()).split("\n")) {
        const t = /^text .* '(.*)'$/.exec(l) ?? /^node .* label='([^']+)'/.exec(l);
        if (t) {
          m.set(t[1]!, rcOf(l));
        }
      }
      return m;
    },
    setDest: async (s) => {
      // the field's click target, not its node: the node spans the format drop-down
      const node = rcOf((await destLine())!);
      const hit = (await layout())
        .split("\n")
        .filter((l) => /^hit .* input=1/.test(l))
        .map(rcOf)
        .find((r) => node.y >= r.y && node.y <= r.y + r.dy);
      await click(hit ?? node);
      await input("key", VK_A, kNgCtrl);
      for (const ch of s) {
        await input("char", ch.charCodeAt(0), 0);
      }
    },
    click,
    close: async () => {
      await ngTool(client, ["close", kNgWindow, "", 0, 0, 0, 0]);
    },
  };
}

async function waitDest(dlg: Dlg, want: string): Promise<void> {
  let got = "";
  await waitFor(`destination should be ${want}`, async () => {
    got = await dlg.dest();
    return got.toLowerCase() === want.toLowerCase() ? true : null;
  }).catch((e) => {
    throw new Error(`${e.message}, is ${got}`);
  });
}

async function waitLabel(dlg: Dlg, want: string, present: boolean): Promise<Map<string, Rc>> {
  let last = new Map<string, Rc>();
  return waitFor(`'${want}' should ${present ? "" : "not "}show`, async () => {
    last = await dlg.labels();
    return last.has(want) === present ? last : null;
  }).catch((e) => {
    throw new Error(`${e.message}; shown: ${[...last.keys()].join(" | ")}`);
  });
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6439");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  const taken = join(dir, "doc-1.png");
  writeFileSync(taken, kKeep);

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    if (USE_NG) {
      await client.request(ControlCommand.TestInvokeCommand, ["CmdConvertPdfToImages"]);
    } else {
      sendCommand(frame, cmdId("CmdConvertPdfToImages"));
    }
    const dlg = USE_NG ? await ngDialog(client) : await origDialog(client, proc.pid);

    // doc-1.png is taken, so the default moves on
    await waitDest(dlg, join(dir, "doc-<N>.1.png"));
    await waitLabel(dlg, "Convert", true);
    await waitLabel(dlg, "Use New Names", false);

    // typing the taken name warns
    await dlg.setDest(join(dir, "doc-<N>.png"));
    // ng's layout dump has no plain labels
    if (!USE_NG) {
      await waitLabel(dlg, "1 file already exists", true);
    }
    await waitLabel(dlg, "Overwrite", true);
    const shown = await waitLabel(dlg, "Use New Names", true);

    // Use New Names goes back to free names
    await dlg.click(shown.get("Use New Names")!);
    await waitDest(dlg, join(dir, "doc-<N>.1.png"));
    await waitLabel(dlg, "Convert", true);
    await waitLabel(dlg, "Use New Names", false);

    await dlg.close();
  } finally {
    client.close();
    await killAndWait(proc);
  }
  if (readFileSync(taken, "latin1") !== kKeep) {
    throw new Error("issue-6439: doc-1.png was overwritten");
  }
  console.log("issue-6439: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
