import string, sys, os, os.path
from time import *
from subprocess import *

g_log_file = None

def log_file_name():
	global g_log_file
	if None == g_log_file:
		g_log_file = "log_" + strftime("%m_%d_%H%M", localtime()) + ".txt"
	return g_log_file

def is_arg(s): return '-' == s[0]

def usage_and_exit():
	print "usage: bench.py [-fitz | -both] file1 file2"
	sys.exit(0)

def is_pdf(p):
	p = p.lower()
	return p.endswith(".pdf")

def do_path(args, p):
	if os.path.isdir(p):
		do_dir(args, p)
	else:
		do_file(args, p)

def do_file(args, fname):
	if not is_pdf(fname): return
	#print fname
	nargs = [el for el in args]
	nargs.append('"' + fname + '"')
	nargs.append(">>%s" % log_file_name())
	nargs.append("2>&1")
	cmd = string.join(nargs, " ")
	print cmd
	call(cmd, shell=True)

def do_dir(args, d):
	dirs_to_visit = [d]
	while len(dirs_to_visit) > 0:
		d = dirs_to_visit[0]
		dirs_to_visit = dirs_to_visit[1:]
		els = os.listdir(d)
		for el in els:
			p = os.path.join(d, el)
			if os.path.isdir(p):
				dirs_to_visit.append(p)
			else:
				do_file(args, p)

def main():
	pdfbench = os.path.join("obj-rel", "pdfbench.exe")
	if not os.path.exists(pdfbench):
		print("%s doesn't exist" % pdfbench)
		sys.exit(1)
	args = [pdfbench]
	non_args = []
	for arg in sys.argv[1:]:
		if is_arg(arg):
			args.append(arg)
		else:
			non_args.append(arg)
	args.append("-timings")
	args.append("-preview")
	if len(non_args) == 0: usage_and_exit()
	for el in non_args: do_path(args, el)

if __name__ == "__main__":
	main()
