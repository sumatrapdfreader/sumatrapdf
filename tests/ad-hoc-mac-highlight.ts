// Run against the ng debug build: bun tests/ad-hoc-mac-highlight.ts
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { connect, sendCommand, controlCommands as C } from "../cmd/ng-dbg-control.ts";

const enum Key {
  Back = 8,
  Escape = 27,
  End = 35,
  Delete = 46,
}
const shift = 2;

function readingPdf(): string {
  const text = "BT /F1 12 Tf 220 400 Td (Alpha beta gamma) Tj 0 70 Td (Commented highlight) Tj ET\n";
  const objects = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R 5 0 R] /Contents 6 0 R /Resources << /Font << /F1 7 0 R >> >> >>",
    "<< /Type /Annot /Subtype /Highlight /Rect [200 380 420 430] /QuadPoints [200 430 420 430 200 380 420 380] /C [1 1 0] >>",
    "<< /Type /Annot /Subtype /Highlight /Rect [200 450 420 500] /QuadPoints [200 500 420 500 200 450 420 450] /C [1 1 0] /Contents (Keep this comment) >>",
    `<< /Length ${text.length} >>\nstream\n${text}endstream`,
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
  ];
  let pdf = "%PDF-1.4\n";
  const offsets = [0];
  for (const [i, object] of objects.entries()) {
    offsets.push(pdf.length);
    pdf += `${i + 1} 0 obj\n${object}\nendobj\n`;
  }
  const xref = pdf.length;
  pdf += `xref\n0 ${offsets.length}\n0000000000 65535 f \n`;
  for (const offset of offsets.slice(1)) pdf += `${String(offset).padStart(10, "0")} 00000 n \n`;
  return `${pdf}trailer\n<< /Size ${offsets.length} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
}

async function testCase(mode: "reading" | "created" | "hover" | "main"): Promise<void> {
  const dir = mkdtempSync(join(tmpdir(), "sumatra-highlight-"));
  const fixture = mode === "reading" || mode === "created";
  const document = fixture ? join(dir, "reading.pdf") : resolve("ext/a-zlib/zlib.3.pdf");
  if (fixture) writeFileSync(document, readingPdf(), "latin1");
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nCheckForUpdates = false\n",
  );
  const pipe = `highlight-${process.pid}-${Date.now()}`;
  const child = spawn(
    resolve("out/mac/dbg/SumatraPDF"),
    ["-for-testing", "-appdata", dir, "-dbg-control", pipe, "-view", "single page", "-zoom", "100", document],
    { stdio: "ignore" },
  );
  const exited = new Promise<void>((r) => child.once("exit", () => r()));
  const socket = await connect(pipe, 15000);
  const request = async (cmd: string, ...args: (number | string)[]): Promise<string> => {
    let timer: ReturnType<typeof setTimeout>;
    const reply = await Promise.race([
      sendCommand(socket, C[cmd]!, args),
      new Promise<never>((_, reject) => {
        timer = setTimeout(() => reject(new Error(`${cmd}: timeout`)), 15000);
      }),
    ]).finally(() => clearTimeout(timer));
    assert.equal(reply[0], 0, `${cmd}: ${reply[1]}`);
    return String(reply[1] ?? "");
  };
  const key = async (vk: number, mods = 0): Promise<void> => {
    await request("TestInput", "key", vk, mods);
    await request("TestInput", "keyup", vk, mods);
  };
  const wait = async (cmd: string, pred: (s: string) => boolean | Promise<boolean>): Promise<string> => {
    const deadline = Date.now() + 5000;
    for (;;) {
      const state = await request(cmd);
      if (await pred(state)) return state;
      assert.ok(Date.now() < deadline, `${cmd}: ${state}`);
      await Bun.sleep(25);
    }
  };
  const select = async (): Promise<void> => {
    await request("TestInvokeCommand", "CmdSelectTextViaKeyboard");
    await key("V".charCodeAt(0));
    await key(Key.End);
    await wait("TestSelectTextKeyboard", (s) => /selRects=[1-9]/.test(s));
  };
  const closeList = async (): Promise<void> => {
    await wait("TestAnnotFilter", (s) => /pending=0/.test(s));
    await request("TestInvokeCommand", "CmdFindAnnotation");
    await wait("TestAnnotFilter", (s) => /floatVisible=0/.test(s));
    await wait("TestUiState", (s) => /frame=1/.test(s) && /edit=0/.test(s) && /popup=0 dialog=0 overlay=0/.test(s));
  };
  try {
    // Wait for the first layout before querying render idle (zoom starts at 0).
    await wait("TestUiState", (s) => Number(/zoom=([\d.]+)/.exec(s)?.[1]) > 0);
    await request("WaitRenderIdle");
    await request("SetNotificationsEnabled", 0);
    if (fixture) {
      await wait("TestPerfStats", (s) => /frames=[1-9]/.test(s));
      const canvas = (state: string): number[] => /canvas=(-?\d+),(-?\d+),/.exec(state)!.slice(1).map(Number);
      const top = canvas(await request("TestUiState"))[1];
      const point = async (x: number, y: number): Promise<number[]> => {
        const layout = await request("TestLayout");
        const page = /page n=1 .*screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(layout)!.slice(1).map(Number);
        const origin = canvas(await request("TestUiState"));
        const scale = Number(/canvasScale=([\d.]+)/.exec(layout)![1]);
        const dx = page[0]! + Math.round((x * page[2]!) / 612);
        const dy = page[1]! + Math.round(((792 - y) * page[3]!) / 792);
        return [Math.round(origin[0]! + dx * scale), Math.round(origin[1]! + dy * scale), dx, dy];
      };
      const visibility = async (x: number, y: number): Promise<string> => {
        const p = await point(x, y);
        return request("TestCommandVisibility", "CmdDeleteAnnotation", "menu", p[2]!, p[3]!);
      };
      // Annotation changes invalidate the page's glyph cache.
      const loadText = async (): Promise<void> => {
        await request("TestInvokeCommand", "CmdSelectTextViaKeyboard");
        await request("TestInvokeCommand", "CmdSelectTextViaKeyboard");
      };
      await loadText();
      if (mode === "created") {
        await select();
        const caret = /caret=(-?\d+),(-?\d+),(\d+),(\d+)/
          .exec(await request("TestSelectTextKeyboard"))!
          .slice(1)
          .map(Number);
        const x = caret[0]! - 10;
        const y = caret[1]! + Math.round(caret[3]! / 2);
        await key("A".charCodeAt(0));
        assert.match(await request("TestUiState"), /annotSel=1/, "A must select new markup in reading mode");
        await key(Key.Escape);
        let hitY = y;
        await wait("TestLayout", async () => {
          for (const dy of [0, -8, 8, -16, 16]) {
            if (/vis=show/.test(await request("TestCommandVisibility", "CmdDeleteAnnotation", "menu", x, y + dy))) {
              hitY = y + dy;
              return true;
            }
          }
          return false;
        });
        const scale = Number(/canvasScale=([\d.]+)/.exec(await request("TestLayout"))![1]);
        const origin = canvas(await request("TestUiState"));
        await request(
          "TestInput",
          "click",
          Math.round(origin[0]! + x * scale),
          Math.round(origin[1]! + hitY * scale),
          0,
          0,
        );
        assert.match(await request("TestUiState"), /annotSel=1/, "New markup must be clickable after deselection");
        await key(Key.Back);
        await wait("TestLayout", async () => /vis=show/.test(await visibility(400, 475)));
        assert.match(await visibility(400, 405), /vis=show/, "The original highlight must remain");
        await request("TestInvokeCommand", "CmdFindAnnotation");
        await wait("TestAnnotFilter", (s) => /pending=0/.test(s));
        assert.match(await request("TestAnnotFilter"), /nAll=2\b/, "Only the newly clicked markup must be deleted");
        console.log("mac-highlight: OK (new markup click/delete without Edit PDF)");
        return;
      }
      const start = await point(225, 405);
      const end = await point(340, 405);
      await request("TestInput", "down", start[0]!, start[1]!, 0, 0);
      await request("TestInput", "move", end[0]!, end[1]!, 1, 0);
      await request("TestInput", "up", end[0]!, end[1]!, 0, 0);
      assert.match(
        await request("TestUiState"),
        /selection=1 annotSel=0/,
        "Dragging over a highlight must select text",
      );
      await key(Key.Escape);
      for (const y of [405, 475]) {
        const p = await point(310, y);
        await request("TestInput", "click", p[0]!, p[1]!, 0, 0);
        assert.match(await request("TestUiState"), /annotSel=1/, "Reading-mode clicks must select existing highlights");
        assert.equal(canvas(await request("TestUiState"))[1], top, "Clicking must not enable Edit PDF mode");
        if (y === 475) {
          await key(Key.Escape);
          assert.match(
            await request("TestUiState"),
            /annotSel=1/,
            "Closing the comment must keep its highlight selected",
          );
        }
        await key(Key.Back);
        if (y === 405) {
          await wait("TestLayout", async () => /vis=show/.test(await visibility(310, 475)));
        }
        assert.match(
          await visibility(310, y),
          /vis=disable/,
          "Delete must remove the clicked highlight in reading mode",
        );
      }
      await request("TestInvokeCommand", "CmdFindAnnotation");
      await wait("TestAnnotFilter", (s) => /pending=0/.test(s));
      assert.match(await request("TestAnnotFilter"), /nAll=0\b/, "Both clicked highlights must be deleted");
      console.log("mac-highlight: OK (reading-mode click/delete, comment, text drag)");
      return;
    }
    await request("TestInvokeCommand", "CmdCreateAnnotText", 350, 300);
    await request("TestInvokeCommand", "CmdCreateAnnotStamp", 550, 300);
    await request("WaitRenderIdle");
    await wait("TestUiState", async (state) => {
      const canvas = /canvas=(-?\d+),(-?\d+),/.exec(state)!;
      await request("TestInput", "move", 350 + Number(canvas[1]), 300 + Number(canvas[2]), 0, 0);
      return /vis=show/.test(await request("TestCommandVisibility", "CmdDeleteAnnotation", "menu", 0, 0));
    });
    if (mode === "hover") {
      assert.match(
        await request("TestCommandVisibility", "CmdDeleteAnnotation", "menu", 800, 600),
        /vis=disable/,
        "An explicit empty point must not reuse the previous hover target",
      );
      await request("TestInvokeCommand", "CmdDeleteAnnotation");
      await request("TestInvokeCommand", "CmdFindAnnotation");
      const rows = await request("TestAnnotFilter");
      assert.match(rows, /nAll=1\b/, "An explicit delete command must remove the hovered note");
      assert.doesNotMatch(rows, /page=1 Text/, "The hovered text note must be deleted");
      assert.match(rows, /page=1 Stamp/, "The other annotation must remain");
      console.log("mac-highlight: OK (hover delete command)");
      return;
    }
    await request("TestInvokeCommand", "CmdDeleteAnnotation", 550, 300);
    await request("TestInvokeCommand", "CmdFindAnnotation");
    const rows = await request("TestAnnotFilter");
    assert.match(rows, /nAll=1\b/, "Only the explicit target must be deleted");
    assert.doesNotMatch(rows, /Stamp/, "The explicit stamp target must win over the hovered text note");
    await closeList();
    await key(Key.Escape);
    await wait("TestUiState", (s) => /annotSel=0/.test(s));
    await key(Key.Back);
    assert.match(await request("TestAnnotFilter"), /nAll=1\b/, "Delete must not remove an unselected hover target");
    // Undo the deletion and the two creations to leave a clean document.
    for (let i = 0; i < 3; i++) await request("TestInvokeCommand", "CmdUndo");
    await wait("TestAnnotFilter", (s) => /nAll=0\b/.test(s));
    await request("TestInvokeCommand", "CmdToggleEditPDF");

    await select();
    await key("A".charCodeAt(0));
    const state = await wait("TestUiState", (s) => /selection=0/.test(s));
    const canvas = /canvas=(-?\d+),(-?\d+),/.exec(state)!;
    await request("TestInput", "move", Number(canvas[1]) + 5, Number(canvas[2]) + 5, 0, 0);
    for (const surface of ["menu", "palette"]) {
      assert.match(
        await request("TestCommandVisibility", "CmdDeleteAnnotation", surface, 0, 0),
        /vis=show/,
        "Selected highlights must be deletable without hovering over them",
      );
    }
    await request("TestInvokeCommand", "CmdProperties");
    await wait("TestUiState", (s) => /dialog=1/.test(s));
    await key(Key.Back);
    await key(Key.Delete);
    await request("TestInvokeCommand", "CmdDeleteAnnotation");
    assert.match(await request("TestUiState"), /annotSel=1/, "Delete must not remove annotations behind a dialog");
    await key(Key.Escape);
    await wait("TestUiState", (s) => /dialog=0/.test(s));
    await request("TestInvokeCommand", "CmdFindAnnotation");
    assert.match(await request("TestAnnotFilter"), /nAll=1\b/, "A must create one highlight");
    await closeList();
    await request("TestInvokeCommand", "CmdToggleEditPDF");

    await select();
    await request("TestInvokeCommand", "CmdProperties");
    await wait("TestUiState", (s) => /dialog=1/.test(s));
    await key("A".charCodeAt(0));
    assert.match(await request("TestUiState"), /selection=1/, "A must not annotate behind a dialog");
    await key(Key.Escape);
    await wait("TestUiState", (s) => /dialog=0/.test(s));
    await key("A".charCodeAt(0), shift);
    await wait("TestUiState", (s) => /selection=0/.test(s) && /edit=1/.test(s));
    await key(Key.Back);
    assert.match(await request("TestUiState"), /edit=1/, "Backspace must not delete an empty annotation");
    await key("A".charCodeAt(0));
    await request("TestInput", "char", "a".charCodeAt(0));
    assert.match(await request("TestUiState"), /editText='a'/, "A must type in the annotation editor");
    await key(Key.Escape);
    await request("TestInvokeCommand", "CmdFindAnnotation");
    assert.match(await request("TestAnnotFilter"), /nAll=2\b/, "Shift+A must create exactly one highlight");
    await closeList();
    await key(Key.Back); // macOS Delete is Backspace.
    assert.match(await request("TestAnnotFilter"), /nAll=1\b/, "Delete must remove the selected highlight");
    await request("TestInvokeCommand", "CmdCreateAnnotSquare openedit", 300, 300);
    await wait("TestUiState", (s) => /edit=1/.test(s));
    await key(Key.Back);
    assert.match(await request("TestUiState"), /edit=1/, "An empty square comment must not delete the square");
    await key(Key.Escape);
    await key(Key.Back);
    assert.match(await request("TestAnnotFilter"), /nAll=1\b/, "Delete must remove only the selected square");
    await request("TestInvokeCommand", "CmdCreateAnnotStamp openedit", 550, 300);
    await wait("TestUiState", (s) => /edit=1/.test(s));
    await key(Key.Escape);
    await request("TestInvokeCommand", "CmdDeleteAnnotation", 0, 0);
    assert.match(
      await request("TestAnnotFilter"),
      /nAll=2\b/,
      "An empty explicit target must not delete the selection",
    );
    await key(Key.Back);
    assert.match(await request("TestAnnotFilter"), /nAll=1\b/, "Delete must still remove the selected annotation");
    console.log("mac-highlight: OK (shortcuts, dialog/input guards, selected/explicit deletion)");
  } finally {
    await request("Quit").catch(() => child.kill());
    socket.destroy();
    await exited;
    rmSync(dir, { recursive: true, force: true });
  }
}

export async function testit(): Promise<void> {
  if (process.platform !== "darwin") {
    console.log("mac-highlight: skipped (macOS only)");
    return;
  }
  await testCase("reading");
  await testCase("created");
  await testCase("hover");
  await testCase("main");
}

if (import.meta.main) await testit();
