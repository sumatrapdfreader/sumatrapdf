import { existsSync, mkdirSync, readdirSync } from "node:fs";
import { join, resolve } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { cmdId, EXE, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { clickAt, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import {
  enumChildWindows,
  enumWindows,
  getClassName,
  getClientRect,
  getControlText,
  getWindowPid,
  getWindowLong,
  getWindowRect,
  getWindowText,
  isWindowVisible,
  sendText,
  clientToScreen,
  captureWindowDCRegionPixels,
} from "./winapi.ts";

const PDF = resolve(import.meta.dir, "issue-1189.pdf");
const IMAGE = resolve(import.meta.dir, "issue-6236.jpg");
const SCRIPTS = resolve(import.meta.dir, "../src/pdf-scripts");

async function checkExamples(): Promise<void> {
  const dir = tmpPath("pdf-script-examples");
  mkdirSync(dir, { recursive: true });
  for (const name of readdirSync(SCRIPTS).filter((name) => name.endsWith(".js"))) {
    const output = join(dir, `${name}.pdf`);
    const extra = name === "copy-outline.js" || name === "merge-pdfs.js" ? PDF : IMAGE;
    const proc = Bun.spawn([EXE, "run", join(SCRIPTS, name), PDF, output, extra], {
      stdout: "pipe",
      stderr: "pipe",
    });
    const [code, stdout, stderr] = await Promise.all([
      proc.exited,
      new Response(proc.stdout).text(),
      new Response(proc.stderr).text(),
    ]);
    if (code !== 0) throw new Error(`${name}: exit ${code}\n${stdout}\n${stderr}`);
    if (name === "convert-pdf-to-images.js" && !existsSync(output + "-1.png")) {
      throw new Error(`${name}: no rendered image`);
    }
    if (
      !name.startsWith("load-") &&
      !name.includes("plain-text") &&
      !name.includes("trace-") &&
      !name.includes("to-images") &&
      !existsSync(output)
    ) {
      throw new Error(`${name}: no output file`);
    }
  }
}

type Node = { x: number; y: number; h: number; role: number; label: string; value: string };

function nodes(raw: string): Node[] {
  const re = /node rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+) role=(\d+) label='(.*?)' value='(.*?)'\n/g;
  return [...raw.matchAll(re)].map((m) => ({
    x: Number(m[1]) + Number(m[3]) / 2,
    y: Number(m[2]) + Number(m[4]) / 2,
    h: Number(m[4]),
    role: Number(m[5]),
    label: m[6]!,
    value: m[7]!,
  }));
}

async function ngUi(client: ControlClient) {
  const request = async (...args: (string | number)[]) =>
    String((await client.request(ControlCommand.TestToolWindow, args))[1] ?? "");
  const layout = async () => nodes(await request("layout", "pdfscript"));
  const click = async (node: Node) =>
    request("input", "pdfscript", "click", Math.round(node.x), Math.round(node.y), 0, 0);
  await pollUntil(layout, (list) => list.some((n) => n.label === "Run Script"), { error: "script dialog" });
  return {
    async argumentGap() {
      const list = await layout();
      const fields = list.filter((n) => n.role === 17);
      const selector = list.find((n) => n.role === 56)!;
      const last = fields[fields.length - 1]!;
      return (selector.y - selector.h / 2 - (last.y + last.h / 2)) / last.h;
    },
    async argumentHeight() {
      const list = await layout();
      return list.find((n) => n.role === 56)!.y - list.find((n) => n.role === 17)!.y;
    },
    async checkBorder() {},
    async add() {
      await click((await layout()).find((n) => n.label === "Add argument")!);
    },
    async remove() {
      const buttons = (await layout()).filter((n) => n.label === "x");
      await click(buttons[buttons.length - 1]!);
    },
    async setArg(text: string) {
      const fields = (await layout()).filter((n) => n.role === 17);
      await click(fields[fields.length - 1]!);
      await request("input", "pdfscript", "key", 65, IS_MAC ? 16 : 1, 0, 0);
      for (const ch of text) await request("input", "pdfscript", "char", ch.codePointAt(0)!, 0, 0, 0);
    },
    async setScript(text: string) {
      const field = (await layout()).find((n) => n.role === 31);
      if (!field) throw new Error("no script editor");
      await click(field);
      await request("input", "pdfscript", "key", 65, IS_MAC ? 16 : 1, 0, 0);
      for (const ch of text) await request("input", "pdfscript", "char", ch.codePointAt(0)!, 0, 0, 0);
    },
    async run() {
      const button = (await layout()).find((n) => n.label === "Run Script");
      if (!button) throw new Error("no Run Script button");
      await click(button);
    },
    async output() {
      const fields = (await layout()).filter((n) => n.role === 31);
      return fields[1]?.value ?? "";
    },
    async args() {
      return (await layout()).filter((n) => n.role === 17).map((n) => n.value);
    },
  };
}

async function originalUi(pid: number) {
  let dialog = 0;
  await pollUntil(
    () => {
      enumWindows((h) => {
        // created hidden and shown once every control exists
        if (getWindowPid(h) === pid && getWindowText(h) === "Run PDF Script" && isWindowVisible(h)) dialog = h;
        return true;
      });
      return dialog;
    },
    Boolean,
    { error: "script dialog" },
  );
  const edits = () => {
    const list: number[] = [];
    enumChildWindows(dialog, (h) => {
      if (getClassName(h) === "Edit") list.push(h);
      return true;
    });
    return list;
  };
  const argEdits = () => edits().filter((h) => !(getWindowLong(h, -16) & 4));
  const scriptEdit = () => edits().find((h) => getWindowLong(h, -16) & 4 && !(getWindowLong(h, -16) & 0x800))!;
  const outputEdit = () => edits().find((h) => getWindowLong(h, -16) & 0x800)!;
  return {
    async argumentGap() {
      let combo = 0;
      enumChildWindows(dialog, (h) => {
        if (getClassName(h) === "ComboBox") combo = h;
        return true;
      });
      const fields = argEdits();
      const last = getWindowRect(fields[fields.length - 1]!);
      return (getWindowRect(combo).top - last.bottom) / (last.bottom - last.top);
    },
    async checkBorder() {
      const fields = argEdits();
      const outer = getWindowRect(dialog);
      const sample = (h: number) => {
        const rc = getWindowRect(h);
        const pixels = captureWindowDCRegionPixels(
          dialog,
          rc.left - outer.left,
          Math.floor((rc.top + rc.bottom) / 2) - outer.top,
          1,
          1,
        );
        if (!pixels) throw new Error("argument border capture failed");
        return [...pixels.slice(0, 3)].join(",");
      };
      const initial = sample(fields[0]!);
      const added = sample(fields[fields.length - 1]!);
      if (initial !== added) throw new Error(`argument borders differ: initial=${initial}, added=${added}`);
    },
    async argumentHeight() {
      let combo = 0;
      enumChildWindows(dialog, (h) => {
        if (getClassName(h) === "ComboBox") combo = h;
        return true;
      });
      return getWindowRect(combo).top - getWindowRect(argEdits()[0]!).top;
    },
    async add() {
      let combo = 0;
      enumChildWindows(dialog, (h) => {
        if (getClassName(h) === "ComboBox") combo = h;
        return true;
      });
      const origin = clientToScreen(dialog, 0, 0);
      await clickAt(dialog, getClientRect(dialog).right - 40, getWindowRect(combo).top - origin.y - 20, 0);
    },
    async remove() {
      const fields = argEdits();
      const rc = getWindowRect(fields[fields.length - 1]!);
      const origin = clientToScreen(dialog, 0, 0);
      await clickAt(dialog, getClientRect(dialog).right - 26, (rc.top + rc.bottom) / 2 - origin.y, 0);
    },
    async setArg(text: string) {
      const fields = argEdits();
      sendText(fields[fields.length - 1]!, text);
    },
    async setScript(text: string) {
      sendText(scriptEdit(), text);
    },
    async run() {
      const rc = getClientRect(dialog);
      await clickAt(dialog, rc.right - 50, rc.bottom - 20, 0);
    },
    async output() {
      const h = outputEdit();
      return isWindowVisible(h) ? getControlText(h) : "";
    },
    async args() {
      return argEdits().map((h) => getControlText(h));
    },
  };
}

export async function testit(): Promise<void> {
  await checkExamples();
  const { proc, client, frame } = await launchControlled([PDF]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdRunPDFScript"));
    const ui = USE_NG ? await ngUi(client) : await originalUi(proc.pid!);
    const args = await ui.args();
    const initialHeight = await ui.argumentHeight();
    const gap = await ui.argumentGap();
    const maxGapRows = 2;
    if (gap > maxGapRows) throw new Error(`argument area leaves ${gap.toFixed(2)} row heights after its last row`);
    if (args.length !== 2 || args[0] !== PDF || args[1] === PDF || existsSync(args[1]!)) {
      throw new Error(`bad default arguments: ${JSON.stringify(args)}`);
    }
    await ui.setScript(
      'print("stdout-ok", scriptArgs.length, "<INPUT>" === scriptArgs[0], "<OUTPUT>" === scriptArgs[1]);',
    );
    await ui.run();
    await pollUntil(
      () => ui.output(),
      (s) => s.includes("stdout-ok 2 true true"),
      { timeoutMs: 10000, error: "captured stdout" },
    );
    await ui.setScript('throw new Error("stderr-ok");');
    await ui.run();
    await pollUntil(
      () => ui.output(),
      (s) => s.includes("stderr-ok") && s.includes("code 1"),
      { timeoutMs: 10000, error: "captured stderr" },
    );
    await ui.add();
    await pollUntil(
      () => ui.args(),
      (args) => args.length === 3,
      { error: "added argument" },
    );
    await ui.checkBorder();
    await pollUntil(
      () => ui.argumentHeight(),
      (height) => height > initialHeight,
      { error: "argument area did not grow" },
    );
    await ui.setScript('print("empty-arg", scriptArgs.length, scriptArgs[2] === "");');
    await ui.run();
    await pollUntil(
      () => ui.output(),
      (s) => s.includes("empty-arg 3 true"),
      { error: (s) => `empty argument: ${s}` },
    );
    const extra = '日本語 "quote" and \\backslash\\';
    await ui.setArg(extra);
    await ui.setScript(`print("quoted-arg", scriptArgs[2] === ${JSON.stringify(extra)});`);
    await ui.run();
    await pollUntil(
      () => ui.output(),
      (s) => s.includes("quoted-arg true"),
      { error: "quoted argument" },
    );
    await ui.remove();
    await pollUntil(
      () => ui.args(),
      (args) => args.length === 2,
      { error: "deleted argument" },
    );
    await pollUntil(
      () => ui.argumentHeight(),
      (height) => height === initialHeight,
      { error: "argument area did not shrink" },
    );
    console.log("pdf-script: examples, defaults, placeholders, stdout and stderr passed");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
