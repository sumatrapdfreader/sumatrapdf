#!/usr/bin/env python
from extract_strings import extract_strings_from_c_files
import os.path
import sys
import string
import hashlib

# Extracts english strings from the source code and uploads them
# to AppTranslator.org
g_my_dir = os.path.dirname(__file__)
g_src_dir = os.path.join(os.path.split(__file__)[0], "..", "src")

SERVER = "localhost:8890"
URL = "/handleUploadStrings?app=SumatraPDF&secret="

# apptranscreds.py needs to have upload secret in the form of:
# uploadsecret = "$secret"
# to protect AppTranslator.org server from potential abuse
try:
    import apptranscreds
except:
    print("apptranscreds.py not present")
    sys.exit(1)

def lastUploadHashFileName():
    return os.path.join(g_my_dir, "strhashfile.txt")

def loadLastUploadHash():
    f = lastUploadHashFileName()
    if not os.path.exists(f): return ""
    return open(f, "rb").read()

def saveLastUploadHash(s):
    open(lastUploadHashFileName(), "wb").write(s)

def uploadStrings(s, secret):
    print("Uploading strings to the server:")
    fullUrl = URL + secret
    print("http://" + SERVER + fullUrl)
    print("Upload done")

def main():
    strings = extract_strings_from_c_files()
    strings.sort()
    s = "AppTranslator strings\n" + string.join(strings, "\n")
    shash = hashlib.sha1(s).hexdigest()
    prevHash = loadLastUploadHash()
    if shash == prevHash:
        print("Skipping upload because strings haven't changed since last upload")
        return
    uploadStrings(s, apptranscreds.uploadsecret)
    saveLastUploadHash(shash)

if __name__ == "__main__":
    main()
