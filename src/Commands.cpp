/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

#include "Commands.h"

#include "utils/Log.h"

#define CMD_NAME(id, txt) #id "\0"
static SeqStrings gCommandNames = COMMANDS(CMD_NAME) "\0";
#undef CMD_NAME

#define CMD_ID(id, txt) id,
static i32 gCommandIds[] = {COMMANDS(CMD_ID)};
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
    ReportIf(idx >= dimof(gCommandIds));
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

CommandArg::~CommandArg() {
    str::Free(strVal);
    str::Free(name);
}

static CommandArg* newArg(CommandArg::Type type, const char* name) {
    auto res = new CommandArg();
    res->type = type;
    res->name = str::Dup(name);
    return res;
}

// arg names are case insensitive
bool IsArgName(const char* name1, const char* name2) {
    return str::EqI(name1, name2);
}

void InsertArg(CommandArg** firstPtr, CommandArg* arg) {
    // for ease of use by callers, we shift null check here
    if (!arg) {
        return;
    }
    arg->next = *firstPtr;
    *firstPtr = arg;
}

void FreeCommandArgs(CommandArg* first) {
    CommandArg* next;
    CommandArg* curr = first;
    while (curr) {
        next = curr->next;
        delete curr;
        curr = next;
    }
}

static CommandArg* FindFirstArgOfType(CommandArg* first, CommandArg::Type type) {
    CommandArg* curr = first;
    while (curr) {
        if (curr->type == type) {
            return curr;
        }
        curr = curr->next;
    }
    return nullptr;
}

CommandArg* FindArg(CommandArg* first, const char* name, CommandArg::Type type) {
    CommandArg* curr = first;
    while (curr) {
        if (IsArgName(curr->name, name)) {
            if (curr->type == type) {
                return curr;
            }
            logf("FindArgByName: found arg of name '%s' by different type (wanted: %d, is: %d)\n", name, type,
                 curr->type);
        }
    }
    return nullptr;
}

CommandArg* FindColorArg(CommandArg* first) {
    return FindFirstArgOfType(first, CommandArg::Type::Color);
}

CommandArg* FindIntArg(CommandArg* first) {
    return FindFirstArgOfType(first, CommandArg::Type::Int);
}

static bool parseBool(const char* s) {
    if (str::EqI(s, "1")) {
        return true;
    }
    if (str::EqI(s, "true")) {
        return true;
    }
    if (str::EqI(s, "yes")) {
        return true;
    }
    // everything else, including invalid values, is false
    return false;
}

// returns null if can't parse `arg` as a given type
CommandArg* MkArg(const char* arg, CommandArg::Type type) {
    ReportIf(str::IsEmpty((arg)));
    if (str::IsEmpty(arg)) {
        return nullptr;
    }

    // note: this might break if values happen to contain '='
    // in which case they should get explicit name
    // maybe add heuristic for string values where we say
    // wihch name we're looking for and undo '=' parsing
    // if the name doesn't match
    StrVec nameVal;
    Split(nameVal, arg, "=", false, 2);
    const char* val = nullptr;
    const char* name = arg;
    if (nameVal.Size() == 2) {
        name = nameVal[0];
        val = nameVal[1];
    }
    if (type == CommandArg::Type::Bool) {
        auto res = newArg(type, name);
        // if value is not explicitly given with =
        // presence of value means "true"
        res->boolVal = val ? parseBool(val) : true;
        return res;
    }

    if (type == CommandArg::Type::String) {
        auto res = newArg(type, name);
        // if value is not explictly given with =
        // we store nullptr and assume the name is the value
        res->strVal = val ? str::Dup(val) : nullptr;
        return res;
    }

    if (type == CommandArg::Type::Int) {
        auto res = newArg(type, name);
        const char* valStr = val ? val : name;
        // permissive parsing, so always succeeds
        res->intVal = ParseInt(valStr);
        return res;
    }

    ReportIf(type != CommandArg::Type::Color);
    const char* valStr = val ? val : name;
    ParsedColor col;
    ParseColor(col, valStr);
    if (!col.parsedOk) {
        return nullptr;
    }
    auto res = newArg(type, name);
    res->colorVal = col;
    return res;
}

CommandArg* FindArgByName(CommandArg* first, const char* name) {
    CommandArg* curr = first;
    while (curr) {
        if (str::EqI(curr->name, name)) {
            return curr;
        }
        curr = curr->next;
    }
    return nullptr;
}

static int gNextCommandWithArgId = (int)CmdFirstWithArg;
static CommandWithArg* gFirstCommandWithArg = nullptr;

CommandWithArg::~CommandWithArg() {
    FreeCommandArgs(firstArg);
    str::Free(name);
    str::Free(definition);
}

CommandWithArg* CreateCommandWithArg(const char* definition, int origCmdId, CommandArg* firstArg) {
    int id = gNextCommandWithArgId++;
    auto cmd = new CommandWithArg();
    cmd->id = id;
    cmd->origId = origCmdId;
    cmd->definition = str::Dup(definition);
    cmd->firstArg = firstArg;
    cmd->next = gFirstCommandWithArg;
    gFirstCommandWithArg = cmd;
    return cmd;
}

CommandWithArg* FindCommandWithArg(int cmdId) {
    auto cmd = gFirstCommandWithArg;
    while (cmd) {
        if (cmd->id == cmdId) {
            return cmd;
        }
        cmd = cmd->next;
    }
    return nullptr;
}

void FreeCommandsWithArg() {
    CommandWithArg* next;
    CommandWithArg* curr = gFirstCommandWithArg;
    while (curr) {
        next = curr->next;
        delete curr;
        curr = next;
    }
    gFirstCommandWithArg = nullptr;
}

int GetFrstIntArg(CommandWithArg* cmd, int defValue) {
    if (!cmd) {
        return defValue;
    }
    auto arg = FindIntArg(cmd->firstArg);
    if (!arg) {
        return defValue;
    }
    return arg->intVal;
}

bool GetBoolArg(CommandWithArg* cmd, const char* name, bool defValue) {
    if (!cmd) {
        return defValue;
    }
    auto arg = FindArg(cmd->firstArg, name, CommandArg::Type::Bool);
    if (!arg) {
        return defValue;
    }
    return true;
}

// some commands can accept arguments. For those we have to create CommandWithArg that
// binds original command id and an arg and creates a unique command id
// we return -1 if unkown command or command doesn't take an argument or argument is invalid
int ParseCommand(const char* definition) {
    int cmdId = GetCommandIdByName(definition);
    if (cmdId > 0) {
        return cmdId;
    }
    // could be command with arg
    bool hasArg = str::FindChar(definition, ' ') != nullptr;
    if (!hasArg) {
        return -1;
    }
    StrVec parts;
    Split(parts, definition, " ", true);
    if (parts.Size() < 2) {
        return -1;
    }
    char* cmdName = parts.At(0);
    cmdId = GetCommandIdByName(cmdName);
    if (cmdId < 0) {
        // TODO: make it a notification
        logf("MaybeCreateCommandWithArg: unknown cmd name '%s'\n", cmdName);
    }
    CommandWithArg* cmd = nullptr;
    switch (cmdId) {
        case CmdCreateAnnotText:
        case CmdCreateAnnotLink:
        case CmdCreateAnnotFreeText:
        case CmdCreateAnnotLine:
        case CmdCreateAnnotSquare:
        case CmdCreateAnnotCircle:
        case CmdCreateAnnotPolygon:
        case CmdCreateAnnotPolyLine:
        case CmdCreateAnnotHighlight:
        case CmdCreateAnnotUnderline:
        case CmdCreateAnnotSquiggly:
        case CmdCreateAnnotStrikeOut:
        case CmdCreateAnnotRedact:
        case CmdCreateAnnotStamp:
        case CmdCreateAnnotCaret:
        case CmdCreateAnnotInk:
        case CmdCreateAnnotPopup:
        case CmdCreateAnnotFileAttachment: {
            // args: color, "openedit" : bool
            // color argument
            int n = parts.Size();
            CommandArg* firstArg = nullptr;
            for (int i = 1; i < n; i++) {
                char* argName = parts.At(i);
                if (IsArgName(argName, kCmdArgOpenEdit)) {
                    auto arg = MkArg(argName, CommandArg::Type::Bool);
                    InsertArg(&firstArg, arg);
                    continue;
                }
                // potentially a single color arg
                auto arg = MkArg(argName, CommandArg::Type::Color);
                InsertArg(&firstArg, arg);
            }
            // must have valid color, "openedit" is optional
            if (!FindColorArg(firstArg)) {
                FreeCommandArgs(firstArg);
                logf("MaybeCreateCommandWithArg: invalid argument in '%s'\n", definition);
                return -1;
            }
            cmd = CreateCommandWithArg(definition, cmdId, firstArg);
            break;
        }
        case CmdScrollUp:
        case CmdScrollDown:
        case CmdGoToNextPage:
        case CmdGoToPrevPage: {
            // int argument
            char* argStr = parts.At(1);
            CommandArg* firstArg = MkArg(argStr, CommandArg::Type::Int);
            if (!firstArg) {
                return -1;
            }
            int n = firstArg->intVal;
            // note: this validation currently applies to all commands
            // but might need to be custom for each command
            if (n <= 0 || n > 100) {
                firstArg->intVal = 1;
            }
            cmd = CreateCommandWithArg(definition, cmdId, firstArg);
            break;
        }
        default: {
            logf("MaybeCreateCommandWithArg: cmd '%s' doesn't accept arguments\n", cmdName);
        }
    }
    if (!cmd) {
        return -1;
    }
    return cmd->id;
}
