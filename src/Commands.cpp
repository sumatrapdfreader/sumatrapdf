/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

#include "Commands.h"

#define CMD_NAME(id, txt) #id "\0"
SeqStrings gCommandNames = COMMANDS(CMD_NAME) "\0";
#undef CMD_NAME

#define CMD_ID(id, txt) id,
int gCommandIds[] = {COMMANDS(CMD_ID)};
#undef CMD_ID

/* returns -1 if not found */
int GetCommandIdByName(const char* cmdName) {
    int idx = seqstrings::StrToIdxIS(gCommandNames, cmdName);
    if (idx < 0) {
        return -1;
    }
    CrashIf(idx >= dimof(gCommandIds));
    int cmdId = gCommandIds[idx];
    return cmdId;
}
