// gen-code.ts - runs all C++ codegen (imports gen-flags, gen-commands, gen-settings;
// gVirtKeysNum generation lives here only)
import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { main as genFlags } from "./gen-flags";
import { main as genCommands } from "./gen-commands";
import { main as genSettings } from "./gen-settings";

// prettier-ignore
// if there are multiple declarations, the first one will be shown in menu
const virtKeys: [string, string][] = [
    ["VK_NUMPAD0", "numpad0"],
    ["VK_NUMPAD1", "numpad1"],
    ["VK_NUMPAD2", "numpad2"],
    ["VK_NUMPAD3", "numpad3"],
    ["VK_NUMPAD4", "numpad4"],
    ["VK_NUMPAD5", "numpad5"],
    ["VK_NUMPAD6", "numpad6"],
    ["VK_NUMPAD7", "numpad7"],
    ["VK_NUMPAD8", "numpad8"],
    ["VK_NUMPAD9", "numpad9"],
    ["VK_TAB", "Tab"],
    ["VK_END", "End"],
    ["VK_HOME", "Home"],
    ["VK_LEFT", "Left"],
    ["VK_RIGHT", "Right"],
    ["VK_UP", "Up"],
    ["VK_DOWN", "Down"],
    ["VK_NEXT", "PageDown"],
    ["VK_NEXT", "PgDown"],
    ["VK_PRIOR", "PageUp"],
    ["VK_PRIOR", "PgUp"],
    ["VK_BACK", "Back"],
    ["VK_BACK", "Backspace"],
    ["VK_DELETE", "Del"],
    ["VK_DELETE", "Delete"],
    ["VK_INSERT", "Ins"],
    ["VK_INSERT", "Insert"],
    ["VK_ESCAPE", "Esc"],
    ["VK_ESCAPE", "Escape"],
    ["VK_RETURN", "Return"],
    ["VK_CONVERT", "Convert"],
    ["VK_NONCONVERT", "NoConvert"],
    ["VK_SPACE", "Space"],
    ["VK_MULTIPLY", "*"],
    ["VK_MULTIPLY", "Multiply"],
    ["VK_MULTIPLY", "Mult"],
    ["VK_ADD", "+"],
    ["VK_OEM_PLUS", "+"],
    ["VK_ADD", "Add"],
    ["VK_OEM_MINUS", "-"],
    ["VK_SUBTRACT", "-"],
    ["VK_SUBTRACT", "Subtract"],
    ["VK_SUBTRACT", "Sub"],
    ["VK_DIVIDE", "/"],
    ["VK_DIVIDE", "Divide"],
    ["VK_DIVIDE", "Div"],
    ["VK_HELP", "Help"],
    ["VK_SELECT", "Select"],
    ["VK_VOLUME_DOWN", "Volume Down"],
    ["VK_VOLUME_DOWN", "VolumeDown"],
    ["VK_VOLUME_UP", "Volume Up"],
    ["VK_VOLUME_UP", "VolumeUp"],
    ["VK_XBUTTON1", "XButton1"],
    ["VK_XBUTTON2", "XButton2"],
    ["VK_F1", "F1"],
    ["VK_F2", "F2"],
    ["VK_F3", "F3"],
    ["VK_F4", "F4"],
    ["VK_F5", "F5"],
    ["VK_F6", "F6"],
    ["VK_F7", "F7"],
    ["VK_F8", "F8"],
    ["VK_F9", "F9"],
    ["VK_F10", "F10"],
    ["VK_F11", "F11"],
    ["VK_F12", "F12"],
    ["VK_F13", "F13"],
    ["VK_F14", "F14"],
    ["VK_F15", "F15"],
    ["VK_F16", "F16"],
    ["VK_F17", "F17"],
    ["VK_F18", "F18"],
    ["VK_F19", "F19"],
    ["VK_F20", "F20"],
    ["VK_F21", "F21"],
    ["VK_F22", "F22"],
    ["VK_F23", "F23"],
    ["VK_F24", "F24"],
    ["VK_CLEAR", "Clear"],
    ["VK_ACCEPT", "Accept"],
    ["VK_MODECHANGE", "ModeChange"],
    ["VK_PRINT", "Print"],
    ["VK_EXECUTE", "Execute"],
    ["VK_SNAPSHOT", "PrtSc"],
    ["VK_SNAPSHOT", "PrintScreen"],
    ["VK_SLEEP", "Sleep"],
    ["VK_SEPARATOR", "Separator"],
    ["VK_DECIMAL", "Decimal"],
    ["VK_SCROLL", "Scroll"],
    ["VK_OEM_1", ";"],
    ["VK_OEM_3", "`"],
    ["VK_OEM_4", "["],
    ["VK_OEM_6", "]"],
];

const vkIds: Record<string, number> = {
    VK_NUMPAD0: 0x60, VK_NUMPAD1: 0x61, VK_NUMPAD2: 0x62, VK_NUMPAD3: 0x63,
    VK_NUMPAD4: 0x64, VK_NUMPAD5: 0x65, VK_NUMPAD6: 0x66, VK_NUMPAD7: 0x67,
    VK_NUMPAD8: 0x68, VK_NUMPAD9: 0x69, VK_TAB: 0x09, VK_END: 0x23,
    VK_HOME: 0x24, VK_LEFT: 0x25, VK_RIGHT: 0x26, VK_UP: 0x27, VK_DOWN: 0x28,
    VK_NEXT: 0x22, VK_PRIOR: 0x21, VK_BACK: 0x08, VK_DELETE: 0x2e, VK_INSERT: 0x2d,
    VK_ESCAPE: 0x1b, VK_RETURN: 0x0d, VK_CONVERT: 0x1c, VK_NONCONVERT: 0x1d,
    VK_SPACE: 0x20, VK_MULTIPLY: 0x6a, VK_ADD: 0x6b, VK_OEM_PLUS: 0xbb,
    VK_OEM_MINUS: 0xbd, VK_SUBTRACT: 0x6d, VK_DIVIDE: 0x6f, VK_HELP: 0x2f,
    VK_SELECT: 0x29, VK_VOLUME_DOWN: 0xae, VK_VOLUME_UP: 0xaf, VK_XBUTTON1: 0x05,
    VK_XBUTTON2: 0x06, VK_F1: 0x70, VK_F2: 0x71, VK_F3: 0x72, VK_F4: 0x73,
    VK_F5: 0x74, VK_F6: 0x75, VK_F7: 0x76, VK_F8: 0x77, VK_F9: 0x78, VK_F10: 0x79,
    VK_F11: 0x7a, VK_F12: 0x7b, VK_F13: 0x7c, VK_F14: 0x7d, VK_F15: 0x7e,
    VK_F16: 0x7f, VK_F17: 0x80, VK_F18: 0x81, VK_F19: 0x82, VK_F20: 0x83,
    VK_F21: 0x84, VK_F22: 0x85, VK_F23: 0x86, VK_F24: 0x87, VK_CLEAR: 0x0c,
    VK_ACCEPT: 0x1e, VK_MODECHANGE: 0x1f, VK_PRINT: 0x2a, VK_EXECUTE: 0x2b,
    VK_SNAPSHOT: 0x2c, VK_SLEEP: 0x5f, VK_SEPARATOR: 0x6c, VK_DECIMAL: 0x6e,
    VK_SCROLL: 0x91, VK_OEM_1: 0xba, VK_OEM_3: 0xc0, VK_OEM_4: 0xdb, VK_OEM_6: 0xdd,
};

// unsigned LEB128 of zigzag-encoded i64 (matches StrUtil.cpp VarIntEncode)
function varIntEncode(val: number): number[] {
    const bytes: number[] = [];
    let n = BigInt(val);
    let zigzag = (n << 1n) ^ (n >> 63n);
    while (true) {
        let b = Number(zigzag & 0x7fn);
        zigzag >>= 7n;
        if (zigzag !== 0n) {
            b |= 0x80;
        }
        bytes.push(b);
        if (zigzag === 0n) {
            break;
        }
    }
    return bytes;
}

function escapeCString(s: string): string {
    return s.replace(/\\/g, "\\\\").replace(/"/g, '\\"');
}

function varIntCString(val: number): string {
    const bytes = varIntEncode(val);
    return '"' + bytes.map((b) => `\\x${b.toString(16).padStart(2, "0")}`).join("") + '"';
}

function generateVirtKeysNum(): string {
    const lines: string[] = [];
    lines.push("// clang-format off");
    lines.push("static SeqStrNum gVirtKeysNum =");
    for (const [vkName, txt] of virtKeys) {
        const vk = vkIds[vkName];
        if (vk === undefined) {
            throw new Error(`unknown virtual key: ${vkName}`);
        }
        const strLit = `"${escapeCString(txt)}\\0"`;
        const numLit = varIntCString(vk);
        lines.push(`    ${strLit} ${numLit} \\`);
    }
    lines.push('    "\\0";');
    lines.push("// clang-format on");
    return lines.join("\n");
}

function replaceBetweenMarkers(content: string, startMarker: string, endMarker: string, generated: string): string {
    const startIdx = content.indexOf(startMarker);
    const endIdx = content.indexOf(endMarker);
    if (startIdx < 0 || endIdx < 0) {
        console.error(`Could not find markers '${startMarker}' and '${endMarker}'`);
        process.exit(1);
    }
    const before = content.substring(0, startIdx + startMarker.length);
    const after = content.substring(endIdx);
    return before + "\n" + generated + "\n" + after;
}

export function genVirtKeys(rootDir: string) {
    const accelPath = join(rootDir, "src", "Accelerators.cpp");
    let content = readFileSync(accelPath, "utf-8");
    content = replaceBetweenMarkers(
        content,
        "// @gen-start virt-keys-num",
        "// @gen-end virt-keys-num",
        generateVirtKeysNum(),
    );
    writeFileSync(accelPath, content, "utf-8");
    console.log("Generated gVirtKeysNum in src/Accelerators.cpp");
}

export async function main() {
    const timeStart = performance.now();
    const rootDir = join(import.meta.dir, "..");

    genFlags();
    genCommands();
    genVirtKeys(rootDir);
    await genSettings();

    const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
    console.log(`gen-code finished in ${elapsed}s`);
}

if (import.meta.main) {
    await main();
}