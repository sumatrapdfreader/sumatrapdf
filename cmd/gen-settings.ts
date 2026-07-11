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
  field("FontSize", Float, 0, "font size, default 8.0"),
  field("LayoutDx", Float, 0, "default is 420"),
  field("LayoutDy", Float, 0, "default is 595"),
  field("IgnoreDocumentCSS", Bool, false, "if true, we ignore ebook's CSS"),
  field("CustomCSS", Str, null, "custom CSS. Might need to set IgnoreDocumentCSS = true"),
  field(
    "WindowBgCol",
    Color,
    "",
    "if given, sets the canvas background color for ebook documents (epub, mobi etc.)",
  ).ver("3.7"),
];

const theme: Field[] = [
  field("Name", Str, "", "name of the theme"),
  field("TextColor", Color, "", "text color"),
  field("BackgroundColor", Color, "", "background color"),
  field("ControlBackgroundColor", Color, "", "control background color"),
  field("LinkColor", Color, "", "link color"),
  field("ColorizeControls", Bool, false, "should we colorize Windows controls and window areas"),
];

const tabFile: Field[] = [field("Path", Str, "", "file path")];

const tabGroup: Field[] = [
  field("Name", Str, "", "name of the tab group"),
  array("TabFiles", tabFile, "files in the tab group"),
];

const windowPos: Field[] = [
  field("X", Int, 0, "x coordinate"),
  field("Y", Int, 0, "y coordinate"),
  field("Dx", Int, 0, "width"),
  field("Dy", Int, 0, "height"),
];

const pointPos: Field[] = [field("X", Int, 0, "x coordinate"), field("Y", Int, 0, "y coordinate")];

const keyboardShortcut: Field[] = [
  field("Cmd", Str, "", "command"),
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

const scrollPos: Field[] = [field("X", Float, 0, "x coordinate"), field("Y", Float, 0, "y coordinate")];

const fileTime: Field[] = [field("DwHighDateTime", Int, 0, ""), field("DwLowDateTime", Int, 0, "")];

const printerDefaults: Field[] = [
  field("PrintScale", Str, "shrink", "default value for scaling (shrink, fit, none)"),
  field("Collate", Str, "default", "default value for collate in the print dialog (default, collate, nocollate)"),
];

const forwardSearch: Field[] = [
  field(
    "HighlightOffset",
    Int,
    0,
    "when set to a positive value, the forward search highlight style will " +
      "be changed to a rectangle at the left of the page (with the indicated " +
      "amount of margin from the page margin)",
  ),
  field("HighlightWidth", Int, 15, "width of the highlight rectangle (if HighlightOffset is > 0)"),
  field("HighlightColor", Color, rgb(0x65, 0x81, 0xff), "color used for the forward search highlight"),
  field(
    "HighlightPermanent",
    Bool,
    false,
    "if true, highlight remains visible until the next mouse click " + "(instead of fading away immediately)",
  ),
];

const windowMarginFixedPageUI: Field[] = [
  field("Top", Int, 2, "size of the top margin between window and document"),
  field("Right", Int, 4, "size of the right margin between window and document"),
  field("Bottom", Int, 2, "size of the bottom margin between window and document"),
  field("Left", Int, 4, "size of the left margin between window and document"),
];

const windowMarginComicBookUI: Field[] = [
  field("Top", Int, 0, "size of the top margin between window and document"),
  field("Right", Int, 0, "size of the right margin between window and document"),
  field("Bottom", Int, 0, "size of the bottom margin between window and document"),
  field("Left", Int, 0, "size of the left margin between window and document"),
];

const pageSpacing: Field[] = [field("Dx", Int, 4, "horizontal difference"), field("Dy", Int, 4, "vertical difference")];

const fixedPageUI: Field[] = [
  field("TextColor", Color, rgb(0x00, 0x00, 0x00), "color value with which black (text) will be substituted"),
  field(
    "BackgroundColor",
    Color,
    rgb(0xff, 0xff, 0xff),
    "color value with which white (background) will be substituted",
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
    "colors to use for the gradient from top to bottom (stops will be inserted " +
      "at regular intervals throughout the document); currently only up to three " +
      "colors are supported; the idea behind this experimental feature is that the " +
      "background might allow to subconsciously determine reading progress; " +
      "suggested values: #2828aa #28aa28 #aa2828",
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
    "if true, default to displaying Comic Book files in manga mode (from right to left if showing 2 pages at a time)",
  ),
  field("WindowBgCol", Color, "", "if given, sets the canvas background color for comic book files").ver("3.7"),
];

const imageUI: Field[] = [
  field("WindowBgCol", Color, "", "if given, sets the canvas background color for image files").ver("3.7"),
  field(
    "DefaultZoom",
    Str,
    "shrink to fit",
    "default zoom for image files. valid values: fit page, fit width, fit content, shrink to fit or percent like 100%",
  ).ver("3.7"),
  field("DefaultZoomFloat", Float, 0, "value of DefaultZoom for internal usage").notSaved(),
];

const chmUI: Field[] = [
  field("UseFixedPageUI", Bool, false, "if true, the UI used for PDF documents will be used for CHM documents as well"),
];

const markdownUI: Field[] = [
  field(
    "UseFixedPageUI",
    Bool,
    false,
    "if true, use MuPDF (cmark-gfm) to render markdown; if false, use WebView2 browser view when available",
  ),
];

const codexBuild: Field[] = [
  field("Model", Str, "gpt-5.5", "Codex model ID for -m (e.g. gpt-5.5, gpt-5.4, o3)"),
  field(
    "Models",
    Str,
    "",
    "extra Codex model IDs for the dropdown, comma-separated; gpt-5.5, gpt-5.4, and o3 are always included",
  ),
  field("Sandbox", Int, 1, "Codex sandbox mode: 0=read-only, 1=workspace-write, 2=danger-full-access"),
  field("SkipSandbox", Bool, false, "if true, pass --dangerously-bypass-approvals-and-sandbox to Codex"),
  field("BgColor", Color, "#ffffff", "background color of the OpenAI Codex chat panel"),
];

const grokBuild: Field[] = [
  field("Model", Str, "grok-composer-2.5-fast", "Grok model ID for --model (e.g. grok-composer-2.5-fast, grok-build)"),
  field(
    "Models",
    Str,
    "",
    "extra Grok model IDs for the dropdown, comma-separated; grok-composer-2.5-fast and grok-build are always included",
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
    "extra Claude model aliases for the dropdown, comma-separated; sonnet, opus, and haiku are always included",
  ),
  field("Effort", Int, 1, "Claude effort level: 0=Low, 1=Medium, 2=High, 3=Max"),
  field("SkipPermissions", Bool, false, "if true, pass --dangerously-skip-permissions to Claude Code"),
  field("BgColor", Color, "#ffffff", "background color of the Claude Code chat panel"),
];

const fullscreen: Field[] = [
  field("ShowToolbar", Bool, false, "if true, show the toolbar in fullscreen mode"),
  field("ShowMenubar", Bool, false, "if true, show the menu bar in fullscreen mode"),
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
];

const annotations: Field[] = [
  field("HighlightColor", Color, rgb(0xff, 0xff, 0x0), "highlight annotation color"),
  field("UnderlineColor", Color, rgb(0x00, 0xff, 0x0), "underline annotation color"),
  field("SquigglyColor", Color, rgb(0xff, 0x00, 0xff), "squiggly annotation color").ver("3.5"),
  field("StrikeOutColor", Color, rgb(0xff, 0x00, 0x00), "strike out annotation color").ver("3.5"),
  field("FreeTextColor", Color, "", "text color of free text annotation").ver("3.5"),
  field("FreeTextBackgroundColor", Color, "", "background color of free text annotation").ver("3.6"),
  field(
    "FreeTextOpacity",
    Int,
    100,
    "opacity of free text annotation in percent (0-100); 0 - fully transparent (invisible), 50 - half transparent, 100 - fully opaque",
  ).ver("3.6"),
  field("FreeTextSize", Int, 12, "size of free text annotation").ver("3.5"),
  field("FreeTextBorderWidth", Int, 1, "width of free text annotation border").ver("3.5"),
  field("TextIconColor", Color, "", "text icon annotation color"),
  field(
    "TextIconType",
    Str,
    "",
    "type of text annotation icon: comment, help, insert, key, new paragraph, note, paragraph. If not set: note.",
  ),
  field(
    "DefaultAuthor",
    Str,
    "",
    "default author for created annotations, use (none) to not add an author at all. If not set will use Windows user name",
  ).ver("3.4"),
  field(
    "SelectionToolbar",
    Bool,
    true,
    "if true, a small floating toolbar with selection actions (copy, read aloud, highlight etc.) pops up after selecting text. Set to false to disable it",
  ).ver("3.7"),
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
];

const fileSettings: Field[] = [
  field("FilePath", Str, null, "path of the document"),
  array("Favorites", favorite, "Values which are persisted for bookmarks/favorites"),
  field(
    "IsPinned",
    Bool,
    false,
    'a document can be "pinned" to the Frequently Read list so that it ' +
      "isn't displaced by recently opened documents",
  ),
  field(
    "IsMissing",
    Bool,
    false,
    "if a document can no longer be found but we still remember valuable state, " +
      "it's classified as missing so that it can be hidden instead of removed",
  ).doc("if true, the file is considered missing and won't be shown in any list"),
  field(
    "OpenCount",
    Int,
    0,
    "in order to prevent documents that haven't been opened for a while " +
      "but used to be opened very frequently constantly remain in top positions, " +
      "the openCount will be cut in half after every week, so that the " +
      "Frequently Read list hopefully better reflects the currently relevant documents",
  ).doc("number of times this document has been opened recently"),
  field(
    "DecryptionKey",
    Str,
    null,
    "Hex encoded MD5 fingerprint of file content (32 chars) followed by " +
      "crypt key (64 chars) - only applies for PDF documents",
  ).doc("data required to open a password protected document without having to " + "ask for the password again"),
  field(
    "UseDefaultState",
    Bool,
    false,
    "if true, we use global defaults when opening this file (instead of " + "the values below)",
  ),
  field(
    "DisplayMode",
    Str,
    "automatic",
    "how pages should be laid out for this document, needs to be synchronized with " +
      "DefaultDisplayMode after deserialization and before serialization",
  ).doc(
    "layout of pages. valid values: automatic, single page, facing, book view, " +
      "continuous, continuous facing, continuous book view",
  ),
  compactStruct("ScrollPos", scrollPos, "how far this document has been scrolled (in x and y direction)").structName(
    "PointF",
  ),
  field("PageNo", Int, 1, "number of the last read page"),
  field("Zoom", Str, "fit page", "zoom (in %) or one of those values: fit page, fit width, fit content"),
  field("Rotation", Int, 0, "how far pages have been rotated as a multiple of 90 degrees"),
  field(
    "WindowState",
    Int,
    0,
    "state of the window. 1 is normal, 2 is maximized, " + "3 is fullscreen, 4 is minimized",
  ),
  compactStruct("WindowPos", windowPos, "default position (can be on any monitor)").structName("Rect"),
  field(
    "ShowToc",
    Bool,
    true,
    "if true, we show table of contents (Bookmarks) sidebar if it's present " + "in the document",
  ),
  field("SidebarDx", Int, 0, "width of the left sidebar panel containing the table of contents"),
  field(
    "DisplayR2L",
    Bool,
    false,
    "if true, the document is displayed right-to-left in facing and book view modes " +
      "(only used for comic book documents)",
  ),
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
      "so that we only have to save a diff instead of all states for the whole " +
      "tree (which can be quite large) (internal)",
  ).doc("data required to determine which parts of the table of contents have been expanded"),
  field(
    "Thumbnail",
    { name: "", ctype: "RenderedBitmap *" },
    "NULL",
    "thumbnails are saved as PNG files in sumatrapdfcache directory",
  ).notSaved(),
  field("Index", Int, 0, "temporary value needed for FileHistory::cmpOpenCount").notSaved(),
  field("Himl", { name: "", ctype: "HIMAGELIST" }, "NULL", "").notSaved(),
  field("IconIdx", Int, -1, "").notSaved(),
];

const tabState: Field[] = [
  field("FilePath", Str, null, "path of the document"),
  field("DisplayMode", Str, "automatic", "same as FileStates -> DisplayMode"),
  field("PageNo", Int, 1, "number of the last read page"),
  field("Zoom", Str, "fit page", "same as FileStates -> Zoom"),
  field("Rotation", Int, 0, "same as FileStates -> Rotation"),
  compactStruct("ScrollPos", scrollPos, "how far this document has been scrolled (in x and y direction)").structName(
    "PointF",
  ),
  field("ShowToc", Bool, true, "if true, the table of contents was shown when the document was closed"),
  compactArray("TocState", Int, null, "same as FileStates -> TocState"),
];

const sessionData: Field[] = [
  array(
    "TabStates",
    tabState,
    "a subset of FileState required for restoring the state of a single tab " +
      "(required for handling documents being opened twice)",
  ).doc("data required for restoring the view state of a single tab"),
  field("TabIndex", Int, 1, "index of the currently selected tab (1-based)"),
  field("WindowState", Int, 0, "same as FileState -> WindowState"),
  compactStruct("WindowPos", windowPos, "default position (can be on any monitor)").structName("Rect"),
  field("SidebarDx", Int, 0, "width of favorites/bookmarks sidebar (if shown)"),
];

const globalPrefs: Field[] = [
  comment(""),
  emptyLine(),

  field(
    "DefaultDisplayMode",
    Str,
    "automatic",
    "how pages should be laid out by default, needs to be synchronized with " +
      "DefaultDisplayMode after deserialization and before serialization",
  ).doc(
    "default layout of pages. valid values: automatic, single page, facing, " +
      "book view, continuous, continuous facing, continuous book view",
  ),
  field(
    "DefaultZoom",
    Str,
    "fit page",
    "default zoom. valid values: fit page, fit width, fit content or percent like 100%",
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
    "if true, we expose the SyncTeX inverse search command line in Settings -> Options",
  ),
  field("EscToExit", Bool, false, "if true, Esc key closes SumatraPDF"),
  field("FullPathInTitle", Bool, false, "if true, we show the full path to a file in the title bar").ver("3.0"),
  field("InverseSearchCmdLine", Str, null, "pattern used to launch the LaTeX editor when doing inverse search"),
  field(
    "LazyLoading",
    Bool,
    false,
    "when restoring session, delay loading of documents until their tab is selected",
  ).ver("3.6"),
  field(
    "MainWindowBackground",
    Color,
    rgba(0xff, 0xf2, 0x00, 0x80),
    "background color of the non-document windows, traditionally yellow",
  ),
  field("NoHomeTab", Bool, false, "if true, doesn't open Home tab"),
  field(
    "HomePageSortByFrequentlyRead",
    Bool,
    false,
    "if true implements pre-3.6 behavior of showing opened files by frequently used count. If false, shows most recently opened first",
  ),
  field(
    "HomePageViewMode",
    Str,
    "thumbnails",
    "how the home page shows the document history: thumbnails (a grid of page previews) or list (one row per file)",
  )
    .ver("3.7")
    .doc("Valid values: thumbnails, list"),
  field(
    "ReloadModifiedDocuments",
    Bool,
    true,
    "if true, a document will be reloaded automatically whenever it's changed " +
      "(currently doesn't work for documents shown in the ebook UI)",
  ).ver("2.5"),
  field("RememberOpenedFiles", Bool, true, "if true, we remember which files we opened and their display settings"),
  field(
    "RememberStatePerDocument",
    Bool,
    true,
    "if true, we store display settings for each document separately (i.e. everything " +
      "after UseDefaultState in FileStates)",
  ),
  field("RestoreSession", Bool, true, "if true and SessionData isn't empty, that session will be restored at startup"),
  field("ReuseInstance", Bool, true, "if true, we'll always open files using existing SumatraPDF process"),
  field(
    "ShowMenubar",
    Bool,
    true,
    "if false, the menu bar will be hidden (use F9 to toggle, persisted across sessions)",
  ).ver("2.5"),
  field("ShowMenubarWithTabs", Bool, false, "if true, show the menu bar when using tabs (useTabs = true)").ver("3.7"),
  field("ShowTips", Bool, true, "if true, we show tips on the home page").ver("3.7"),
  field(
    "CustomColors",
    Str,
    null,
    "up to 13 custom colors for the background color picker, separated by space (e.g. '#ff0000 #00ff00 #0000ff')",
  )
    .internal()
    .ver("3.7"),
  field("ShowToolbar", Bool, true, "if true, we show the toolbar at the top of the window"),
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
  field("ShowFavorites", Bool, false, "if true, we show the Favorites sidebar"),
  field(
    "ShowToc",
    Bool,
    true,
    "if true, we show table of contents (Bookmarks) sidebar if it's present " + "in the document",
  ),
  field("ShowLinks", Bool, false, "if true we draw a blue border around links in the document").ver("3.6"),
  field("ShowStartPage", Bool, true, "if true, we show a list of frequently read documents when no document is loaded"),
  field("SidebarDx", Int, 0, "width of favorites/bookmarks sidebar (if shown)").internal(),
  field(
    "Scrollbars",
    Str,
    "windows",
    "scrollbar mode: windows (standard Windows scrollbar), smart (overlay scrollbar with auto-hide), overlay (always visible overlay scrollbar), hidden (no scrollbars)",
  ).ver("3.7"),
  field("ScrollbarInSinglePage", Bool, false, "if true, we show scrollbar in single page mode").ver("3.6"),
  field("SmoothScroll", Bool, false, "if true, implements smooth scrolling").ver("3.6"),
  field(
    "CitationHoverDelay",
    Int,
    -1,
    "how long to hover an internal-document link (in ms) before we show a popup rendering the destination region (citation entry, figure, footnote). -1 (the default) disables the popup; set a positive value like 300 to enable it",
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
  field("TabWidth", Int, 300, "maximum width of a single tab"),
  field("Theme", Str, "", "the name of the theme to use. System picks the last used light or dark theme based on the Windows app mode")
    .ver("3.5")
    .doc("Valid themes: light, dark, darker, system"),
  field("LastLightTheme", Str, "", "the light theme the light/dark toggle and the System theme switch to").ver("3.7"),
  field("LastDarkTheme", Str, "", "the dark theme the light/dark toggle and the System theme switch to").ver("3.7"),
  field(
    "DocumentColorsFollowTheme",
    Str,
    "off",
    "how fixed-page documents (PDF, XPS, DjVu, EPUB, MOBI, FB2, HTML, etc.) follow the UI theme: off (keep original page colors), smart (recolor text and background but not images), or legacy (recolor text and background and images, pre-3.7 behavior)",
  )
    .ver("3.7")
    .doc("Valid values: off, smart, legacy"),
  field(
    "TocDy",
    Int,
    0,
    "if both favorites and bookmarks parts of sidebar are visible, this is " +
      "the height of bookmarks (table of contents) part",
  ).internal(),
  field("ToolbarSize", Int, 18, "height of toolbar").ver("3.4"),
  field(
    "TreeFontName",
    Str,
    "automatic",
    "font name for bookmarks and favorites tree views. automatic means Windows default",
  ),
  field("TreeFontSize", Int, 0, "font size for bookmarks and favorites tree views. 0 means Windows default").ver("3.3"),
  field("UIFontSize", Int, 0, "over-ride application font size. 0 means Windows default").ver("3.6"),
  field("DisableAntiAlias", Bool, false, "if true, disables anti-aliasing for rendering PDF documents").ver("3.6"),
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
  ),
  field(
    "UseSysColors",
    Bool,
    false,
    "if true, we use Windows system colors for background/text color. Over-rides other settings",
  ),
  field("UseTabs", Bool, true, "if true, documents are opened in tabs instead of new windows").ver("3.0"),
  field(
    "TabsMru",
    Bool,
    false,
    "if true, Ctrl+Tab and Ctrl+Shift+Tab show the tab switcher in most recently used order instead of tab-strip order",
  ),
  compactArray(
    "ZoomLevels",
    Float,
    "",
    "zoom levels which zooming steps through in addition to Fit Page, Fit Width and " +
      "the minimum and maximum allowed values (8.33 and 6400)",
  ).doc("sequence of zoom levels when zooming in/out; all values must lie between 8.33 and 6400"),
  compactArray("ZoomLevelsCmdIds", Int, "", "").notSaved(),
  field(
    "ZoomIncrement",
    Float,
    0,
    "zoom step size in percents relative to the current zoom level. " +
      "if zero or negative, the values from ZoomLevels are used instead",
  ),

  emptyLine(),

  struct("FixedPageUI", fixedPageUI, "customization options for PDF, XPS, DjVu and PostScript UI"),
  emptyLine(),
  struct("EBookUI", ebookUI, "customization options for eBookUI"),
  emptyLine(),
  struct("ComicBookUI", comicBookUI, "customization options for Comic Book UI"),
  emptyLine(),
  struct("ImageUI", imageUI, "customization options for image files UI"),
  emptyLine(),
  struct(
    "ChmUI",
    chmUI,
    "customization options for CHM UI. If UseFixedPageUI is true, FixedPageUI settings apply instead",
  ),
  emptyLine(),
  struct(
    "MarkdownUI",
    markdownUI,
    "customization options for Markdown UI. If UseFixedPageUI is true, MuPDF is used; otherwise WebView2 browser view is used when available",
  ),
  emptyLine(),
  struct("ClaudeCode", claudeCode, "settings for the Claude Code chat sidebar").ver("3.7"),
  emptyLine(),
  struct("GrokBuild", grokBuild, "settings for the Grok Build chat sidebar").ver("3.7"),
  emptyLine(),
  struct("CodexBuild", codexBuild, "settings for the OpenAI Codex chat sidebar").ver("3.7"),
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
  ).ver("3.7"),
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
    "customization options for how we show forward search results (used from " + "LaTeX editors)",
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
    "actual resolution of the main screen in DPI (if this value " + "isn't positive, the system's UI setting is used)",
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
  field("VersionToSkip", Str, null, "we won't ask again to update to this version"),
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
  field("CheckForUpdates", Bool, true, "if true, we check once a day if an update is available").internal(),
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
