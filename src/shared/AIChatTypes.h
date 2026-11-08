/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SUMATRA_AI_CHAT_TYPES_H
#define SUMATRA_AI_CHAT_TYPES_H

constexpr int kAIChatProviderCount = 5;

enum class AIChatBackend {
    Claude,
    Grok,
    Codex,
    AntiGravity,
    CustomAPI,
    None,
};

#endif
