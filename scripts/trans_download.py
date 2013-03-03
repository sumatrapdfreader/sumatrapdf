#!/usr/bin/env python

# Downloads latest translations from apptranslator.org.
# If changed, saves them as strings/translations.txt and
# re-generates src/Translations_txt.cpp etc.

import os, sys, urllib2, util
from trans_gen import gen_c_code, extract_strings_from_c_files

g_my_dir = os.path.dirname(__file__)
g_strings_dir = os.path.join(g_my_dir, "..", "strings")

use_local_for_testing = False

if use_local_for_testing:
    SERVER = "172.21.12.12"  # mac book
    #SERVER = "10.37.129.2"    # mac pro
    PORT = 5000
else:
    SERVER = "www.apptranslator.org"
    PORT = 80

def lastDownloadFilePath():
    return os.path.join(g_strings_dir, "translations.txt")

def validSha1(s): return len(s) == 40

def lastDownloadHash():
    f = lastDownloadFilePath()
    if not os.path.exists(f): return "0" * 40
    lines = open(f, "rb").read().split("\n")
    sha1 =  lines[1]
    assert(validSha1(sha1))
    #print("lastDownloadHash(): %s" % sha1)
    return sha1

def saveLastDownload(s):
    open(lastDownloadFilePath(), "wb").write(s)

def downloadTranslations():
    print("Downloading translations from the server...")
    vals = (SERVER, str(PORT), "SumatraPDF",  lastDownloadHash())
    url = "http://%s:%s/dltrans?app=%s&sha1=%s" % vals
    s = urllib2.urlopen(url).read()
    print("Download done")
    return s

# Returns 'strings' dict that maps an original, untranslated string to
# an array of translation, where each translation is a tuple
# (language, text translated into this language)
def parseTranslations(s):
    lines = [l for l in s.split("\n")[2:]]
    # strip empty lines from the end
    if len(lines[-1]) == 0:
        lines = lines[:-1]
    strings = {}
    curr_str = None
    curr_translations = None
    for l in lines:
        if l[0] == ':':
            if curr_str != None:
                assert curr_translations != None
                strings[curr_str] = curr_translations
            curr_str = l[1:]
            curr_translations = []
        else:
            (lang, trans) = l.split(":", 1)
            curr_translations.append([lang, trans])
    if curr_str != None:
        assert curr_translations != None
        strings[curr_str] = curr_translations
    return strings

g_src_dir = os.path.join(os.path.dirname(__file__), "..", "src")

def get_lang_list(strings_dict):
    langs = []
    for translations in strings_dict.values():
        for t in translations:
            lang = t[0]
            if lang not in langs:
                langs.append(lang)
    return langs

def get_missing_for_language(strings, strings_dict, lang):
    untranslated = []
    for s in strings:
        if not s in strings_dict:
            untranslated.append(s)
            continue
        translations = strings_dict[s]
        found = filter(lambda tr: tr[0] == lang, translations)
        if not found and s not in untranslated:
            untranslated.append(s)
    return untranslated

def langs_sort_func(x, y):
    return cmp(len(y[1]), len(x[1])) or cmp(x[0], y[0])

# strings_dict maps a string to a list of [lang, translations...] list
def dump_missing_per_language(strings, strings_dict, dump_strings=False):
    untranslated_dict = {}
    for lang in get_lang_list(strings_dict):
        untranslated_dict[lang] = get_missing_for_language(strings, strings_dict, lang)
    items = untranslated_dict.items()
    items.sort(langs_sort_func)

    print("\nMissing translations:")
    strs = []
    for (lang, untranslated) in items:
        strs.append("%5s: %3d" % (lang, len(untranslated)))
    per_line = 5
    while len(strs) > 0:
        line_strs = strs[:per_line]
        strs = strs[per_line:]
        print("  ".join(line_strs))
    return untranslated_dict

def get_untranslated_as_list(untranslated_dict):
    return util.uniquify(sum(untranslated_dict.values(), []))

# Generate the various Translations_txt.cpp files based on translations
# in s that we downloaded from the server
def generate_code(s):
    strings_dict = parseTranslations(s)
    strings = extract_strings_from_c_files(True)
    strings_list = [tmp[0] for tmp in strings]
    for s in strings_dict.keys():
        if s not in strings_list:
            del strings_dict[s]
    untranslated_dict = dump_missing_per_language(strings_list, strings_dict)
    untranslated = get_untranslated_as_list(untranslated_dict)
    for s in untranslated:
        if s not in strings_dict:
            strings_dict[s] = []
    gen_c_code(strings_dict, strings)

# returns True if translation files have been re-generated and
# need to be commited
def downloadAndUpdateTranslationsIfChanged():
    try:
        s = downloadTranslations()
    except:
        # might fail due to intermitten network problems, ignore that
        print("skipping because downloadTranslations() failed")
        return
    lines =  s.split("\n")
    if len(lines) < 2:
        print("Bad response, less than 2 lines: '%s'" % s)
        return False
    if lines[0] != "AppTranslator: SumatraPDF":
        print("Bad response, invalid first line: '%s'" % lines[0])
        print(s)
        return False
    sha1 = lines[1]
    if sha1.startswith("No change"):
        print("skipping because translations haven't changed")
        return False
    if not validSha1(sha1):
        print("Bad reponse, invalid sha1 on second line: '%s'", sha1)
        return False
    print("Translation data size: %d" % len(s))
    #print(s)
    generate_code(s)
    saveLastDownload(s)
    return True

def regenerateLangs():
    s = open(lastDownloadFilePath(), "rb").read()
    generate_code(s)
    sys.exit(1)

def main():
    #regenerateLangs()
    changed = downloadAndUpdateTranslationsIfChanged()
    if changed:
        print("\nNew translations downloaded from the server! Check them in!")

if __name__ == "__main__":
    main()
