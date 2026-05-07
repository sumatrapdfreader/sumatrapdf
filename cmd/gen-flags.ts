import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";

// prettier-ignore
const args = [
    "Silent", "s",
    "Silent2", "silent",
    "FastInstall", "fast-install",
    "PrintToDefault", "print-to-default",
    "PrintDialog", "print-dialog",
    "Help", "h",
    "Help2", "?",
    "Help3", "help",
    "ExitWhenDone", "exit-when-done",
    "ExitOnPrint", "exit-on-print",
    "Restrict", "restrict",
    "Presentation", "presentation",
    "FullScreen", "fullscreen",
    "InvertColors", "invertcolors",
    "InvertColors2", "invert-colors",
    "Console", "console",
    "Install", "install",
    "UnInstall", "uninstall",
    "WithFilter", "with-filter",
    "WithSearch", "with-search",
    "WithPreview", "with-preview",
    "Rand", "rand",
    "Regress", "regress",
    "Extract", "x",
    "Tester", "tester",
    "TestApp", "testapp",
    "TestPlugin", "test-plugin",
    "TestPreview", "test-preview",
    "NewWindow", "new-window",
    "Log", "log",
    "LogToFile", "log-to-file",
    "CrashOnOpen", "crash-on-open",
    "ReuseInstance", "reuse-instance",
    "EscToExit", "esc-to-exit",
    "ArgEnumPrinters", "enum-printers",
    "ListPrinters", "list-printers",
    "SleepMs", "sleep-ms",
    "PrintTo", "print-to",
    "PrintSettings", "print-settings",
    "InverseSearch", "inverse-search",
    "ForwardSearch1", "forward-search",
    "ForwardSearch2", "fwdsearch",
    "NamedDest", "nameddest",
    "NamedDest2", "named-dest",
    "Page", "page",
    "View", "view",
    "Zoom", "zoom",
    "Scroll", "scroll",
    "AppData", "appdata",
    "Plugin", "plugin",
    "StressTest", "stress-test",
    "N", "n",
    "Max", "max",
    "MaxFiles", "max-files",
    "Render", "render",
    "ExtractText", "extract-text",
    "Bench", "bench",
    "Dir", "d",
    "InstallDir", "install-dir",
    "Lang", "lang",
    "UpdateSelfTo", "update-self-to",
    "ArgDeleteFile", "delete-file",
    "BgCol", "bgcolor",
    "BgCol2", "bg-color",
    "FwdSearchOffset", "fwdsearch-offset",
    "FwdSearchWidth", "fwdsearch-width",
    "FwdSearchColor", "fwdsearch-color",
    "FwdSearchPermanent", "fwdsearch-permanent",
    "MangaMode", "manga-mode",
    "Search", "search",
    "AllUsers", "all-users",
    "AllUsers2", "allusers",
    "RunInstallNow", "run-install-now",
    "Adobe", "a",
    "DDE", "dde",
    "EngineDump", "engine-dump",
    "SetColorRange", "set-color-range",
    "UpgradeFrom", "upgrade-from",
];

function generateCode(): string {
    const lines: string[] = [];

    // collect enum names and string names
    const enumNames: string[] = [];
    const strNames: string[] = [];
    for (let i = 0; i < args.length; i += 2) {
        enumNames.push(args[i]);
        strNames.push(args[i + 1]);
    }

    // generate enum class Arg, 4 per line
    lines.push("// clang-format off");
    lines.push("enum class Arg {");
    lines.push("    Unknown = -1,");
    for (let i = 0; i < enumNames.length; i += 4) {
        const chunk = enumNames.slice(i, i + 4);
        const parts = chunk.map((name, j) => `${name} = ${i + j}`);
        lines.push(`    ${parts.join(", ")},`);
    }
    lines.push("};");
    lines.push("");

    // generate gArgNames, 4 per line
    lines.push("static const char* gArgNames =");
    for (let i = 0; i < strNames.length; i += 4) {
        const chunk = strNames.slice(i, i + 4);
        const parts = chunk.map(s => `"${s}\\0"`).join(" ");
        const isLast = i + 4 >= strNames.length;
        lines.push(`    ${parts}${isLast ? ";" : ""}`);
    }
    lines.push("// clang-format on");

    return lines.join("\n");
}

function main() {
    const rootDir = join(import.meta.dir, "..");
    const flagsPath = join(rootDir, "src", "Flags.cpp");
    const content = readFileSync(flagsPath, "utf-8");

    const startMarker = "// @gen-start flags";
    const endMarker = "// @gen-end flags";

    const startIdx = content.indexOf(startMarker);
    const endIdx = content.indexOf(endMarker);
    if (startIdx < 0 || endIdx < 0) {
        console.error("Could not find gen markers in src/Flags.cpp");
        process.exit(1);
    }

    const generated = generateCode();
    const before = content.substring(0, startIdx + startMarker.length);
    const after = content.substring(endIdx);
    const newContent = before + "\n" + generated + "\n" + after;

    writeFileSync(flagsPath, newContent, "utf-8");
    console.log("Generated flags code in src/Flags.cpp");
}

main();
