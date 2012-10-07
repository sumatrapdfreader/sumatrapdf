#!/usr/bin/env python
from extract_strings import extract_strings_from_c_files
import os.path, sys, string, hashlib, httplib, urllib
from util import load_config
import buildbot

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
    if not os.path.exists(f): return ""
    return open(f, "rb").read()

def saveLastUploaded(s):
    open(lastUploadedFilePath(), "wb").write(s)

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
    # Note: this check might be confusing due to how svn work
    # Unforunately, if you have local latest revision 5 and do a checkin to create
    # revision 6, svn info says that locally you're still on revision 5, even though
    # the code is actually as revision 6.
    # You need to do "svn update" to update local version number
    # Unfortunately I can't do it automatically here since it would be dangerous
    # (i.e. it would update code locally).
    # svn update is called in build-release.py, so it's not a problem if it's run
    # from  ./scripts/build-release.bat or ./scripts/build-pre-release.bat
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
    s = s.encode("utf8")

    if lastUploaded() == s:
        print("Skipping upload because strings haven't changed since last upload")
    else:
        uploadStringsToServer(s, uploadsecret)
        saveLastUploaded(s)
        print("Don't forget to checkin strings/last_uploaded.txt")

if __name__ == "__main__":
    uploadStringsIfChanged()
