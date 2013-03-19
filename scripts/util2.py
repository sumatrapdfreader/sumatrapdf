# only add code here that doesn't initialize slowly:
# importing util.py currently adds about 0.4s on my system
# (mainly due to smtplib, subprocess, zipfile), 
# importing util2.py currently only adds about 0.05s

import os

def chdir_top():
	os.chdir(os.path.join(os.path.dirname(__file__), ".."))

def group(list, size):
	i = 0
	while list[i:]:
		yield list[i:i + size]
		i += size

def uniquify(array):
	return list(set(array))

if __name__ == "__main__":
	pass
