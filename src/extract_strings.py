import codecs

c_files_to_process = ["SumatraPDF.cpp", "SumatraDialogs.cc"]
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
def is_newline_char(c): return c == '\n' or c == '\r'

def is_comment_line(l): 
    if 0 == len(l): return False
    return l[0] == '#'

def line_strip_newline(l):
    while True:
        if 0  == len(l): return l
        if not is_newline_char(l[-1]): return l
        l = l[:-1]

def line_with_translation(l):
    if len(l) < 3: return False
    if ':' == l[2]: return True
    if len(l) < 6: return False
    if ':' == l[5]: return True
    return False

def parse_line_with_translation(l):
    if ':' == l[2]:
        idx = 2
    elif ':' == l[5]:
        idx = 5
    else:
        assert 0
    lang = l[:idx]
    txt = l[idx+1:]
    return (lang, txt)

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
            assert ST_NONE == state
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
                report_error(line_no, l, "lang '%s' is not in declared list of languages '%s'" % (lang, str(langs)))
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

def extract_strings_with_mark(l, mark_start, mark_end):
    strings = []
    start_pos = 0
    while True:
        tr_start = l.find(mark_start, start_pos)
        if -1 == tr_start:
            return strings
        tr_start = tr_start + len(mark_start)
        tr_end = l.find(mark_end, tr_start)
        if -1 == tr_end:
            print l
            assert tr_end != -1 # if it is, it's suspicious
        strings.append(l[tr_start:tr_end])
        start_pos = tr_end

def extract_strings_from_line(l):
    strings1 = extract_strings_with_mark(l, '_TR("', '")')
    strings2 = extract_strings_with_mark(l, '_TRN("', '")')
    strings3 = extract_strings_with_mark(l, '_TRW("', '")')
    strings4 = extract_strings_with_mark(l, '_TRWN("', '")')
    strings5 = extract_strings_with_mark(l, '_TRA("', '")')
    return strings1 + strings2 + strings3 + strings4 + strings5

def extract_strings_from_c_file(f, strings):
    fo = open(f, "rb")
    for l in fo.readlines():
        strs = extract_strings_from_line(l)
        for s in strs:
            strings.append(s)
    fo.close()
    
def extract_strings_from_c_files(files):
    strings = []
    for f in files:
        extract_strings_from_c_file(f, strings)
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
    print "Only in C code:"
    for (s, str_state) in strings_all.items():
        if SS_ONLY_IN_C == str_state:
            print "%s" % s
    print
    print "Only in %s file:" % strings_file
    for (s, str_state) in strings_all.items():
        if SS_ONLY_IN_TXT == str_state:
            print "'%s'" % s

def langs_sort_func(x,y):
    return cmp(len(y[1]),len(x[1]))

def dump_missing_per_language(strings_dict, dump_strings=False):
    untranslated_dict = {}
    # strings that don't need to be translated
    exceptions = ["6400%", "3200%", "1600%", "800%", "400%", "200%", "150%", "100%", "125%", "50%", "25%", "12.5%", "8.33%"]
    for lang in get_lang_list(strings_dict):
        untranslated = []
        for txt in strings_dict.keys():
            if txt in exceptions: continue
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
                

def main():
    (strings_dict, langs) = load_strings_file(strings_file)
    strings = extract_strings_from_c_files(c_files_to_process)
    dump_missing_per_language(strings_dict)
    print
    dump_diffs(strings_dict, strings)

if __name__ == "__main__":
    main()

