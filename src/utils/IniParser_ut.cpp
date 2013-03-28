/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "IniParser.h"

static void IniTest()
{
    IniFile pNull((const char *)NULL);
    assert(pNull.sections.Count() == 0);

    IniFile pEmpty("");
    assert(pEmpty.sections.Count() == 1 && !pEmpty.sections.At(0)->name);
    assert(pEmpty.FindSection(NULL)->lines.Count() == 0);

    IniFile pNoNL1("key = value");
    assert(str::Eq(pNoNL1.sections.At(0)->lines.At(0).value, "value"));

    IniFile pNoNL2("[Section ]");
    assert(pNoNL2.FindSection("Section "));

    static const char *iniData = "\
; NULL section \n\
key1=value1\n\
[Section 1]\n\
  key2 = value2 \n\
  # comment \n\
  key3: \"quoted value\" \n\n\
[ Empty Section ]\n\n\
  ]Section 2 \n\
key without separator\n\
";
    IniLine verify[] = {
        IniLine(NULL, NULL), IniLine("key1", "value1"),
        IniLine("Section 1", NULL), IniLine("Key2", "value2"), IniLine("key3", "\"quoted value\""),
        IniLine(" Empty Section ", NULL),
        IniLine("Section 2", NULL), IniLine("key", "without separator"),
    };

    IniFile p(iniData);
    size_t idxSec = (size_t)-1, idxLine = (size_t)-1;
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
    assert(p.sections.Count() == idxSec + 1 && p.sections.Last()->lines.Count() == idxLine);
    assert(p.FindSection("Section 1", 1) && p.FindSection("Section 2", 2));
    assert(!p.FindSection("missing") && !p.FindSection("Section 1", 2));
    assert(!p.FindSection("Section 1")->FindLine("missing"));
}
