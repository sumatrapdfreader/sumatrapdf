/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "IniParser.h"

static void IniTest()
{
    static const char *iniData = "\
; NULL section \n\
key1=value1\n\
[Section 1]\n\
  key2 = value2 \n\
  # comment \n\
  key3: \"quoted value\" \n\n\
  ]Section 2 \n\
key without separator\n\
";
    IniLine verify[] = {
        IniLine(), IniLine("key1", "value1"),
        IniLine("Section 1", NULL), IniLine("Key2", "value2"), IniLine("key3", "\"quoted value\""),
        IniLine("Section 2", NULL), IniLine("key", "without separator"),
    };

    IniFile p(iniData);
    size_t idxSec = -1, idxLine = -1;
    for (int i = 0; i < dimof(verify); i++) {
        if (!verify[i].value) {
            if (idxSec != (size_t)-1) {
                assert(p.sections.At(idxSec)->lines.Count() == idxLine);
            }
            idxSec++;
            idxLine = 0;
            assert(p.FindSection(verify[i].key) == p.sections.At(idxSec));
            continue;
        }
        assert(idxSec < p.sections.Count() && idxLine < p.sections.At(idxSec)->lines.Count());
        IniLine *line = p.sections.At(idxSec)->FindLine(verify[i].key);
        assert(line && str::EqI(line->key, verify[i].key) && str::Eq(line->value, verify[i].value));
        idxLine++;
    }
    assert(p.sections.Count() == idxSec + 1);
    assert(!p.FindSection("missing") && !p.FindSection("Section 1", 1));
    assert(!p.FindSection("Section 1")->FindLine("missing"));
}
