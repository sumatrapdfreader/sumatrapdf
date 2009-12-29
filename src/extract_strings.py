import codecs, re

c_files_to_process = ["SumatraPDF.cpp", "SumatraDialogs.cc"]
translation_pattern = r'_TRN?\("(.*?)"\)'
strings_file = "strings.txt"

(ST_NONE, ST_BEFORE_ORIG, ST_IN_TRANSLATIONS) = range(3)

def state_name(state):
    if ST_NONE == state: return "ST_NONE"
    if ST_BEFORE_ORIG == state: return "ST_BEFORE_ORIG"
    if ST_IN_TRANSLATIONS == state: return "ST_IN_TRANSLATIONS"
    assert 0
    return "UNKNOWN STATE"

LANG_TXT = "Lang:"

def is_lang_line(l): return l.startswith(LANG_TXT)
def is_separator_line(l): return l == "-"
def is_comment_line(l): return l.startswith("#")

def line_strip_newline(l, newline_chars="\r\n"):
    while True:
        if 0  == len(l): return l
        if l[-1] not in newline_chars: return l
        l = l[:-1]

def line_with_translation(l):
    if len(l) > 2 and ':' == l[2]: return True
    if len(l) > 5 and ':' == l[5]: return True
    return False

def parse_line_with_translation(l):
    lang, translation = l.split(':', 1)
    return lang, translation

def parse_lang_line(l):
    assert is_lang_line(l)
    l = l[len(LANG_TXT)+1:]
    l_parts = l.split(" ", 1)
    assert len(l_parts) == 2
    lang_iso = l_parts[0]
    lang_name = l_parts[1].strip()
    # lang format is either "fr" or "en-us"
    assert 2 == len(lang_iso) or 5 == len(lang_iso)
    return (lang_iso, lang_name)

def report_error(line_no, line, err_txt):
    print "Error on line %d:" % line_no
    print "'%s'" % line
    print err_txt
    assert 0

def assert_unique_translation(curr_trans, lang, trans, line_no):
    for el in curr_trans[1:]:
        (lang2, trans2) = el
        if lang == lang2:
            print("Duplicate translation in lang '%s' at line %d" % (lang, line_no))
            assert 0

# Returns a tuple (strings, langs)
# 'strings' maps an original, untranslated string to
# an array of translation, where each translation is a tuple 
# [language, text translated into this language]
# 'langs' is an array of language definition tuples. First item in a tuple
# is language iso code (e.g. "en" or "sp-rs" and second is language name
def load_strings_file(file_name):
    strings_dict = {}
    langs = []
    lang_codes = {}
    fo = codecs.open(file_name, "r", "utf-8-sig")
    state = ST_NONE
    curr_trans = None
    line_no = 0
    for l in fo.readlines():
        line_no = line_no + 1
        #print "'%s'" % l
        l = line_strip_newline(l)
        if 0 == len(l):
            continue
        #print state_name(state)
        if is_comment_line(l):
            assert ST_NONE == state or ST_BEFORE_ORIG == state
            continue
        if is_lang_line(l):
            assert ST_NONE == state
            lang_info = parse_lang_line(l)
            langs.append(lang_info)
            lang_iso = lang_info[0]
            assert lang_iso not in lang_codes
            lang_codes[lang_iso] = True
            continue
        if is_separator_line(l):
            if None != curr_trans:
                key = curr_trans[0]
                value = curr_trans[1:]
                if key in strings_dict:
                    report_error(line_no, l, "'%s' is a duplicate text" % key)
                strings_dict[key] = value
            state = ST_BEFORE_ORIG
            curr_trans = None
            continue
        if 0 == len(langs):
            report_error(line_no, l, "Expected list of languages (Languages: ...)")
        if ST_BEFORE_ORIG == state:
            if None != curr_trans:
                print curr_trans
                assert None == curr_trans
            if line_with_translation(l):
                report_error(line_no, l, "Looks like a line with translation and expected the original string")
            curr_trans = [l]
            state = ST_IN_TRANSLATIONS
        elif ST_IN_TRANSLATIONS == state:
            if not line_with_translation(l):
                report_error(line_no, l, "Expected line with translation")
            (lang, txt) = parse_line_with_translation(l)
            if lang not in lang_codes:
                langnames = [e[0] for e in langs]
                report_error(line_no, l, "lang '%s' is not in declared list of languages '%s'" % (lang, ", ".join(langnames)))
            assert_unique_translation(curr_trans, lang, txt, line_no)
            curr_trans.append([lang, txt])
        else:
            assert 0
        #print l
    fo.close()
    # TODO: repeat of a code above
    if None != curr_trans:
        key = curr_trans[0]
        value = curr_trans[1:]
        if key in strings_dict:
            report_error(line_no, l, "'%s' is a duplicate text" % key)
        strings_dict[key] = value
    return (strings_dict, langs)

def get_lang_list(strings_dict):
    langs = []
    for translations in strings_dict.values():
        for t in translations:
            lang = t[0]
            if lang not in langs:
                langs.append(lang)
    return langs

def extract_strings_from_c_files(files):
    strings = []
    for f in files:
        file_content = file(f, "rb").read()
        strings += re.findall(translation_pattern, file_content)
    return strings

(SS_ONLY_IN_C, SS_ONLY_IN_TXT, SS_IN_BOTH) = range(3)

def gen_diff(strings_dict, strings):
    strings_all = {}
    for s in strings:
        if s in strings_dict:
            strings_all[s] = SS_IN_BOTH
        else:
            strings_all[s] = SS_ONLY_IN_C
    for s in strings_dict.keys():
        if s not in strings_all:
            strings_all[s] = SS_ONLY_IN_TXT
        else:
            assert strings_all[s] == SS_IN_BOTH
    return strings_all

def dump_diffs(strings_dict, strings):
    strings_all = gen_diff(strings_dict, strings)
    only_in_c = [s for (s, state) in strings_all.items() if state == SS_ONLY_IN_C]
    if only_in_c:
        print "\nOnly in C code:"
        print "\n".join(only_in_c) + "\n"
    only_in_txt = [s for (s, state) in strings_all.items() if state == SS_ONLY_IN_TXT]
    if only_in_txt:
        print "\nOnly in %s file:" % strings_file
        print "\n".join(only_in_txt) + "\n"

def langs_sort_func(x,y):
    return cmp(len(y[1]),len(x[1]))

# strings that don't need to be translated
translation_exceptions = ["6400%", "3200%", "1600%", "800%", "400%", "200%", "150%", "100%", "125%", "50%", "25%", "12.5%", "8.33%"]

def dump_missing_per_language(strings_dict, dump_strings=False):
    untranslated_dict = {}
    for lang in get_lang_list(strings_dict):
        untranslated = []
        for txt in strings_dict.keys():
            if txt in translation_exceptions: continue
            translations = strings_dict[txt]
            found_translation = False
            for tr in translations:
                tr_lang = tr[0]
                if lang == tr_lang:
                    found_translation = True
                    break
            if not found_translation:
                untranslated.append(txt)
        untranslated_dict[lang] = untranslated
    items = untranslated_dict.items()
    items.sort(langs_sort_func)
    for (lang, untranslated) in items:
        print "Language %s: %3d untranslated" % (lang, len(untranslated))
        if not dump_strings: continue
        for u in untranslated:
            print "  " + u

def dump_missing_for_language(strings_dict, lang):
    print "Untranslated strings for '%s':" % lang
    for k in strings_dict:
        is_translated = len([item[1] for item in strings_dict[k] if item[0] == lang]) == 1
        if not is_translated and not k in translation_exceptions:
            print k


def main():
    import sys
    (strings_dict, langs) = load_strings_file(strings_file)
    strings = extract_strings_from_c_files(c_files_to_process)
    if len(sys.argv) == 1:
        dump_missing_per_language(strings_dict)
        dump_diffs(strings_dict, strings)
    else:
        dump_missing_for_language(strings_dict, sys.argv[1])

if __name__ == "__main__":
    main()

