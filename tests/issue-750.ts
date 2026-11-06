import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { assemblePdf, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { launchControlled, killAndWait, waitForExit, findCanvas } from "./win-automation.ts";
import { captureWindowPixels, postMessage, sleep, VK_ESCAPE, WM_KEYDOWN } from "./winapi.ts";

async function measure(client: ControlClient, action = "state", ...args: (number | string)[]): Promise<string> {
  const res = await client.request(ControlCommand.TestMeasurement, [action, ...args]);
  if (res[0] !== 0) throw new Error(`issue-750: ${res[1]}`);
  return String(res[1]);
}

function near(state: string, key: string, expected: number): void {
  const value = Number(new RegExp(`\\b${key}=([^ ]+)`).exec(state)?.[1]);
  if (!Number.isFinite(value) || Math.abs(value - expected) > Math.max(0.00001, expected * 0.02)) {
    throw new Error(`issue-750: expected ${key}=${expected}: ${state}`);
  }
}

function count(state: string): number {
  return Number(/\bcount=(\d+)/.exec(state)?.[1] ?? -1);
}

// Snapping to the drawing, arithmetic in the known length, radius, guide
// lines, Ctrl + click delete, Clear and Reset scale. The page has one line,
// (100, 100)-(300, 100) in PDF space: (100, 200)-(300, 200) on the 400 x 300 page.
async function testTools(): Promise<void> {
  const pdf = tmpPath("issue-750-line.pdf");
  const stream = "1 w 100 100 m 300 100 l S";
  writeFileSync(
    pdf,
    assemblePdf([
      "<< /Type /Catalog /Pages 2 0 R >>",
      "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 300] /Contents 4 0 R >>",
      `<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`,
    ]),
  );
  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "100", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.request(ControlCommand.TestInvokeCommand, ["CmdMeasure"]);

    // clicks near the line's ends snap onto them: 200 long
    await measure(client, "calibrate");
    let st = await measure(client, "click", 1, 103, 203);
    near(st, "x", 100);
    near(st, "y", 200);
    await measure(client, "click", 1, 297, 198);
    near(await measure(client, "apply", "2*50", "m"), "scale", 0.5);

    // without snapping a click stays where it is
    await measure(client, "snap", 0);
    await measure(client, "length");
    st = await measure(client, "click", 1, 103, 203);
    near(st, "x", 103);
    await measure(client, "snap", 1);

    // radius through (150, 100), (250, 100) and (200, 150): 50 long, 25 m
    await measure(client, "radius");
    await measure(client, "click", 1, 150, 100);
    await measure(client, "click", 1, 250, 100);
    st = await measure(client, "click", 1, 200, 150);
    near(st, "radius", 25);

    await measure(client, "guide");
    await measure(client, "click", 1, 50, 50);
    st = await measure(client, "click", 1, 60, 60);
    if (count(st) !== 2) throw new Error(`issue-750: expected 2 measurements: ${st}`);

    // Ctrl + click on the guide line, away from the points that made it
    st = await measure(client, "erase", 1, 150, 150);
    if (count(st) !== 1) throw new Error(`issue-750: Ctrl+click did not delete the guide line: ${st}`);
    st = await measure(client, "erase", 1, 380, 20);
    if (count(st) !== 1) throw new Error(`issue-750: Ctrl+click on nothing deleted something: ${st}`);

    st = await measure(client, "clear");
    if (count(st) !== 0) throw new Error(`issue-750: Clear left measurements: ${st}`);
    st = await measure(client, "reset");
    near(st, "scale", 1);
    if (!st.includes("unit=pt")) throw new Error(`issue-750: Reset scale kept the unit: ${st}`);

    // Esc drops the measurement being clicked, then closes the tool
    const esc = async () => {
      // keydown only: a posted keyup without its transition bit translates to another Esc
      postMessage(frame, WM_KEYDOWN, VK_ESCAPE, 1);
      await sleep(300);
    };
    await measure(client, "length");
    await measure(client, "click", 1, 50, 50);
    await esc();
    st = await measure(client);
    if (!st.includes("show=1") || !st.includes("complete=1")) {
      throw new Error(`issue-750: Esc should cancel the measurement and keep the tool: ${st}`);
    }
    await esc();
    if (!(await measure(client)).includes("show=0"))
      throw new Error("issue-750: the second Esc did not close the tool");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  if (!IS_WIN || USE_NG) {
    console.log("issue-750: measurement toolbar is Windows-only");
    return;
  }
  await testTools();
  const pdf = tmpPath("issue-750.pdf");
  writeFileSync(
    pdf,
    assemblePdf([
      "<< /Type /Catalog /Pages 2 0 R >>",
      "<< /Type /Pages /Count 2 /Kids [3 0 R 4 0 R] >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 300] >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 300] >>",
    ]),
  );
  const appdata = tmpPath("issue-750-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  const settings = join(appdata, "SumatraPDF-settings.txt");
  writeFileSync(settings, "RestoreSession = false\nCheckForUpdates = false\nReuseInstance = false\n");
  const args = ["-appdata", appdata, "-view", "single page", "-zoom", "100", pdf];
  const { proc, client, frame } = await launchControlled(args, { saveSettings: true });
  try {
    await client.waitForRenderIdle();
    await client.request(ControlCommand.TestInvokeCommand, ["CmdMeasure"]);
    await measure(client, "calibrate");
    await measure(client, "click", 1, 48, 48);
    await measure(client, "click", 1, 48, 48);
    near(await measure(client, "apply", "12", "m"), "scale", 1);
    await measure(client, "click", 1, 48, 48);
    await measure(client, "click", 1, 168, 48);
    for (const invalid of ["0", "-1", "nan", "inf", "1e999", "1e-999", "10junk", ""]) {
      near(await measure(client, "apply", invalid, "m"), "scale", 1);
    }
    near(await measure(client, "apply", "12", " "), "scale", 1);
    near(await measure(client, "apply", "12", "m"), "scale", 0.1);
    await measure(client, "length");
    await measure(client, "click", 1, 50, 50);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdZoom200"]);
    await client.waitForRenderIdle();
    const line = await measure(client, "click", 1, 110, 130);
    near(line, "length", 10);
    if (!line.includes("unit=m")) throw new Error(`issue-750: missing units: ${line}`);

    await measure(client, "area");
    await client.request(ControlCommand.TestInvokeCommand, ["CmdRotateRight"]);
    await client.waitForRenderIdle();
    await measure(client, "click", 1, 50, 50);
    const area = await measure(client, "click", 1, 110, 130);
    near(area, "area", 48);
    near(area, "length", 10);
    await client.waitForRenderIdle();
    const pixels = captureWindowPixels(findCanvas(frame));
    let red = 0;
    if (pixels) {
      for (let i = 0; i < pixels.data.length; i += 4) {
        if (pixels.data[i + 2] > 180 && pixels.data[i + 1] < 100 && pixels.data[i] < 100) red++;
      }
    }
    if (red < 100) throw new Error(`issue-750: measurement overlay is missing (${red} red pixels)`);
    await measure(client, "length");
    await measure(client, "click", 1, 50, 50);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdGoToNextPage"]);
    await client.waitForRenderIdle();
    const otherPage = await measure(client, "click", 2, 110, 130);
    if (!otherPage.includes("complete=0")) throw new Error(`issue-750: accepted another page: ${otherPage}`);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdMeasure"]);
    if (!(await measure(client)).includes("show=0")) throw new Error("issue-750: toolbar did not close");
    await client.request(ControlCommand.TestInvokeCommand, ["CmdExit"]);
    if (!(await waitForExit(proc))) throw new Error("issue-750: did not exit");
  } finally {
    client.close();
    await killAndWait(proc);
  }
  const saved = readFileSync(settings, "utf8");
  near(`scale=${/MeasurementScale = ([^\r\n]+)/.exec(saved)?.[1]}`, "scale", 0.1);
  if (!/MeasurementUnit = m\b/.test(saved)) {
    throw new Error(`issue-750: calibration was not saved: ${saved}`);
  }
  const reopened = await launchControlled(args);
  try {
    await reopened.client.waitForRenderIdle();
    await reopened.client.request(ControlCommand.TestInvokeCommand, ["CmdMeasure"]);
    const state = await measure(reopened.client);
    near(state, "scale", 0.1);
    if (!state.includes("unit=m")) throw new Error(`issue-750: units not restored: ${state}`);
    await reopened.client.request(ControlCommand.TestInvokeCommand, ["CmdGoToHomePage"]);
    if (!(await measure(reopened.client)).includes("show=0"))
      throw new Error("issue-750: toolbar survived a tab switch");
  } finally {
    reopened.client.close();
    await killAndWait(reopened.proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
