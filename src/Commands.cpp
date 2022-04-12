/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

#include "Commands.h"

#define CMD_NAME(id, txt) #id "\0"
SeqStrings gCommandNames = COMMANDS(CMD_NAME) "\0";
#undef CMD_NAME

#define CMD_ID(id, txt) id,
i32 gCommandIds[] = {COMMANDS(CMD_ID)};
#undef CMD_ID

#define CMD_DESC(id, txt) txt "\0"
SeqStrings gCommandDescriptions = COMMANDS(CMD_DESC) "\0";
#undef CMD_DESC

/* returns -1 if not found */
static NO_INLINE int GetCommandIdByNameOrDesc(SeqStrings commands, const char* s) {
    int idx = seqstrings::StrToIdxIS(commands, s);
    if (idx < 0) {
        return -1;
    }
    CrashIf(idx >= dimof(gCommandIds));
    int cmdId = gCommandIds[idx];
    return (int)cmdId;
}

/* returns -1 if not found */
int GetCommandIdByName(const char* cmdName) {
    return GetCommandIdByNameOrDesc(gCommandNames, cmdName);
}

/* returns -1 if not found */
int GetCommandIdByDesc(const char* cmdDesc) {
    return GetCommandIdByNameOrDesc(gCommandDescriptions, cmdDesc);
}
