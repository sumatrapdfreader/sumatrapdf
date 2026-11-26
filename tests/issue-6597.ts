// Home thumbnails support modifier clicks, marquee selection and bulk actions.
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, waitForHome, withControlledSumatra } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { EXE, makeBlankPdf, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { findCanvas, waitForFrame } from "./win-automation.ts";
import {
  MK_CONTROL,
  MK_LBUTTON,
  packCoords,
  sendMessage,
  VK_ESCAPE,
  VK_RIGHT,
  WM_KEYDOWN,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";

function seedHome(dir: string, appdata: string) {
  const pdf = makeBlankPdf();
  const paths = [join(dir, "one", "a.pdf"), join(dir, "one", "b.pdf"), join(dir, "two", "c.pdf")];
  mkdirSync(join(dir, "one"));
  mkdirSync(join(dir, "two"));
  mkdirSync(appdata);
  for (const path of paths) writeFileSync(path, pdf, "latin1");
  const states = paths.map((path, i) => `[\nFilePath = ${path}\nOpenCount = ${paths.length - i}\n]`);
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nRememberOpenedFiles = true\n" +
      "HomePageViewMode = thumbnails\nFilePicker = os\n" +
      `FileStates [\n${states.join("\n")}\n]\n`,
  );
  return paths[2]!;
}

type Pt = { x: number; y: number };

function center(rect: number[]): Pt {
  return { x: Math.floor(rect[0]! + rect[2]! / 2), y: Math.floor(rect[1]! + rect[3]! / 2) };
}

export async function testit(): Promise<void> {
  if (!IS_WIN && !USE_NG) return;
  const dir = mkdtempSync(tmpPath("issue-6597-"));
  try {
    const appdata = join(dir, "appdata");
    const remaining = seedHome(dir, appdata);
    await withControlledSumatra(
      EXE,
      async (client, proc) => {
        const canvas = USE_NG ? 0 : findCanvas(await waitForFrame(proc.pid));
        if (!USE_NG && !canvas) throw new Error("issue-6597: no Home canvas");

        const input = async (kind: string, a: number, b: number, c: number, d: number) => {
          const [, raw] = await client.request(ControlCommand.TestInput, [kind, a, b, c, d]);
          if (!String(raw).startsWith("OK")) throw new Error(`issue-6597: ${kind} failed ${raw}`);
        };
        const key = async (vk: number) => {
          if (USE_NG) await input("key", vk, 0, 0, 0);
          else sendMessage(canvas, WM_KEYDOWN, vk, 0);
        };
        const ctrlClick = async (pt: Pt) => {
          if (USE_NG) {
            await input("click", pt.x, pt.y, 0, 1);
            return;
          }
          const lp = packCoords(pt.x, pt.y);
          sendMessage(canvas, WM_LBUTTONDOWN, MK_CONTROL | MK_LBUTTON, lp);
          sendMessage(canvas, WM_LBUTTONUP, MK_CONTROL, lp);
        };
        async function outlineAt(index: number) {
          const home = await waitForHome(
            client,
            (h) => h.entries === 3 && h.sel === index && h.outline[2]! > 0,
            `issue-6597: no outline for thumbnail ${index}`,
          );
          return home.outlineFull;
        }
        const waitSelected = (indices: string, dragging: boolean) =>
          waitForHome(
            client,
            (h) => {
              const m = /^marquee=-?\d+,-?\d+,(\d+),(\d+) selected=([0-9,]*)$/m.exec(h.raw);
              return !!m && m[3] === indices && (Number(m[1]) > 0 && Number(m[2]) > 0) === dragging;
            },
            `issue-6597: expected selection ${indices || "empty"}${dragging ? " during drag" : ""}`,
          );
        const runMenu = async (cmd: string) => {
          const [, raw] = await client.request(ControlCommand.TestContextMenuAt, [0, 0, cmd]);
          if (!String(raw).startsWith("OK")) throw new Error(`issue-6597: ${cmd} failed ${raw}`);
          return String(raw);
        };

        const first = await outlineAt(0);
        await key(VK_RIGHT);
        const second = await outlineAt(1);
        await key(VK_RIGHT);
        const third = await outlineAt(2);
        await ctrlClick(center(first));
        await ctrlClick(center(second));
        await waitSelected("0,1,", false);
        await ctrlClick(center(second));
        await waitSelected("0,", false);
        await ctrlClick(center(second));
        await ctrlClick(center(third));
        await waitSelected("0,1,2,", false);
        const folders = await runMenu("CmdShowInFolder");
        if (folders !== "OK launchesBlocked=2") {
          throw new Error(`issue-6597: Show in Folder should open two directories for three files (${folders})`);
        }

        await key(VK_ESCAPE);
        const cleared = await waitSelected("", false);
        if (cleared.outline[2] !== 0) throw new Error("issue-6597: Escape left a selection outline");

        const start = { x: Math.floor((second[0]! + second[2]! + third[0]!) / 2), y: center(first).y - 15 };
        const end = { x: center(first).x, y: center(first).y + 15 };
        if (USE_NG) {
          await input("down", start.x, start.y, 0, 0);
          await input("move", end.x, end.y, 1, 0);
        } else {
          sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(start.x, start.y));
          sendMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON, packCoords(end.x, end.y));
        }
        await waitSelected("0,1,", true);
        if (USE_NG) await input("up", end.x, end.y, 0, 0);
        else sendMessage(canvas, WM_LBUTTONUP, 0, packCoords(end.x, end.y));
        await waitSelected("0,1,", false);

        await runMenu("CmdForgetSelectedDocument");
        await waitForHome(
          client,
          (h) => h.entries === 1 && h.path.toLowerCase() === remaining.toLowerCase(),
          "issue-6597: Remove from History did not remove exactly the selected files",
        );
      },
      ["-appdata", appdata, "-window-pos", "1060x720@20x20"],
    );
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
