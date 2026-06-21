# AI Chat with document

**Available in pre-release 3.7+ on Windows 10 or later.**

SumatraPDF can show a chat sidebar where you ask questions about the document you are reading. Answers come from an AI agent CLI running on your computer — SumatraPDF does not send your files to its own servers.

Three backends are supported: [Claude Code](#claude-code), [Grok Build](#grok-build), and [OpenAI Codex](#openai-codex).

## Claude Code

This feature requires **[Claude Code](https://docs.anthropic.com/en/docs/claude-code)** to be installed and available on your system. SumatraPDF launches the `claude` command-line tool when you send a message.

If Claude Code is missing, the chat panel shows an error such as *Cannot find claude. Is Claude Code installed?*

Install and set up Claude Code using Anthropic's official guide:

- [Set up Claude Code](https://code.claude.com/docs/en/setup)

After installation, make sure `claude` (or `claude.exe`) is on your `PATH`, or in one of the usual install locations under your user profile.

## Grok Build

This feature can also use **[Grok Build](https://x.ai/news/grok-build-cli)** (the `grok` command-line tool).

If Grok Build is missing, the chat panel shows an error such as *Cannot find grok. Is Grok Build installed?*

Install Grok Build and sign in using xAI's instructions. SumatraPDF looks for `grok.exe` in `%USERPROFILE%\.grok\bin\` and on `PATH`.

Open the panel with **View → Grok chat** (`CmdAIChatWithGrokBuild`), or search for `Grok` in the command palette.

Grok Build settings are in the `GrokBuild` section of [advanced settings](Advanced-options-settings.md). The **Always Approve** checkbox passes `--always-approve` to Grok Build.

## OpenAI Codex

This feature can also use **[OpenAI Codex](https://developers.openai.com/codex/cli)** (the `codex` command-line tool).

If OpenAI Codex is missing, the chat panel shows an error such as *Cannot find codex. Is OpenAI Codex installed?*

Install and sign in using OpenAI's official guides:

- [Codex CLI](https://developers.openai.com/codex/cli)
- [Codex quickstart](https://developers.openai.com/codex/quickstart)
- [Codex on Windows](https://developers.openai.com/codex/windows)

After installation, make sure `codex` (or `codex.exe`) is on your `PATH`, or in `%USERPROFILE%\.codex\bin\` or `%USERPROFILE%\.local\bin\`.

Open the panel with **View → Codex chat** (`CmdAIChatWithOpenAICodex`), or search for `Codex` in the command palette.

Codex settings are in the `CodexBuild` section of [advanced settings](Advanced-options-settings.md). The **Skip Sandbox** checkbox passes `--dangerously-bypass-approvals-and-sandbox` to Codex — use only if you understand the security implications.

In the chat panel you can pick a model (default `gpt-5.5`; `gpt-5.4` and `o3` are also available) and a sandbox mode: **Read-only**, **Workspace write**, or **Full access**.

## How to use

1. Open a supported document (see below).
2. Open a chat sidebar for your preferred backend:
   - **View → Claude chat** (`CmdAIChatWithClaudeCode`)
   - **View → Grok chat** (`CmdAIChatWithGrokBuild`)
   - **View → Codex chat** (`CmdAIChatWithOpenAICodex`)
   
   Or open the [command palette](Command-Palette.md) (`Ctrl + K`) and search for `Claude`, `Grok`, or `Codex`.
3. Type a question in the input box at the bottom of the sidebar and press `Enter`.
4. Drag the splitter between the document and the chat panel to resize the sidebar.

Each document tab has its own chat session. Switching tabs switches the sidebar to that tab's conversation history.

You can pick a previous session from the session dropdown and choose model options. Backend-specific controls:

- **Claude Code:** model, effort level, and optionally **Skip Permissions** (passes `--dangerously-skip-permissions` — use only if you understand the security implications).
- **Grok Build:** model, effort level, and optionally **Always Approve** (passes `--always-approve`).
- **OpenAI Codex:** model, sandbox mode, and optionally **Skip Sandbox**.

While the agent is working on a reply, use **Stop** to cancel the current request.

## Supported documents

AI Chat is available only for file types the agent CLIs can work with directly:

- **PDF** (`.pdf`)
- **Single image files** (e.g. `.png`, `.jpg`, `.webp`, `.gif`, `.tiff`, `.bmp`, and other image formats SumatraPDF opens as a single image)

It is **not** available for comic archives (`.cbr`, `.cbz`, etc.), folders of images, ebooks (EPUB, MOBI, …), CHM, DjVu, XPS, PostScript, plain text, and other formats. On unsupported tabs the command is disabled and the panel shows that the feature is only available for PDF and image files.

## Settings

Sidebar width is shared by all backends via the `AIChatSidebarDx` advanced setting.

Backend-specific options are in [advanced settings](Advanced-options-settings.md) (`SumatraPDF-settings.txt`):

- `ClaudeCode` — default model, effort level, **Skip Permissions**, background color
- `GrokBuild` — default model, effort level, **Always Approve**, background color
- `CodexBuild` — default model, extra models, sandbox mode, **Skip Sandbox**, background color

You can assign your own keyboard shortcut to any of the chat commands — there is no default key binding. See [Customize keyboard shortcuts](Customize-keyboard-shortcuts.md).

## Requirements and limitations

- **Windows 10+** only (uses WebView2 for the chat UI).
- Requires a working installation of at least one supported agent CLI and network access as required by that CLI.
- Session history is stored by each CLI in its own location; SumatraPDF can list and resume sessions for the current document's folder:
  - Claude Code: `~/.claude/projects/` (encoded by project directory)
  - Grok Build: `~/.grok/sessions/`
  - OpenAI Codex: `~/.codex/sessions/` (with descriptions from `~/.codex/history.jsonl`)
- Each agent runs as a separate process; behavior, models, and billing follow that provider's terms and your account.

## See also

- [Commands](Commands.md) — `CmdAIChatWithClaudeCode`, `CmdAIChatWithGrokBuild`, `CmdAIChatWithOpenAICodex`
- [Command Palette](Command-Palette.md)
- [Advanced options / settings](Advanced-options-settings.md) — `ClaudeCode`, `GrokBuild`, and `CodexBuild` sections
- [Version history](Version-history.md) — 3.7 AI Chat entry
- [Claude Code documentation](https://docs.anthropic.com/en/docs/claude-code) (Anthropic)
- [Grok Build](https://x.ai/news/grok-build-cli) (xAI)
- [OpenAI Codex CLI](https://developers.openai.com/codex/cli) (OpenAI)