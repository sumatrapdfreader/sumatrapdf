#!/usr/bin/env python
import os
import sys
import string
import httplib
import urllib
import json
import util
from trans_gen import extract_strings_from_c_files

# Extracts english strings from the source code and uploads them
# to apptranslator.org
g_strings_dir = os.path.join(os.path.dirname(__file__), "..", "strings")

use_local_for_testing = False

if use_local_for_testing:
    SERVER = "172.21.12.12"
    PORT = 5000
else:
    SERVER = "www.apptranslator.org"
    PORT = 80


def lastUploadedFilePath():
    return os.path.join(g_strings_dir, "last_uploaded.txt")


def lastUploaded():
    f = lastUploadedFilePath()
    if not os.path.exists(f):
        return ""
    return open(f, "rb").read()


def saveLastUploaded(s):
    open(lastUploadedFilePath(), "wb").write(s)


def uploadStringsToServer(strings, secret):
    print("Uploading strings to the server...")
    params = urllib.urlencode(
        {'strings': strings, 'app': 'SumatraPDF', 'secret': secret})
    headers = {"Content-type": "application/x-www-form-urlencoded",
               "Accept": "text/plain"}
    conn = httplib.HTTPConnection(SERVER, PORT)
    conn.request("POST", "/uploadstrings", params, headers)
    resp = conn.getresponse()
    print resp.status
    print resp.reason
    print resp.read()
    conn.close()
    print("Upload done")


def uploadStringsIfChanged():
    # needs to have upload secret to protect apptranslator.org server from abuse
    path = os.path.join("scripts", "secrets.json")
    with open(path) as file:
        d = file.read()
    config = json.loads(d)
    print(config)
    uploadsecret = config["TranslationUploadSecret"]
    if None is uploadsecret:
        print("Skipping string upload because don't have upload secret")
        return

    # TODO: we used to have a check if svn is up-to-date
    # should we restore it for git?

    strings = extract_strings_from_c_files()
    strings.sort()
    s = "AppTranslator strings\n" + string.join(strings, "\n")
    s = s.encode("utf8")

    if lastUploaded() == s:
        print(
            "Skipping upload because strings haven't changed since last upload")
    else:
        uploadStringsToServer(s, uploadsecret)
        saveLastUploaded(s)
        print("Don't forget to checkin strings/last_uploaded.txt")

if __name__ == "__main__":
    uploadStringsIfChanged()
