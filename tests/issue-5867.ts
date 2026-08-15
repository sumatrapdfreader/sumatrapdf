// #5867: a malformed ToolbarSvgIcon in the settings file killed the app.
// RenderSvgIconPixmap() called mupdf without an fz_try, and mupdf aborts the
// process on an uncaught exception ("aborting process from uncaught error!").
// Easy to hit by accident: the settings watcher reloads while an editor is
// half-way through writing the file, so a long one-line SVG is briefly
// truncated.
//
// Uses -appdata so it never touches the user's real settings.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util";
import { launchControlled, sendCommandSync, killAndWait } from "./win-automation";

// truncated in the middle of an attribute value -- what the settings watcher
// sees while the file is being written
const BAD_SVG =
  `<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none">` +
  `<rect x="0" y="0" width="24" height="24" fill="none"/><path d="M23 8V20H12Z" fill="#000"/><path d="M2`;

// the icon from the issue: valid, and must still render
const GOOD_SVG =
  `<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none">` +
  `<path d="M23 8V20H12Z" fill="#000"/><text x="3" y="15.5" font-family="Arial" font-size="8" ` +
  `font-weight="bold" fill="#000">PDF</text></svg>`;

function settings(svg: string): string {
  return `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
Shortcuts [
\t[
\t\tCmd = CmdNone
\t\tKey = e
\t\tName = SVG test
\t\tToolbarSvgIcon =${svg}
\t]
]
`;
}

// launches with the given ToolbarSvgIcon and requires the app to come up and
// stay up; returns nothing, throws on failure
async function runWithIcon(name: string, svg: string): Promise<void> {
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const dir = tmpPath(`issue-5867-${name}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), settings(svg));

  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf]);
  try {
    await client.waitForRenderIdle();
    if (proc.exitCode !== null) {
      throw new Error(`${name}: app died with exit code ${proc.exitCode}`);
    }
    sendCommandSync(frame, cmdId("CmdExit"));
    await proc.exited;
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  await runWithIcon("bad", BAD_SVG);
  await runWithIcon("good", GOOD_SVG);
}

if (import.meta.main) {
  await runStandalone(testit);
}
