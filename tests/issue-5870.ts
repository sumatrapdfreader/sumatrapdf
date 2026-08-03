// #5870: the home page list view showed "0.00 KB" for every file. The cached
// size on ThumbnailLayout used -2 as "not fetched yet", but those layouts are
// created with Vec::AppendBlanks(), which zero-fills -- default member
// initializers never run, so the field read back as 0, the sentinel never
// matched and file::GetSize() was never called.
//
// Renders the same list row twice, with the same name and path but different
// file contents, and requires the row to look different. With the bug both
// rows read "0.00 KB" and the pixels are identical.
import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, ROOT, runStandalone, tmpPath } from "./util";
import { readWindowDCColumn, sleep, waitForTopWindow } from "./winapi";
import { FRAME_CLASS } from "./win-automation";

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = true
HomePageViewMode = list
`;

// the size column sits at the right end of the row; sample a block of columns
// there, over the first few rows
function rowPixels(hwnd: number): number[] {
    const out: number[] = [];
    for (let x = 700; x < 780; x += 4) {
        out.push(...readWindowDCColumn(hwnd, x, 180, 60));
    }
    return out;
}

// renders the home page with `src` copied to <dir>/a.pdf and samples the row
async function renderWith(dir: string, docPath: string, src: string): Promise<number[]> {
    copyFileSync(src, docPath);
    const proc = Bun.spawn([EXE, "-appdata", dir], { stdout: "ignore", stderr: "ignore" });
    try {
        const frame = await waitForTopWindow(proc.pid, FRAME_CLASS);
        if (!frame) {
            throw new Error("no frame window");
        }
        await sleep(3000); // home page layout + paint
        return rowPixels(frame);
    } finally {
        proc.kill();
        await proc.exited;
        await sleep(400);
    }
}

export async function testit(): Promise<void> {
    const dir = tmpPath("issue-5870");
    rmSync(dir, { recursive: true, force: true });
    mkdirSync(dir, { recursive: true });
    const docPath = join(dir, "a.pdf");
    // same appdata dir and same document path for both runs, so the row differs
    // only in the rendered size
    writeFileSync(join(dir, "SumatraPDF-settings.txt"), `${SETTINGS}FileStates [\n\t[\n\t\tFilePath = ${docPath}\n\t\tOpenCount = 3\n\t]\n]\n`);

    const small = join(ROOT, "ext", "a-zlib", "zlib.3.pdf"); // ~27 KB
    const big = join(ROOT, "ext", "brotli", "docs", "brotli-comparison-study-2015-09-22.pdf"); // ~210 KB

    const a = await renderWith(dir, docPath, small);
    const b = await renderWith(dir, docPath, big);
    if (a.length !== b.length || a.length === 0) {
        throw new Error(`bad pixel samples: ${a.length} vs ${b.length}`);
    }
    let nDiff = 0;
    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i]) {
            nDiff++;
        }
    }
    if (nDiff === 0) {
        throw new Error("home page list row renders identically for a 27 KB and a 210 KB file (file size not shown)");
    }
}

if (import.meta.main) {
    await runStandalone(testit);
}
