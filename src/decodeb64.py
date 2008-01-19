import base64, sys

file_in = sys.argv[1]
file_out = sys.argv[2]
fo = open(file_in, "rb")
d = fo.read()
fo.close()

decoded = base64.b64decode(d)
fo = open(file_out, "wb")
fo.write(decoded)
fo.close()
