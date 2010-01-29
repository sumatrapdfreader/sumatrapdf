#!/usr/bin/env python
from extract_strings import load_strings_file_old
import codecs

"""
TODO:
 * put untranslated strings as comments at the end of the file

This is a one-time script used to split strings.txt file with translation
for all languages into one file per translation.
"""

STRINGS_FILE = "strings.txt"

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

def gen_strings_for_lang(lang_id, lang_name, contributors, translations):
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
    fo.close()
    print("%d translations in %s" % (len(trans), file_name))

def main():
    (strings_dict, langs) = load_strings_file_old(STRINGS_FILE)
    translations_for_langs = gen_translations_for_languages(strings_dict)
    for (lang_id, lang_name) in langs:
        if 'en' == lang_id: continue
        gen_strings_for_lang(lang_id, lang_name, [], translations_for_langs)

if __name__ == "__main__":
    main()
