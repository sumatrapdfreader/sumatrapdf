// gen-settings.ts - replaces Go "-gen-settings" flag
// Generates src/Settings.h, website HTML docs, and markdown docs from settings definitions

import { existsSync, readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { join, dirname, resolve, basename } from "node:path";
import { extractSumatraVersion, detectVisualStudio, runLogged, isGitClean } from "./util";

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

interface Field {
  Name: string;
  Type: Type;
  Default: any;
  Comment: string;
  NotSaved: boolean;
  CName: string;
  Expert: boolean;
  // saved & deserialized like any setting, but hidden from the advanced
  // settings dialog (app-managed / deprecated-for-migration values)
  Internal: boolean;
  DocComment: string;
  Version: string;
  StructName: string;
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

function mkField(name: string, typ: Type, def: any, comment: string): Field {
  const f: Field = {
    Name: name,
    Type: typ,
    Default: def,
    Comment: comment,
    NotSaved: false,
    CName: name !== "" ? toCName(name) : "",
    Expert: false,
    Internal: false,
    DocComment: comment,
    Version: "2.3",
    StructName: "",
  };
  return f;
}

function setExpert(f: Field): Field {
  f.Expert = true;
  return f;
}

function setInternal(f: Field): Field {
  f.Internal = true;
  return f;
}

function notSaved(f: Field): Field {
  f.NotSaved = true;
  return f;
}

function setVersion(f: Field, v: string): Field {
  f.Version = v;
  return f;
}

function setDoc(f: Field, s: string): Field {
  f.DocComment = s;
  return f;
}

function setStructName(f: Field, structName: string): Field {
  f.StructName = structName;
  if (f.Type.name === "Array") {
    f.Type.ctype = `Vec<${structName}*>*`;
  }
  if (f.Type.name === "Struct") {
    f.Type.ctype = structName;
  }
  if (f.Type.name === "Compact") {
    f.Type.ctype = structName;
  }
  return f;
}

function mkStruct(name: string, fields: Field[], comment: string): Field {
  const typ: Type = { name: "Struct", ctype: name };
  const res = mkField(name, typ, fields, comment);
  res.StructName = name;
  return res;
}

function mkCompactStruct(name: string, fields: Field[], comment: string): Field {
  const res = mkStruct(name, fields, comment);
  res.Type.name = "Compact";
  return res;
}

function mkArray(name: string, fields: Field[], comment: string): Field {
  let structName = name;
  if (structName.endsWith("s")) {
    structName = structName.slice(0, -1);
  }
  const ctype = `Vec<${structName}*>*`;
  const typ: Type = { name: "Array", ctype };
  const res = mkField(name, typ, fields, comment);
  res.StructName = structName;
  return res;
}

function mkCompactArray(name: string, typ: Type, def: any, comment: string): Field {
  const typ2: Type = { name: `${typ.name}Array`, ctype: `Vec<${typ.ctype}>*` };
  return mkField(name, typ2, def, comment);
}

function mkComment(comment: string): Field {
  return mkField("", Comment, null, comment);
}

function mkEmptyLine(): Field {
  return mkComment("");
}

function mkRGB(r: number, g: number, b: number): string {
  const hex = (n: number) => n.toString(16).padStart(2, "0");
  return `#${hex(r)}${hex(g)}${hex(b)}`;
}

function mkRGBA(r: number, g: number, b: number, a: number): string {
  const hex = (n: number) => n.toString(16).padStart(2, "0");
  return `#${hex(a)}${hex(r)}${hex(g)}${hex(b)}`;
}

function isComment(f: Field): boolean {
  return f.Type.name === "Comment";
}

const ebookUI: Field[] = [
  mkField("FontSize", Float, 0, "font size, default 8.0"),
  mkField("LayoutDx", Float, 0, "default is 420"),
  mkField("LayoutDy", Float, 0, "default is 595"),
  mkField("IgnoreDocumentCSS", Bool, false, "if true, we ignore ebook's CSS"),
  mkField("CustomCSS", Str, null, "custom CSS. Might need to set IgnoreDocumentCSS = true"),
  setVersion(
    mkField(
      "WindowBgCol",
      Color,
      "",
      "if given, sets the canvas background color for ebook documents (epub, mobi etc.)",
    ),
    "3.7",
  ),
];

const theme: Field[] = [
  mkField("Name", Str, "", "name of the theme"),
  mkField("TextColor", Color, "", "text color"),
  mkField("BackgroundColor", Color, "", "background color"),
  mkField("ControlBackgroundColor", Color, "", "control background color"),
  mkField("LinkColor", Color, "", "link color"),
  mkField("ColorizeControls", Bool, false, "should we colorize Windows controls and window areas"),
];

const tabFile: Field[] = [mkField("Path", Str, "", "file path")];

const tabGroup: Field[] = [
  mkField("Name", Str, "", "name of the tab group"),
  mkArray("TabFiles", tabFile, "files in the tab group"),
];

const windowPos: Field[] = [
  mkField("X", Int, 0, "x coordinate"),
  mkField("Y", Int, 0, "y coordinate"),
  mkField("Dx", Int, 0, "width"),
  mkField("Dy", Int, 0, "height"),
];

const pointPos: Field[] = [mkField("X", Int, 0, "x coordinate"), mkField("Y", Int, 0, "y coordinate")];

const keyboardShortcut: Field[] = [
  mkField("Cmd", Str, "", "command"),
  mkField("Key", Str, "", "keyboard shortcut (e.g. Ctrl-Alt-F)"),
  setVersion(mkField("Name", Str, null, "name shown in command palette"), "3.6"),
  setVersion(mkField("ToolbarText", Str, null, "if given, shows in toolbar"), "3.6"),
  setVersion(
    mkField(
      "ToolbarSvgIcon",
      Str,
      null,
      "optional SVG icon for toolbar button; if both ToolbarSvgIcon and ToolbarText are set, the icon is used",
    ),
    "3.7",
  ),
  notSaved(setVersion(mkField("CmdId", Int, null, "command id"), "3.6")),
];

const scrollPos: Field[] = [mkField("X", Float, 0, "x coordinate"), mkField("Y", Float, 0, "y coordinate")];

const fileTime: Field[] = [mkField("DwHighDateTime", Int, 0, ""), mkField("DwLowDateTime", Int, 0, "")];

const printerDefaults: Field[] = [
  mkField("PrintScale", Str, "shrink", "default value for scaling (shrink, fit, none)"),
  mkField("Collate", Str, "default", "default value for collate in the print dialog (default, collate, nocollate)"),
];

const forwardSearch: Field[] = [
  mkField(
    "HighlightOffset",
    Int,
    0,
    "when set to a positive value, the forward search highlight style will " +
      "be changed to a rectangle at the left of the page (with the indicated " +
      "amount of margin from the page margin)",
  ),
  mkField("HighlightWidth", Int, 15, "width of the highlight rectangle (if HighlightOffset is > 0)"),
  mkField("HighlightColor", Color, mkRGB(0x65, 0x81, 0xff), "color used for the forward search highlight"),
  mkField(
    "HighlightPermanent",
    Bool,
    false,
    "if true, highlight remains visible until the next mouse click " + "(instead of fading away immediately)",
  ),
];

const windowMarginFixedPageUI: Field[] = [
  mkField("Top", Int, 2, "size of the top margin between window and document"),
  mkField("Right", Int, 4, "size of the right margin between window and document"),
  mkField("Bottom", Int, 2, "size of the bottom margin between window and document"),
  mkField("Left", Int, 4, "size of the left margin between window and document"),
];

const windowMarginComicBookUI: Field[] = [
  mkField("Top", Int, 0, "size of the top margin between window and document"),
  mkField("Right", Int, 0, "size of the right margin between window and document"),
  mkField("Bottom", Int, 0, "size of the bottom margin between window and document"),
  mkField("Left", Int, 0, "size of the left margin between window and document"),
];

const pageSpacing: Field[] = [
  mkField("Dx", Int, 4, "horizontal difference"),
  mkField("Dy", Int, 4, "vertical difference"),
];

const fixedPageUI: Field[] = [
  mkField("TextColor", Color, mkRGB(0x00, 0x00, 0x00), "color value with which black (text) will be substituted"),
  mkField(
    "BackgroundColor",
    Color,
    mkRGB(0xff, 0xff, 0xff),
    "color value with which white (background) will be substituted",
  ),
  setVersion(
    mkField(
      "SelectionColor",
      Color,
      mkRGB(0xff, 0xff, 0x00),
      "color value for the text selection rectangle (also used to highlight found text). " +
        "Use an #aarrggbb value to control opacity: a smaller alpha (e.g. #40ffff00) makes the " +
        "selection more transparent so the selected text stays crisp; #rrggbb uses the default opacity",
    ),
    "2.4",
  ),
  mkCompactStruct(
    "WindowMargin",
    windowMarginFixedPageUI,
    "top, right, bottom and left margin (in that order) between window and document",
  ),
  setStructName(
    mkCompactStruct(
      "PageSpacing",
      pageSpacing,
      "horizontal and vertical distance between two pages in facing and book view modes",
    ),
    "Size",
  ),
  mkCompactArray(
    "GradientColors",
    Color,
    null,
    "colors to use for the gradient from top to bottom (stops will be inserted " +
      "at regular intervals throughout the document); currently only up to three " +
      "colors are supported; the idea behind this experimental feature is that the " +
      "background might allow to subconsciously determine reading progress; " +
      "suggested values: #2828aa #28aa28 #aa2828",
  ),
  mkField("InvertColors", Bool, false, "if true, TextColor and BackgroundColor of the document will be swapped"),
  setVersion(
    mkField(
      "WindowBgCol",
      Color,
      "",
      "if given, sets the canvas background color for PDF files",
    ),
    "3.7",
  ),
];

const comicBookUI: Field[] = [
  mkCompactStruct(
    "WindowMargin",
    windowMarginComicBookUI,
    "top, right, bottom and left margin (in that order) between window and document",
  ),
  setStructName(
    mkCompactStruct(
      "PageSpacing",
      pageSpacing,
      "horizontal and vertical distance between two pages in facing and book view modes",
    ),
    "Size",
  ),
  mkField(
    "CbxMangaMode",
    Bool,
    false,
    "if true, default to displaying Comic Book files in manga mode (from right to left if showing 2 pages at a time)",
  ),
  setVersion(
    mkField(
      "WindowBgCol",
      Color,
      "",
      "if given, sets the canvas background color for comic book files",
    ),
    "3.7",
  ),
];

const imageUI: Field[] = [
  setVersion(
    mkField("WindowBgCol", Color, "", "if given, sets the canvas background color for image files"),
    "3.7",
  ),
  setVersion(
    mkField(
      "DefaultZoom",
      Str,
      "shrink to fit",
      "default zoom for image files. valid values: fit page, fit width, fit content, shrink to fit or percent like 100%",
    ),
    "3.7",
  ),
  notSaved(mkField("DefaultZoomFloat", Float, 0, "value of DefaultZoom for internal usage")),
];

const chmUI: Field[] = [
  mkField(
    "UseFixedPageUI",
    Bool,
    false,
    "if true, the UI used for PDF documents will be used for CHM documents as well",
  ),
];

const markdownUI: Field[] = [
  mkField(
    "UseFixedPageUI",
    Bool,
    false,
    "if true, use MuPDF (cmark-gfm) to render markdown; if false, use WebView2 browser view when available",
  ),
];

const codexBuild: Field[] = [
  mkField("Model", Str, "gpt-5.5", "Codex model ID for -m (e.g. gpt-5.5, gpt-5.4, o3)"),
  mkField(
    "Models",
    Str,
    "",
    "extra Codex model IDs for the dropdown, comma-separated; gpt-5.5, gpt-5.4, and o3 are always included",
  ),
  mkField("Sandbox", Int, 1, "Codex sandbox mode: 0=read-only, 1=workspace-write, 2=danger-full-access"),
  mkField(
    "SkipSandbox",
    Bool,
    false,
    "if true, pass --dangerously-bypass-approvals-and-sandbox to Codex",
  ),
  mkField("BgColor", Color, "#ffffff", "background color of the OpenAI Codex chat panel"),
];

const grokBuild: Field[] = [
  mkField(
    "Model",
    Str,
    "grok-composer-2.5-fast",
    "Grok model ID for --model (e.g. grok-composer-2.5-fast, grok-build)",
  ),
  mkField(
    "Models",
    Str,
    "",
    "extra Grok model IDs for the dropdown, comma-separated; grok-composer-2.5-fast and grok-build are always included",
  ),
  mkField("Effort", Int, 1, "Grok effort level: 0=Low, 1=Medium, 2=High, 3=XHigh, 4=Max"),
  mkField(
    "AlwaysApprove",
    Bool,
    false,
    "if true, pass --always-approve to Grok Build (auto-approve tool executions)",
  ),
  mkField("BgColor", Color, "#ffffff", "background color of the Grok Build chat panel"),
];

const claudeCode: Field[] = [
  mkField(
    "Model",
    Str,
    "sonnet",
    "Claude model alias for --model (e.g. sonnet, opus, haiku); uses opus if not in the model list",
  ),
  mkField(
    "Models",
    Str,
    "",
    "extra Claude model aliases for the dropdown, comma-separated; sonnet, opus, and haiku are always included",
  ),
  mkField("Effort", Int, 1, "Claude effort level: 0=Low, 1=Medium, 2=High, 3=Max"),
  mkField(
    "SkipPermissions",
    Bool,
    false,
    "if true, pass --dangerously-skip-permissions to Claude Code",
  ),
  mkField("BgColor", Color, "#ffffff", "background color of the Claude Code chat panel"),
];

const fullscreen: Field[] = [
  mkField("ShowToolbar", Bool, false, "if true, show the toolbar in fullscreen mode"),
  mkField("ShowMenubar", Bool, false, "if true, show the menu bar in fullscreen mode"),
];

const externalViewer: Field[] = [
  mkField(
    "CommandLine",
    Str,
    null,
    "command line with which to call the external viewer, may contain " +
      '%p for page number and "%1" for the file name (add quotation ' +
      "marks around paths containing spaces)",
  ),
  mkField("Name", Str, null, "name of the external viewer to be shown in the menu (implied by CommandLine if missing)"),
  mkField(
    "Filter",
    Str,
    null,
    "optional filter for which file types the menu item is to be shown; separate multiple entries using ';' and don't include any spaces (e.g. *.pdf;*.xps for all PDF and XPS documents)",
  ),
  setVersion(mkField("Key", Str, null, "optional: keyboard shortcut e.g. Alt + 7"), "3.6"),
  setVersion(mkField("ToolbarText", Str, null, "if given, shows in toolbar"), "3.7"),
  setVersion(
    mkField(
      "ToolbarSvgIcon",
      Str,
      null,
      "optional SVG icon for toolbar button; if both ToolbarSvgIcon and ToolbarText are set, the icon is used",
    ),
    "3.7",
  ),
];

const selectionHandler: Field[] = [
  mkField(
    "URL",
    Str,
    null,
    "url to invoke for the selection. ${selection} will be replaced with current selection and ${userlang} with language code for current UI (e.g. 'de' for German)",
  ),
  mkField("Name", Str, null, "name shown in context menu"),
  setVersion(mkField("Key", Str, null, "keyboard shortcut"), "3.6"),
];

const annotations: Field[] = [
  mkField("HighlightColor", Color, mkRGB(0xff, 0xff, 0x0), "highlight annotation color"),
  mkField("UnderlineColor", Color, mkRGB(0x00, 0xff, 0x0), "underline annotation color"),
  setVersion(mkField("SquigglyColor", Color, mkRGB(0xff, 0x00, 0xff), "squiggly annotation color"), "3.5"),
  setVersion(mkField("StrikeOutColor", Color, mkRGB(0xff, 0x00, 0x00), "strike out annotation color"), "3.5"),
  setVersion(mkField("FreeTextColor", Color, "", "text color of free text annotation"), "3.5"),
  setVersion(mkField("FreeTextBackgroundColor", Color, "", "background color of free text annotation"), "3.6"),
  setVersion(
    mkField(
      "FreeTextOpacity",
      Int,
      100,
      "opacity of free text annotation in percent (0-100); 0 - fully transparent (invisible), 50 - half transparent, 100 - fully opaque",
    ),
    "3.6",
  ),
  setVersion(mkField("FreeTextSize", Int, 12, "size of free text annotation"), "3.5"),
  setVersion(mkField("FreeTextBorderWidth", Int, 1, "width of free text annotation border"), "3.5"),
  mkField("TextIconColor", Color, "", "text icon annotation color"),
  mkField(
    "TextIconType",
    Str,
    "",
    "type of text annotation icon: comment, help, insert, key, new paragraph, note, paragraph. If not set: note.",
  ),
  setVersion(
    mkField(
      "DefaultAuthor",
      Str,
      "",
      "default author for created annotations, use (none) to not add an author at all. If not set will use Windows user name",
    ),
    "3.4",
  ),
];

const favorite: Field[] = [
  mkField("Name", Str, null, "name of this favorite as shown in the menu"),
  mkField("PageNo", Int, 0, "number of the bookmarked page"),
  mkField(
    "PageLabel",
    Str,
    null,
    "label for this page (only present if logical and physical page numbers are not the same)",
  ),
  notSaved(mkField("MenuId", Int, 0, "id of this favorite in the menu (assigned by AppendFavMenuItems)")),
];

const fileSettings: Field[] = [
  mkField("FilePath", Str, null, "path of the document"),
  mkArray("Favorites", favorite, "Values which are persisted for bookmarks/favorites"),
  mkField(
    "IsPinned",
    Bool,
    false,
    'a document can be "pinned" to the Frequently Read list so that it ' +
      "isn't displaced by recently opened documents",
  ),
  setDoc(
    mkField(
      "IsMissing",
      Bool,
      false,
      "if a document can no longer be found but we still remember valuable state, " +
        "it's classified as missing so that it can be hidden instead of removed",
    ),
    "if true, the file is considered missing and won't be shown in any list",
  ),
  setDoc(
    mkField(
      "OpenCount",
      Int,
      0,
      "in order to prevent documents that haven't been opened for a while " +
        "but used to be opened very frequently constantly remain in top positions, " +
        "the openCount will be cut in half after every week, so that the " +
        "Frequently Read list hopefully better reflects the currently relevant documents",
    ),
    "number of times this document has been opened recently",
  ),
  setDoc(
    mkField(
      "DecryptionKey",
      Str,
      null,
      "Hex encoded MD5 fingerprint of file content (32 chars) followed by " +
        "crypt key (64 chars) - only applies for PDF documents",
    ),
    "data required to open a password protected document without having to " + "ask for the password again",
  ),
  mkField(
    "UseDefaultState",
    Bool,
    false,
    "if true, we use global defaults when opening this file (instead of " + "the values below)",
  ),
  setDoc(
    mkField(
      "DisplayMode",
      Str,
      "automatic",
      "how pages should be laid out for this document, needs to be synchronized with " +
        "DefaultDisplayMode after deserialization and before serialization",
    ),
    "layout of pages. valid values: automatic, single page, facing, book view, " +
      "continuous, continuous facing, continuous book view",
  ),
  setStructName(
    mkCompactStruct("ScrollPos", scrollPos, "how far this document has been scrolled (in x and y direction)"),
    "PointF",
  ),
  mkField("PageNo", Int, 1, "number of the last read page"),
  mkField("Zoom", Str, "fit page", "zoom (in %) or one of those values: fit page, fit width, fit content"),
  mkField("Rotation", Int, 0, "how far pages have been rotated as a multiple of 90 degrees"),
  mkField(
    "WindowState",
    Int,
    0,
    "state of the window. 1 is normal, 2 is maximized, " + "3 is fullscreen, 4 is minimized",
  ),
  setStructName(mkCompactStruct("WindowPos", windowPos, "default position (can be on any monitor)"), "Rect"),
  mkField(
    "ShowToc",
    Bool,
    true,
    "if true, we show table of contents (Bookmarks) sidebar if it's present " + "in the document",
  ),
  mkField("SidebarDx", Int, 0, "width of the left sidebar panel containing the table of contents"),
  mkField(
    "DisplayR2L",
    Bool,
    false,
    "if true, the document is displayed right-to-left in facing and book view modes " +
      "(only used for comic book documents)",
  ),
  setVersion(
    mkField("BgCol", Color, "", "if given, overrides the background color for this document"),
    "3.7",
  ),
  setVersion(
    mkField("TabCol", Color, "", "if given, overrides the tab color for this document"),
    "3.7",
  ),
  setDoc(
    mkField(
      "ReparseIdx",
      Int,
      0,
      "index into an ebook's HTML data from which reparsing has to happen " +
        "in order to restore the last viewed page (i.e. the equivalent of PageNo for the ebook UI)",
    ),
    "data required to restore the last read page in the ebook UI",
  ),
  setDoc(
    mkCompactArray(
      "TocState",
      Int,
      null,
      "tocState is an array of ids for ToC items that have been toggled by " +
        "the user (i.e. aren't in their default expansion state). - " +
        "Note: We intentionally track toggle state as opposed to expansion state " +
        "so that we only have to save a diff instead of all states for the whole " +
        "tree (which can be quite large) (internal)",
    ),
    "data required to determine which parts of the table of contents have been expanded",
  ),
  notSaved(
    mkField(
      "Thumbnail",
      { name: "", ctype: "RenderedBitmap *" },
      "NULL",
      "thumbnails are saved as PNG files in sumatrapdfcache directory",
    ),
  ),
  notSaved(
    mkField("Index", Int, 0, "temporary value needed for FileHistory::cmpOpenCount"),
  ),
  notSaved(mkField("Himl", { name: "", ctype: "HIMAGELIST" }, "NULL", "")),
  notSaved(mkField("IconIdx", Int, -1, "")),
];

const tabState: Field[] = [
  mkField("FilePath", Str, null, "path of the document"),
  mkField("DisplayMode", Str, "automatic", "same as FileStates -> DisplayMode"),
  mkField("PageNo", Int, 1, "number of the last read page"),
  mkField("Zoom", Str, "fit page", "same as FileStates -> Zoom"),
  mkField("Rotation", Int, 0, "same as FileStates -> Rotation"),
  setStructName(
    mkCompactStruct("ScrollPos", scrollPos, "how far this document has been scrolled (in x and y direction)"),
    "PointF",
  ),
  mkField("ShowToc", Bool, true, "if true, the table of contents was shown when the document was closed"),
  mkCompactArray("TocState", Int, null, "same as FileStates -> TocState"),
];

const sessionData: Field[] = [
  setDoc(
    mkArray(
      "TabStates",
      tabState,
      "a subset of FileState required for restoring the state of a single tab " +
        "(required for handling documents being opened twice)",
    ),
    "data required for restoring the view state of a single tab",
  ),
  mkField("TabIndex", Int, 1, "index of the currently selected tab (1-based)"),
  mkField("WindowState", Int, 0, "same as FileState -> WindowState"),
  setStructName(mkCompactStruct("WindowPos", windowPos, "default position (can be on any monitor)"), "Rect"),
  mkField("SidebarDx", Int, 0, "width of favorites/bookmarks sidebar (if shown)"),
];

const globalPrefs: Field[] = [
  mkComment(""),
  mkEmptyLine(),

  setDoc(
    mkField(
      "DefaultDisplayMode",
      Str,
      "automatic",
      "how pages should be laid out by default, needs to be synchronized with " +
        "DefaultDisplayMode after deserialization and before serialization",
    ),
    "default layout of pages. valid values: automatic, single page, facing, " +
      "book view, continuous, continuous facing, continuous book view",
  ),
  mkField(
    "DefaultZoom",
    Str,
    "fit page",
    "default zoom. valid values: fit page, fit width, fit content or percent like 100%",
  ),
  setVersion(
    setExpert(
      mkField(
        "DisableJavaScript",
        Bool,
        false,
        "if true, JavaScript in PDF documents is disabled (e.g. form-field calculations won't run)",
      ),
    ),
    "3.7",
  ),
  setVersion(
    setExpert(
      mkField(
        "AllowExternalImages",
        Bool,
        false,
        "if true, a PDF may load an image stored in a separate file referenced by name (an external " +
          "image stream); the file must sit next to the PDF. Off by default for security (matches Acrobat)",
      ),
    ),
    "3.7",
  ),
  mkField(
    "EnableTeXEnhancements",
    Bool,
    false,
    "if true, we expose the SyncTeX inverse search command line in Settings -> Options",
  ),
  setExpert(mkField("EscToExit", Bool, false, "if true, Esc key closes SumatraPDF")),
  setVersion(
    setExpert(mkField("FullPathInTitle", Bool, false, "if true, we show the full path to a file in the title bar")),
    "3.0",
  ),
  mkField("InverseSearchCmdLine", Str, null, "pattern used to launch the LaTeX editor when doing inverse search"),
  setVersion(
    mkField(
      "LazyLoading",
      Bool,
      false,
      "when restoring session, delay loading of documents until their tab is selected",
    ),
    "3.6",
  ),
  setExpert(
    mkField(
      "MainWindowBackground",
      Color,
      mkRGBA(0xff, 0xf2, 0x00, 0x80),
      "background color of the non-document windows, traditionally yellow",
    ),
  ),
  mkField("NoHomeTab", Bool, false, "if true, doesn't open Home tab"),
  mkField(
    "HomePageSortByFrequentlyRead",
    Bool,
    false,
    "if true implements pre-3.6 behavior of showing opened files by frequently used count. If false, shows most recently opened first",
  ),
  mkField(
    "HomePageShowList",
    Bool,
    false,
    "if true, shows the home page document history as a list instead of thumbnails",
  ),
  setVersion(
    setExpert(
      mkField(
        "ReloadModifiedDocuments",
        Bool,
        true,
        "if true, a document will be reloaded automatically whenever it's changed " +
          "(currently doesn't work for documents shown in the ebook UI)",
      ),
    ),
    "2.5",
  ),
  mkField("RememberOpenedFiles", Bool, true, "if true, we remember which files we opened and their display settings"),
  mkField(
    "RememberStatePerDocument",
    Bool,
    true,
    "if true, we store display settings for each document separately (i.e. everything " +
      "after UseDefaultState in FileStates)",
  ),
  setExpert(
    mkField(
      "RestoreSession",
      Bool,
      true,
      "if true and SessionData isn't empty, that session will be restored at startup",
    ),
  ),
  setExpert(mkField("ReuseInstance", Bool, true, "if true, we'll always open files using existing SumatraPDF process")),
  setVersion(
    setExpert(
      mkField(
        "ShowMenubar",
        Bool,
        true,
        "if false, the menu bar will be hidden (use F9 to toggle, persisted across sessions)",
      ),
    ),
    "2.5",
  ),
  setVersion(
    mkField("ShowMenubarWithTabs", Bool, false, "if true, show the menu bar when using tabs (useTabs = true)"),
    "3.7",
  ),
  setVersion(mkField("ShowTips", Bool, true, "if true, we show tips on the home page"), "3.7"),
  setVersion(setInternal(mkField("CustomColors", Str, null, "up to 13 custom colors for the background color picker, separated by space (e.g. '#ff0000 #00ff00 #0000ff')")), "3.7"),
  mkField("ShowToolbar", Bool, true, "if true, we show the toolbar at the top of the window"),
  setVersion(
    mkField(
      "Toolbar",
      Str,
      null,
      "toolbar mode: show (pinned), hide (no toolbar), overlay (toolbar floats over " +
        "the page, sized to its natural width and centered, only shown when the mouse is near it). " +
        "if empty, derived from ShowToolbar",
    ),
    "3.7",
  ),
  setVersion(
    mkField(
      "ToolbarPosition",
      Str,
      "top",
      "where the toolbar is placed: top or bottom (applies to both show and overlay modes)",
    ),
    "3.7",
  ),
  setVersion(
    setExpert(
      mkField(
        "SearchUIFloating",
        Bool,
        false,
        "if true, the find UI is a floating, movable window with a results list " +
          "instead of the compact toolbar overlay",
      ),
    ),
    "3.7",
  ),
  mkField("ShowFavorites", Bool, false, "if true, we show the Favorites sidebar"),
  mkField(
    "ShowToc",
    Bool,
    true,
    "if true, we show table of contents (Bookmarks) sidebar if it's present " + "in the document",
  ),
  setVersion(mkField("ShowLinks", Bool, false, "if true we draw a blue border around links in the document"), "3.6"),
  mkField(
    "ShowStartPage",
    Bool,
    true,
    "if true, we show a list of frequently read documents when no document is loaded",
  ),
  setInternal(mkField("SidebarDx", Int, 0, "width of favorites/bookmarks sidebar (if shown)")),
  setVersion(
    mkField(
      "Scrollbars",
      Str,
      "windows",
      "scrollbar mode: windows (standard Windows scrollbar), smart (overlay scrollbar with auto-hide), overlay (always visible overlay scrollbar), hidden (no scrollbars)",
    ),
    "3.7",
  ),
  setVersion(mkField("ScrollbarInSinglePage", Bool, false, "if true, we show scrollbar in single page mode"), "3.6"),
  setVersion(mkField("SmoothScroll", Bool, false, "if true, implements smooth scrolling"), "3.6"),
  setVersion(
    mkField(
      "CitationHoverDelay",
      Int,
      -1,
      "how long to hover an internal-document link (in ms) before we show a popup rendering the destination region (citation entry, figure, footnote). -1 (the default) disables the popup; set a positive value like 300 to enable it",
    ),
    "3.7",
  ),
  setVersion(
    setExpert(
      mkField(
        "ReadAloudVoiceId",
        Str,
        null,
        "voice id for Read Aloud text-to-speech; empty or unset means system default. Voice ids match those used internally by the Read Aloud Voice menu (WinRT voice id or SAPI token id)",
      ),
    ),
    "3.7",
  ),
  setVersion(
    setExpert(
      mkField(
        "ReadAloudSpeed",
        Float,
        1,
        "playback speed multiplier for Read Aloud text-to-speech (0.5 .. 3.0), 1 is normal speed; can also be changed from the Read Aloud playback bar",
      ),
    ),
    "3.7",
  ),
  setVersion(
    mkField(
      "FastScrollOverScrollbar",
      Bool,
      false,
      "if true, mouse wheel scrolling is faster when mouse is over a scrollbar",
    ),
    "3.6",
  ),
  mkField(
    "PreventSleepInFullscreen",
    Bool,
    true,
    "if true, prevents the screen from turning off when in fullscreen or presentation mode",
  ),
  mkField("TabWidth", Int, 300, "maximum width of a single tab"),
  setDoc(
    setVersion(mkField("Theme", Str, "", "the name of the theme to use"), "3.5"),
    "Valid themes: light, dark, darker",
  ),
  setInternal(
    mkField(
      "TocDy",
      Int,
      0,
      "if both favorites and bookmarks parts of sidebar are visible, this is " +
        "the height of bookmarks (table of contents) part",
    ),
  ),
  setVersion(mkField("ToolbarSize", Int, 18, "height of toolbar"), "3.4"),
  mkField(
    "TreeFontName",
    Str,
    "automatic",
    "font name for bookmarks and favorites tree views. automatic means Windows default",
  ),
  setVersion(
    mkField("TreeFontSize", Int, 0, "font size for bookmarks and favorites tree views. 0 means Windows default"),
    "3.3",
  ),
  setVersion(mkField("UIFontSize", Int, 0, "over-ride application font size. 0 means Windows default"), "3.6"),
  setVersion(
    mkField("DisableAntiAlias", Bool, false, "if true, disables anti-aliasing for rendering PDF documents"),
    "3.6",
  ),
  setExpert(
    mkField(
      "DisableAutoLinks",
      Bool,
      false,
      "if true, disables auto-linking of URLs and email addresses found in PDF text",
    ),
  ),
  setExpert(
    mkField(
      "UseSysColors",
      Bool,
      false,
      "if true, we use Windows system colors for background/text color. Over-rides other settings",
    ),
  ),
  setVersion(mkField("UseTabs", Bool, true, "if true, documents are opened in tabs instead of new windows"), "3.0"),
  mkField(
    "TabsMru",
    Bool,
    false,
    "if true, Ctrl+Tab and Ctrl+Shift+Tab show the tab switcher in most recently used order instead of tab-strip order",
  ),
  setDoc(
    setExpert(
      mkCompactArray(
        "ZoomLevels",
        Float,
        "",
        "zoom levels which zooming steps through in addition to Fit Page, Fit Width and " +
          "the minimum and maximum allowed values (8.33 and 6400)",
      ),
    ),
    "sequence of zoom levels when zooming in/out; all values must lie between 8.33 and 6400",
  ),
  notSaved(mkCompactArray("ZoomLevelsCmdIds", Int, "", "")),
  setExpert(
    mkField(
      "ZoomIncrement",
      Float,
      0,
      "zoom step size in percents relative to the current zoom level. " +
        "if zero or negative, the values from ZoomLevels are used instead",
    ),
  ),

  mkEmptyLine(),

  setExpert(mkStruct("FixedPageUI", fixedPageUI, "customization options for PDF, XPS, DjVu and PostScript UI")),
  mkEmptyLine(),
  setExpert(mkStruct("EBookUI", ebookUI, "customization options for eBookUI")),
  mkEmptyLine(),
  setExpert(mkStruct("ComicBookUI", comicBookUI, "customization options for Comic Book UI")),
  mkEmptyLine(),
  setExpert(mkStruct("ImageUI", imageUI, "customization options for image files UI")),
  mkEmptyLine(),
  setExpert(
    mkStruct(
      "ChmUI",
      chmUI,
      "customization options for CHM UI. If UseFixedPageUI is true, FixedPageUI settings apply instead",
    ),
  ),
  mkEmptyLine(),
  setExpert(
    mkStruct(
      "MarkdownUI",
      markdownUI,
      "customization options for Markdown UI. If UseFixedPageUI is true, MuPDF is used; otherwise WebView2 browser view is used when available",
    ),
  ),
  mkEmptyLine(),
  setVersion(
    setExpert(mkStruct("ClaudeCode", claudeCode, "settings for the Claude Code chat sidebar")),
    "3.7",
  ),
  mkEmptyLine(),
  setVersion(
    setExpert(mkStruct("GrokBuild", grokBuild, "settings for the Grok Build chat sidebar")),
    "3.7",
  ),
  mkEmptyLine(),
  setVersion(
    setExpert(mkStruct("CodexBuild", codexBuild, "settings for the OpenAI Codex chat sidebar")),
    "3.7",
  ),
  mkEmptyLine(),
  setVersion(
    setInternal(
      setExpert(
        mkField(
          "AIChatSidebarDx",
          Int,
          0,
          "width of the AI chat sidebar (0 = use default); shared by Claude Code, Grok Build, and OpenAI Codex (internal)",
        ),
      ),
    ),
    "3.7",
  ),
  mkEmptyLine(),
  setVersion(
    setExpert(
      mkField(
        "TranslateToLang",
        Str,
        "",
        "remembered destination language for selection translation; empty uses OS UI language",
      ),
    ),
    "3.7",
  ),
  mkEmptyLine(),
  setVersion(setExpert(mkStruct("Annotations", annotations, "default values for annotations in PDF documents")), "3.3"),
  mkEmptyLine(),
  setExpert(
    mkArray(
      "ExternalViewers",
      externalViewer,
      "list of additional external viewers for various file types. " +
        "See [docs for more information](https://www.sumatrapdfreader.org/docs/Customize-external-viewers)",
    ),
  ),
  mkEmptyLine(),
  setExpert(
    mkStruct(
      "ForwardSearch",
      forwardSearch,
      "customization options for how we show forward search results (used from " + "LaTeX editors)",
    ),
  ),
  mkEmptyLine(),
  setExpert(mkStruct("PrinterDefaults", printerDefaults, "these override the default settings in the Print dialog")),
  mkEmptyLine(),
  setVersion(setExpert(mkStruct("Fullscreen", fullscreen, "options for fullscreen mode")), "3.7"),
  mkEmptyLine(),
  mkArray(
    "SelectionHandlers",
    selectionHandler,
    "list of handlers for selected text, shown in context menu when text selection is active. See [docs for more information](https://www.sumatrapdfreader.org/docs/Customize-search-translation-services)",
  ),
  mkEmptyLine(),
  mkArray("Shortcuts", keyboardShortcut, "custom keyboard shortcuts"),
  mkEmptyLine(),
  setVersion(mkArray("Themes", theme, "color themes"), "3.6"),
  mkEmptyLine(),
  setVersion(mkArray("TabGroups", tabGroup, "saved groups of tabs"), "3.7"),
  mkEmptyLine(),

  setVersion(
    setExpert(
      mkField(
        "CustomScreenDPI",
        Int,
        0,
        "actual resolution of the main screen in DPI (if this value " +
          "isn't positive, the system's UI setting is used)",
      ),
    ),
    "2.5",
  ),
  mkEmptyLine(),

  mkComment("You're not expected to change those manually"),
  setDoc(
    setVersion(
      setExpert(
        mkCompactArray("DefaultPasswords", Str, null, "passwords to try when opening a password protected document"),
      ),
      "2.4",
    ),
    "a whitespace separated list of passwords to try when opening a password protected document " +
      "(passwords containing spaces must be quoted)",
  ),
  setDoc(
    mkField("UiLanguage", Str, null, "ISO code of the current UI language"),
    "[ISO code](langs.html) of the current UI language",
  ),
  mkField("VersionToSkip", Str, null, "we won't ask again to update to this version"),
  setDoc(
    mkField("WindowState", Int, 1, "default state of new windows (same as the last closed)"),
    "default state of the window. 1 is normal, 2 is maximized, " + "3 is fullscreen, 4 is minimized",
  ),
  setDoc(
    setStructName(mkCompactStruct("WindowPos", windowPos, "default position (can be on any monitor)"), "Rect"),
    "default position (x, y) and size (width, height) of the window",
  ),
  setStructName(
    mkCompactStruct("SearchUIWindowPos", windowPos, "position/size of the floating find window (see SearchUIFloating)"),
    "Rect",
  ),

  mkArray("FileStates", fileSettings, "information about opened files (in most recently used order)"),
  setVersion(mkArray("SessionData", sessionData, "state of the last session, usage depends on RestoreSession"), "3.1"),

  setDoc(
    setVersion(
      mkCompactArray(
        "ReopenOnce",
        Str,
        null,
        "a list of paths for files to be reopened at the next start " +
          'or the string "SessionData" if this data is saved in SessionData ' +
          "(needed for auto-updating)",
      ),
      "3.0",
    ),
    "data required for reloading documents after an auto-update",
  ),
  setDoc(
    setStructName(mkCompactStruct("TimeOfLastUpdateCheck", fileTime, "timestamp of the last update check"), "FILETIME"),
    "data required to determine when SumatraPDF last checked for updates",
  ),

  setDoc(
    mkField("OpenCountWeek", Int, 0, 'week count since 2011-01-01 needed to "age" openCount values in file history'),
    "value required to determine recency for the OpenCount value in FileStates",
  ),
  notSaved(
    setStructName(
      mkCompactStruct("LastPrefUpdate", fileTime, "modification time of the preferences file when it was last read"),
      "FILETIME",
    ),
  ),
  notSaved(
    mkField(
      "DefaultDisplayModeEnum",
      { name: "", ctype: "DisplayMode" },
      "DM_AUTOMATIC",
      "value of DefaultDisplayMode for internal usage",
    ),
  ),
  notSaved(mkField("DefaultZoomFloat", Float, -1, "value of DefaultZoom for internal usage")),
  setStructName(mkCompactStruct("PropWinPos", pointPos, "position of the document properties window"), "Point"),
  // saved & honored, but hidden from the advanced settings dialog (edited via
  // the "Automatically check for updates" checkbox in Options instead)
  setInternal(mkField("CheckForUpdates", Bool, true, "if true, we check once a day if an update is available")),
  mkEmptyLine(),
  mkComment("Settings below are not recognized by the current version"),
];

const globalPrefsStruct = mkStruct("GlobalPrefs", globalPrefs, "Preferences are persisted in SumatraPDF-settings.txt");

const themes: Field[] = [setVersion(mkArray("Themes", theme, "color themes"), "3.6")];
const themesStruct = mkStruct("Themes", themes, "for parsing themes");

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
  if (["Struct", "Array", "Compact", "Prerelease"].includes(typeName)) {
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
    lines.push(`\t${field.Type.ctype} ${field.CName};`);
    if (field.Type.name === "Color") {
      lines.push(`\tParsedColor ${field.CName}Parsed;`);
    } else if (["Struct", "Compact", "Array", "Prerelease"].includes(field.Type.name)) {
      const name = field.Name;
      if (name === field.StructName || name === field.StructName + "s") {
        if (built[name] === undefined) {
          const s = buildStruct(field, built);
          required.push(s, "");
          built[name] = (built[name] || 0) + 1;
        }
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
    if (["Struct", "Prerelease", "Compact", "Array"].includes(field.Type.name)) {
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
  lines.push(
    `static ${constStr}StructInfo g${fullName}Info = { sizeof(${struc.StructName}), ${names.length}, g${fullName}Fields, "${namesStr}", "${commentsStr}" };`,
  );
  return lines.join("\n");
}

const settingsStructsHeader = `// !!!!! This file is auto-generated by do/settings_gen_code.go

/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

struct RenderedBitmap;

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
constexpr float kZoomActualSize = 100.0F;
constexpr float kZoomMax = 6400.F; /* max zoom in % */
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
  let insideExpert = false;

  const fields = struc.Default as Field[];
  for (const field of fields) {
    if (field.NotSaved || isComment(field)) continue;
    const startIdx = lines.length;
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
    } else if (field.Type.name === "Struct") {
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
    if (field.Expert && !insideExpert) {
      lines[startIdx] = `<div>` + lines[startIdx];
    } else if (!field.Expert && insideExpert) {
      lines[startIdx] = `</div>` + lines[startIdx];
    }
    insideExpert = field.Expert;
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
    } else if (field.Type.name === "Struct") {
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
For example #ff0000 means red color. You can use <a href="https://galactic.ink/sphere/">Sphere</a> to pick a color.
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
- \`aa\` : alpha (transparency). ff is fully transparent, 0 is not transparent, and 7f is 50% transparent
- \`rr\` : red component
- \`gg\` : green component
- \`bb\` : blue component

For example #ff0000 means red color. #7fff0000 is half-transparent red.

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

export async function main() {
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
  const { clangFormatPath } = detectVisualStudio();
  if (!clangFormatPath) throw new Error("couldn't find clang-format.exe");
  await runLogged(clangFormatPath, ["-i", "-style=file", settingsPath]);
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
