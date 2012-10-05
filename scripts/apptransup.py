#!/usr/bin/env python
from extract_strings import extract_strings_from_c_files
import os.path, sys, string, hashlib, httplib, urllib
from util import load_config
import buildbot

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

def lastUploadHashFileName():
    return os.path.join(g_my_dir, "apptransup-lastuploadhash.txt")

def lastUploadHash():
    f = lastUploadHashFileName()
    if not os.path.exists(f): return ""
    return open(f, "rb").read()

def saveLastUploadHash(s):
    open(lastUploadHashFileName(), "wb").write(s)

def uploadStringsToServer(strings, secret):
    print("Uploading strings to the server...")
    params = urllib.urlencode({'strings': strings, 'app': 'SumatraPDF', 'secret': secret})
    headers = {"Content-type": "application/x-www-form-urlencoded", "Accept": "text/plain"}
    conn = httplib.HTTPConnection(SERVER, PORT)
    conn.request("POST", "/uploadstrings", params, headers)
    resp = conn.getresponse()
    print resp.status
    print resp.reason
    print resp.read()
    conn.close()
    print("Upload done")

def uploadStringsIfChanged():
    (local_ver, latest_ver) = buildbot.get_svn_versions()
    if int(latest_ver) > int(local_ver):
        print("Skipping string upload because your local version (%s) is older than latest in svn (%s)" % (local_ver, latest_ver))
        return
    # needs to have upload secret to protect apptranslator.org server from abuse
    config = load_config()
    uploadsecret = config.trans_ul_secret
    if None is uploadsecret:
        print("Skipping string upload because don't have upload secret")
        return
    strings = extract_strings_from_c_files()
    strings.sort()
    s = "AppTranslator strings\n" + string.join(strings, "\n")
    shash = hashlib.sha1(s).hexdigest()
    prevHash = lastUploadHash()
    if shash == prevHash:
        print("Skipping upload because strings haven't changed since last upload")
    else:
        uploadStringsToServer(s, uploadsecret)
        saveLastUploadHash(shash)

if __name__ == "__main__":
    uploadStringsIfChanged()
