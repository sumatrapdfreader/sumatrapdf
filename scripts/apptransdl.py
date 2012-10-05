#!/usr/bin/env python

# Downloads latest translations from apptranslator.org.
# If changed, saves them as strings/translations.txt and
# re-generates src/Translations_txt.cpp

from extract_strings import extract_strings_from_c_files
import os.path, sys, string, urllib2
from util import load_config

g_my_dir = os.path.dirname(__file__)
g_strings_dir = os.path.join(g_my_dir, "..", "strings")

use_local_for_testing = True

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

from langs import g_langs

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

from extract_strings import extract_strings_from_c_files, dump_missing_per_language, load_lang_index
from update_translations import get_untranslated_as_list, remove_incomplete_translations, gen_c_code
from update_translations import lang_sort_func

g_src_dir = os.path.join(os.path.split(__file__)[0], "..", "src")

# Generate Translations_txt.cpp based on translations in s that we downloaded
# from the server
def generateCode(s):
    strings_dict = parseTranslations(s)
    strings = extract_strings_from_c_files()
    langs_idx = load_lang_index()
    langs = g_langs
    for s in strings_dict.keys():
        if s not in strings:
            del strings_dict[s]
    untranslated_dict = dump_missing_per_language(strings, strings_dict)
    untranslated = get_untranslated_as_list(untranslated_dict)
    for s in untranslated:
        if s not in strings_dict:
            strings_dict[s] = []

    langs.sort(lang_sort_func)
    c_file_name = os.path.join(g_src_dir, "Translations_txt.cpp")
    remove_incomplete_translations(langs, strings, strings_dict)
    gen_c_code(langs, strings_dict, c_file_name, langs_idx)

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
    generateCode(s)
    saveLastDownload(s)
    return True

if __name__ == "__main__":
    downloadAndUpdateTranslationsIfChanged()
