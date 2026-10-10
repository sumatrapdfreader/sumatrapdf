/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by SumatraControlCommon.cpp and each app's SumatraControl.cpp ---

TempStr PageBoxesResultTemp(int pageNo, int* exitCodeOut);
void AppendLayoutRect(str::Builder& out, Str name, bool visible, Rect rect);
TempStr SelectionVarsResultTemp(Str pattern, int* exitCodeOut);
TempStr DocumentSignaturesResultTemp(int* exitCodeOut);
TempStr DocumentFontListResultTemp(int* exitCodeOut);
TempStr DocumentPropertiesResultTemp(int* exitCodeOut);
enum class ControlArgType : u16 {
    End = 0,
    Int32 = 1,
    Bytes = 2,
    String = 3,
    List = 4,
};
struct ControlArg {
    ControlArgType type = ControlArgType::End;
    i32 intVal = 0;
    u8* bytes = nullptr;
    u32 bytesLen = 0;
    Str str;
    Vec<ControlArg*>* list = nullptr;
};
void DeleteControlArg(ControlArg* arg);
void AppendU16(str::Builder& s, u16 v);
void AppendU32(str::Builder& s, u32 v);
void AppendArgEnd(str::Builder& s);
void AppendArgInt(str::Builder& s, i32 v);
void AppendArgString(str::Builder& s, Str str);
