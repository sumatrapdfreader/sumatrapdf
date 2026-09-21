// discussion #6224: Toggle Free Pan lets the view go past the page edges by
// half a window, and the state is remembered per document.
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommand, sendCommandSync, waitForExit } from "./win-automation.ts";

function buildPdf(): string {
  const content = "BT /F1 24 Tf 72 720 Td (Free pan) Tj ET";
  const objects = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>",
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
  ];
  let pdf = "%PDF-1.5\n";
  const offsets: number[] = [];
  objects.forEach((obj, i) => {
    offsets.push(pdf.length);
    pdf += `${i + 1} 0 obj\n${obj}\nendobj\n`;
  });
  const xref = pdf.length;
  pdf += `xref\n0 ${objects.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    pdf += `${off.toString().padStart(10, "0")} 00000 n \n`;
  }
  pdf += `trailer\n<< /Size ${objects.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return pdf;
}

// vertical scroll offset of the window's top relative to the page top, in
// page units; -1 means "not scrolled" without free pan
async function scrollY(client: ControlClient): Promise<number> {
  const res = await client.request(ControlCommand.TestFavoriteNav, ["page", 0]);
  const raw = String(res[1] ?? "");
  const m = /OK page=\d+ y=(-?\d+)/.exec(raw);
  if (!m) {
    throw new Error(`issue-6224: can't read scroll state: ${raw.trim()}`);
  }
  return Number(m[1]);
}

// scroll all the way in one direction and report where it stopped
async function scrollToEnd(client: ControlClient, frame: number, cmd: string): Promise<number> {
  const id = cmdId(cmd);
  for (let i = 0; i < 40; i++) {
    sendCommandSync(frame, id);
  }
  await client.waitForRenderIdle();
  return scrollY(client);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6224");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "issue-6224.pdf");
  writeFileSync(pdf, buildPdf(), "latin1");
  const settingsPath = join(dir, "SumatraPDF-settings.txt");
  writeFileSync(
    settingsPath,
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nReuseInstance = false\nRememberStatePerDocument = true\n",
  );
  // 300% makes the page taller and wider than the test window
  const args = ["-appdata", dir, "-view", "single page", "-zoom", "300", pdf];
  const toggle = cmdId("CmdToggleFreePan");

  let { proc, client, frame } = await launchControlled(args, { saveSettings: true });
  try {
    await client.waitForRenderIdle();

    const maxOff = await scrollToEnd(client, frame, "CmdScrollDownPage");
    const minOff = await scrollToEnd(client, frame, "CmdScrollUpPage");
    if (maxOff <= 0 || minOff > 0) {
      throw new Error(`issue-6224: unexpected scroll range without free pan: ${minOff}..${maxOff}`);
    }

    sendCommandSync(frame, toggle);
    await client.waitForRenderIdle();
    const maxOn = await scrollToEnd(client, frame, "CmdScrollDownPage");
    const minOn = await scrollToEnd(client, frame, "CmdScrollUpPage");
    if (maxOn <= maxOff) {
      throw new Error(`issue-6224: free pan didn't add scroll room below the page: ${maxOn} vs ${maxOff}`);
    }
    if (minOn >= -10) {
      throw new Error(`issue-6224: free pan didn't add scroll room above the page: ${minOn}`);
    }

    sendCommandSync(frame, toggle);
    await client.waitForRenderIdle();
    const maxAgain = await scrollToEnd(client, frame, "CmdScrollDownPage");
    if (Math.abs(maxAgain - maxOff) > 1) {
      throw new Error(`issue-6224: turning free pan off didn't restore the scroll range: ${maxAgain} vs ${maxOff}`);
    }

    // leave it on and exit so the file state is saved
    sendCommandSync(frame, toggle);
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdExit"));
    if (!(await waitForExit(proc))) {
      throw new Error("issue-6224: SumatraPDF didn't exit after CmdExit");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  const txt = readFileSync(settingsPath, "utf8");
  if (!/\bFreePan = true\b/.test(txt.split("FileStates")[1] ?? "")) {
    throw new Error("issue-6224: FreePan = true wasn't saved in the file state");
  }

  ({ proc, client, frame } = await launchControlled(args, { saveSettings: true }));
  try {
    await client.waitForRenderIdle();
    const minRestored = await scrollToEnd(client, frame, "CmdScrollUpPage");
    if (minRestored >= -10) {
      throw new Error(`issue-6224: free pan wasn't restored from the file state: ${minRestored}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
