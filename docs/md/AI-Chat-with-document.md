# AI Chat with document

**Available in pre-release 3.7+ on Windows 10 or later.**

SumatraPDF can show a chat sidebar where you ask questions about the document you are reading. Answers come from [Claude Code](https://docs.anthropic.com/en/docs/claude-code) running on your computer — SumatraPDF does not send your files to its own servers.

## Claude Code

This feature requires **[Claude Code](https://docs.anthropic.com/en/docs/claude-code)** to be installed and available on your system. SumatraPDF launches the `claude` command-line tool when you send a message.

If Claude Code is missing, the chat panel shows an error such as *Cannot find claude. Is Claude Code installed?*

Install and set up Claude Code using Anthropic's official guide:

- [Set up Claude Code](https://code.claude.com/docs/en/setup)

After installation, make sure `claude` (or `claude.exe`) is on your `PATH`, or in one of the usual install locations under your user profile.

## How to use

1. Open a supported document (see below).
2. **View → AI Chat with document using Claude Code** (`CmdAIChatWithClaudeCode`), or open the [command palette](Command-Palette.md) (`Ctrl + K`) and search for `AI Chat`.
3. Type a question in the input box at the bottom of the sidebar and press `Enter`.
4. Drag the splitter between the document and the chat panel to resize the sidebar.

Each document tab has its own chat session. Switching tabs switches the sidebar to that tab's conversation history.

You can pick a previous session from the session dropdown, choose model and effort level, and optionally enable **Skip Permissions** (passes `--dangerously-skip-permissions` to Claude Code — use only if you understand the security implications).

While Claude is working on a reply, use **Stop** to cancel the current request.

## Supported documents

AI Chat is available only for file types Claude Code can work with directly:

- **PDF** (`.pdf`)
- **Single image files** (e.g. `.png`, `.jpg`, `.webp`, `.gif`, `.tiff`, `.bmp`, and other image formats SumatraPDF opens as a single image)

It is **not** available for comic archives (`.cbr`, `.cbz`, etc.), folders of images, ebooks (EPUB, MOBI, …), CHM, DjVu, XPS, PostScript, plain text, and other formats. On unsupported tabs the command is disabled and the panel shows *Claude Code is only available for PDF and image files.*

## Settings

Sidebar width, default model, effort level, background color, and related options are in the `ClaudeCode` section of [advanced settings](Advanced-options-settings.md) (`SumatraPDF-settings.txt`).

You can assign your own keyboard shortcut to `CmdAIChatWithClaudeCode` — there is no default key binding. See [Customizing keyboard shortcuts](Customizing-keyboard-shortcuts.md).

## Requirements and limitations

- **Windows 10+** only (uses WebView2 for the chat UI).
- Requires a working **Claude Code** installation and network access as required by Claude Code itself.
- Session history is stored by Claude Code under `~/.claude/projects/` (encoded by project directory); SumatraPDF can list and resume sessions for the current document's folder.
- Claude Code runs as a separate process; behavior, models, and billing follow Anthropic's Claude Code terms and your account.

## See also

- [Commands](Commands.md) — `CmdAIChatWithClaudeCode`
- [Command Palette](Command-Palette.md)
- [Advanced options / settings](Advanced-options-settings.md) — `ClaudeCode` section
- [Version history](Version-history.md) — 3.7 AI Chat entry
- [Claude Code documentation](https://docs.anthropic.com/en/docs/claude-code) (Anthropic)