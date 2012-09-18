#!/usr/bin/env python
from extract_strings import extract_strings_from_c_files
import os.path, sys, string, hashlib, httplib, urllib

# Extracts english strings from the source code and uploads them
# to apptranslator.org
g_my_dir = os.path.dirname(__file__)

use_local_for_testing = True

if use_local_for_testing:
    SERVER = "127.0.0.1"
    PORT = 5000
else:
    SERVER = "www.apptranslator.org"
    PORT = 80

# secrets.py needs to have upload secret in the form of:
# uploadsecret = "$secret"
# to protect AppTranslator.org server from potential abuse
try:
    import secrets
    # force crash if secrets.uploadsecret is not defined
    print("uploadsecret:" + secrets.uploadsecret)
except:
    print("secrets.py not present")
    sys.exit(1)

def lastUploadHashFileName():
    return os.path.join(g_my_dir, "apptransup-lastuploadhash.txt")

def lastUploadHash():
    f = lastUploadHashFileName()
    if not os.path.exists(f): return ""
    return open(f, "rb").read()

def saveLastUploadHash(s):
    open(lastUploadHashFileName(), "wb").write(s)

def uploadStrings(strings, secret):
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

def main():
    strings = extract_strings_from_c_files()
    strings.sort()
    s = "AppTranslator strings\n" + string.join(strings, "\n")
    shash = hashlib.sha1(s).hexdigest()
    prevHash = lastUploadHash()
    if shash == prevHash:
        print("Skipping upload because strings haven't changed since last upload")
    else:
        uploadStrings(s, secrets.uploadsecret)
        saveLastUploadHash(shash)

if __name__ == "__main__":
    main()
