/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "DisplayMode.h"
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

struct ArgSpec {
    int cmdId;
    const char* name;
    CommandArg::Type type;
};

// arguments for the same command should follow each other
// first argument is default and can be specified without a name
static const ArgSpec argSpecs[] = {
    {CmdSelectionHandler, kCmdArgURL, CommandArg::Type::String}, // default
    {CmdSelectionHandler, kCmdArgExe, CommandArg::Type::String},

    {CmdExec, kCmdArgExe, CommandArg::Type::String}, // default
    {CmdExec, kCmdArgFilter, CommandArg::Type::String},

    // and all CmdCreateAnnot* commands
    {CmdCreateAnnotText, kCmdArgColor, CommandArg::Type::Color}, // default
    {CmdCreateAnnotText, kCmdArgOpenEdit, CommandArg::Type::Bool},
    {CmdCreateAnnotText, kCmdArgCopyToClipboard, CommandArg::Type::Bool},
    {CmdCreateAnnotText, kCmdArgSetContent, CommandArg::Type::Bool},

    // and  CmdScrollDown, CmdGoToNextPage, CmdGoToPrevPage
    {CmdScrollUp, kCmdArgN, CommandArg::Type::Int}, // default

    {CmdSetTheme, kCmdArgTheme, CommandArg::Type::String}, // default

    {CmdZoomCustom, kCmdArgLevel, CommandArg::Type::String}, // default

    {CmdCommandPalette, kCmdArgMode, CommandArg::Type::String}, // default

    {CmdNone, "", CommandArg::Type::None}, // sentinel
};

CustomCommand* gFirstCustomCommand = nullptr;

// returns -1 if not found
static NO_INLINE int GetCommandIdByNameOrDesc(SeqStrings commands, const char* s) {
    int idx = seqstrings::StrToIdxIS(commands, s);
    if (idx < 0) {
        return -1;
    }
    ReportIf(idx >= dimofi(gCommandIds));
    int cmdId = gCommandIds[idx];
    return (int)cmdId;
}

// cmdName is "CmdOpenFile" etc.
// returns -1 if not found
int GetCommandIdByName(const char* cmdName) {
    int cmdId = GetCommandIdByNameOrDesc(gCommandNames, cmdName);
    if (cmdId >= 0) {
        return cmdId;
    }
    auto curr = gFirstCustomCommand;
    while (curr) {
        if (curr->idStr && str::EqI(cmdName, curr->idStr)) {
            return curr->id;
        }
        curr = curr->next;
    }
    return -1;
}

// returns -1 if not found
int GetCommandIdByDesc(const char* cmdDesc) {
    int cmdId = GetCommandIdByNameOrDesc(gCommandDescriptions, cmdDesc);
    if (cmdId >= 0) {
        return cmdId;
    }
    auto curr = gFirstCustomCommand;
    while (curr) {
        if (curr->name && str::EqI(cmdDesc, curr->name)) {
            return curr->id;
        }
        curr = curr->next;
    }
    return -1;
}

CommandArg::~CommandArg() {
    str::Free(strVal);
    str::Free(name);
}

// arg names are case insensitive
static bool IsArgName(const char* name, const char* argName) {
    if (str::EqI(name, argName)) {
        return true;
    }
    if (!str::StartsWithI(name, argName)) {
        return false;
    }
    char c = name[str::Len(argName)];
    return c == '=';
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
        curr = curr->next;
    }
    return nullptr;
}

static int gNextCustomCommandId = (int)CmdFirstCustom;

CustomCommand::~CustomCommand() {
    FreeCommandArgs(firstArg);
    str::Free(name);
    str::Free(key);
    str::Free(idStr);
    str::Free(definition);
}

CustomCommand* CreateCustomCommand(const char* definition, int origCmdId, CommandArg* args) {
    // if no args we retain original command id
    // only when we have unique args we have to create a new command id
    int id = origCmdId;
    if (args != nullptr) {
        id = gNextCustomCommandId++;
    } else {
#if 0
        auto existingCmd = FindCustomCommand(origCmdId);
        if (existingCmd) {
            return existingCmd;
        }
#endif
    }
    auto cmd = new CustomCommand();
    cmd->id = id;
    cmd->origId = origCmdId;
    cmd->definition = str::Dup(definition);
    cmd->firstArg = args;
    cmd->next = gFirstCustomCommand;
    gFirstCustomCommand = cmd;
    return cmd;
}

CustomCommand* FindCustomCommand(int cmdId) {
    auto cmd = gFirstCustomCommand;
    while (cmd) {
        if (cmd->id == cmdId) {
            return cmd;
        }
        cmd = cmd->next;
    }
    return nullptr;
}

void FreeCustomCommands() {
    CustomCommand* next;
    CustomCommand* curr = gFirstCustomCommand;
    while (curr) {
        next = curr->next;
        delete curr;
        curr = next;
    }
    gFirstCustomCommand = nullptr;
}

void GetCommandsWithOrigId(Vec<CustomCommand*>& commands, int origId) {
    CustomCommand* curr = gFirstCustomCommand;
    while (curr) {
        if (curr->origId == origId) {
            commands.Append(curr);
        }
        curr = curr->next;
    }
    // reverse so that they are returned in the order they were inserted
    commands.Reverse();
}

static CommandArg* NewArg(CommandArg::Type type, const char* name) {
    auto res = new CommandArg();
    res->type = type;
    res->name = str::Dup(name);
    return res;
}

CommandArg* NewStringArg(const char* name, const char* val) {
    auto res = new CommandArg();
    res->type = CommandArg::Type::String;
    res->name = str::Dup(name);
    res->strVal = str::Dup(val);
    return res;
}

CommandArg* NewFloatArg(const char* name, float val) {
    auto res = new CommandArg();
    res->type = CommandArg::Type::Float;
    res->name = str::Dup(name);
    res->floatVal = val;
    return res;
}

static CommandArg* ParseArgOfType(const char* argName, CommandArg::Type type, const char* val) {
    if (type == CommandArg::Type::Color) {
        ParsedColor col;
        ParseColor(col, val);
        if (!col.parsedOk) {
            // invalid value, skip it
            logf("parseArgOfType: invalid color value '%s'\n", val);
            return nullptr;
        }
        auto arg = NewArg(type, argName);
        arg->colorVal = col;
        return arg;
    }

    if (type == CommandArg::Type::Int) {
        auto arg = NewArg(type, argName);
        arg->intVal = ParseInt(val);
        return arg;
    }

    if (type == CommandArg::Type::String) {
        auto arg = NewArg(type, argName);
        arg->strVal = str::Dup(val);
        return arg;
    }

    ReportIf(true);
    return nullptr;
}

CommandArg* TryParseDefaultArg(int defaultArgIdx, const char** argsInOut) {
    // first is default value
    const char* valStart = str::SkipChar(*argsInOut, ' ');
    const char* valEnd = str::FindChar(valStart, ' ');
    const char* argName = argSpecs[defaultArgIdx].name;
    CommandArg::Type type = argSpecs[defaultArgIdx].type;
    if (type == CommandArg::Type::String) {
        // for strings we eat it all to avoid the need for proper quoting
        // creates a problem: all named args must be before default string arg
        valEnd = nullptr;
    }
    TempStr val = nullptr;
    if (valEnd == nullptr) {
        val = str::Dup(valStart);
    } else {
        val = str::Dup(valStart, valEnd - valStart);
        valEnd = str::SkipChar(valEnd, ' ');
    }
    // no matter what, we advance past the value
    *argsInOut = valEnd;

    // we don't support bool because we don't have to yet
    // (no command have default bool value)
    return ParseArgOfType(argName, type, val);
}

// 1  : true
// 0  : false
// -1 : not a known boolean string
static int ParseBool(const char* s) {
    if (str::EqI(s, "1") || str::EqI(s, "true") || str::EqI(s, "yes")) {
        return true;
    }
    if (str::EqI(s, "0") || str::EqI(s, "false") || str::EqI(s, "no")) {
        return true;
    }
    return false;
}

// parse:
//   <name> <value>
//   <name>: <value>
//   <name>=<value>
// for booleans only <name> works as well and represents true
CommandArg* TryParseNamedArg(int firstArgIdx, const char** argsInOut) {
    const char* valStart = nullptr;
    const char* argName = nullptr;
    CommandArg::Type type = CommandArg::Type::None;
    const char* s = *argsInOut;
    int cmdId = argSpecs[firstArgIdx].cmdId;
    for (int i = firstArgIdx;; i++) {
        if (argSpecs[i].cmdId != cmdId) {
            // not a known argument for this command
            return nullptr;
        }
        argName = argSpecs[i].name;
        if (!str::StartsWithI(s, argName)) {
            continue;
        }
        type = argSpecs[i].type;
        break;
    }
    s += str::Len(argName);
    if (s[0] == 0) {
        if (type == CommandArg::Type::Bool) {
            // name of bool arg followed by nothing is true
            *argsInOut = nullptr;
            auto arg = NewArg(type, argName);
            arg->boolVal = true;
            return arg;
        }
    } else if (s[0] == ' ') {
        if (type == CommandArg::Type::Bool) {
            // name of bool arg followed by nothing is true
            s = str::SkipChar(s, ' ');
            *argsInOut = s;
            auto arg = NewArg(type, argName);
            arg->boolVal = true;
            return arg;
        }
        valStart = str::SkipChar(s, ' ');
    } else if (s[0] == ':' && s[1] == ' ') {
        valStart = str::SkipChar(s + 1, ' ');
    } else if (s[0] == '=') {
        valStart = s + 1;
    }
    if (valStart == nullptr) {
        // <args> doesn't start with any of the available commands for this command
        return nullptr;
    }
    const char* valEnd = str::FindChar(valStart, ' ');
    TempStr val = nullptr;
    if (valEnd == nullptr) {
        val = str::DupTemp(valStart);
    } else {
        val = str::DupTemp(valStart, valEnd - valStart);
        valEnd++;
    }
    if (type == CommandArg::Type::Bool) {
        auto bv = ParseBool(val);
        bool b;
        if (bv == 0) {
            b = false;
            *argsInOut = valEnd;
        } else if (bv == 1) {
            b = true;
            *argsInOut = valEnd;
        } else {
            // bv is -1, which means not a recognized bool value, so assume
            // it wasn't given
            // TODO: should apply only if arg doesn't end with ':' or '='
            b = true;
            *argsInOut = valStart;
        }
        auto arg = NewArg(type, argName);
        arg->boolVal = b;
        return arg;
    }

    *argsInOut = valEnd;
    return ParseArgOfType(argName, type, val);
}

// create custom command as defined in Shortcuts section in advanced settings.
// we return null if unkown command
CustomCommand* CreateCommandFromDefinition(const char* definition) {
    StrVec parts;
    Split(&parts, definition, " ", true, 2);
    const char* cmd = parts[0];
    int cmdId = GetCommandIdByName(cmd);
    if (cmdId < 0) {
        // TODO: make it a notification
        logf("CreateCommandFromDefinition: unknown cmd name in '%s'\n", definition);
        return nullptr;
    }
    if (parts.Size() == 1) {
        return CreateCustomCommand(definition, cmdId, nullptr);
    }

    // some commands share the same arguments, so cannonalize them
    int argCmdId = cmdId;
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
            argCmdId = CmdCreateAnnotText;
            break;
        }
        case CmdScrollUp:
        case CmdScrollDown:
        case CmdGoToNextPage:
        case CmdGoToPrevPage: {
            argCmdId = CmdScrollUp;
            break;
        }
    }

    // find arguments for this cmdId
    int firstArgIdx = -1;
    for (int i = 0;; i++) {
        int id = argSpecs[i].cmdId;
        if (id == CmdNone) {
            // the command doesn't accept any arguments
            logf("CreateCommandFromDefinition: cmd '%s' doesn't accept arguments\n", definition);
            return CreateCustomCommand(definition, cmdId, nullptr);
        }
        if (id != argCmdId) {
            continue;
        }
        firstArgIdx = i;
        break;
    }
    if (firstArgIdx < 0) {
        // shouldn't happen, we already filtered commands without arguments
        logf("CreateCommandFromDefinition: didn't find arguments for: '%s', cmdId: %d, argCmdId: '%d'\n", definition,
             cmdId, argCmdId);
        ReportIf(true);
        return nullptr;
    }

    const char* currArg = parts[1];

    CommandArg* firstArg = nullptr;
    CommandArg* arg;
    for (; currArg;) {
        arg = TryParseNamedArg(firstArgIdx, &currArg);
        if (!arg) {
            arg = TryParseDefaultArg(firstArgIdx, &currArg);
        }
        if (arg) {
            InsertArg(&firstArg, arg);
        }
    }
    if (!firstArg) {
        logf("CreateCommandFromDefinition: failed to parse arguments for '%s'\n", definition);
        return nullptr;
    }

    if (cmdId == CmdCommandPalette && firstArg) {
        // validate mode
        const char* s = firstArg->strVal;
        static SeqStrings validModes = ">\0#\0@\0:\0"; // TODO: "@@\0" ?
        if (seqstrings::StrToIdx(validModes, s) < 0) {
            logf("CreateCommandFromDefinition: invalid CmdCommandPalette mode in '%s'\n", definition);
            FreeCommandArgs(firstArg);
            firstArg = nullptr;
        }
    }

    if (cmdId == CmdZoomCustom) {
        // special case: the argument is declared as string but it really is float
        // we convert it in-place here
        float zoomVal = ZoomFromString(firstArg->strVal, 0);
        if (0 == zoomVal) {
            FreeCommandArgs(firstArg);
            logf("CreateCommandFromDefinition: failed to parse arguments in '%s'\n", definition);
            return nullptr;
        }
        firstArg->type = CommandArg::Type::Float;
        firstArg->floatVal = zoomVal;
    }
    auto res = CreateCustomCommand(definition, cmdId, firstArg);
    return res;
}

CommandArg* GetCommandArg(CustomCommand* cmd, const char* name) {
    if (!cmd) {
        return nullptr;
    }
    CommandArg* curr = cmd->firstArg;
    while (curr) {
        if (str::EqI(curr->name, name)) {
            return curr;
        }
        curr = curr->next;
    }
    return nullptr;
}

int GetCommandIntArg(CustomCommand* cmd, const char* name, int defValue) {
    auto arg = GetCommandArg(cmd, name);
    if (arg) {
        return arg->intVal;
    }
    return defValue;
}

bool GetCommandBoolArg(CustomCommand* cmd, const char* name, bool defValue) {
    auto arg = GetCommandArg(cmd, name);
    if (arg) {
        return arg->boolVal;
    }
    return defValue;
}

const char* GetCommandStringArg(CustomCommand* cmd, const char* name, const char* defValue) {
    auto arg = GetCommandArg(cmd, name);
    if (arg) {
        return arg->strVal;
    }
    return defValue;
}
