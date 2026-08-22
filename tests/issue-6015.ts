// discussion #6015: SelectionHandlers ${selectionPosition} is the current
// selection's bounding box in screen pixels (x,y,dx,dy), so a helper can sit
// next to the selection. SelectionToolbarLayout hides and reorders the
// floating selection toolbar's built-in buttons.
//
// Run: bun tests/issue-6015.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { ControlCommand } from "./control.ts";
import { findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";
import { getWindowRect } from "./winapi.ts";

function makeTextPdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const line = "The quick brown fox jumps over the lazy dog";
  const body: Record<number, Buffer> = {};
  body[1] = enc("<< /Type /Catalog /Pages 2 0 R >>");
  body[2] = enc("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
  body[6] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");
  const stream = `BT /F1 18 Tf 72 700 Td (${line}) Tj ET`;
  body[10] = enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`);
  body[3] = enc(
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
      `/Resources << /Font << /F1 6 0 R >> >> /Contents 10 0 R >>`,
  );
  const maxN = 12;
  const parts: Buffer[] = [enc("%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0]!.length;
  for (let n = 1; n <= maxN; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n] ?? enc("null"), enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (let n = 1; n <= maxN; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

function writeAppData(dir: string, extra: string[] = []): string {
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    [
      "RestoreSession = false",
      "CheckForUpdates = false",
      "ShowToc = false",
      "SelectionToolbar = true",
      ...extra,
      "",
    ].join("\n"),
  );
  return dir;
}

function parseToolbarCmds(raw: string): number[] {
  const cmds: number[] = [];
  for (const line of raw.split("\n")) {
    const m = /^cmd=(-?\d+)$/.exec(line.trim());
    if (m) {
      cmds.push(+m[1]!);
    }
  }
  return cmds;
}

function parseExpanded(raw: string): string {
  const m = /^expanded=(.*)$/m.exec(raw);
  if (!m) {
    throw new Error(`issue-6015: no expanded= in:\n${raw}`);
  }
  return m[1]!.trim();
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6015.pdf");
  writeFileSync(pdf, makeTextPdf());

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();

    const emptyRaw = String(
      (await client.request(ControlCommand.TestSelectionVars, ["pos=${selectionPosition}"]))[1] ?? "",
    );
    const empty = parseExpanded(emptyRaw);
    if (empty !== "pos=") {
      throw new Error(`issue-6015: expected empty position before selection, got '${empty}'\n${emptyRaw}`);
    }

    sendCommandSync(frame, cmdId("CmdSelectAll"));
    await client.waitForRenderIdle();

    const res = await client.request(ControlCommand.TestSelectionVars, ["${selectionPosition}"]);
    const raw = String(res[1] ?? "");
    const pos = parseExpanded(raw);
    const m = /^(-?\d+),(-?\d+),(-?\d+),(-?\d+)$/.exec(pos);
    if (!m) {
      throw new Error(`issue-6015: expected x,y,dx,dy, got '${pos}'\n${raw}`);
    }
    const x = +m[1]!;
    const y = +m[2]!;
    const dx = +m[3]!;
    const dy = +m[4]!;
    if (dx < 8 || dy < 8) {
      throw new Error(`issue-6015: selection rect too small ${pos}\n${raw}`);
    }

    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-6015: canvas not found");
    }
    const rc = getWindowRect(canvas);
    if (x + dx < rc.left || x > rc.right || y + dy < rc.top || y > rc.bottom) {
      throw new Error(
        `issue-6015: ${pos} does not overlap canvas ${rc.left},${rc.top},${rc.right},${rc.bottom}\n${raw}`,
      );
    }

    const caseRes = await client.request(ControlCommand.TestSelectionVars, ["${SelectionPosition}"]);
    if (parseExpanded(String(caseRes[1] ?? "")) !== pos) {
      throw new Error(`issue-6015: ${"${SelectionPosition}"} should match case-insensitively\n${raw}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  const copyId = cmdId("CmdCopySelection");
  const highlightId = cmdId("CmdCreateAnnotHighlight");
  const underlineId = cmdId("CmdCreateAnnotUnderline");

  {
    const dir = writeAppData(tmpPath("issue-6015-default"));
    const { proc: p0, client: c0 } = await launchControlled(["-appdata", dir, pdf]);
    try {
      await c0.waitForRenderIdle();
      const dump = String((await c0.request(ControlCommand.TestSelectionToolbar, []))[1] ?? "");
      const cmds = parseToolbarCmds(dump);
      const copyAt = cmds.indexOf(copyId);
      const hlAt = cmds.indexOf(highlightId);
      if (copyAt < 0 || hlAt < 0) {
        throw new Error(`issue-6015: default layout missing Copy or Highlight:\n${dump}`);
      }
      if (copyAt >= hlAt) {
        throw new Error(`issue-6015: default layout should list Copy before Highlight:\n${dump}`);
      }
    } finally {
      c0.close();
      await killAndWait(p0);
    }
  }

  {
    const dir = writeAppData(tmpPath("issue-6015-layout"), [
      "SelectionToolbarLayout = CmdCreateAnnotUnderline CmdCopySelection",
    ]);
    const { proc: p1, client: c1 } = await launchControlled(["-appdata", dir, pdf]);
    try {
      await c1.waitForRenderIdle();
      const dump = String((await c1.request(ControlCommand.TestSelectionToolbar, []))[1] ?? "");
      const cmds = parseToolbarCmds(dump);
      if (cmds.includes(highlightId)) {
        throw new Error(`issue-6015: Highlight should be hidden by SelectionToolbarLayout:\n${dump}`);
      }
      const ulAt = cmds.indexOf(underlineId);
      const copyAt = cmds.indexOf(copyId);
      if (ulAt < 0 || copyAt < 0) {
        throw new Error(`issue-6015: custom layout missing Underline or Copy:\n${dump}`);
      }
      if (ulAt >= copyAt) {
        throw new Error(`issue-6015: custom layout should list Underline before Copy:\n${dump}`);
      }
      if (cmds.length !== 2) {
        throw new Error(`issue-6015: custom layout should have exactly 2 buttons, got ${cmds.length}:\n${dump}`);
      }
    } finally {
      c1.close();
      await killAndWait(p1);
    }
  }

  {
    const dir = writeAppData(tmpPath("issue-6015-main-tb"), [
      "SelectionHandlers [",
      "    [",
      "        Name = Duck",
      "        URL = https://example.com/?q=${selection}",
      "        ToolbarText = DuckTB",
      "    ]",
      "]",
    ]);
    const { proc: p2, client: c2 } = await launchControlled(["-appdata", dir, pdf]);
    try {
      await c2.waitForRenderIdle();
      const dump = String((await c2.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
      if (!/text=DuckTB/.test(dump)) {
        throw new Error(`issue-6015: main toolbar missing SelectionHandlers ToolbarText=DuckTB:\n${dump}`);
      }
    } finally {
      c2.close();
      await killAndWait(p2);
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
