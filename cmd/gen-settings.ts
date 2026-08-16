// gen-settings.ts - replaces Go "-gen-settings" flag
// Generates src/Settings.h, website HTML docs, and markdown docs from settings definitions

import { existsSync, readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { join, dirname, resolve, basename } from "node:path";
import { extractSumatraVersion, clangFormatFiles, runLogged, isGitClean } from "./util";

async function runCapture(cmd: string, args: string[], cwd?: string): Promise<string> {
  const proc = Bun.spawn([cmd, ...args], { stdout: "pipe", stderr: "pipe", cwd });
  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    throw new Error(`${cmd} failed with exit code ${exitCode}`);
  }
  return await new Response(proc.stdout).text();
}

function writeFileMust(path: string, data: string): void {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, data, "utf-8");
}

interface Type {
  name: string;
  ctype: string;
}

class Field {
  Name: string;
  Type: Type;
  Default: any;
  Comment: string;
  NotSaved = false;
  CName: string;
  // saved & deserialized like any setting, but hidden from the advanced
  // settings dialog (app-managed / deprecated-for-migration values)
  Internal = false;
  DocComment: string;
  Version = "2.3";
  StructName = "";

  constructor(name: string, typ: Type, def: any, comment: string) {
    this.Name = name;
    this.Type = typ;
    this.Default = def;
    this.Comment = comment;
    this.CName = name !== "" ? toCName(name) : "";
    this.DocComment = comment;
  }

  // fluent modifiers, chainable: field(...).ver("3.6").internal()
  ver(v: string): this {
    this.Version = v;
    return this;
  }
  internal(): this {
    this.Internal = true;
    return this;
  }
  notSaved(): this {
    this.NotSaved = true;
    return this;
  }
  doc(s: string): this {
    this.DocComment = s;
    return this;
  }
  structName(structName: string): this {
    this.StructName = structName;
    if (this.Type.name === "Array") {
      this.Type.ctype = `Vec<${structName}*>*`;
    }
    if (this.Type.name === "Struct" || this.Type.name === "Compact") {
      this.Type.ctype = structName;
    }
    if (this.Type.name === "StructPtr") {
      this.Type.ctype = `${structName} *`;
    }
    return this;
  }
}

const Bool: Type = { name: "Bool", ctype: "bool" };
const Color: Type = { name: "Color", ctype: "Str" };
const Float: Type = { name: "Float", ctype: "float" };
const Int: Type = { name: "Int", ctype: "int" };
const Str: Type = { name: "String", ctype: "Str" };
const Comment: Type = { name: "Comment", ctype: "" };

function toCName(name: string): string {
  if (name === "URL") return "url";
  // "AIChat..." -> "aiChat..." (not "aIChat...")
  if (name.startsWith("AI") && name.length > 2) {
    return "ai" + name.slice(2);
  }
  return name[0].toLowerCase() + name.slice(1);
}

function field(name: string, typ: Type, def: any, comment: string): Field {
  return new Field(name, typ, def, comment);
}

function struct(name: string, fields: Field[], comment: string): Field {
  const typ: Type = { name: "Struct", ctype: name };
  const res = field(name, typ, fields, comment);
  res.StructName = name;
  return res;
}

function compactStruct(name: string, fields: Field[], comment: string): Field {
  const res = struct(name, fields, comment);
  res.Type.name = "Compact";
  return res;
}

// an optional sub-struct: a pointer that is null until the user sets it, and
// isn't written out at all while it's null. use it for a block that only some
// instances of a much-repeated struct (e.g. FileState) ever have
function structPtr(name: string, structName: string, fields: Field[], comment: string): Field {
  const typ: Type = { name: "StructPtr", ctype: `${structName} *` };
  const res = field(name, typ, fields, comment);
  res.StructName = structName;
  return res;
}

function array(name: string, fields: Field[], comment: string): Field {
  let structName = name;
  if (structName.endsWith("s")) {
    structName = structName.slice(0, -1);
  }
  const ctype = `Vec<${structName}*>*`;
  const typ: Type = { name: "Array", ctype };
  const res = field(name, typ, fields, comment);
  res.StructName = structName;
  return res;
}

function compactArray(name: string, typ: Type, def: any, comment: string): Field {
  const typ2: Type = { name: `${typ.name}Array`, ctype: `Vec<${typ.ctype}>*` };
  return field(name, typ2, def, comment);
}

function comment(text: string): Field {
  return field("", Comment, null, text);
}

function emptyLine(): Field {
  return comment("");
}

function rgb(r: number, g: number, b: number): string {
  const hex = (n: number) => n.toString(16).padStart(2, "0");
  return `#${hex(r)}${hex(g)}${hex(b)}`;
}

function rgba(r: number, g: number, b: number, a: number): string {
  const hex = (n: number) => n.toString(16).padStart(2, "0");
  return `#${hex(a)}${hex(r)}${hex(g)}${hex(b)}`;
}

function isComment(f: Field): boolean {
  return f.Type.name === "Comment";
}

const ebookUI: Field[] = [
  field(
    "FontName",
    Str,
    "",
    "default font family for ebooks (e.g. Segoe UI, Georgia, Microsoft YaHei). " +
      "empty uses the engine default (typically a serif). applied as user CSS " +
      "with !important, which beats the document's own font-family even when it " +
      "comes from an inline style attribute; leave empty to keep the publisher's " +
      "fonts. wrapping quotes are stripped. a name that can't be loaded is " +
      "reported with a notification when the document opens",
  ).ver("3.7"),
  field("FontSize", Float, 0, "font size in points; 0 means the default (8.0)"),
  compactArray(
    "Margin",
    Float,
    null,
    "white space around the text, in points (not screen pixels), like LayoutDx. " +
      "one number sets all four sides, two are top/bottom and left/right, four " +
      "are top, right, bottom, left - the same order as in CSS. empty keeps the " +
      "default (3 em above and below, 2 em left and right, so it follows the font " +
      "size); 0 leaves no margin at all. each value can be up to 200",
  ).ver("3.7"),
  field(
    "LineSpacing",
    Float,
    0,
    "line-height multiplier for ebook text (e.g. 1.5); 0 keeps the document or engine default. " +
      "values from 0.5 to 5 are accepted",
  ).ver("3.7"),
  field(
    "LayoutDx",
    Float,
    0,
    "width of the page the ebook is laid out into, in points (not screen pixels); 0 means the default (420)",
  ),
  field(
    "LayoutDy",
    Float,
    0,
    "height of the page the ebook is laid out into, in points (not screen pixels); 0 derives it " +
      "from the window's shape when the document is opened, so Fit Width shows a whole page",
  ),
  field("IgnoreDocumentCSS", Bool, false, "if true, the CSS in the ebook is ignored and only CustomCSS applies"),
  field(
    "CustomCSS",
    Str,
    null,
    "additional CSS applied to ebooks; set IgnoreDocumentCSS = true if the document's own CSS overrides it",
  ),
  field(
    "WindowBgCol",
    Color,
    "",
    "if given, sets the canvas background color for ebook documents (epub, mobi etc.)",
  ).ver("3.7"),
  field(
    "DefaultDisplayMode",
    Str,
    "",
    "default page layout for ebooks; empty uses the global DefaultDisplayMode. " +
      "valid values: automatic, single page, facing, book view, continuous, " +
      "continuous facing, continuous book view",
  ).ver("3.7"),
];

// the colors after LinkColor are optional: when empty they're derived from
// TextColor / BackgroundColor / ControlBackgroundColor. That's said in each of
// their doc comments because it's the main thing you need to know when writing
// a custom theme
const theme: Field[] = [
  field("Name", Str, "", "name of the theme, as shown in the Settings / Theme menu"),
  field("TextColor", Color, "", "color of text in menus, toolbar, tabs and sidebars"),
  field("BackgroundColor", Color, "", "background color of the window around the document"),
  field("ControlBackgroundColor", Color, "", "background color of toolbar, tabs, sidebars and dialogs"),
  field("LinkColor", Color, "", "color of clickable links in the UI"),
  field(
    "DisabledTextColor",
    Color,
    "",
    "color of disabled (grayed out) text; if empty, derived from the colors above",
  ).ver("3.7"),
  field(
    "DarkerTextColor",
    Color,
    "",
    "color of secondary / muted text like the page label; if empty, derived from the colors above",
  ).ver("3.7"),
  field(
    "HotBackgroundColor",
    Color,
    "",
    "background color of a control the mouse is over; if empty, derived from the colors above",
  ).ver("3.7"),
  field("EdgeColor", Color, "", "color of control borders and separators; if empty, derived from the colors above").ver(
    "3.7",
  ),
  field(
    "HotEdgeColor",
    Color,
    "",
    "border color of a control the mouse is over; if empty, derived from the colors above",
  ).ver("3.7"),
  field(
    "DisabledEdgeColor",
    Color,
    "",
    "border color of a disabled control; if empty, derived from the colors above",
  ).ver("3.7"),
  field(
    "ErrorBackgroundColor",
    Color,
    "",
    "background color of error messages; if empty, derived from the colors above",
  ).ver("3.7"),
  field(
    "NotificationBackgroundColor",
    Color,
    "",
    "background color of notification tips; if empty, derived from the colors above",
  ).ver("3.7"),
  field(
    "NotificationHighlightColor",
    Color,
    "",
    "background color of a highlighted notification tip; if empty, derived from the colors above",
  ).ver("3.7"),
  field(
    "NotificationHighlightTextColor",
    Color,
    "",
    "text color of a highlighted notification tip; if empty, derived from the colors above",
  ).ver("3.7"),
  field("ColorizeControls", Bool, false, "if true, apply the theme colors to Windows controls and window areas too"),
];

const tabFile: Field[] = [field("Path", Str, "", "path of the document")];

const tabGroup: Field[] = [
  field("Name", Str, "", "name of the tab group, as shown when restoring it"),
  array("TabFiles", tabFile, "documents that belong to this tab group"),
];

// screen coordinates in real (already DPI-scaled) pixels: these are saved from
// the actual window, not authored by hand, so they must not be scaled again
const windowPos: Field[] = [
  field("X", Int, 0, "x coordinate of the top-left corner, in screen pixels"),
  field("Y", Int, 0, "y coordinate of the top-left corner, in screen pixels"),
  field("Dx", Int, 0, "width, in screen pixels"),
  field("Dy", Int, 0, "height, in screen pixels"),
];

const pointPos: Field[] = [
  field("X", Int, 0, "x coordinate, in screen pixels"),
  field("Y", Int, 0, "y coordinate, in screen pixels"),
];

const keyboardShortcut: Field[] = [
  field(
    "Cmd",
    Str,
    "",
    "command to run, e.g. CmdOpenFile. See [the list of commands](https://www.sumatrapdfreader.org/docs/Commands)",
  ),
  field("Key", Str, "", "keyboard shortcut (e.g. Ctrl-Alt-F)"),
  field("Name", Str, null, "name shown in command palette").ver("3.6"),
  field("ToolbarText", Str, null, "if given, shows in toolbar").ver("3.6"),
  field(
    "ToolbarSvgIcon",
    Str,
    null,
    "optional SVG icon for toolbar button; if both ToolbarSvgIcon and ToolbarText are set, the icon is used",
  ).ver("3.7"),
  field("CmdId", Int, null, "command id").ver("3.6").notSaved(),
];

const scrollPos: Field[] = [
  field("X", Float, 0, "horizontal scroll offset, in document units"),
  field("Y", Float, 0, "vertical scroll offset, in document units"),
];

// a Windows FILETIME (100-nanosecond intervals since 1601-01-01), split in two
// because the settings file has no 64-bit integer type
const fileTime: Field[] = [
  field("DwHighDateTime", Int, 0, "high 32 bits of the FILETIME"),
  field("DwLowDateTime", Int, 0, "low 32 bits of the FILETIME"),
];

const printerDefaults: Field[] = [
  field("PrintScale", Str, "shrink", "default value for scaling (shrink, fit, none)"),
  field("Collate", Str, "default", "default value for collate in the print dialog (default, collate, nocollate)").ver(
    "3.7",
  ),
];

// HighlightOffset / HighlightWidth are in document units (multiplied by the
// current zoom when drawn), not screen pixels, so they must not be DPI-scaled
const forwardSearch: Field[] = [
  field(
    "HighlightOffset",
    Int,
    0,
    "if greater than 0, the forward search result is marked with a bar down the left " +
      "side of the page instead of highlighting the matched text. The value is how far " +
      "the bar sits from the left edge of the page, in document units",
  ),
  field(
    "HighlightWidth",
    Int,
    15,
    "width of that bar in document units (only used when HighlightOffset is greater than 0)",
  ),
  field("HighlightColor", Color, rgb(0x65, 0x81, 0xff), "color used for the forward search highlight"),
  field(
    "HighlightPermanent",
    Bool,
    false,
    "if true, the highlight stays visible until the next mouse click instead of fading away after a few seconds",
  ),
];

// WindowMargin / PageSpacing are pixels at 100% display scaling: DisplayModel
// ::SetUiDpi() scales them to the dpi of the window showing the document
const windowMarginFixedPageUI: Field[] = [
  field("Top", Int, 2, "space between the top of the window and the document, in pixels at 100% display scaling"),
  field("Right", Int, 4, "space between the right of the window and the document, in pixels at 100% display scaling"),
  field("Bottom", Int, 2, "space between the bottom of the window and the document, in pixels at 100% display scaling"),
  field("Left", Int, 4, "space between the left of the window and the document, in pixels at 100% display scaling"),
];

const windowMarginComicBookUI: Field[] = [
  field("Top", Int, 0, "space between the top of the window and the document, in pixels at 100% display scaling"),
  field("Right", Int, 0, "space between the right of the window and the document, in pixels at 100% display scaling"),
  field("Bottom", Int, 0, "space between the bottom of the window and the document, in pixels at 100% display scaling"),
  field("Left", Int, 0, "space between the left of the window and the document, in pixels at 100% display scaling"),
];

const pageSpacing: Field[] = [
  field("Dx", Int, 4, "horizontal gap between two pages, in pixels at 100% display scaling"),
  field("Dy", Int, 4, "vertical gap between two pages, in pixels at 100% display scaling"),
];

const fixedPageUI: Field[] = [
  field("TextColor", Color, rgb(0x00, 0x00, 0x00), "color used instead of black for the document's text"),
  field(
    "BackgroundColor",
    Color,
    rgb(0xff, 0xff, 0xff),
    "color used instead of white for the document's page background",
  ),
  field(
    "SelectionColor",
    Color,
    rgb(0xff, 0xff, 0x00),
    "color value for the text selection rectangle (also used to highlight found text). " +
      "Use an #aarrggbb value to control opacity: a smaller alpha (e.g. #40ffff00) makes the " +
      "selection more transparent so the selected text stays crisp; #rrggbb uses the default opacity",
  ).ver("2.4"),
  compactStruct(
    "WindowMargin",
    windowMarginFixedPageUI,
    "top, right, bottom and left margin (in that order) between window and document",
  ),
  compactStruct(
    "PageSpacing",
    pageSpacing,
    "horizontal and vertical distance between two pages in facing and book view modes",
  ).structName("Size"),
  compactArray(
    "GradientColors",
    Color,
    null,
    "experimental: instead of a single background color, fade through these colors from the top " +
      "of the document to the bottom (stops are spread evenly, at most 3 colors). The shifting " +
      "background is meant to give a subconscious sense of reading progress. " +
      "Suggested values: #2828aa #28aa28 #aa2828",
  ),
  field("WindowBgCol", Color, "", "if given, sets the canvas background color for PDF files").ver("3.7"),
];

const comicBookUI: Field[] = [
  compactStruct(
    "WindowMargin",
    windowMarginComicBookUI,
    "top, right, bottom and left margin (in that order) between window and document",
  ),
  compactStruct(
    "PageSpacing",
    pageSpacing,
    "horizontal and vertical distance between two pages in facing and book view modes",
  ).structName("Size"),
  field(
    "CbxMangaMode",
    Bool,
    false,
    "if true, documents that don't state their own reading direction default to manga mode, i.e. right to left. A document that states a direction (e.g. an EPUB with page-progression-direction) is shown the way it asks for",
  ),
  field("WindowBgCol", Color, "", "if given, sets the canvas background color for comic book files").ver("3.7"),
  field(
    "LimitToWindowWidth",
    Bool,
    false,
    "if true, absolute zoom never makes a page wider than the window (each page is capped at Fit Width). " +
      "Useful for comics/manga with double-page spreads that are much wider than regular pages (issue #2197)",
  ).ver("3.7"),
  field(
    "LimitToWindowHeight",
    Bool,
    false,
    "if true, absolute zoom never makes a page taller than the window (each page is capped at Fit Height)",
  ).ver("3.7"),
  field(
    "DefaultDisplayMode",
    Str,
    "",
    "default page layout for comic books; empty uses the global DefaultDisplayMode. " +
      "valid values: automatic, single page, facing, book view, continuous, " +
      "continuous facing, continuous book view",
  ).ver("3.7"),
  field(
    "DefaultZoom",
    Str,
    "",
    "default zoom for comic books; empty uses the global DefaultZoom. " +
      "valid values: fit page, fit width, fit height, fit content, shrink to fit or percent like 100%",
  ).ver("3.7"),
  field("DefaultZoomFloat", Float, 0, "value of DefaultZoom for internal usage").notSaved(),
];

const imageUI: Field[] = [
  field("WindowBgCol", Color, "", "if given, sets the canvas background color for image files").ver("3.7"),
  field(
    "DefaultZoom",
    Str,
    "shrink to fit",
    "default zoom for image files. valid values: fit page, fit width, fit height, fit content, shrink to fit or percent like 100%",
  ).ver("3.7"),
  field("DefaultZoomFloat", Float, 0, "value of DefaultZoom for internal usage").notSaved(),
  field(
    "LimitToWindowWidth",
    Bool,
    false,
    "if true, absolute zoom never makes a page wider than the window (each page is capped at Fit Width). " +
      "Useful for image folders with mixed aspect ratios (issue #2197)",
  ).ver("3.7"),
  field(
    "LimitToWindowHeight",
    Bool,
    false,
    "if true, absolute zoom never makes a page taller than the window (each page is capped at Fit Height)",
  ).ver("3.7"),
];

const chmUI: Field[] = [
  field("UseFixedPageUI", Bool, false, "if true, the UI used for PDF documents will be used for CHM documents as well"),
  field(
    "FontName",
    Str,
    null,
    "font family for the CHM fixed-page view (e.g. Segoe UI, Georgia, Microsoft YaHei). " +
      "empty uses EBookUI.FontName or the engine default. overrides fonts specified by the document; " +
      "wrapping quotes are stripped",
  ).ver("3.7"),
];

const markdownUI: Field[] = [
  field(
    "UseFixedPageUI",
    Bool,
    false,
    "if true, use MuPDF (cmark-gfm) to render markdown; if false, use WebView2 browser view when available",
  ),
];

const htmlUI: Field[] = [
  field(
    "UseFixedPageUI",
    Bool,
    false,
    "if true, use MuPDF to render HTML; if false, use WebView2 browser view when available",
  ),
];

const codexBuild: Field[] = [
  field("Model", Str, "gpt-5.5", "Codex model ID for -m (e.g. gpt-5.5, gpt-5.4, o3)"),
  field(
    "Models",
    Str,
    "",
    "extra Codex model IDs for the dropdown, comma-separated; used in addition to models reported by Codex",
  ),
  field("Sandbox", Int, 1, "Codex sandbox mode: 0=read-only, 1=workspace-write, 2=danger-full-access"),
  field("SkipSandbox", Bool, false, "if true, pass --dangerously-bypass-approvals-and-sandbox to Codex"),
  field("BgColor", Color, "#ffffff", "background color of the OpenAI Codex chat panel"),
];

const grokBuild: Field[] = [
  field("Model", Str, "grok-4.5", "Grok model ID for --model (e.g. grok-4.5)"),
  field(
    "Models",
    Str,
    "",
    "extra Grok model IDs for the dropdown, comma-separated; used in addition to models reported by Grok",
  ),
  field("Effort", Int, 1, "Grok effort level: 0=Low, 1=Medium, 2=High, 3=XHigh, 4=Max"),
  field("AlwaysApprove", Bool, false, "if true, pass --always-approve to Grok Build (auto-approve tool executions)"),
  field("BgColor", Color, "#ffffff", "background color of the Grok Build chat panel"),
];

const claudeCode: Field[] = [
  field(
    "Model",
    Str,
    "sonnet",
    "Claude model alias for --model (e.g. sonnet, opus, haiku); uses opus if not in the model list",
  ),
  field(
    "Models",
    Str,
    "",
    "extra Claude model aliases for the dropdown, comma-separated; documented Claude Code aliases are always included",
  ),
  field("Effort", Int, 1, "Claude effort level: 0=Low, 1=Medium, 2=High, 3=Max"),
  field("SkipPermissions", Bool, false, "if true, pass --dangerously-skip-permissions to Claude Code"),
  field("BgColor", Color, "#ffffff", "background color of the Claude Code chat panel"),
];

const antiGravity: Field[] = [
  field("Model", Str, "gemini-3.6-flash", "Antigravity model ID for --model (e.g. gemini-3.6-flash)"),
  field("Models", Str, "", "extra Antigravity model IDs for the dropdown, comma-separated"),
  field("Effort", Int, 1, "Antigravity effort level: 0=Low, 1=Medium, 2=High, 3=Max"),
  field(
    "AutoApprove",
    Bool,
    true,
    "if true, pass --dangerously-skip-permissions to Antigravity CLI so it can read the current file " +
      "etc. in headless print mode (agy cannot prompt for permissions with -p)",
  ),
  field("BgColor", Color, "#ffffff", "background color of the Antigravity chat panel"),
];

const fullscreen: Field[] = [
  field(
    "ShowToolbar",
    Bool,
    false,
    "legacy bool for fullscreen toolbar; if Fullscreen.Toolbar is empty, " +
      "derived as show/hide (internal; use Fullscreen.Toolbar instead)",
  ).internal(),
  field(
    "Toolbar",
    Str,
    null,
    "toolbar mode in fullscreen: show (pinned), hide (no toolbar), overlay " +
      "(toolbar floats over the page, only shown when the mouse is near it). " +
      "if empty, derived from Fullscreen.ShowToolbar",
  ).ver("3.7"),
  field("ShowMenubar", Bool, false, "if true, show the menu bar in fullscreen mode"),
  field(
    "DisplayMode",
    Str,
    "",
    "page layout in presentation (Ctrl+L) and windowed fullscreen (Shift+Ctrl+L / F11). " +
      "empty keeps the current behavior: presentation uses single page, windowed fullscreen " +
      "keeps the existing layout. valid values: automatic, single page, facing, book view, " +
      "continuous, continuous facing, continuous book view",
  ).ver("3.7"),
];

const externalViewer: Field[] = [
  field(
    "CommandLine",
    Str,
    null,
    "command line with which to call the external viewer, may contain " +
      '%p for page number and "%1" for the file name (add quotation ' +
      "marks around paths containing spaces)",
  ),
  field("Name", Str, null, "name of the external viewer to be shown in the menu (implied by CommandLine if missing)"),
  field(
    "Filter",
    Str,
    null,
    "optional filter for which file types the menu item is to be shown; separate multiple entries using ';' and don't include any spaces (e.g. *.pdf;*.xps for all PDF and XPS documents)",
  ),
  field("Key", Str, null, "optional: keyboard shortcut e.g. Alt + 7").ver("3.6"),
  field("ToolbarText", Str, null, "if given, shows in toolbar").ver("3.7"),
  field(
    "ToolbarSvgIcon",
    Str,
    null,
    "optional SVG icon for toolbar button; if both ToolbarSvgIcon and ToolbarText are set, the icon is used",
  ).ver("3.7"),
];

const selectionHandler: Field[] = [
  field(
    "URL",
    Str,
    null,
    "url to invoke for the selection. ${selection} will be replaced with current selection and ${userlang} with language code for current UI (e.g. 'de' for German)",
  ),
  field("Name", Str, null, "name shown in context menu"),
  field("Key", Str, null, "keyboard shortcut").ver("3.6"),
  field(
    "Exe",
    Str,
    null,
    "command line of a program to run instead of opening a URL. Use ${selectionfile} to pass " +
      "the selection as a temporary utf-8 file, which has no length limit. If set, URL is ignored",
  ).ver("3.7"),
  field(
    "Method",
    Str,
    "GET",
    "how to send the selection. GET (default) puts it in the URL, which limits how much text " +
      "fits. POST sends it in the request body with no length limit and shows the response. " +
      "POST-VIA-BROWSER submits a form from your browser instead, so the service sees your " +
      "normal browser session (cookies, logins)",
  ).ver("3.7"),
  field(
    "Body",
    Str,
    null,
    "request body for POST / POST-VIA-BROWSER; the same ${selection}, ${selectionjson} and " +
      "${userlang} substitutions apply. If unset, the body is the raw selection",
  ).ver("3.7"),
  field(
    "ContentType",
    Str,
    null,
    "value of the Content-Type header for POST. Defaults to 'text/plain; charset=utf-8', which " +
      "matches the default raw-selection body. Use 'application/json' or " +
      "'application/x-www-form-urlencoded' when Body is in that format. Ignored by " +
      "POST-VIA-BROWSER, which always submits a form",
  ).ver("3.7"),
  field(
    "Headers",
    Str,
    null,
    "extra HTTP headers for POST, one per line as 'Name: value' (use \\n to separate them in " +
      "this file). Needed for services that authenticate with an api key, e.g. " +
      "'Authorization: Bearer sk-...'. Ignored by POST-VIA-BROWSER. Note that anything you put " +
      "here is stored in plain text in this settings file",
  ).ver("3.7"),
  field(
    "SelectToolbarNameOrSvg",
    Str,
    null,
    "if set, the handler also gets a button in the toolbar that pops up over a text selection. " +
      "The value is the button's text, or, if it starts with '<svg', an icon to draw instead",
  ).ver("3.7"),
];

const annotations: Field[] = [
  field(
    "HighlightColor",
    Color,
    rgb(0xff, 0xff, 0x0),
    "color of newly created highlight annotations. Use an #aarrggbb value to set " +
      "default opacity (00 = transparent, FF = opaque); #rrggbb is fully opaque",
  ),
  field(
    "UnderlineColor",
    Color,
    rgb(0x00, 0xff, 0x0),
    "color of newly created underline annotations. #aarrggbb sets default opacity " + "the same way as HighlightColor",
  ),
  field(
    "SquigglyColor",
    Color,
    rgb(0xff, 0x00, 0xff),
    "color of newly created squiggly underline annotations. #aarrggbb sets default opacity " +
      "the same way as HighlightColor",
  ).ver("3.5"),
  field(
    "StrikeOutColor",
    Color,
    rgb(0xff, 0x00, 0x00),
    "color of newly created strike out annotations. #aarrggbb sets default opacity " + "the same way as HighlightColor",
  ).ver("3.5"),
  field("FreeTextColor", Color, "", "text color of newly created free text annotations").ver("3.5"),
  field("FreeTextBackgroundColor", Color, "", "background color of newly created free text annotations").ver("3.6"),
  field(
    "FreeTextOpacity",
    Int,
    100,
    "opacity of free text annotation in percent (0-100); 0 - fully transparent (invisible), 50 - half transparent, 100 - fully opaque",
  ).ver("3.6"),
  // sizes are in PDF user space units (points), not screen pixels: they're
  // part of the document, so they must not be DPI-scaled
  field("FreeTextSize", Int, 12, "font size of free text annotations, in points").ver("3.5"),
  field("FreeTextBorderWidth", Int, 1, "border width of free text annotations, in points").ver("3.5"),
  field(
    "FreeTextAlignment",
    Str,
    "left",
    "how text is aligned in newly created free text annotations (Text Alignment in the annotation " +
      "editor): left, center or right. Right-to-left scripts (Arabic, Hebrew, Persian) want right",
  ).ver("3.7"),
  field("TextIconColor", Color, "", "color of newly created text (sticky note) annotations"),
  field(
    "TextIconType",
    Str,
    "",
    "icon shown for text (sticky note) annotations: comment, help, insert, key, " +
      "new paragraph, note or paragraph. If not set, note is used",
  ),
  field(
    "DefaultAuthor",
    Str,
    "",
    "author recorded on newly created annotations. If not set, the Windows user name is used; " +
      "set it to (none) to leave the author out entirely",
  ).ver("3.4"),
];

const favorite: Field[] = [
  field("Name", Str, null, "name of this favorite as shown in the menu"),
  field("PageNo", Int, 0, "number of the bookmarked page"),
  field(
    "PageLabel",
    Str,
    null,
    "label for this page (only present if logical and physical page numbers are not the same)",
  ),
  field("MenuId", Int, 0, "id of this favorite in the menu (assigned by AppendFavMenuItems)").notSaved(),
  // search-start mark ("/") from Find; session-only. Field is in metadata so
  // SerializeStruct can skip array elements with IsTemporary=true; the field
  // itself is never written (SettingsUtil) (issue #5862)
  field("IsTemporary", Bool, false, "session-only favorite; omitted when serializing array elements")
    .internal()
    .ver("3.7"),
];

// per-document version of the EBookUI section. Every field means "unset" when
// it's empty / 0, in which case the global EBookUI value is used, so a document
// can override just the font and keep the rest.
const fileEBookUI: Field[] = [
  field(
    "FontName",
    Str,
    "",
    "font family for this document (e.g. Segoe UI, Microsoft YaHei); " + "empty uses EBookUI.FontName",
  ).ver("3.7"),
  field("FontSize", Float, 0, "font size in points for this document; 0 uses EBookUI.FontSize").ver("3.7"),
  compactArray(
    "Margin",
    Float,
    null,
    "white space around the text for this document, in points; one, two or four " +
      "values like EBookUI.Margin. empty uses EBookUI.Margin",
  ).ver("3.7"),
  field(
    "LineSpacing",
    Float,
    0,
    "line-height multiplier for this document (e.g. 1.5); 0 uses EBookUI.LineSpacing",
  ).ver("3.7"),
  field(
    "LayoutDx",
    Float,
    0,
    "width of the page this document is laid out into, in points; 0 uses EBookUI.LayoutDx",
  ).ver("3.7"),
  field(
    "LayoutDy",
    Float,
    0,
    "height of the page this document is laid out into, in points; 0 uses EBookUI.LayoutDy",
  ).ver("3.7"),
  field(
    "IgnoreDocumentCSS",
    Str,
    "",
    "whether the CSS in this document is ignored: true or false; " + "empty uses EBookUI.IgnoreDocumentCSS",
  ).ver("3.7"),
  field(
    "CustomCSS",
    Str,
    "",
    "additional CSS applied to this document; empty uses EBookUI.CustomCSS",
  ).ver("3.7"),
];

const fileSettings: Field[] = [
  field("FilePath", Str, null, "path of the document"),
  array("Favorites", favorite, "pages of this document bookmarked in the Favorites menu"),
  field(
    "IsPinned",
    Bool,
    false,
    'if true, the document is "pinned" to the Frequently Read list, so that ' +
      "recently opened documents don't displace it",
  ),
  field(
    "IsMissing",
    Bool,
    false,
    "if true, the document can no longer be found. State worth keeping is still " +
      "remembered, so the entry is hidden rather than removed",
  ).doc("if true, the file is considered missing and won't be shown in any list"),
  field(
    "OpenCount",
    Int,
    0,
    "how often the document was opened, halved every week so that the Frequently Read " +
      "list reflects what's relevant now instead of what used to be opened a lot",
  ).doc("number of times this document has been opened recently"),
  field(
    "DecryptionKey",
    Str,
    null,
    "hex encoded MD5 fingerprint of the file content (32 chars) followed by the " +
      "crypt key (64 chars); only applies to PDF documents",
  ).doc("data required to open a password protected document without having to " + "ask for the password again"),
  structPtr(
    "EBookUI",
    "FileEBookUI",
    fileEBookUI,
    "reflowable (ebook) settings for just this document. The block is absent " +
      "until you add it; a field left empty or 0 uses the global EBookUI value. " +
      "The global section's WindowBgCol and DefaultDisplayMode are already " +
      "per-document as BgCol and DisplayMode below",
  ).ver("3.7"),
  field(
    "UseDefaultState",
    Bool,
    false,
    "if true, this document opens with the global defaults instead of the values below",
  ),
  field(
    "DisplayMode",
    Str,
    "automatic",
    "how pages are laid out for this document. The string is the persisted form of " +
      "DisplayModel::displayMode, so it's parsed after deserialization and written back " +
      "before serialization",
  ).doc(
    "layout of pages. valid values: automatic, single page, facing, book view, " +
      "continuous, continuous facing, continuous book view",
  ),
  compactStruct("ScrollPos", scrollPos, "how far this document has been scrolled (in x and y direction)").structName(
    "PointF",
  ),
  field("PageNo", Int, 1, "number of the last read page"),
  field("Zoom", Str, "fit page", "zoom (in %) or one of those values: fit page, fit width, fit height, fit content"),
  field("Rotation", Int, 0, "how far pages have been rotated as a multiple of 90 degrees"),
  field(
    "WindowState",
    Int,
    0,
    "state of the window. 1 is normal, 2 is maximized, " + "3 is fullscreen, 4 is minimized",
  ),
  compactStruct("WindowPos", windowPos, "default position (can be on any monitor)").structName("Rect"),
  field("ShowToc", Bool, true, "if true, show the table of contents (Bookmarks) sidebar when the document has one"),
  field("SidebarDx", Int, 0, "width of the bookmarks / favorites sidebar in screen pixels, as last resized"),
  field("DisplayR2L", Bool, false, "if true, the document is displayed right-to-left in facing and book view modes"),
  field("BgCol", Color, "", "if given, overrides the background color for this document").ver("3.7"),
  field("TabCol", Color, "", "if given, overrides the tab color for this document").ver("3.7"),
  field(
    "ReparseIdx",
    Int,
    0,
    "index into an ebook's HTML data from which reparsing has to happen " +
      "in order to restore the last viewed page (i.e. the equivalent of PageNo for the ebook UI)",
  ).doc("data required to restore the last read page in the ebook UI"),
  compactArray(
    "TocState",
    Int,
    null,
    "tocState is an array of ids for ToC items that have been toggled by " +
      "the user (i.e. aren't in their default expansion state). - " +
      "Note: We intentionally track toggle state as opposed to expansion state " +
      "so that only a diff has to be saved instead of all states for the whole " +
      "tree (which can be quite large) (internal)",
  ).doc("data required to determine which parts of the table of contents have been expanded"),
  field(
    "Thumbnail",
    { name: "", ctype: "Pixmap *" },
    "NULL",
    "thumbnails are saved as PNG files in sumatrapdfcache directory",
  ).notSaved(),
  field("Index", Int, 0, "temporary value needed for FileHistory::cmpOpenCount").notSaved(),
  field("Himl", { name: "", ctype: "HIMAGELIST" }, "NULL", "image list holding the file's shell icon").notSaved(),
  field("IconIdx", Int, -1, "index of the file's shell icon in Himl, -1 if not loaded yet").notSaved(),
];

const tabState: Field[] = [
  field("FilePath", Str, null, "path of the document"),
  field(
    "DisplayMode",
    Str,
    "automatic",
    "layout of pages in this tab. valid values: automatic, single page, facing, " +
      "book view, continuous, continuous facing, continuous book view",
  ),
  field("PageNo", Int, 1, "number of the last read page"),
  field("Zoom", Str, "fit page", "zoom (in %) or one of those values: fit page, fit width, fit height, fit content"),
  field("Rotation", Int, 0, "how far pages have been rotated as a multiple of 90 degrees"),
  compactStruct("ScrollPos", scrollPos, "how far this document has been scrolled (in x and y direction)").structName(
    "PointF",
  ),
  field("ShowToc", Bool, true, "if true, the table of contents was shown when the document was closed"),
  compactArray("TocState", Int, null, "which table of contents items were expanded (see FileStates -> TocState)"),
];

const sessionData: Field[] = [
  array(
    "TabStates",
    tabState,
    "a subset of FileState required for restoring the state of a single tab " +
      "(required for handling documents being opened twice)",
  ).doc("data required for restoring the view state of a single tab"),
  field("TabIndex", Int, 1, "index of the currently selected tab (1-based)"),
  field("WindowState", Int, 0, "state of the window. 1 is normal, 2 is maximized, 3 is fullscreen, 4 is minimized"),
  compactStruct("WindowPos", windowPos, "position of the window (can be on any monitor)").structName("Rect"),
  field("SidebarDx", Int, 0, "width of the favorites / bookmarks sidebar in screen pixels (0 if it wasn't shown)"),
];

const globalPrefs: Field[] = [
  comment(""),
  emptyLine(),

  field(
    "DefaultDisplayMode",
    Str,
    "automatic",
    "how pages are laid out by default. The string is the persisted form of " +
      "DefaultDisplayModeEnum, so it's parsed after deserialization and written back " +
      "before serialization",
  ).doc(
    "default layout of pages. valid values: automatic, single page, facing, " +
      "book view, continuous, continuous facing, continuous book view, page aspect. " +
      "page aspect (3.7+): first open of a PDF, XPS, DjVu or PostScript file " +
      "uses page 1 — taller than wide is continuous + fit width, wider than tall " +
      "is single page + fit page; a remembered FileState still wins",
  ),
  field(
    "DefaultZoom",
    Str,
    "fit page",
    "default zoom. valid values: fit page, fit width, fit height, fit content or percent like 100%",
  ),
  field(
    "DisableJavaScript",
    Bool,
    false,
    "if true, JavaScript in PDF documents is disabled (e.g. form-field calculations won't run)",
  ).ver("3.7"),
  field(
    "AllowExternalImages",
    Bool,
    false,
    "if true, a PDF may load an image stored in a separate file referenced by name (an external " +
      "image stream); the file must sit next to the PDF. Off by default for security (matches Acrobat)",
  ).ver("3.7"),
  field(
    "EnableTeXEnhancements",
    Bool,
    false,
    "if true, show the SyncTeX inverse search command line in Settings -> Options, so a " +
      "double-click in the document can jump to the matching line in a LaTeX editor",
  ),
  field("EscToExit", Bool, false, "if true, Esc key closes SumatraPDF"),
  field("FullPathInTitle", Bool, false, "if true, show the full path to the document in the title bar").ver("3.0"),
  field("InverseSearchCmdLine", Str, null, "pattern used to launch the LaTeX editor when doing inverse search"),
  field(
    "LazyLoading",
    Bool,
    false,
    "if true, restoring a session delays loading each document until its tab is selected",
  ).ver("3.6"),
  field(
    "MainWindowBackground",
    Color,
    rgba(0xff, 0xf2, 0x00, 0x80),
    "background color of the area around the document, traditionally yellow. Only applies to the " +
      'Light theme; the default #80fff200 is a marker meaning "use the theme\'s color", so setting ' +
      "any other value also colorizes the toolbar and sidebars",
  ),
  field("NoHomeTab", Bool, false, "if true, doesn't open Home tab"),
  field(
    "HomePageSortByFrequentlyRead",
    Bool,
    false,
    "if true, the home page lists documents by how often they've been opened (the pre-3.6 " +
      "behavior); if false, the most recently opened come first",
  ),
  field(
    "HomePageViewMode",
    Str,
    "thumbnails",
    "how the home page shows the document history: thumbnails (a grid of page previews) or list (one row per file)",
  )
    .ver("3.7")
    .doc("valid values: thumbnails, list"),
  field(
    "FilePicker",
    Str,
    "",
    "file open dialog used by Open File: empty or os (standard Windows file picker), " +
      "or sumatrapdf (Navigate Files in Folder). Toggled by Settings / SumatraPDF File Picker",
  )
    .ver("3.7")
    .doc("valid values: (empty), os, sumatrapdf"),
  field(
    "ReloadModifiedDocuments",
    Bool,
    true,
    "if true, a document will be reloaded automatically whenever it's changed " +
      "(currently doesn't work for documents shown in the ebook UI)",
  ).ver("2.5"),
  field("RememberOpenedFiles", Bool, true, "if true, remember which documents were opened and their display settings"),
  field(
    "RememberStatePerDocument",
    Bool,
    true,
    "if true, store display settings for each document separately (i.e. everything " +
      "after UseDefaultState in FileStates)",
  ),
  field("RestoreSession", Bool, true, "if true and SessionData isn't empty, that session will be restored at startup"),
  field(
    "ReuseInstance",
    Bool,
    true,
    "if true, open documents in the already running SumatraPDF instead of starting a new one",
  ),
  field(
    "ShowMenubar",
    Bool,
    true,
    "if true, show the menu bar (F9 toggles it; the choice is remembered across sessions)",
  ).ver("2.5"),
  field("ShowMenubarWithTabs", Bool, false, "if true, show the menu bar when using tabs (useTabs = true)").ver("3.7"),
  field("ShowTips", Bool, true, "if true, show tips on the home page").ver("3.7"),
  field(
    "CustomColors",
    Str,
    null,
    "up to 13 custom colors for the background color picker, separated by space (e.g. '#ff0000 #00ff00 #0000ff')",
  )
    .internal()
    .ver("3.7"),
  field(
    "ShowToolbar",
    Bool,
    true,
    "legacy bool for toolbar; if Toolbar is empty, derived as show/hide " + "(internal; use Toolbar instead)",
  ).internal(),
  field(
    "Toolbar",
    Str,
    null,
    "toolbar mode: show (pinned), hide (no toolbar), overlay (toolbar floats over " +
      "the page, sized to its natural width and centered, only shown when the mouse is near it). " +
      "if empty, derived from ShowToolbar",
  ).ver("3.7"),
  field(
    "ToolbarPosition",
    Str,
    "top",
    "where the toolbar is placed: top or bottom (applies to both show and overlay modes)",
  ).ver("3.7"),
  field(
    "SearchUIFloating",
    Bool,
    false,
    "if true, the find UI is a floating, movable window with a results list " +
      "instead of the compact toolbar overlay",
  ).ver("3.7"),
  field("ShowFavorites", Bool, false, "if true, show the Favorites sidebar"),
  field(
    "SortFavoritesByName",
    Bool,
    false,
    "if true, favorites within each file are sorted alphabetically by name " +
      "(or page label); if false (the default), they are sorted by page number",
  ).ver("3.7"),
  field("ShowToc", Bool, true, "if true, show the table of contents (Bookmarks) sidebar when the document has one"),
  field(
    "SidebarOnRight",
    Bool,
    false,
    "if true, put the bookmarks / favorites sidebar on the right of the window " +
      "(left is the default; right-to-left UI languages already put it on the right)",
  ).ver("3.7"),
  field("ShowLinks", Bool, false, "if true, draw a blue border around links in the document").ver("3.6"),
  field(
    "ClickEdgeToTurnPage",
    Bool,
    false,
    "if true, a click (not a drag) on the left fifth of the page area goes to " +
      "the previous page and a click on the right fifth goes to the next page " +
      "(reversed in manga / right-to-left mode). Links, annotations and " +
      "presentation-mode clicks are unchanged",
  ).ver("3.7"),
  field(
    "DisableLinks",
    Bool,
    false,
    "if true, document links are ignored so you can select and read " +
      "(useful for drawings with many links); if false, clicking a link follows it",
  ).ver("3.7"),
  field(
    "RememberViewOffsetOnPageTurn",
    Bool,
    false,
    "if true, next/previous page keeps the same view position on the page " +
      "instead of jumping to the top (useful when zoomed in on similarly sized pages)",
  ).ver("3.7"),
  field(
    "ShowDocumentFocusIndicator",
    Bool,
    false,
    "if true, draw a focus ring around the document when it has keyboard focus (Tab to the page area)",
  ).ver("3.7"),
  field(
    "ShowAnnotationNotification",
    Bool,
    true,
    'if true, show a tip when hovering an annotation (e.g. "Highlight annotation. Ctrl+click to edit.")',
  ).ver("3.7"),
  field(
    "ShowTocPageNumbers",
    Bool,
    true,
    "if true, show page numbers (labels) right-aligned on bookmark / table-of-contents entries",
  ).ver("3.7"),
  field("ShowStartPage", Bool, true, "if true, show a list of frequently read documents when no document is loaded"),
  field(
    "SidebarDx",
    Int,
    0,
    "width of the favorites / bookmarks sidebar in screen pixels, as last resized (0 means the default)",
  ).internal(),
  field(
    "Scrollbars",
    Str,
    "windows",
    "scrollbar mode: windows (standard Windows scrollbar), smart (overlay scrollbar with auto-hide), overlay (always visible overlay scrollbar), hidden (no scrollbars)",
  ).ver("3.7"),
  field("ScrollbarInSinglePage", Bool, false, "if true, show a scrollbar in single page mode as well").ver("3.6"),
  field(
    "SmoothScroll",
    Bool,
    true,
    "if true, smooth mouse-wheel scrolling (exponential chase of the target; continuous wheel input stays fluid)",
  ).ver("3.6"),
  field(
    "ScrollLineAmount",
    Int,
    16,
    "distance, in screen pixels at 96 DPI, scrolled by an arrow-key press or one mouse-wheel line; values below 1 use 16",
  ).ver("3.7"),
  field(
    "PaddingAfterLastPage",
    Bool,
    false,
    "if true, continuous view has extra scroll room after the last page so you can scroll the end of the document to the top of the window",
  ).ver("3.7"),
  field(
    "IgnoreDestinationZoom",
    Bool,
    false,
    "if true, going to a destination (clicking a bookmark or a link inside the document) keeps " +
      "the current zoom instead of applying the zoom the destination asks for; it still goes to " +
      "the page and the position. Same as Adobe Reader's 'forbid the change of the current zoom " +
      "factor during execution of Go to Destination actions'",
  ).ver("3.7"),
  field(
    "CitationHoverDelay",
    Int,
    -1,
    "how long an internal-document link has to be hovered, in milliseconds, before a popup " +
      "rendering the destination region (citation entry, figure, footnote) appears. -1 (the " +
      "default) disables the popup; set a positive value like 300 to enable it",
  ).ver("3.7"),
  field(
    "ReadAloudVoiceId",
    Str,
    null,
    "voice id for Read Aloud text-to-speech; empty or unset means system default. Voice ids match those used internally by the Read Aloud Voice menu (WinRT voice id or SAPI token id)",
  ).ver("3.7"),
  field(
    "ReadAloudSpeed",
    Float,
    1,
    "playback speed multiplier for Read Aloud text-to-speech (0.5 .. 3.0), 1 is normal speed; can also be changed from the Read Aloud playback bar",
  ).ver("3.7"),
  field(
    "FastScrollOverScrollbar",
    Bool,
    false,
    "if true, mouse wheel scrolling is faster when mouse is over a scrollbar",
  ).ver("3.6"),
  field(
    "PreventSleepInFullscreen",
    Bool,
    true,
    "if true, prevents the screen from turning off when in fullscreen or presentation mode",
  ),
  field("TabWidth", Int, 300, "maximum width of a single tab, in pixels at 100% display scaling (at least 60)"),
  // Built-in names must stay in sync with themesTxt in src/Theme.cpp (plus System)
  field(
    "Theme",
    Str,
    "",
    "the name of the theme to use. System follows the Windows light/dark app mode " +
      "and switches between LastLightTheme and LastDarkTheme. Built-in themes: " +
      "Light, Dark, Light Warm, Dark from 3.5, Charcoal, Solarized Light, " +
      "Solarized Dark, Dracula, Nebula, Greeny, Choco, Purpy, One Dark, Monokai, " +
      "Nord, GitHub Dark, Catppuccin Mocha, Tokyo Night, Gruvbox, Night Owl, Ayu, " +
      "Palenight, System (custom Themes[] entries can add more)",
  )
    .ver("3.5")
    .doc(
      "valid themes: Light, Dark, Light Warm, Dark from 3.5, Charcoal, Solarized Light, " +
        "Solarized Dark, Dracula, Nebula, Greeny, Choco, Purpy, One Dark, Monokai, Nord, " +
        "GitHub Dark, Catppuccin Mocha, Tokyo Night, Gruvbox, Night Owl, Ayu, Palenight, System",
    ),
  // remembered by the light/dark toggle and System theme; not user-facing knobs
  field("LastLightTheme", Str, "", "the light theme the light/dark toggle and the System theme switch to")
    .internal()
    .ver("3.7"),
  field("LastDarkTheme", Str, "", "the dark theme the light/dark toggle and the System theme switch to")
    .internal()
    .ver("3.7"),
  // Full text is shown in Advanced Settings and Advanced-options-settings.md
  // (do not use a short .doc() here — that would replace this description).
  field(
    "DocumentColorsFollowTheme",
    Str,
    "off",
    "how MuPDF-rendered documents (PDF, XPS, DjVu, EPUB, MOBI, FB2, CBZ, images, etc.) " +
      "use UI / FixedPageUI colors for the page. Values: off (document's own colors; default); " +
      "smart (recolor text and page background, keep photos/images as-is — best for dark reading); " +
      "legacy (also recolor images; pre-3.7 invert-style). Does not change menus/toolbars — use Theme " +
      "for UI chrome. Settings / Theme and the CmdSetDocumentColorsFollowTheme command set " +
      "all three values. Shift+I (Invert Colors) is separate: it swaps the page colors for " +
      "the session whatever this is set to",
  ).ver("3.7"),
  field(
    "TocDy",
    Int,
    0,
    "if both the favorites and the bookmarks part of the sidebar are visible, this is " +
      "the height of the bookmarks (table of contents) part, in screen pixels",
  ).internal(),
  field(
    "ToolbarCustomLayout",
    Str,
    "",
    "the toolbar's built-in buttons, in the order you want them, e.g. " +
      "CmdOpenFile CmdPrint PageInfo | CmdFindFirst. Leave a button out to hide it. " +
      "| is a separator and PageInfo is the page number box. Empty (the default) means " +
      "the standard layout. Buttons you added yourself (see Shortcuts) still come last",
  ).ver("3.7"),
  field(
    "ToolbarShowReadAloud",
    Bool,
    false,
    "if true, the toolbar has a Read Aloud button (with a drop-down for voice, speed and " +
      "what to read). Read Aloud is still reachable from the Read Aloud menu when this is false",
  ).ver("3.7"),
  field(
    "ToolbarSize",
    Int,
    18,
    "size of the toolbar icons in pixels at 100% display scaling (8-64); the toolbar itself " +
      "is a few pixels taller",
  ).ver("3.4"),
  field(
    "TreeFontName",
    Str,
    "automatic",
    "font name for bookmarks and favorites tree views. automatic means Windows default",
  ),
  // font sizes are used as-is at every dpi (see GetAppMenuFontSizeForDpi):
  // a user who picks a size wants that size, not a scaled one
  field(
    "TreeFontSize",
    Int,
    0,
    "font size for bookmarks and favorites tree views, in pixels; 0 means the Windows default. " +
      "Not scaled by the display scaling",
  ).ver("3.3"),
  field(
    "UIFontSize",
    Int,
    0,
    "overrides the font size used for menus, toolbar and dialogs, in pixels; 0 means the " +
      "Windows default. Not scaled by the display scaling",
  ).ver("3.6"),
  field(
    "DisableAntiAlias",
    Bool,
    false,
    "if true, render MuPDF-based documents (PDF, XPS, DjVu, EPUB etc.) without anti-aliasing, " +
      "giving sharper but jagged edges",
  ).ver("3.6"),
  field(
    "EngineeringDrawingEnhance",
    Str,
    "auto",
    "CAD/engineering PDF line rendering: off, auto (enhance if a CAD drawing is detected) or on",
  ).ver("3.7"),
  field(
    "DisableAutoLinks",
    Bool,
    false,
    "if true, disables auto-linking of URLs and email addresses found in PDF text",
  ).ver("3.7"),
  field(
    "UseSysColors",
    Bool,
    false,
    "if true, use the Windows system colors for the document background and text. Overrides other color settings",
  ),
  field("UseTabs", Bool, true, "if true, documents are opened in tabs instead of new windows").ver("3.0"),
  field(
    "SelectionToolbar",
    Bool,
    true,
    "if true, a small floating toolbar with selection actions (copy, read aloud, highlight etc.) pops up after selecting text. Set to false to disable it",
  ).ver("3.7"),
  field(
    "TabsMru",
    Bool,
    false,
    "if true, Ctrl+Tab and Ctrl+Shift+Tab show the tab switcher in most recently used order instead of tab-strip order",
  ).ver("3.7"),
  field(
    "CtrlTabSimple",
    Bool,
    false,
    "if true, Ctrl+Tab and Ctrl+Shift+Tab immediately switch to the next / previous tab in tab-strip order " +
      "(the behavior before version 3.6) instead of showing the tab switcher",
  ).ver("3.7"),
  compactArray(
    "ZoomLevels",
    Float,
    "",
    "zoom levels which zooming steps through in addition to Fit Page and Fit Width. " +
      "The largest value is also the highest zoom that can be set at all, so listing " +
      "levels above 6400 (up to 1000000) is how you zoom further into e.g. a large map",
  ).doc(
    "sequence of zoom levels when zooming in/out; values must lie between 8.33 and 1000000 " +
      "(the largest one becomes the maximum zoom, which is 6400 by default)",
  ),
  compactArray("ZoomLevelsCmdIds", Int, "", "command id assigned to each entry of ZoomLevels").notSaved(),
  field(
    "ZoomIncrement",
    Float,
    0,
    "how much a single zoom in / zoom out step changes the zoom, as a percentage of the " +
      "current zoom level. If 0 or negative, zooming steps through ZoomLevels instead",
  ),

  emptyLine(),

  struct("FixedPageUI", fixedPageUI, "customization options for PDF, XPS, DjVu and PostScript UI"),
  emptyLine(),
  struct("EBookUI", ebookUI, "customization options for the ebook UI (EPUB, MOBI, FB2, PDB and plain text)"),
  emptyLine(),
  struct("ComicBookUI", comicBookUI, "customization options for Comic Book UI"),
  emptyLine(),
  struct("ImageUI", imageUI, "customization options for image files UI"),
  emptyLine(),
  struct(
    "ChmUI",
    chmUI,
    "customization options for CHM UI. UseFixedPageUI switches to the PDF-style view; FontName applies to that view",
  ),
  emptyLine(),
  struct(
    "MarkdownUI",
    markdownUI,
    "customization options for Markdown UI. If UseFixedPageUI is true, MuPDF is used; otherwise WebView2 browser view is used when available",
  ),
  emptyLine(),
  struct(
    "HtmlUI",
    htmlUI,
    "customization options for HTML UI. If UseFixedPageUI is true, MuPDF is used; otherwise WebView2 browser view is used when available",
  ).ver("3.7"),
  emptyLine(),
  struct("ClaudeCode", claudeCode, "settings for the Claude Code chat sidebar").ver("3.7"),
  emptyLine(),
  struct("GrokBuild", grokBuild, "settings for the Grok Build chat sidebar").ver("3.7"),
  emptyLine(),
  struct("CodexBuild", codexBuild, "settings for the OpenAI Codex chat sidebar").ver("3.7"),
  emptyLine(),
  struct("AntiGravity", antiGravity, "settings for the Antigravity chat sidebar").ver("3.7"),
  emptyLine(),
  field(
    "AIChatSidebarDx",
    Int,
    0,
    "width of the AI chat sidebar (0 = use default); shared by Claude Code, Grok Build, and OpenAI Codex (internal)",
  )
    .internal()
    .ver("3.7"),
  emptyLine(),
  field(
    "TranslateToLang",
    Str,
    "",
    "remembered destination language for selection translation; empty uses OS UI language",
  )
    .internal()
    .ver("3.7"),
  field("TranslateFromLang", Str, "", "remembered source language for selection translation; empty means Auto")
    .internal()
    .ver("3.7"),
  field(
    "TranslateEngine",
    Str,
    "",
    "remembered engine for Translate Selection: Google, DeepL, Grok Build, Claude Code, OpenAI Codex or Antigravity",
  )
    .internal()
    .ver("3.7"),
  emptyLine(),
  struct("Annotations", annotations, "default values for annotations in PDF documents").ver("3.3"),
  emptyLine(),
  array(
    "ExternalViewers",
    externalViewer,
    "list of additional external viewers for various file types. " +
      "See [docs for more information](https://www.sumatrapdfreader.org/docs/Customize-external-viewers)",
  ),
  emptyLine(),
  struct(
    "ForwardSearch",
    forwardSearch,
    "customization options for how forward search results are shown (used from LaTeX editors)",
  ),
  emptyLine(),
  struct("PrinterDefaults", printerDefaults, "these override the default settings in the Print dialog"),
  emptyLine(),
  struct("Fullscreen", fullscreen, "options for fullscreen mode").ver("3.7"),
  emptyLine(),
  array(
    "SelectionHandlers",
    selectionHandler,
    "list of handlers for selected text, shown in context menu when text selection is active. See [docs for more information](https://www.sumatrapdfreader.org/docs/Customize-search-translation-services)",
  ),
  emptyLine(),
  array("Shortcuts", keyboardShortcut, "custom keyboard shortcuts"),
  emptyLine(),
  array("Themes", theme, "color themes").ver("3.6"),
  emptyLine(),
  array("TabGroups", tabGroup, "saved groups of tabs").ver("3.7"),
  emptyLine(),

  field(
    "CustomScreenDPI",
    Int,
    0,
    "actual resolution of the main screen in DPI, used to show documents at their physical " +
      "size; if 0 or negative, the resolution reported by Windows is used",
  ).ver("2.5"),
  emptyLine(),

  comment("You're not expected to change those manually"),
  compactArray("DefaultPasswords", Str, null, "passwords to try when opening a password protected document")
    .ver("2.4")
    .doc(
      "a whitespace separated list of passwords to try when opening a password protected document " +
        "(passwords containing spaces must be quoted)",
    ),
  field("UiLanguage", Str, null, "ISO code of the current UI language").doc(
    "[ISO code](langs.html) of the current UI language",
  ),
  field("VersionToSkip", Str, null, "SumatraPDF won't offer to update to this version again"),
  field("WindowState", Int, 1, "default state of new windows (same as the last closed)").doc(
    "default state of the window. 1 is normal, 2 is maximized, " + "3 is fullscreen, 4 is minimized",
  ),
  compactStruct("WindowPos", windowPos, "default position (can be on any monitor)")
    .structName("Rect")
    .doc("default position (x, y) and size (width, height) of the window"),
  compactStruct(
    "SearchUIWindowPos",
    windowPos,
    "position/size of the floating find window (see SearchUIFloating)",
  ).structName("Rect"),

  array("FileStates", fileSettings, "information about opened files (in most recently used order)"),
  array("SessionData", sessionData, "state of the last session, usage depends on RestoreSession").ver("3.1"),

  compactArray(
    "ReopenOnce",
    Str,
    null,
    "a list of paths for files to be reopened at the next start " +
      'or the string "SessionData" if this data is saved in SessionData ' +
      "(needed for auto-updating)",
  )
    .ver("3.0")
    .doc("data required for reloading documents after an auto-update"),
  compactStruct("TimeOfLastUpdateCheck", fileTime, "timestamp of the last update check")
    .structName("FILETIME")
    .doc("data required to determine when SumatraPDF last checked for updates"),

  field("OpenCountWeek", Int, 0, 'week count since 2011-01-01 needed to "age" openCount values in file history').doc(
    "value required to determine recency for the OpenCount value in FileStates",
  ),
  compactStruct("LastPrefUpdate", fileTime, "modification time of the preferences file when it was last read")
    .structName("FILETIME")
    .notSaved(),
  field(
    "DefaultDisplayModeEnum",
    { name: "", ctype: "DisplayMode" },
    "DM_AUTOMATIC",
    "value of DefaultDisplayMode for internal usage",
  ).notSaved(),
  field("DefaultZoomFloat", Float, -1, "value of DefaultZoom for internal usage").notSaved(),
  compactStruct("PropWinPos", pointPos, "position of the document properties window").structName("Point"),
  // saved & honored, but hidden from the advanced settings dialog (edited via
  // the "Automatically check for updates" checkbox in Options instead)
  field("CheckForUpdates", Bool, true, "if true, check once a day whether an update is available").internal(),
  emptyLine(),
  comment("Settings below are not recognized by the current version"),
];

const globalPrefsStruct = struct("GlobalPrefs", globalPrefs, "Preferences are persisted in SumatraPDF-settings.txt");

const themes: Field[] = [array("Themes", theme, "color themes").ver("3.6")];
const themesStruct = struct("Themes", themes, "for parsing themes");

// ---------------------------------------------------------------------------
// C code generation
// ---------------------------------------------------------------------------

function formatComment(comment: string, start: string): string[] {
  const lines: string[] = [];
  const parts = comment.split(" ");
  let line = start;
  for (const part of parts) {
    if (line.length + part.length > 71) {
      lines.push(line);
      line = start;
    }
    line += " " + part;
  }
  if (line !== start) {
    lines.push(line);
  }
  return lines;
}

function formatArrayLines(data: string[][]): string[] {
  return data.map((ld) => {
    // 4th element (internal flag) is only emitted when true; otherwise the
    // FieldInfo::internal default (false) applies
    const extra = ld.length > 3 ? `, ${ld[3]}` : "";
    return `\t{ ${ld[0]}, ${ld[1]}, ${ld[2]}${extra} },`;
  });
}

// escape a string so it can be embedded in a C string literal
function escapeCStr(s: string): string {
  return s
    .replace(/\\/g, "\\\\")
    .replace(/"/g, '\\"')
    .replace(/\r/g, "\\r")
    .replace(/\n/g, "\\n")
    .replace(/\t/g, "\\t");
}

function cdefault(f: Field, built: Record<string, number>): string {
  if (f.Type === Bool) {
    return `${f.Default}`;
  }
  if (f.Type === Color) {
    return `(intptr_t)"${f.Default}"`;
  }
  if (f.Type === Float) {
    return `(intptr_t)"${f.Default}"`;
  }
  if (f.Type === Int) {
    return `${f.Default}`;
  }
  if (f.Type === Str) {
    if (f.Default === null || f.Default === undefined) {
      return "0";
    }
    return `(intptr_t)"${f.Default}"`;
  }
  const typeName = f.Type.name;
  if (["Struct", "StructPtr", "Array", "Compact"].includes(typeName)) {
    let idStr = "";
    const id = built[f.StructName] || 0;
    if (id > 0) {
      idStr = `_${id}_`;
    }
    return `(intptr_t)&g${f.StructName}${idStr}Info`;
  }
  if (["ColorArray", "FloatArray", "IntArray"].includes(typeName)) {
    if (f.Default === null || f.Default === undefined) {
      return "0";
    }
    return `(intptr_t)"${f.Default}"`;
  }
  if (typeName === "StringArray") {
    if (f.Default === null || f.Default === undefined) {
      return "0";
    }
    return `(intptr_t)"${f.Default}"`;
  }
  if (typeName === "Comment") {
    if (f.Comment === "") {
      return "0";
    }
    return `(intptr_t)"${f.Comment}"`;
  }
  throw new Error(`Unknown type name: '${typeName}'`);
}

function initDefault(f: Field): string {
  if (f.Type === Bool) {
    return `${f.Name} = ${f.Default}`;
  }
  if (f.Type === Color) {
    return `${f.Name} = ${f.Default as string}`;
  }
  if (f.Type === Float) {
    return `${f.Name} = ${f.Default}`;
  }
  if (f.Type === Int) {
    return `${f.Name} = ${f.Default}`;
  }
  if (f.Type === Str) {
    if (f.Default !== null && f.Default !== undefined) {
      return `${f.Name} = ${f.Default}`;
    }
    return ` ${f.Name} =`;
  }
  const typeName = f.Type.name;
  if (typeName === "Compact") {
    const fields = f.Default as Field[];
    const vals: string[] = [];
    for (const field of fields) {
      const v = initDefault(field);
      const parts = v.split(" = ");
      vals.push(parts.slice(1).join(" = "));
    }
    return `${f.Name} = ${vals.join(" ")}`;
  }
  if (["ColorArray", "FloatArray", "IntArray"].includes(typeName)) {
    if (f.Default !== null && f.Default !== undefined) {
      return `${f.Name} = ${f.Default}`;
    }
    return ` ${f.Name} =`;
  }
  if (typeName === "StringArray") {
    if (f.Default !== null && f.Default !== undefined) {
      return `${f.Name} = ${f.Default}`;
    }
    return ` ${f.Name} =`;
  }
  throw new Error(`initDefault: unknown type '${typeName}'`);
}

function buildStruct(struc: Field, built: Record<string, number>): string {
  const lines: string[] = [];
  const required: string[] = [];
  if (struc.Comment !== "") {
    const comments = formatComment(struc.Comment, "//");
    lines.push(...comments);
  }
  lines.push(`struct ${struc.StructName} {`);
  const fields = struc.Default as Field[];
  for (const field of fields) {
    if (isComment(field)) continue;
    const comments = formatComment(field.Comment, "\t//");
    lines.push(...comments);
    if (field.Type.name === "Color") {
      // a color setting is its text plus the parse of it; see ParsedColor
      lines.push(`\tParsedColor ${field.CName};`);
    } else {
      lines.push(`\t${field.Type.ctype} ${field.CName};`);
    }
    if (["Struct", "Compact", "Array"].includes(field.Type.name)) {
      const name = field.Name;
      if (name === field.StructName || name === field.StructName + "s") {
        if (built[name] === undefined) {
          const s = buildStruct(field, built);
          required.push(s, "");
          built[name] = (built[name] || 0) + 1;
        }
      }
    } else if (field.Type.name === "StructPtr") {
      const name = field.StructName;
      if (built[name] === undefined) {
        const s = buildStruct(field, built);
        required.push(s, "");
        built[name] = (built[name] || 0) + 1;
      }
    }
  }
  lines.push("};", "");
  const s1 = required.join("\n");
  const s2 = lines.join("\n");
  return s1 + s2;
}

function buildMetaData(struc: Field, built: Record<string, number>): string {
  const lines: string[] = [];
  const names: string[] = [];
  const comments: string[] = [];
  const data: string[][] = [];
  let suffix = "";
  const n = built[struc.StructName] || 0;
  if (n > 0) {
    suffix = `_${n}_`;
  }
  const fullName = struc.StructName + suffix;
  const fields = struc.Default as Field[];
  // everything following the "You're not expected to change those manually"
  // marker comment is app-managed state; mark it internal so it's hidden from
  // the advanced settings dialog (without needing a runtime comment check)
  let internalRest = false;
  for (const field of fields) {
    if (field.NotSaved) continue;
    if (field.Type.name === "Comment" && field.Comment.startsWith("You're not expected to change")) {
      internalRest = true;
    }
    const dataLine: string[] = [];
    dataLine.push(`offsetof(${struc.StructName}, ${field.CName})`);
    dataLine.push(`SettingType::${field.Type.name}`);
    dataLine.push(cdefault(field, built));
    names.push(field.Name);
    // per-field doc comment, aligned with names; used by the advanced settings
    // dialog to describe the selected setting
    comments.push(field.DocComment || "");
    if (["Struct", "StructPtr", "Compact", "Array"].includes(field.Type.name)) {
      const sublines = buildMetaData(field, built);
      lines.push(sublines, "");
      built[field.StructName] = (built[field.StructName] || 0) + 1;
    } else if (field.Type.name === "Comment") {
      dataLine[0] = "(size_t)-1";
    }
    if (field.Internal || internalRest) {
      dataLine.push("true");
    }
    data.push(dataLine);
  }
  lines.push(`static const FieldInfo g${fullName}Fields[] = {`);
  lines.push(...formatArrayLines(data));
  lines.push("};");
  const constStr = fullName !== "FileState" ? "const " : "";
  const namesStr = names.join("\\0");
  const commentsStr = comments.map(escapeCStr).join("\\0");
  // true if metadata includes Bool IsTemporary (SerializeStruct skips those array elements)
  let couldBeTemporary = false;
  for (const field of fields) {
    if (field.NotSaved || isComment(field)) {
      continue;
    }
    if (field.Name === "IsTemporary" && field.Type.name === "Bool") {
      couldBeTemporary = true;
      break;
    }
  }
  lines.push(
    `static ${constStr}StructInfo g${fullName}Info = { sizeof(${struc.StructName}), ${names.length}, g${fullName}Fields, "${namesStr}", "${commentsStr}", ${couldBeTemporary} };`,
  );
  return lines.join("\n");
}

const settingsStructsHeader = `// !!!!! This file is auto-generated by cmd/gen-settings.ts

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

struct Pixmap;

enum class DisplayMode {
\t// automatic means: the continuous form of single page, facing or
\t// book view - depending on the document's desired PageLayout
\tAutomatic = 0,
\tSinglePage,
\tFacing,
\tBookView,
\tContinuous,
\tContinuousFacing,
\tContinuousBookView,
};

constexpr float kZoomFitPage = -1.F;
constexpr float kZoomFitWidth = -2.F;
constexpr float kZoomFitContent = -3.F;
constexpr float kZoomShrinkToFit = -4.F;
constexpr float kZoomFitByOrientation = -5.F;
constexpr float kZoomFitHeight = -6.F;
constexpr float kZoomActualSize = 100.0F;
constexpr float kZoomMaxDefault = 6400.F;     /* max zoom in %, unless ZoomLevels raises it */
constexpr float kZoomMaxAllowed = 1000000.F;  /* the highest ZoomLevels can raise it to */
extern float kZoomMax;                        /* the max zoom in % in force, see DisplayMode.cpp */
constexpr float kZoomMin = 8.33F;  /* min zoom in % */
constexpr float kInvalidZoom = -99.0F;

{{structDef}}

#ifdef INCLUDE_SETTINGSSTRUCTS_METADATA

{{structMetadata}}

#endif
`;

function genSettingsStruct(): string {
  const builtDef: Record<string, number> = {};
  const builtMeta: Record<string, number> = {};
  let structDef = buildStruct(globalPrefsStruct, builtDef);
  let structMetaData = buildMetaData(globalPrefsStruct, builtMeta);

  structDef += buildStruct(themesStruct, builtDef);
  structMetaData += buildMetaData(themesStruct, builtMeta);

  let content = settingsStructsHeader;
  content = content.replaceAll("{{structDef}}", structDef);
  content = content.replaceAll("{{structMetadata}}", structMetaData);
  return content;
}

// ---------------------------------------------------------------------------
// HTML generation
// ---------------------------------------------------------------------------

const indentStr = "    ";

function escapeHTML(s: string): string {
  return s
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

function extractURL(s: string): string[] {
  if (!s.endsWith(")")) return [s];
  const wordEnd = s.indexOf("]");
  if (wordEnd === -1) throw new Error("extractURL: missing ]");
  const word = s.slice(0, wordEnd);
  if (s[wordEnd + 1] !== "(") throw new Error("extractURL: missing (");
  const url = s.slice(wordEnd + 2, s.length - 1);
  return [word, url];
}

function rstrip(s: string): string {
  return s.replace(/[\s]+$/, "");
}

function lstrip(s: string): string {
  return s.replace(/^[\s]+/, "");
}

function genCommentHTML(comment: string, fieldID: string, start: string, first: boolean): string {
  const lineLen = 100;
  let s = first ? "" : "\n";
  s = s + start + `<span class="cm" id="${fieldID}">`;
  let left = lineLen - start.length;
  let hrefText = "";
  comment = escapeHTML(comment);
  const words = comment.split(" ");
  for (let word of words) {
    if (word[0] === "[") {
      const wordURL = extractURL(word.slice(1));
      if (wordURL.length === 2) {
        s += `<a href="${wordURL[1]}">${wordURL[0]}</a>`;
        continue;
      }
      hrefText = wordURL[0];
      continue;
    } else if (hrefText !== "") {
      const wordURL = extractURL(word);
      hrefText = hrefText + " " + wordURL[0];
      if (wordURL.length === 2) {
        s += `<a href="${wordURL[1]}">${hrefText}</a> `;
        hrefText = "";
      }
      continue;
    }
    if (left < word.length) {
      s = rstrip(s) + "\n" + start;
      left = lineLen - start.length;
    }
    word += " ";
    left -= word.length;
    if (word === "color ") {
      word = `<a href="#color">color</a> `;
    } else if (word === "colors ") {
      word = `<a href="#color">colors</a> `;
    }
    s += word;
  }
  s = rstrip(s);
  s += `</span>`;
  return s;
}

function genStructHTML(struc: Field, indent: string): string {
  const lines: string[] = [];
  let first = true;

  const fields = struc.Default as Field[];
  for (const field of fields) {
    if (field.NotSaved || isComment(field)) continue;
    let comment = field.DocComment;
    if (field.Version !== "2.3") {
      comment += ` (introduced in version ${field.Version})`;
    }
    let fieldID = field.Name;
    if (indent !== "") {
      fieldID = struc.Name + "_" + field.Name;
    }
    lines.push(genCommentHTML(comment, fieldID, indent, first));

    if (field.Type.name === "Array") {
      const indent2 = indent + indentStr.slice(0, indentStr.length / 2);
      const start = `${indent}${field.Name} [\n${indent2}[`;
      const end = `${indent2}]\n${indent}]`;
      const inside = genStructHTML(field, indent + indentStr);
      lines.push(start, inside, end);
    } else if (field.Type.name === "Struct" || field.Type.name === "StructPtr") {
      const start = `${indent}${field.Name} [`;
      const end = `${indent}]`;
      const inside = genStructHTML(field, indent + indentStr);
      lines.push(start, inside, end);
    } else {
      let s = initDefault(field);
      s = lstrip(s);
      lines.push(indent + s);
    }
    first = false;
  }
  return lines.join("\n");
}

// ---------------------------------------------------------------------------
// Markdown generation
// ---------------------------------------------------------------------------

function genCommentMarkdown(comment: string, indent: string, first: boolean): string {
  const lineLen = 80;
  let s = first ? "" : "\n";
  const prefix = indent + "; ";
  s = s + prefix;
  let left = lineLen - prefix.length;
  let hrefText = "";
  const words = comment.split(" ");
  for (let word of words) {
    if (word[0] === "[") {
      const wordURL = extractURL(word.slice(1));
      if (wordURL.length === 2) {
        s += `${wordURL[0]} (${wordURL[1]})`;
        continue;
      }
      hrefText = wordURL[0];
      continue;
    } else if (hrefText !== "") {
      const wordURL = extractURL(word);
      hrefText = hrefText + " " + wordURL[0];
      if (wordURL.length === 2) {
        s += `${hrefText} (${wordURL[1]}) `;
        hrefText = "";
      }
      continue;
    }
    if (left < word.length) {
      s = rstrip(s) + "\n" + prefix;
      left = lineLen - prefix.length;
    }
    word += " ";
    left -= word.length;
    s += word;
  }
  s = rstrip(s);
  return s;
}

function genStructMarkdown(struc: Field, indent: string): string {
  const lines: string[] = [];
  let first = true;

  const fields = struc.Default as Field[];
  for (const field of fields) {
    if (field.NotSaved || isComment(field)) continue;
    let comment = field.DocComment;
    if (field.Version !== "2.3") {
      comment += ` (introduced in version ${field.Version})`;
    }
    lines.push(genCommentMarkdown(comment, indent, first));

    if (field.Type.name === "Array") {
      const indent2 = indent + indentStr.slice(0, indentStr.length / 2);
      const start = `${indent}${field.Name} [\n${indent2}[`;
      const end = `${indent2}]\n${indent}]`;
      const inside = genStructMarkdown(field, indent + indentStr);
      lines.push(start, inside, end);
    } else if (field.Type.name === "Struct" || field.Type.name === "StructPtr") {
      const start = `${indent}${field.Name} [`;
      const end = `${indent}]`;
      const inside = genStructMarkdown(field, indent + indentStr);
      lines.push(start, inside, end);
    } else {
      let s = initDefault(field);
      s = lstrip(s);
      lines.push(indent + s);
    }
    first = false;
  }
  return lines.join("\n");
}

// ---------------------------------------------------------------------------
// Templates
// ---------------------------------------------------------------------------

const tmplHTML = `<!doctype html>

<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title>Customizing SumatraPDF %VER%</title>
<style type="text/css">
body {
    font-size: 90%;
    background-color: #f5f5f5;
}

.desc {
    padding: 0px 10px 0px 10px;
}

.txt1 {
    /* bold doesn't look good in the fonts above */
    font-family: Monaco, 'DejaVu Sans Mono', 'Bitstream Vera Sans Mono', 'Lucida Console', monospace;
    font-size: 88%;
    color: #800; /* this is brown */
}

.txt2 {
    font-family: Verdana, Arial, sans-serif;
    font-family: serif;
    font-size: 90%;
    font-weight: bold;
    color: #800; /* this is brown */
}

.txt {
    font-family: serif;
    font-size: 95%;
    font-weight: bold;
    color: #800; /* this is brown */
    color: #000;
    background-color: #ececec;
    border: 1px solid #fff;
    border-radius: 10px;
    -webkit-border-radius: 10px;
    box-shadow: rgba(0, 0, 0, .15) 3px 3px 4px;
    -webkit-box-shadow: rgba(0, 0, 0, .15) 3px 3px 4px;
    padding: 10px 10px 10px 20px;
}

.cm {
    color: #800;   /* this is brown, a bit aggressive */
    color: #8c8c8c; /* this is gray */
    color: #555; /* this is darker gray */
    font-weight: normal;
}

</style>
</head>

<body>

<div class="desc">

<h2>Customizing SumatraPDF %VER%</h2>

<p>You can change the look and behavior of
<a href="https://www.sumatrapdfreader.org/">SumatraPDF</a>
by editing the file <code>SumatraPDF-settings.txt</code>. The file is stored in
<code>%LOCALAPPDATA%\\SumatraPDF</code> directory for the installed version or in the
same directory as <code>SumatraPDF.exe</code> executable for the portable version.</p>

<p>Use the menu item <code>Settings -> Advanced Settings...</code> to open the settings file
with your default text editor.</p>

<p>The file is in a simple text format. Below is an explanation of
what the different settings mean and what their default values are.</p>

<p>Highlighted settings can't be changed from the UI. Modifying other settings
directly in this file is not recommended.</p>

<p>If you add or remove lines with square brackets, <b>make sure to always add/remove
square brackets in pairs</b>! Else you risk losing all the data following them.</p>

</div>

<pre class="txt">
%INSIDE%
</pre>

<div class="desc">
<h3 id="color">Syntax for color values</h3>

<p>
The syntax for colors is: <code>#rrggbb</code> or <code>#rrggbbaa</code>.</p>
<p>The components are hex values (ranging from 00 to FF) and stand for:
<ul>
  <li><code>rr</code> : red component</li>
  <li><code>gg</code> : green component</li>
  <li><code>bb</code> : blue component</li>
  <li><code>aa</code> : alpha (transparency) component</li>
  </ul>
For example #ff0000 means red color. You can use <a href="https://colorsphere.app/">Sphere</a> to pick a color.
</p>
</div>

</body>
</html>
`;

const tmplLangsHTML = `<!doctype html>

<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title>Languages supported by SumatraPDF %VER%</title>
<style type="text/css">
body {
    font-size: 90%;
    background-color: #f5f5f5;
}

.txt1 {
    /* bold doesn't look good in the fonts above */
    font-family: Monaco, 'DejaVu Sans Mono', 'Bitstream Vera Sans Mono', 'Lucida Console', monospace;
    font-size: 88%;
    color: #800; /* this is brown */
}

.txt2 {
    font-family: Verdana, Arial, sans-serif;
    font-family: serif;
    font-size: 90%;
    font-weight: bold;
    color: #800; /* this is brown */
}

.txt {
    font-family: serif;
    font-size: 95%;
    font-weight: bold;
    color: #800; /* this is brown */
    color: #000;
    background-color: #ececec;
}

.cm {
    color: #800;   /* this is brown, a bit aggressive */
    color: #8c8c8c; /* this is gray */
    color: #555; /* this is darker gray */
    font-weight: normal;
}
</style>
</head>

<body>

<h2>Languages supported by SumatraPDF %VER%</h2>

<p>Languages supported by SumatraPDF. You can use ISO code as a value
of <code>UiLanguage</code> setting in <a href="settings%VER_URL%">settings file</a>.
</p>

<p>Note: not all languages are fully translated. Help us <a href="http://www.apptranslator.org/app/SumatraPDF">translate SumatraPDF</a>.</p>

<table>
<tr><th>Language name</th><th>ISO code</th></tr>
%INSIDE%
</table>

</body>
</html>
`;

const tmplMarkdown = `Below is an explanation of what the different settings mean and what their default values are.

If you add or remove lines with square brackets, **make sure to always add/remove square brackets in pairs**! Else you risk losing all the data following them.

\`\`\`
%INSIDE%
\`\`\`

## Syntax for color values

The syntax for colors is: \`#rrggbb\` or \`#aarrggbb\`.

The components are hex values (ranging from 00 to FF) and stand for:
- \`aa\` : alpha (opacity). \`00\` is fully transparent, \`FF\` is fully opaque, and \`7F\` is about 50% opaque
- \`rr\` : red component
- \`gg\` : green component
- \`bb\` : blue component

For example \`#ff0000\` is opaque red. \`#7fff0000\` is half-transparent red. \`#rrggbb\` (6 digits) is the same as \`#FFrrggbb\`. You can use [Sphere](https://colorsphere.app/) to pick a color. This applies to annotation colors too (\`Annotations.HighlightColor\`, underline, squiggly, strike-out).

`;

// ---------------------------------------------------------------------------
// Language list
// ---------------------------------------------------------------------------

const gLangs: string[][] = [
  ["af", "Afrikaans"],
  ["am", "Armenian (Հայերեն)"],
  ["ar", "Arabic (الْعَرَبيّة)"],
  ["az", "Azerbaijani (Azərbaycanca)"],
  ["bg", "Bulgarian (Български)"],
  ["bn", "Bengali (বাংলা)"],
  ["co", "Corsican (Corsu)"],
  ["br", "Portuguese - Brazil (Português)"],
  ["bs", "Bosnian (Bosanski)"],
  ["by", "Belarusian (Беларуская)"],
  ["ca", "Catalan (Català)"],
  ["ca-xv", "Catalan-Valencian (Català-Valencià)"],
  ["cn", "Chinese Simplified (简体中文)"],
  ["cy", "Welsh (Cymraeg)"],
  ["cz", "Czech (Čeština)"],
  ["de", "German (Deutsch)"],
  ["dk", "Danish (Dansk)"],
  ["el", "Greek (Ελληνικά)"],
  ["en", "English"],
  ["es", "Spanish (Español)"],
  ["et", "Estonian (Eesti)"],
  ["eu", "Basque (Euskara)"],
  ["fa", "Persian (فارسی)"],
  ["fi", "Finnish (Suomi)"],
  ["fr", "French (Français)"],
  ["fo", "Faroese (Føroyskt)"],
  ["fy-nl", "Frisian (Frysk)"],
  ["ga", "Irish (Gaeilge)"],
  ["gl", "Galician (Galego)"],
  ["he", "Hebrew (עברית)"],
  ["hi", "Hindi (हिंदी)"],
  ["hr", "Croatian (Hrvatski)"],
  ["hu", "Hungarian (Magyar)"],
  ["id", "Indonesian (Bahasa Indonesia)"],
  ["it", "Italian (Italiano)"],
  ["ja", "Japanese (日本語)"],
  ["jv", "Javanese (ꦧꦱꦗꦮ)"],
  ["ka", "Georgian (ქართული)"],
  ["kr", "Korean (한국어)"],
  ["ku", "Kurdish (كوردی)"],
  ["kw", "Cornish (Kernewek)"],
  ["lt", "Lithuanian (Lietuvių)"],
  ["lv", "Latvian (latviešu valoda)"],
  ["mk", "Macedonian (македонски)"],
  ["ml", "Malayalam (മലയാളം)"],
  ["mm", "Burmese (ဗမာ စာ)"],
  ["my", "Malaysian (Bahasa Melayu)"],
  ["ne", "Nepali (नेपाली)"],
  ["nl", "Dutch (Nederlands)"],
  ["nn", "Norwegian Neo-Norwegian (Norsk nynorsk)"],
  ["no", "Norwegian (Norsk)"],
  ["pa", "Punjabi (ਪੰਜਾਬੀ)"],
  ["pl", "Polish (Polski)"],
  ["pt", "Portuguese - Portugal (Português)"],
  ["ro", "Romanian (Română)"],
  ["ru", "Russian (Русский)"],
  ["sat", "Santali (ᱥᱟᱱᱛᱟᱲᱤ)"],
  ["si", "Sinhala (සිංහල)"],
  ["sk", "Slovak (Slovenčina)"],
  ["sl", "Slovenian (Slovenščina)"],
  ["sn", "Shona (Shona)"],
  ["sp-rs", "Serbian (Latin)"],
  ["sq", "Albanian (Shqip)"],
  ["sr-rs", "Serbian (Cyrillic)"],
  ["sv", "Swedish (Svenska)"],
  ["ta", "Tamil (தமிழ்)"],
  ["th", "Thai (ภาษาไทย)"],
  ["tl", "Tagalog (Tagalog)"],
  ["tr", "Turkish (Türkçe)"],
  ["tw", "Chinese Traditional (繁體中文)"],
  ["uk", "Ukrainian (Українська)"],
  ["uz", "Uzbek (O'zbek)"],
  ["vn", "Vietnamese (Việt Nam)"],
];

// ---------------------------------------------------------------------------
// Website update
// ---------------------------------------------------------------------------

function getWebsiteDir(): string {
  return join("..", "hack", "webapps", "sumatra-website");
}

async function getCurrentBranch(dir: string): Promise<string> {
  const out = await runCapture("git", ["branch"], dir);
  if (out.includes("(HEAD detached")) return "master";
  const lines = out.split("\n").map((l) => l.trim());
  for (const l of lines) {
    if (l.startsWith("* ")) return l.slice(2);
  }
  return "";
}

async function updateSumatraWebsite(): Promise<string> {
  let dir = resolve(getWebsiteDir());
  console.log(`sumatra website dir: '${dir}'`);
  if (!existsSync(dir)) {
    throw new Error(`directory for sumatra website '${dir}' doesn't exist`);
  }
  if (!(await isGitClean(dir))) {
    throw new Error(`github repository '${dir}' must be clean`);
  }
  const branch = await getCurrentBranch(dir);
  if (branch !== "master") {
    throw new Error(`github repository '${dir}' must be on master branch`);
  }
  await runLogged("git", ["pull"], dir);
  dir = join(dir, "www");
  if (!existsSync(dir)) {
    throw new Error(`directory for sumatra website '${dir}' doesn't exist`);
  }
  return dir;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

export async function main(opts?: { formatOutput?: boolean }) {
  const timeStart = performance.now();
  const updateWebsite = process.argv.includes("-website");

  const ver = extractSumatraVersion();
  const verUrlized = ver.replaceAll(".", "-");

  const settingsFileName = `settings${verUrlized}.html`;
  const langsFileName = `langs${verUrlized}.html`;

  const helpURI = `For documentation, see https://www.sumatrapdfreader.org/settings/settings${verUrlized}.html`;
  globalPrefs[0].Comment = helpURI;

  // Generate C header
  let s = genSettingsStruct();
  s = s.replaceAll("\t", "    ");
  const settingsPath = join("src", "Settings.h");
  writeFileMust(settingsPath, s);
  if (opts?.formatOutput !== false) {
    const rootDir = join(import.meta.dir, "..");
    await clangFormatFiles(rootDir, [settingsPath]);
  }
  console.log(`Wrote '${settingsPath}'`);

  // Generate settings markdown
  {
    const inside = genStructMarkdown(globalPrefsStruct, "");
    const mdContent = tmplMarkdown.replaceAll("%INSIDE%", inside);
    const mdPath = join("docs", "md", "Advanced-options-settings.md");
    const existing = readFileSync(mdPath, "utf-8");
    const marker = "## Settings";
    const idx = existing.indexOf(marker);
    if (idx === -1) throw new Error(`marker '${marker}' not found in '${mdPath}'`);
    const prefix = existing.slice(0, idx + marker.length) + "\n\n";
    writeFileMust(mdPath, prefix + mdContent);
    console.log(`Wrote '${mdPath}'`);
  }

  if (updateWebsite) {
    const websiteDir = await updateSumatraWebsite();
    const websiteSettingsDir = join(websiteDir, "settings");

    // Generate settings HTML
    {
      const inside = genStructHTML(globalPrefsStruct, "");
      let html = tmplHTML.replaceAll("%INSIDE%", inside);
      html = html.replaceAll("%VER%", ver);
      html = html.replaceAll("langs.html", langsFileName);
      const path = join(websiteSettingsDir, settingsFileName);
      writeFileMust(path, html);
      console.log(`Wrote '${path}'`);
    }

    // Generate langs HTML
    {
      const langs = gLangs.map((el) => ({ name: el[1], code: el[0] }));
      langs.sort((a, b) => a.name.localeCompare(b.name));
      const rows = langs.map((l) => `<tr><td>${l.name}</td><td>${l.code}</td></tr>`);
      const inside = rows.join("\n");
      let html = tmplLangsHTML.replaceAll("%INSIDE%", inside);
      html = html.replaceAll("%VER%", ver);
      html = html.replaceAll("%VER_URL%", verUrlized);
      html = html.replaceAll("settings.html", settingsFileName);
      html = html.replaceAll("\n", "\r\n");
      const path = join(websiteSettingsDir, langsFileName);
      writeFileMust(path, html);
    }

    console.log("!!!!!! checkin sumatra website repo!!!!");
  }

  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`gen-settings finished in ${elapsed}s`);
}

if (import.meta.main) {
  await main();
}
