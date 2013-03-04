#!/usr/bin/env python

import os, re, util, codecs

# List of languages we support, their iso codes and id as understood
# by Windows SDK (LANG_* and SUBLANG_*_*).
# See http://msdn.microsoft.com/en-us/library/dd318693.aspx for the full list.
g_langs = [
    ('af'    , 'Afrikaans',                              '_LANGID(LANG_AFRIKAANS)'),
    ('am'    , 'Armenian (Հայերեն)',                     '_LANGID(LANG_ARMENIAN)'),
    ('ar'    , 'Arabic (الْعَرَبيّة)',                         '_LANGID(LANG_ARABIC)', 'RTL'),
    ('az'    , 'Azerbaijani (آذربایجان دیلی)',              '_LANGID(LANG_AZERI)'),
    ('bg'    , 'Bulgarian (Български)',                  '_LANGID(LANG_BULGARIAN)'),
    ('bn'    , 'Bengali (বাংলা)',                      '_LANGID(LANG_BENGALI)'),
    ('br'    , 'Portuguese - Brazil (Português)',         'MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE_BRAZILIAN)'),
    ('bs'    , 'Bosnian (Bosanski)',                      'MAKELANGID(LANG_BOSNIAN, SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_LATIN)'),
    ('by'    , 'Belarusian (Беларуская)',                 '_LANGID(LANG_BELARUSIAN)'),
    ('ca'    , 'Catalan (Català)',                        '_LANGID(LANG_CATALAN)'),
    ('ca-xv' , 'Catalan-Valencian (Català-Valencià)',     '-1'),
    ('cn'    , 'Chinese Simplified (简体中文)',           'MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED)'),
    ('cy'    , 'Welsh (Cymraeg)',                         '_LANGID(LANG_WELSH)'),
    ('cz'    , 'Czech (Čeština)',                         '_LANGID(LANG_CZECH)'),
    ('de'    , 'German (Deutsch)',                        '_LANGID(LANG_GERMAN)'),
    ('dk'    , 'Danish (Dansk)',                          '_LANGID(LANG_DANISH)'),
    ('el'    , 'Greek (Ελληνικά)',                        '_LANGID(LANG_GREEK)'),
    ('en'    , 'English',                                 '_LANGID(LANG_ENGLISH)'),
    ('es'    , 'Spanish (Español)',                       '_LANGID(LANG_SPANISH)'),
    ('et'    , 'Estonian (Eesti)',                        '_LANGID(LANG_ESTONIAN)'),
    ('eu'    , 'Basque (Euskara)',                        '_LANGID(LANG_BASQUE)'),
    ('fa'    , 'Persian (فارسی)',                         '_LANGID(LANG_FARSI)', 'RTL'),
    ('fi'    , 'Finnish (Suomi)',                         '_LANGID(LANG_FINNISH)'),
    ('fr'    , 'French (Français)',                       '_LANGID(LANG_FRENCH)'),
    ('fy-nl' , 'Frisian (Frysk)',                         '_LANGID(LANG_FRISIAN)'),
    ('ga'    , 'Irish (Gaeilge)',                         '_LANGID(LANG_IRISH)'),
    ('gl'    , 'Galician (Galego)',                       '_LANGID(LANG_GALICIAN)'),
    ('he'    , 'Hebrew (עברית)',                          '_LANGID(LANG_HEBREW)', 'RTL'),
    ('hi'    , 'Hindi (हिंदी)',                            '_LANGID(LANG_HINDI)'),
    ('hr'    , 'Croatian (Hrvatski)',                     '_LANGID(LANG_CROATIAN)'),
    ('hu'    , 'Hungarian (Magyar)',                      '_LANGID(LANG_HUNGARIAN)'),
    ('id'    , 'Indonesian (Bahasa Indonesia)',           '_LANGID(LANG_INDONESIAN)'),
    ('it'    , 'Italian (Italiano)',                      '_LANGID(LANG_ITALIAN)'),
    ('ja'    , 'Japanese (日本語)',                       '_LANGID(LANG_JAPANESE)'),
    ('ka'    , 'Georgian (ქართული)',                  '_LANGID(LANG_GEORGIAN)'),
    ('kr'    , 'Korean (한국어)',                         '_LANGID(LANG_KOREAN)'),
    ('ku'    , 'Kurdish (كوردی)',                          'MAKELANGID(LANG_CENTRAL_KURDISH, SUBLANG_CENTRAL_KURDISH_CENTRAL_KURDISH_IRAQ)', 'RTL'),
    ('kw'    , 'Cornish (Kernewek)',                      '-1'),
    ('lt'    , 'Lithuanian (Lietuvių)',                   '_LANGID(LANG_LITHUANIAN)'),
    ('mk'    , 'Macedonian (македонски)',                 '_LANGID(LANG_MACEDONIAN)'),
    ('ml'    , 'Malayalam (മലയാളം)',                   '_LANGID(LANG_MALAYALAM)'),
    ('mm'    , 'Burmese (ဗမာ စာ)',                     '-1'),
    ('my'    , 'Malaysian (Bahasa Melayu)',               '_LANGID(LANG_MALAY)'),
    ('ne'    , 'Nepali (नेपाली)',                         '_LANGID(LANG_NEPALI)'),
    ('nl'    , 'Dutch (Nederlands)',                      '_LANGID(LANG_DUTCH)'),
    ('nn'    , 'Norwegian Neo-Norwegian (Norsk nynorsk)', 'MAKELANGID(LANG_NORWEGIAN, SUBLANG_NORWEGIAN_NYNORSK)'),
    ('no'    , 'Norwegian (Norsk)',                       'MAKELANGID(LANG_NORWEGIAN, SUBLANG_NORWEGIAN_BOKMAL)'),
    ('pa'    , 'Punjabi (ਪੰਜਾਬੀ)',                             '_LANGID(LANG_PUNJABI)'),
    ('pl'    , 'Polish (Polski)',                         '_LANGID(LANG_POLISH)'),
    ('pt'    , 'Portuguese - Portugal (Português)',       '_LANGID(LANG_PORTUGUESE)'),
    ('ro'    , 'Romanian (Română)',                       '_LANGID(LANG_ROMANIAN)'),
    ('ru'    , 'Russian (Русский)',                       '_LANGID(LANG_RUSSIAN)'),
    ('si'    , 'Sinhala (සිංහල)',                     '_LANGID(LANG_SINHALESE)'),
    ('sk'    , 'Slovak (Slovenčina)',                    '_LANGID(LANG_SLOVAK)'),
    ('sl'    , 'Slovenian (Slovenščina)',                '_LANGID(LANG_SLOVENIAN)'),
    ('sn'    , 'Shona (Shona)',                          '-1'),
    ('sp-rs' , 'Serbian (Latin)',                        'MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_LATIN)'),
    ('sq'    , 'Albanian (Shqip)',                       '_LANGID(LANG_ALBANIAN)'),
    ('sr-rs' , 'Serbian (Cyrillic)',                     'MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_CYRILLIC)'),
    ('sv'    , 'Swedish (Svenska)',                      '_LANGID(LANG_SWEDISH)'),
    ('ta'    , 'Tamil (தமிழ்)',                         '_LANGID(LANG_TAMIL)'),
    ('th'    , 'Thai (ภาษาไทย)',                       '_LANGID(LANG_THAI)'),
    ('tl'    , 'Tagalog (Tagalog)',                     '_LANGID(LANG_FILIPINO)'),
    ('tr'    , 'Turkish (Türkçe)',                      '_LANGID(LANG_TURKISH)'),
    ('tw'    , 'Chinese Traditional (繁體中文)',        'MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL)'),
    ('uk'    , 'Ukrainian (Українська)',                '_LANGID(LANG_UKRAINIAN)'),
    ('uz'    , 'Uzbek (O\'zbek)',                       '_LANGID(LANG_UZBEK)'),
    ('vn'    , 'Vietnamese (Việt Nam)',                 '_LANGID(LANG_VIETNAMESE)'),
]

class Lang(object):
    def __init__(self, desc):
        assert len(desc) <= 4
        self.desc = desc
        self.code = desc[0] # "af"
        self.name = desc[1] # "Afrikaans"
        self.ms_lang_id_info = desc[2]
        self.isRtl = False
        if len(desc) > 3:
            assert desc[3] == 'RTL'
            self.isRtl = True

        # code that can be used as part of C identifier i.e.:
        # "ca-xv" => "ca_xv"
        self.code_safe = self.code.replace("-", "_")
        self.c_translations_array_name = "gTranslations_" + self.code_safe
        self.ms_lang_id_string = make_lang_id(self.desc)
        self.translations = []

def get_lang_objects(langs_defs):
    return [Lang(desc) for desc in langs_defs]

# number of missing translations for a language to be considered
# incomplete (will be excluded from Translations_txt.cpp) as a
# percentage of total string count of that specific file
INCOMPLETE_MISSING_THRESHOLD = 0.2

SRC_DIR = os.path.join(os.path.dirname(__file__), "..", "src")
C_TRANS_FILENAME = "Translations_txt.cpp"

C_DIRS_TO_PROCESS = [".", "installer", "browserplugin"]
# produce a simpler format for these dirs
C_SIMPLE_FORMAT_DIRS = ["installer", "browserplugin"]
# whitelist some files as an optimization
C_FILES_TO_EXCLUDE = [C_TRANS_FILENAME.lower()]

def should_translate(file_name):
    file_name = file_name.lower()
    if not file_name.endswith(".cpp"):
        return False
    return file_name not in C_FILES_TO_EXCLUDE

C_FILES_TO_PROCESS = []
for dir in C_DIRS_TO_PROCESS:
    d = os.path.join(SRC_DIR, dir)
    C_FILES_TO_PROCESS += [os.path.join(d, f) for f in os.listdir(d) if should_translate(f)]

TRANSLATION_PATTERN = r'\b_TRN?\("(.*?)"\)'

def extract_strings_from_c_files(with_paths=False):
    strings = []
    for f in C_FILES_TO_PROCESS:
        file_content = open(f, "r").read()
        file_strings = re.findall(TRANSLATION_PATTERN, file_content)
        if with_paths:
            strings += [(s, os.path.basename(os.path.dirname(f))) for s in file_strings]
        else:
            strings += file_strings
    return util.uniquify(strings)

TRANSLATIONS_TXT_SIMPLE = """\
/* Generated by scripts\\update_translations.py - DO NOT EDIT MANUALLY */

#ifndef MAKELANGID
#include <windows.h>
#endif

int gTranslationsCount = %(translations_count)d;

const WCHAR * const gTranslations[] = {
%(translations)s
};

const char * const gLanguages[] = { %(langs_list)s };

// from http://msdn.microsoft.com/en-us/library/windows/desktop/dd318693(v=vs.85).aspx
// those definition are not present in 7.0A SDK my VS 2010 uses
#ifndef LANG_CENTRAL_KURDISH
#define LANG_CENTRAL_KURDISH 0x92
#endif

#ifndef SUBLANG_CENTRAL_KURDISH_CENTRAL_KURDISH_IRAQ
#define SUBLANG_CENTRAL_KURDISH_CENTRAL_KURDISH_IRAQ 0x01
#endif

// note: this index isn't guaranteed to remain stable over restarts, so
// persist gLanguages[index/gTranslationsCount] instead
int GetLanguageIndex(LANGID id)
{
    switch (id) {
#define _LANGID(lang) MAKELANGID(lang, SUBLANG_NEUTRAL)
    %(lang_id_to_index)s
#undef _LANGID
    }
}

bool IsLanguageRtL(int index)
{
    return %(rtl_lang_cmp)s;
}
"""

# use octal escapes because hexadecimal ones can consist of
# up to four characters, e.g. \xABc isn't the same as \253c
def c_oct(c):
    o = "00" + oct(ord(c))
    return "\\" + o[-3:]

def c_escape(txt):
    if txt is None:
        return "NULL"
    # escape all quotes
    txt = txt.replace('"', r'\"')
    # and all non-7-bit characters of the UTF-8 encoded string
    txt = re.sub(r"[\x80-\xFF]", lambda m: c_oct(m.group(0)[0]), txt)
    return '"%s"' % txt

def c_escape_for_compact(txt):
    if txt is None:
        return '"\\0"'
    # escape all quotes
    txt = txt.replace('"', r'\"')
    # and all non-7-bit characters of the UTF-8 encoded string
    txt = re.sub(r"[\x80-\xFF]", lambda m: c_oct(m.group(0)[0]), txt)
    return '"%s\\0"' % txt

def get_trans_for_lang(strings_dict, keys, lang_arg):
    if lang_arg == "en":
        return keys
    trans, untrans = [], []
    for k in keys:
        found = [tr for (lang, tr) in strings_dict[k] if lang == lang_arg]
        if found:
            assert len(found) == 1
            # don't include a translation, if it's the same as the default
            if found[0] == k:
                found[0] = None
            trans.append(found[0])
        else:
            trans.append(None)
            untrans.append(k)
    if len(untrans) > INCOMPLETE_MISSING_THRESHOLD * len(keys):
        return None
    return trans

def lang_sort_func(x,y):
    # special case: default language is first
    if x[0] == "en": return -1
    if y[0] == "en": return 1
    return cmp(x[1], y[1])

def make_lang_id(lang): return lang[2]

def is_rtl_lang(lang):
    return "true" if len(lang) > 3 and lang[3] == "RTL" else "false"

# correctly sorts strings containing escaped tabulators
def key_sort_func(a, b):
    return cmp(a.replace(r"\t", "\t"), b.replace(r"\t", "\t"))

def build_trans_for_langs(langs, strings_dict, keys):
    incomplete = []
    for lang in langs:
        lang.translations = get_trans_for_lang(strings_dict, keys, lang.code)
        if not lang.translations:
            incomplete.append(lang)
    for lang in incomplete:
        langs.remove(lang)
    return langs

TRANSLATIONS_TXT_C = """\
/* Generated by scripts\\update_translations.py - DO NOT EDIT MANUALLY */

#include <windows.h>

namespace trans {

#define LANGS_COUNT   %(langs_count)d
#define STRINGS_COUNT %(translations_count)d

%(translations)s

const char *gLangCodes = \
%(langcodes)s;

const char *gLangNames = \
%(langnames)s;

// from http://msdn.microsoft.com/en-us/library/windows/desktop/dd318693(v=vs.85).aspx
// those definition are not present in 7.0A SDK my VS 2010 uses
#ifndef LANG_CENTRAL_KURDISH
#define LANG_CENTRAL_KURDISH 0x92
#endif

#ifndef SUBLANG_CENTRAL_KURDISH_CENTRAL_KURDISH_IRAQ
#define SUBLANG_CENTRAL_KURDISH_CENTRAL_KURDISH_IRAQ 0x01
#endif

#define _LANGID(lang) MAKELANGID(lang, SUBLANG_NEUTRAL)
const LANGID gLangIds[LANGS_COUNT] = {
%(langids)s
};
#undef _LANGID

const char *gTranslations[LANGS_COUNT] = {
%(translations_refs)s
};

#define RTL_LANGS_COUNT %(rtl_langs_count)d
static const int gRtlLangs[RTL_LANGS_COUNT] = { %(rtl_langs)s };

/* TODO: could be optimized further to be something like:
  if (langIdx < 32)
    return bit::IsSet($bitmask_for_first_32_langs, langIdx);
  if (langIdx < 64)
    rturn bit::IsSet($bitmask_for_33_to_64_langs, langIdx-32)
  return false;
*/
bool IsLangRtl(int langIdx)
{
    for (int i = 0; i < RTL_LANGS_COUNT; i++) {
        if (gRtlLangs[i] == langIdx)
            return true;
    }
    return false;
}

int gLangsCount = LANGS_COUNT;
int gStringsCount = STRINGS_COUNT;
const char *  gCurrLangStrings[STRINGS_COUNT] = { 0 };
const WCHAR * gCurrLangTransCache[STRINGS_COUNT] = { 0 };

// Note: don't know how to expose gLangIds as a symbol.
// Seems C++ is a bit too strict about T* vs. T[n]
const LANGID *GetLangIds() { return &gLangIds[0]; }

} // namespace trans
"""

# generate unique names for translations files for each binary, to simplify build
def file_name_from_dir_name(dir_name):
    if dir_name == ".":
        return "Trans_sumatra_txt.cpp"
    return "Trans_%s_txt.cpp" % dir_name

def gen_c_code_for_dir(strings_dict, keys, dir_name):
    langs = get_lang_objects(sorted(g_langs, cmp=lang_sort_func))
    assert "en" == langs[0].code
    langs = build_trans_for_langs(langs, strings_dict, keys)

    langcodes = " \\\n".join(["  %s" % c_escape_for_compact(lang.code) for lang in langs])
    langnames = " \\\n".join(["  %s" % c_escape_for_compact(lang.name) for lang in langs])
    langids = ",\n".join(["  %s" % lang.ms_lang_id_string for lang in langs])

    rtl_langs = []
    for idx in range(len(langs)):
        if langs[idx].isRtl:
            rtl_langs.append(str(idx))
    rtl_langs_count = len(rtl_langs)
    rtl_langs = ", ".join(rtl_langs)

    total_size = 0
    lines = []
    for lang in langs:
        lines.append("const char * %s = " % lang.c_translations_array_name)
        for t in lang.translations:
            total_size += 1 # terminating zero
            if t != None:
                total_size = total_size + len(t)
        lines += ["  %s \\" % c_escape_for_compact(t) for t in lang.translations]
        lines.append(";\n")
    translations = "\n".join(lines)

    translations_refs = ", \n".join(["  %s" % lang.c_translations_array_name for lang in langs])

    langs_count = len(langs)
    translations_count = len(keys)
    file_content = TRANSLATIONS_TXT_C % locals()
    file_path = os.path.join(SRC_DIR, dir_name, file_name_from_dir_name(dir_name))
    file(file_path, "wb").write(file_content)

    print("\nTotal size of translations: %d" % total_size)

def gen_c_code_simple(strings_dict, keys, dir_name):
    langs_idx = sorted(g_langs, cmp=lang_sort_func)
    assert "en" == langs_idx[0][0]

    lines = []
    incomplete_langs = []
    for lang in langs_idx:
        trans = get_trans_for_lang(strings_dict, keys, lang[0])
        if not trans:
            incomplete_langs.append(lang)
            continue
        lines.append('  /* Translations for language %s */' % lang[0])
        lines += ['  L"%s",' % t.replace('"', '\\"') if t else '  NULL,' for t in trans]
        lines.append("")
    lines.pop()
    for lang in incomplete_langs:
        langs_idx.remove(lang)
    translations = "\n".join(lines)

    langs_list = ", ".join(['"%s"' % lang[0] for lang in langs_idx] + ["NULL"])
    lang_id_to_index = "\n    ".join(["case %s: return %d;" % (make_lang_id(lang), langs_idx.index(lang) * len(keys)) for lang in langs_idx] + ["default: return -1;"])
    rtl_lang_cmp = " || ".join(["%d == index" % langs_idx.index(lang) * len(keys) for lang in langs_idx if is_rtl_lang(lang) == "true"]) or "false"

    translations_count = len(keys)
    file_content = codecs.BOM_UTF8 + TRANSLATIONS_TXT_SIMPLE % locals()
    file_name = os.path.join(SRC_DIR, dir_name, C_TRANS_FILENAME)
    file(file_name, "wb").write(file_content)

"""
A format for the compact storage of strings:

Generated C code:

uint32_t        gLangsCompressedSize[LANGS_COUNT] = { ... };
uint32_t        gLangsUncompressedSize[LANGS_COUNT] = { ... };
const uint8_t * gLangsCompressedData[LANGS_COUNT] = { ... };
const void *    gLangsUncompressedData[LANGS_COUNT] = { ... };

Format of compressed data (after it's de-compressed)
  string[strings_count]    strings, sequentially laid

Note: english strings are not included, they are stored as before, as an
array of char *, so that the compiler can merge those string constants
with strings in code (inside _TR() macros). TODO: verify that the strings
are actually de-duped by the compiler.
"""
def gen_c_compact(strings_dict, strings):
    pass # TODO: write me

def gen_c_code(strings_dict, strings):
    for dir in C_DIRS_TO_PROCESS:
        keys = [s[0] for s in strings if s[1] == dir and s[0] in strings_dict]
        keys.sort(cmp=key_sort_func)
        if dir not in C_SIMPLE_FORMAT_DIRS:
            gen_c_code_for_dir(strings_dict, keys, dir)
        else:
            gen_c_code_simple(strings_dict, keys, dir)

def main():
    import trans_download
    trans_download.regenerateLangs()

if __name__ == "__main__":
    main()
