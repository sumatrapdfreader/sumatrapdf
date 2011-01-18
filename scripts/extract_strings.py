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

C_FILES_TO_PROCESS = ["SumatraPDF.cpp", "SumatraAbout.cpp", "SumatraProperties.cpp", "SumatraDialogs.cc", "CrashHandler.cpp"]
C_FILES_TO_PROCESS = [os.path.join("..", "src", f) for f in C_FILES_TO_PROCESS]
STRINGS_PATH = os.path.join("..", "src", "strings")
TRANSLATION_PATTERN = r'_TRN?\("(.*?)"\)'

LANG_TXT = "Lang:"
CONTRIBUTOR_TXT = "Contributor:"

def is_lang_line(l): return l.startswith(LANG_TXT)
def is_comment_line(l): return l.startswith("#")
def is_contributor_line(l): return l.startswith(CONTRIBUTOR_TXT)

def seq_uniq(seq): 
    # Not order preserving
    return list(set(seq))

def parse_lang_line(l):
    assert is_lang_line(l)
    l = l[len(LANG_TXT)+1:]
    l_parts = l.split(" ", 1)
    assert len(l_parts) == 2
    lang_iso = l_parts[0]
    lang_name = l_parts[1].strip()
    # lang format is either "fr" or "en-us"
    assert len(lang_iso) in [2, 5]
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
# (e.g. "br.txt" or "sr-sr.txt"). Returns None if file name doesn't fit expected pattern
def lang_code_from_file_name(file_name):
    match = re.match(r"(\w{2}(?:-\w{2})?)\.txt", file_name)
    return match and match.group(1)

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
        l = l[:-1] # strip trailing new line
        #print "'%s'" % l
        if 0 == len(l):
            if curr_orig is None: continue
            assert curr_orig not in all_origs, "Duplicate entry for '%s'" % curr_orig
            assert curr_trans is not None, "File %s, line %d" % (file_path, line_no)
            if curr_orig not in strings_dict:
                strings_dict[curr_orig] = [(lang_code, curr_trans)]
            else:
                strings_dict[curr_orig].append((lang_code, curr_trans))
            all_origs[curr_orig] = True
            curr_orig = None
            curr_trans = None
        elif is_comment_line(l):
            if seen_lang:
                bottom_comments.append(l)
            else:
                top_comments.append(l)
        elif is_lang_line(l):
            assert not seen_lang
            assert 0 == len(contributors)
            (lang_iso, lang_name) = parse_lang_line(l)
            assert lang_iso == lang_code, "lang code ('%s') in file '%s' must match code in file name ('%s')" % (lang_iso, file_path, lang_code)
            assert lang_iso not in langs_dict
            langs_dict[lang_iso] = lang_name
            seen_lang = True
        elif is_contributor_line(l):
            assert seen_lang
            contributors.append(parse_contrib_line(l))
        elif curr_orig is None:
            curr_orig = l
            #print l
        else:
            if curr_trans != None:
                print("curr_trans: '%s', line: %d in '%s'" % (curr_trans, line_no, os.path.basename(file_path)))
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
    strings_dict = {}
    langs_dict = { "en" : "English" }
    contributors_dict = {}
    lang_codes = [lang_code_from_file_name(f) for f in os.listdir(STRINGS_PATH) if lang_code_from_file_name(f) is not None]
    for lang_code in lang_codes:
        path = os.path.join(STRINGS_PATH, lang_code + ".txt")
        load_one_strings_file(path, lang_code, strings_dict, langs_dict, contributors_dict)
    return (strings_dict, langs_dict.items(), contributors_dict)

def get_lang_list(strings_dict):
    langs = []
    for translations in strings_dict.values():
        for t in translations:
            lang = t[0]
            if lang not in langs:
                langs.append(lang)
    return langs

def extract_strings_from_c_files():
    strings = []
    for f in C_FILES_TO_PROCESS:
        file_content = open(f, "r").read()
        strings += re.findall(TRANSLATION_PATTERN, file_content)
    return seq_uniq(strings)

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

def langs_sort_func(x,y):
    return cmp(len(y[1]),len(x[1]))

# strings_dict maps a string to a list of [lang, translations...] list
def dump_missing_per_language(strings, strings_dict, dump_strings=False):
    untranslated_dict = {}
    for lang in get_lang_list(strings_dict):
        untranslated = []
        for s in strings:
            if not s in strings_dict:
                untranslated.append(s)
                continue
            translations = strings_dict[s]
            found = filter(lambda tr: tr[0] == lang, translations)
            if not found and s not in untranslated:
                untranslated.append(s)
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

# correctly sorts strings containing escaped tabulators
def key_sort_func(a, b):
    return cmp(a.replace(r"\t", "\t"), b.replace(r"\t", "\t"))

# converts strings_dict which maps english text to array of (lang, translation)
# tuples to a hash that maps language to an array (english phrase, translation)
def gen_translations_for_languages(strings_dict):
    translations_for_language = {}
    for english in sorted(strings_dict.keys(), cmp=key_sort_func):
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
    file_name = os.path.join(STRINGS_PATH, lang_id + ".txt")
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
        if not is_translated:
            print k

def untranslated_count_for_lang(strings_dict, lang):
    if 'en' == lang: return 0 # special case since the test below thinks all english are untranslated
    count = 0
    for k in strings_dict:
        is_translated = len([item[1] for item in strings_dict[k] if item[0] == lang]) == 1
        if not is_translated:
            #print("%s: %s" % (lang, k))
            count += 1
    return count

def load_lang_index():
    index = open(os.path.join(STRINGS_PATH, "index.tsv"), "r").read()
    index = re.sub(r"#.*", "", index)
    return re.findall("^(\S+)\t([^\t\n]*)(?:\t(.*))?", index, re.M)

def main_obsolete():
    (strings_dict, langs, contributors) = load_strings_file_new()
    strings = extract_strings_from_c_files()
    if len(sys.argv) == 1:
        untranslated = dump_missing_per_language(strings, strings_dict)
        write_out_strings_files(strings_dict, langs, contributors, untranslated)
    else:
        dump_missing_for_language(strings_dict, sys.argv[1])
    
if __name__ == "__main__":
    print("Run update_translations.py instead")
