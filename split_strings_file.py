#!/usr/bin/env python
from extract_strings import load_strings_file_old, write_out_strings_files
"""
This is a one-time script used to split strings.txt file with translation
for all languages into one file per translation.
"""

def main():
    (strings_dict, langs) = load_strings_file_old()
    write_out_strings_files(strings_dict, langs)

if __name__ == "__main__":
    main()
