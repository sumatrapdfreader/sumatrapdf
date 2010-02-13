import codecs
import os
import os.path
import re
import sys

"""
Extracts translatable strings from *.c and *.h files, dumps statistics
about untranslated strings to stdout and adds untranslated strings as
comments at the end of strings file for each language.
"""

c_files_to_process = ["SumatraPDF.cpp", "SumatraDialogs.cc"]
translation_pattern = r'_TRN?\("(.*?)"\)'
STRINGS_FILE = "strings_obsolete.txt"
SCRIPT_PATH = os.path.realpath(".")
STRINGS_PATH = SCRIPT_PATH

# strings that don't need to be translated
TRANSLATION_EXCEPTIONS = ["6400%", "3200%", "1600%", "800%", "400%", "200%", "150%", "100%", "125%", "50%", "25%", "12.5%", "8.33%"]

(ST_NONE, ST_BEFORE_ORIG, ST_IN_TRANSLATIONS) = range(3)

def state_name(state):
    if ST_NONE == state: return "ST_NONE"
    if ST_BEFORE_ORIG == state: return "ST_BEFORE_ORIG"
    assert ST_IN_TRANSLATIONS == state, "UNKNOWN STATE %d" % state
    if ST_IN_TRANSLATIONS == state: return "ST_IN_TRANSLATIONS"
    return "UNKNOWN STATE"

LANG_TXT = "Lang:"
CONTRIBUTOR_TXT = "Contributor:"

def is_lang_line(l): return l.startswith(LANG_TXT)
def is_separator_line(l): return l == "-"
def is_comment_line(l): return l.startswith("#")
def is_contributor_line(l): return l.startswith(CONTRIBUTOR_TXT)

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

def parse_contrib_line(l):
    assert is_contributor_line(l)
    parts = l.split(":", 1)
    assert 2 == len(parts), "line: '%s'" % l
    return parts[1].strip()

def report_error(line_no, line, err_txt):
    print "Error on line %d:" % line_no
    print "'%s'" % line
    print err_txt
    raise ValueError

def assert_unique_translation(curr_trans, lang, line_no):
    for el in curr_trans[1:]:
        (lang2, trans2) = el
        assert lang != lang2, "Duplicate translation in lang '%s' at line %d" % (lang, line_no)

# Extract language code (e.g. "br") from language translation file name
# (e.g. "strings-br.txt"). Returns None if file name doesn't fit expected pattern
def lang_code_from_file_name(file_name):
    if not file_name.startswith("strings-"): return None
    if not file_name.endswith(".txt"): return None
    return file_name[len("strings-"):-len(".txt")]

# The structure of strings file should be: comments section at the beginning of the file,
# Lang: and Contributor: lines, translations, (optional) comments sectin at the end of the file
def load_one_strings_file(file_path, lang_code, strings_dict, langs_dict, contributors_dict):
    fo = codecs.open(file_path, "r", "utf-8-sig")
    seen_lang = False
    top_comments = []
    bottom_comments = []
    contributors = []
    curr_orig = None
    curr_trans = None
    line_no = 0
    all_origs = {}
    for l in fo.readlines():
        line_no = line_no + 1
        #print "'%s'" % l
        l = line_strip_newline(l)
        if 0 == len(l):
            if curr_orig is None: continue
            assert curr_orig not in all_origs, "Duplicate entry for '%s'" % curr_orig
            assert curr_trans is not None
            if curr_orig not in strings_dict:
                strings_dict[curr_orig] = [(lang_code, curr_trans)]
            else:
                strings_dict[curr_orig].append((lang_code, curr_trans))
            all_origs[curr_orig] = True
            curr_orig = None
            curr_trans = None
            continue
        #print state_name(state)
        if is_comment_line(l):
            if seen_lang:
                bottom_comments.append(l)
            else:
                top_comments.append(l)
            continue
        if is_lang_line(l):
            assert not seen_lang
            assert 0 == len(contributors)
            (lang_iso, lang_name) = parse_lang_line(l)
            assert lang_iso == lang_code, "lang code ('%s') in file '%s' must match code in file name ('%s')" % (lang_iso, file_path, lang_code)
            assert lang_iso not in langs_dict
            langs_dict[lang_iso] = lang_name
            seen_lang = True
            continue
        if is_contributor_line(l):
            assert seen_lang
            contributors.append(parse_contrib_line(l))
            continue
        if curr_orig is None:
            curr_orig = l
        else:
            assert curr_trans is None, "curr_trans: '%s', line: %d in '%s'" % (curr_trans, line_no, os.path.basename(file_path))
            curr_trans = l
        #print l

    if curr_orig:
        assert curr_trans
        if curr_orig not in strings_dict:
            strings_dict[curr_orig] = [(lang_code, curr_trans)]
        else:
            strings_dict[curr_orig].append((lang_code, curr_trans))
        all_origs[curr_orig] = True

    contributors_dict[lang_code] = contributors
    fo.close()
    #print("Parsing '%s', %d translations" % (os.path.basename(file_path), len(all_origs)))

# Returns a tuple (strings, langs)
# 'strings' maps an original, untranslated string to
# an array of translation, where each translation is a tuple 
# [language, text translated into this language]
# 'langs' is an array of language definition tuples. First item in a tuple
# is language iso code (e.g. "en" or "sp-rs" and second is language name
def load_strings_file_new():
    files_path = STRINGS_PATH
    strings_dict = {}
    langs_dict = { "en" : "English" }
    contributors_dict = {}
    lang_codes = [lang_code_from_file_name(f) for f in os.listdir(files_path) if lang_code_from_file_name(f) is not None]
    for lang_code in lang_codes:
        path = os.path.join(files_path, "strings-" + lang_code + ".txt")
        load_one_strings_file(path, lang_code, strings_dict, langs_dict, contributors_dict)
    for s in TRANSLATION_EXCEPTIONS:
        if s not in strings_dict:
            strings_dict[s] = []
    return (strings_dict, langs_dict.items(), contributors_dict)

# Returns a tuple (strings, langs)
# 'strings' maps an original, untranslated string to
# an array of translation, where each translation is a tuple 
# [language, text translated into this language]
# 'langs' is an array of language definition tuples. First item in a tuple
# is language iso code (e.g. "en" or "sp-rs" and second is language name
def load_strings_file_old():
    file_name = STRINGS_FILE
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
            assert_unique_translation(curr_trans, lang, line_no)
            curr_trans.append([lang, txt])
        else:
            raise ValueError
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
    #only_in_c = ["'" + s + "'" for s in only_in_c]
    if only_in_c:
        print "\nOnly in C code:"
        print "\n".join(only_in_c) + "\n"
    only_in_txt = [s for (s, state) in strings_all.items() if state == SS_ONLY_IN_TXT]
    if only_in_txt:
        print "\nOnly in %s file:" % STRINGS_FILE
        print "\n".join(only_in_txt) + "\n"

def langs_sort_func(x,y):
    return cmp(len(y[1]),len(x[1]))

def dump_missing_per_language(strings_dict, dump_strings=False):
    untranslated_dict = {}
    for lang in get_lang_list(strings_dict):
        untranslated = []
        for txt in strings_dict.keys():
            if txt in TRANSLATION_EXCEPTIONS: continue
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
    return untranslated_dict

TOP_COMMENT = """
# Lines at the beginning starting with # are comments
# This file must be in utf-8 encoding. Make sure you use a proper
# text editor (notepad++ or notepad2 will work) and set utf8 encoding.

"""

# converts strings_dict which maps english text to array of (lang, translation)
# tuples to a hash that maps language to an array (english phrase, translation)
def gen_translations_for_languages(strings_dict):
    translations_for_language = {}
    for english in sorted(strings_dict.keys()):
        translations = strings_dict[english]
        for (lang_id, translation) in translations:
            if 'en' == lang_id:
                print(translation)
            if lang_id not in translations_for_language:
                translations_for_language[lang_id] = []
            trans = translations_for_language[lang_id]
            trans.append([english, translation])
    return translations_for_language

def gen_strings_file_for_lang(lang_id, lang_name, translations, contributors, untranslated):
    file_name = "strings-" + lang_id + ".txt"
    trans = translations[lang_id]
    fo = codecs.open(file_name, "w", "utf-8-sig")
    fo.write("# Translations for %s (%s) language\n" % (lang_name, lang_id))
    fo.write(TOP_COMMENT)
    fo.write("Lang: %s %s\n\n" % (lang_id, lang_name))
    for c in contributors:
        fo.write("Contributor: %s\n" % c)
    if len(contributors) > 0:
        fo.write("\n")
    for (english, tr) in trans:
        fo.write(english + "\n")
        fo.write("%s\n\n" % tr)
    
    if len(untranslated) > 0:
        fo.write("# Untranslated:\n")
        for s in untranslated:
            fo.write("#%s\n" % s)
        fo.write("\n")
    fo.close()
    #print("%d translations in %s" % (len(trans), file_name))

def write_out_strings_files(strings_dict, langs, contributors={}, untranslated={}):
    translations_for_langs = gen_translations_for_languages(strings_dict)
    for (lang_id, lang_name) in langs:
        if 'en' == lang_id: continue
        contributors_for_lang = contributors.get(lang_id, [])
        untranslated_for_lang = sorted(untranslated.get(lang_id, []))
        gen_strings_file_for_lang(lang_id, lang_name, translations_for_langs, contributors_for_lang, untranslated_for_lang)

def dump_missing_for_language(strings_dict, lang):
    print "Untranslated strings for '%s':" % lang
    for k in strings_dict:
        is_translated = len([item[1] for item in strings_dict[k] if item[0] == lang]) == 1
        if not is_translated and k not in TRANSLATION_EXCEPTIONS:
            print k

def untranslated_count_for_lang(strings_dict, lang):
    if 'en' == lang: return 0 # special case since the test below thinks all english are untranslated
    count = 0
    for k in strings_dict:
        is_translated = len([item[1] for item in strings_dict[k] if item[0] == lang]) == 1
        if not is_translated and k not in TRANSLATION_EXCEPTIONS:
            count += 1
    return count

def main():
    (strings_dict, langs, contributors) = load_strings_file_new()
    strings = extract_strings_from_c_files(c_files_to_process)
    if len(sys.argv) == 1:
        untranslated = dump_missing_per_language(strings_dict)
        dump_diffs(strings_dict, strings)
        write_out_strings_files(strings_dict, langs, contributors, untranslated)
    else:
        dump_missing_for_language(strings_dict, sys.argv[1])
    
def main_old():
    (strings_dict, langs) = load_strings_file_old()
    strings = extract_strings_from_c_files(c_files_to_process)
    if len(sys.argv) == 1:
        dump_missing_per_language(strings_dict)
        dump_diffs(strings_dict, strings)
    else:
        dump_missing_for_language(strings_dict, sys.argv[1])

if __name__ == "__main__":
    main()
