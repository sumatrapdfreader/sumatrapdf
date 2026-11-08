/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

constexpr int kAIChatProviderCount = 5;

enum class AIChatBackend {
    Claude,
    Grok,
    Codex,
    AntiGravity,
    CustomAPI,
    None,
};
