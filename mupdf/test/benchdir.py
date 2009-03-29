#!/usr/bin/env python
import sys, os, os.path, time

# Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
# This code is in public domain.
#
# Usage: benchdir.py <pdfbench-path> <dir-with-pdf-files>
# It runs <pdfbench-path> pdfbench executable on each pdf file in 
# <dir-with-pdf-files> directory.
# It appends the result to automatically created unique log file
# named fitz-log-NNN.txt, where NNN is a number that guarantees that 
# this file doesn't already exist (to prevent accidental overwriting of
# test results from previous runs).

def file_exists(path):
    if os.path.exists(path):
        return os.path.isfile(path)
    return False

def dir_exists(path):
    if os.path.exists(path):
        return os.path.isdir(path)
    return False

# make a directory if doesn't exist yet. 
def make_dir(path):
    if not dir_exists(path): os.makedirs(path)

_log_name_cached = None
def gen_unique_log_filename():
    global _log_name_cached
    if _log_name_cached != None:
        return _log_name_cached
    n = 0
    while n <= 999:
        _log_name_cached = "fitz-log-%03d.txt" % n
        if not file_exists(_log_name_cached):
            return _log_name_cached
        n = n+1
    print "Couldn't generate unique log name"
    sys.exit(1)

def usage():
    print "%s: pdfbench-path [dir-with-pdf-files | file-with-names-of-pdf-files]" % sys.argv[0]
    sys.exit(1)

def file_must_exist(filepath):
    if not file_exists(filepath):
        print "file '%s' doesn't exist" % filepath
        usage()

def dir_must_exist(dirpath):
    if not dir_exists(dirpath):
        print "dir '%s' doesn't exist" % dirpath
        usage()

def quoted(txt):
    assert '"' not in txt
    return '"%s"' % txt

def do_one_pdf(pdfbench, pdffile, logfile):
    cmd = pdfbench + " " + quoted(pdffile) + ">>" + logfile + " 2>&1"
    print "Running '%s'" % cmd
    os.system(cmd)

def benchdir(pdfbench, pdf_files_dir, logfile):
    file_no = 0
    start_time = time.time()
    for pdffile in os.listdir(pdf_files_dir):
        pdffilepath = os.path.join(pdf_files_dir, pdffile)
        pdffilepath = os.path.realpath(pdffilepath)
        do_one_pdf(pdfbench, pdffilepath, logfile)
        file_no += 1
        if 0 == file_no % 50:
            end_time = time.time()
            (minutes, seconds) = divmod(end_time - start_time, 60)
            print "Processed %d files, curr time: %dm%ds" % (file_no, minutes, seconds)

def benchfromfile(pdfbench, file_with_pdf_names, logfile):
    for pdffile in open(file_with_pdf_names):
        pdffile = pdffile.strip()
        pdffilepath = os.path.realpath(pdffile)
        do_one_pdf(pdfbench, pdffilepath, logfile)

def main():
    if 3 != len(sys.argv):
        usage()
    pdfbench = sys.argv[1]
    file_must_exist(pdfbench)
    print "Using '%s' binary for pdfbench" % pdfbench

    pdf_files_dir = None
    file_with_pdf_names = None
    if file_exists(sys.argv[2]):
        file_with_pdf_names = sys.argv[2]
    else:
        pdf_files_dir = sys.argv[2]
        dir_must_exist(pdf_files_dir)
        print "Benchmarking pdf files in '%s' directory" % pdf_files_dir

    logfile = gen_unique_log_filename()
    print "Log written to '%s' file" % logfile
    # TODO: support recursive directory traversal?
    start_time = time.time()
    if pdf_files_dir is not None:
        benchdir(pdfbench, pdf_files_dir, logfile)
    else:
        assert(file_with_pdf_names is not None)
        benchfromfile(pdfbench, file_with_pdf_names, logfile)
    end_time = time.time()
    duration = end_time - start_time
    (minutes, seconds) = divmod(duration, 60)
    print "Total time: %.2f seconds (%d minutes and %d seconds)" % (duration, minutes, seconds)

if __name__ == "__main__":
    main()
