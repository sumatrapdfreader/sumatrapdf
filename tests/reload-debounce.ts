// Auto-reload debounce: while another program is still writing the file we
// watch (a LaTeX run rewriting its .pdf), SumatraPDF must not reload a
// half-written document. It used to reload twice per rewrite -- once on the
// truncated file ("cannot find startxref" / "document has no pages"), once on
// the finished one. AUTO_RELOAD_TIMER now re-arms itself while size/mtime keep
// changing, so a rewrite produces exactly one reload of the final file.
//
// Uses -appdata so it never touches the user's real settings.
import { copyFileSync, mkdirSync, openSync, readFileSync, rmSync, writeFileSync, writeSync, closeSync } from "node:fs";
import { join } from "node:path";
import { EXE, ROOT, runStandalone, tmpPath } from "./util";
import { sleep } from "./winapi";

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
ReloadModifiedDocuments = true
`;

export async function testit(): Promise<void> {
    const src = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
    const dir = tmpPath("reload-debounce");
    rmSync(dir, { recursive: true, force: true });
    mkdirSync(dir, { recursive: true });
    const doc = join(dir, "paper.pdf");
    copyFileSync(src, doc);
    writeFileSync(join(dir, "SumatraPDF-settings.txt"), SETTINGS);

    const proc = Bun.spawn([EXE, "-appdata", dir, doc], { stdout: "pipe", stderr: "ignore" });
    let out = "";
    const pump = (async () => {
        for await (const chunk of proc.stdout as ReadableStream) {
            out += new TextDecoder().decode(chunk);
        }
    })();
    try {
        await sleep(3000);
        out = ""; // don't count the initial load

        // rewrite the way pdflatex does: truncate, then write over ~2s
        const data = readFileSync(src);
        const fd = openSync(doc, "w");
        const nChunks = 8;
        const chunkSize = Math.ceil(data.length / nChunks);
        for (let i = 0; i < nChunks; i++) {
            writeSync(fd, data.subarray(i * chunkSize, (i + 1) * chunkSize));
            await sleep(250);
        }
        closeSync(fd);
        await sleep(4000);

        const reloads = (out.match(/ReloadDocument: .*auto refresh: 1/g) || []).length;
        const broken = (out.match(/document has no pages|cannot find startxref/g) || []).length;
        if (broken > 0) {
            throw new Error(`reloaded a half-written file ${broken} time(s) (reloads: ${reloads})`);
        }
        if (reloads !== 1) {
            throw new Error(`expected exactly 1 reload of the finished file, got ${reloads}`);
        }
    } finally {
        proc.kill();
        await proc.exited;
        await pump;
    }
}

if (import.meta.main) {
    await runStandalone(testit);
}
