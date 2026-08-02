// #5868: copied text lost its word spacing. The tracking-space filter added for
// #5627 dropped any space whose glyph gap was under 0.2em, and any MuPDF
// synthetic space under 0.35em. A PDF that positions words with TJ offsets
// instead of space glyphs (groff/troff output, e.g. a man page) has *every*
// word space synthesized at a normal 0.25-0.3em gap, so nearly all of them
// were deleted.
//
// Also checks the #5627 direction still holds, if that repro file is around:
// the filter must still drop the near-zero "tracking" spaces that split words
// into syllables.
import { existsSync } from "node:fs";
import { join } from "node:path";
import { EXE, ROOT, runStandalone } from "./util";

// -extract-text prints "text on page N: '<hex bytes>'"
function extractPageText(pdf: string, pageNo: number): string {
    const res = Bun.spawnSync([EXE, "-extract-text", String(pageNo), pdf], { stdout: "pipe", stderr: "pipe" });
    const out = new TextDecoder().decode(res.stdout) + new TextDecoder().decode(res.stderr);
    const m = out.match(/text on page \d+: '([0-9a-f ]*)'/);
    if (!m) {
        throw new Error(`no text extracted from ${pdf} (is this a debug build? -extract-text is DEBUG-only)`);
    }
    const bytes = m[1].length === 0 ? [] : m[1].split(" ").map((h) => parseInt(h, 16));
    // the extractor uses \n as the line separator; keep it as a space for matching
    return new TextDecoder().decode(new Uint8Array(bytes)).replaceAll("\n", " ");
}

export async function testit(): Promise<void> {
    const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
    const text = extractPageText(pdf, 1);
    // spans a font change (roman -> bold -> roman), where the spaces used to go
    const wanted = ["Library Functions Manual", "see zlib.h for full description", "The zlib library is a general"];
    for (const w of wanted) {
        if (!text.includes(w)) {
            throw new Error(`extracted text is missing word spacing: expected '${w}'`);
        }
    }

    // the #5627 repro isn't in the repo (see agents.md); check it when present
    const repro = "C:\\Users\\kjk\\OneDrive\\!sumatra\\bugs\\bug-5627.pdf";
    if (!existsSync(repro)) {
        console.log(`issue-5868: SKIP tracking-space half (no ${repro})`);
        return;
    }
    const trk = extractPageText(repro, 1);
    for (const bad of ["kro nik", "im mün", "dün ya"]) {
        if (trk.includes(bad)) {
            throw new Error(`tracking spaces are back: extracted text contains '${bad}'`);
        }
    }
    // ...without swallowing the real word spaces around them
    for (const w of ["Helicobacter pylori", "Turkiye Klinikleri J Dermatol"]) {
        if (!trk.includes(w)) {
            throw new Error(`tracking-space filter ate a real word space: expected '${w}'`);
        }
    }
}

if (import.meta.main) {
    await runStandalone(testit);
}
