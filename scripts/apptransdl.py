#!/usr/bin/env python
from extract_strings import extract_strings_from_c_files
import os.path, sys, string, urllib2
from util import load_config

# Extracts english strings from the source code and uploads them
# to apptranslator.org
g_my_dir = os.path.dirname(__file__)

use_local_for_testing = True

if use_local_for_testing:
    SERVER = "172.21.12.12"
    PORT = 5000
else:
    SERVER = "www.apptranslator.org"
    PORT = 80

def lastDownloadFilePath():
    return os.path.join(g_my_dir, "apptransdl-last.txt")

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
    saveLastDownload(s)
    # TODO: re-generate source code
    return False

if __name__ == "__main__":
    downloadAndUpdateTranslationsIfChanged()
