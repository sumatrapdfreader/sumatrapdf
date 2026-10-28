import { copyFileSync, existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { websiteDocsDir } from "../cmd/util.ts";
import { IS_MAC } from "./host.ts";
import { cmdId, EXE, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import {
  enumChildWindows,
  enumWindows,
  getClassName,
  getControlText,
  getWindowPid,
  getWindowLong,
  getWindowRect,
  getWindowText,
  isWindowVisible,
  sendText,
  captureWindowDCRegionPixels,
} from "./winapi.ts";

const PDF = resolve(import.meta.dir, "issue-1189.pdf");
const IMAGE = resolve(import.meta.dir, "issue-6236.jpg");
const SCRIPTS = resolve(import.meta.dir, "../src/pdf-scripts");

async function checkExamples(): Promise<void> {
  const dir = tmpPath("pdf-script-examples");
  mkdirSync(dir, { recursive: true });
  const websiteScripts = join(websiteDocsDir, "scripts");
  const useWebsite = existsSync(websiteScripts);
  for (const name of readdirSync(SCRIPTS).filter((name) => name.endsWith(".js"))) {
    const output = join(dir, `${name}.${name === "draw-graphics-with-device.js" ? "png" : "pdf"}`);
    const extra = name === "copy-outline.js" || name === "merge-pdfs.js" ? PDF : IMAGE;
    const source = join(useWebsite ? websiteScripts : SCRIPTS, name);
    let args = [PDF, output, extra];
    if (useWebsite) {
      if (name === "create-a-pdf.js" || name === "create-pdf-low-level.js") args = [output];
      if (name === "create-pdf.js" || name === "draw-graphics-with-device.js") args = [output, IMAGE];
      if (name === "convert-pdf-to-images.js") args = [PDF, output + "-[page].png"];
    }
    if (name === "trace-device.js") args = useWebsite ? [PDF, "1"] : [PDF, "", "1"];
    const proc = Bun.spawn([EXE, "run", source, ...args], {
      stdout: "pipe",
      stderr: "pipe",
    });
    const [code, stdout, stderr] = await Promise.all([
      proc.exited,
      new Response(proc.stdout).text(),
      new Response(proc.stderr).text(),
    ]);
    if (code !== 0) throw new Error(`${name}: exit ${code}\n${stdout}\n${stderr}`);
    if (useWebsite && name === "trace-device.js") {
      const invalid = Bun.spawnSync([EXE, "run", source, PDF, "0"]);
      if (invalid.exitCode === 0) throw new Error("trace-device.js ignored its page-number argument");
    }
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

// position in the embedded bundle; gen-pdf-scripts.ts orders it, so adding a script shifts indices
function exampleIndex(info: string): number {
  const scripts = readFileSync(resolve(import.meta.dir, "../.work/scripts.js"), "utf8").split("\n// ---\n");
  const idx = scripts.findIndex((s) => s.includes(`// Info: ${info}\n`));
  if (idx < 0) throw new Error(`no example script '${info}'`);
  return idx;
}

type Node = { x: number; y: number; w: number; h: number; role: number; label: string; value: string };

function nodes(raw: string): Node[] {
  const re = /node rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+) role=(\d+) label='(.*?)' value='([\s\S]*?)'\n/g;
  return [...raw.matchAll(re)].map((m) => ({
    x: Number(m[1]) + Number(m[3]) / 2,
    y: Number(m[2]) + Number(m[4]) / 2,
    w: Number(m[3]),
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
  const editorWidth = (await layout()).find((n) => n.role === 31)!.w;
  return {
    async argumentIds() {
      return [] as number[];
    },
    async argumentRowGap() {
      const fields = (await layout()).filter((n) => n.role === 17);
      return (fields[1]!.y - fields[0]!.y - fields[0]!.h) / fields[0]!.h;
    },
    async argumentGap() {
      const list = await layout();
      const fields = list.filter((n) => n.role === 17);
      const selector = list.find((n) => n.role === 31)!;
      const last = fields[fields.length - 1]!;
      // Add argument sits under the last row; the gap is what follows it
      const add = list.find((n) => n.label === "Add argument");
      const bottom = add ? add.y + add.h / 2 : last.y + last.h / 2;
      return (selector.y - selector.h / 2 - bottom) / last.h;
    },
    async argumentHeight() {
      const list = await layout();
      // the editor's top: it shrinks once there is output, which moves its center up
      const editor = list.find((n) => n.role === 31)!;
      return editor.y - editor.h / 2 - list.find((n) => n.role === 17)!.y;
    },
    async checkBorder() {},
    async add() {
      await click((await layout()).find((n) => n.label === "Add argument")!);
    },
    async remove() {
      const buttons = (await layout()).filter((n) => n.label === "Remove");
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
    async examples() {
      await click((await layout()).find((n) => n.label === "Example Scripts")!);
    },
    async pickExample(name: string) {
      name = name.replace(/\.$/, "");
      const list = await pollUntil(
        layout,
        (list) => list.some((n) => n.label === name) && list.find((n) => n.role === 31)?.w === editorWidth,
        {
          error: (list) =>
            `script list not shown: ${list
              .map((n) => n.label)
              .filter(Boolean)
              .join("; ")}`,
        },
      );
      await click(list.find((n) => n.label === name)!);
    },
    async script() {
      return (await layout()).find((n) => n.role === 31)?.value ?? "";
    },
  };
}

async function originalUi(pid: number, client: ControlClient) {
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
  // TestPdfScript does what the drawn buttons do. Its reply has the Add
  // argument button's screen rect.
  const scriptCmd = async (action: string, idx = -1) => {
    const res = await client.request(ControlCommand.TestPdfScript, [action, idx]);
    const raw = String(res[1] ?? "");
    if (res[0] !== 0) throw new Error(`TestPdfScript ${action}: ${raw}`);
    return raw;
  };
  const argEdits = () => edits().filter((h) => !(getWindowLong(h, -16) & 4));
  const scriptEdit = () => edits().find((h) => getWindowLong(h, -16) & 4 && !(getWindowLong(h, -16) & 0x800))!;
  const outputEdit = () => edits().find((h) => getWindowLong(h, -16) & 0x800)!;
  return {
    async argumentIds() {
      return argEdits();
    },
    async argumentRowGap() {
      const fields = argEdits().map(getWindowRect);
      return (fields[1]!.top - fields[0]!.bottom) / (fields[0]!.bottom - fields[0]!.top);
    },
    async argumentGap() {
      const fields = argEdits();
      const last = getWindowRect(fields[fields.length - 1]!);
      // Add argument sits under the last row; the gap is what follows it
      const add = /add=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(await scriptCmd("state"));
      if (!add) throw new Error("no Add argument rect");
      const addBottom = Number(add[2]) + Number(add[4]);
      return (getWindowRect(scriptEdit()).top - addBottom) / (last.bottom - last.top);
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
      return getWindowRect(scriptEdit()).top - getWindowRect(argEdits()[0]!).top;
    },
    async add() {
      await scriptCmd("add");
    },
    async remove() {
      await scriptCmd("remove");
    },
    async setArg(text: string) {
      const fields = argEdits();
      sendText(fields[fields.length - 1]!, text);
    },
    async setScript(text: string) {
      sendText(scriptEdit(), text);
    },
    async run() {
      await scriptCmd("run");
    },
    async output() {
      const h = outputEdit();
      return isWindowVisible(h) ? getControlText(h) : "";
    },
    async args() {
      return argEdits().map((h) => getControlText(h));
    },
    // the list's buttons and rows are drawn, not windows: the control channel
    // does what clicking them does
    async examples() {
      await scriptCmd("examples");
    },
    async pickExample(name: string) {
      await scriptCmd("pick", exampleIndex(name));
    },
    async script() {
      return getControlText(scriptEdit());
    },
  };
}

export async function testit(): Promise<void> {
  await checkExamples();
  const input = tmpPath("pdf-script-hints/A 'quoted' file.v2.pdf");
  const stem = input.slice(0, -".pdf".length);
  mkdirSync(dirname(input), { recursive: true });
  copyFileSync(PDF, input);
  writeFileSync(stem + ".1.pdf", "reserved");
  writeFileSync(stem + ".2.pdf", "reserved");
  const { proc, client, frame } = await launchControlled([input]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdRunPDFScript"));
    const ui = USE_NG ? await ngUi(client) : await originalUi(proc.pid!, client);
    if (existsSync(join(websiteDocsDir, "scripts"))) {
      if (!(await ui.script()).includes("\n")) throw new Error("initial script lost its line breaks");
      if (!(await ui.script()).startsWith("// Info: Print the page count of a PDF.")) {
        throw new Error("reading script metadata changed the source");
      }
      const firstArgs = await ui.args();
      if (firstArgs.length !== 1 || firstArgs[0] !== input)
        throw new Error("initial example needs only its input argument");
      // the usual dialog run: one input path and no output argument
      await ui.run();
      await pollUntil(
        () => ui.output(),
        (s) => s.trim() === "1",
        { timeoutMs: 10000, error: (s) => `one-argument script: ${JSON.stringify(s)}` },
      );
      await ui.examples();
      await ui.pickExample("Add a free-text annotation to the first page.");
      await pollUntil(
        () => ui.script(),
        (s) => s.includes("Add a free-text annotation to the first page."),
        {
          error: (s) => `script list selection did not load the script: ${JSON.stringify(s)}`,
        },
      );
      if (!(await ui.script()).startsWith("// Info:")) throw new Error("example script starts with empty lines");
      // a label row over each field would put a field's height between them
      if ((await ui.argumentRowGap()) > 1) throw new Error("argument fields still have label rows");
      const argumentIds = (await ui.argumentIds()).slice(0, 1);
      await ui.examples();
      await ui.add();
      await pollUntil(
        () => ui.args(),
        (args) => args.length === 3,
        { error: "extra argument before changing example" },
      );
      await ui.setArg("discard this value");
      await ui.examples();
      await ui.pickExample("Render every page as a PNG at 96 DPI.");
      await pollUntil(
        () => ui.args(),
        (args) => args.length === 2 && args[1] === join(dirname(input), basename(stem) + "-[page].png"),
        {
          error: (args) => `DIR and BASE hints: ${JSON.stringify(args)}`,
        },
      );
      if (JSON.stringify((await ui.argumentIds()).slice(0, 1)) !== JSON.stringify(argumentIds)) {
        throw new Error("selecting an example recreated its first argument control");
      }
      await ui.pickExample("Copy bookmarks from another document.");
      await pollUntil(
        () => ui.args(),
        (args) => args.length === 3 && args[2] === "",
        {
          error: (args) => `empty hinted argument: ${JSON.stringify(args)}`,
        },
      );
      await ui.pickExample("Create a PDF with one blank page.");
      await pollUntil(
        () => ui.args(),
        (args) => args.length === 1 && args[0] === stem + ".3.pdf",
        {
          error: (args) => `output-only example: ${JSON.stringify(args)}`,
        },
      );
      await ui.pickExample("Crop page margins using rendered content bounds.");
      await pollUntil(
        () => ui.args(),
        (args) => args.length === 4 && args[2] === "6" && args[3] === "all",
        {
          error: (args) => `numeric and text defaults: ${JSON.stringify(args)}`,
        },
      );
      await ui.pickExample("Add a free-text annotation to the first page.");
      await pollUntil(
        () => ui.args(),
        (args) => args.length === 2,
        { error: "argument rows did not shrink" },
      );
      await ui.examples();
    } else {
      if ((await ui.args()).length || (await ui.script()).length)
        throw new Error("unexpected examples without the website checkout");
      for (const value of [input, stem + ".3.pdf"]) {
        const count = (await ui.args()).length + 1;
        await ui.add();
        await pollUntil(
          () => ui.args(),
          (args) => args.length === count,
          { error: "manual argument without examples" },
        );
        await ui.setArg(value);
      }
    }
    const args = await ui.args();
    const initialHeight = await ui.argumentHeight();
    const gap = await ui.argumentGap();
    const maxGapRows = 2;
    if (gap > maxGapRows) throw new Error(`argument area leaves ${gap.toFixed(2)} row heights after its last row`);
    if (args.length !== 2 || args[0] !== input || args[1] !== stem + ".3.pdf" || existsSync(args[1]!)) {
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
    console.log("pdf-script: argument hints, rebuilt rows, unique paths, defaults, stdout and stderr passed");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
