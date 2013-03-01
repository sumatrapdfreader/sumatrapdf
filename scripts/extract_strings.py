import os, re
from util import uniquify

"""
Extracts translatable strings from *.c and *.h files, dumps statistics
about untranslated strings to stdout and adds untranslated strings as
comments at the end of strings file for each language.
"""

# whitelist some files as an optimization
C_FILES_TO_EXCLUDE = ["Translations_txt.cpp"]
def should_translate(file_name):
    file_name = file_name.lower()
    if not file_name.endswith(".cpp"):
        return False
    return file_name not in C_FILES_TO_EXCLUDE

SRC_DIR = os.path.join(os.path.dirname(__file__), "..", "src")
C_FILES_TO_PROCESS = []

def add_files_from_dir(d):
    global C_FILES_TO_PROCESS
    for f in os.listdir(d):
        if should_translate(f):
            C_FILES_TO_PROCESS.append(os.path.join(d, f))

add_files_from_dir(SRC_DIR)
add_files_from_dir(os.path.join(SRC_DIR, "installer"))
C_FILES_TO_PROCESS.append(os.path.join(SRC_DIR, "browserplugin", "npPdfViewer.cpp"))

STRINGS_PATH = os.path.join(os.path.dirname(__file__), "..", "strings")
TRANSLATION_PATTERN = r'\b_TRN?\("(.*?)"\)'

def is_comment_line(l): return l.startswith("#")
def is_lang_line(l): return l.startswith("Lang:")
def is_contributor_line(l): return l.startswith("Contributor:")

def parse_lang_line(l):
    assert is_lang_line(l)
    match = re.match(r"Lang: (\w{2}(?:-\w{2})?) (.*)", l)
    assert match
    lang_iso, lang_name = match.group(1), match.group(2)
    # lang format is either "fr" or "en-us"
    return (lang_iso, lang_name)

def parse_contrib_line(l):
    assert is_contributor_line(l)
    return l[12:].strip()

# Extract language code (e.g. "br") from language translation file name
# (e.g. "br.txt" or "sr-sr.txt"). Returns None if file name doesn't fit expected pattern
def lang_code_from_file_name(file_name):
    match = re.match(r"(\w{2}(?:-\w{2})?)\.txt$", file_name)
    return match and match.group(1)

def extract_strings_from_c_files():
    strings = []
    for f in C_FILES_TO_PROCESS:
        file_content = open(f, "r").read()
        strings += re.findall(TRANSLATION_PATTERN, file_content)
    return uniquify(strings)

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

if __name__ == "__main__":
    print("Run update_translations.py instead")
