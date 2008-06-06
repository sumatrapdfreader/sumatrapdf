#!/usr/bin/python
import sys, os, os.path, shutil

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
    print "%s: pdftool-path pdf-file-list" % sys.argv[0]
    sys.exit(1)

def file_must_exist(filepath):
    if not file_exists(filepath):
        print "file '%s' doesn't exist" % filepath
        usage()

def quoted(txt):
    assert '"' not in txt
    return '"%s"' % txt

def do_one_pdf(pdftool, pdffile, logfile):
    cmd = pdftool + " draw -m " + quoted(pdffile) + ">>" + logfile + " 2>&1"
    print "Running '%s'" % cmd
    os.system(cmd)

def main():
    if 3 != len(sys.argv):
        usage()
    pdftool = sys.argv[1]
    file_must_exist(pdftool)
    print "Using '%s' binary for pdftool" % pdftool
    
    pdf_file_list = sys.argv[2]
    file_must_exist(pdf_file_list)
    print "Using '%s' file for list of pdfs to process" % pdf_file_list

    logfile = gen_unique_log_filename()
    print "Logs writtent to '%s' file" % logfile

    #localpdfpath = os.path.expanduser("~/fitzstresspdfs")
    localpdfpath = "/mnt/hgfs/macpro/fitz-stress-pdfs"
    make_dir(localpdfpath)
    print "PDFs copied to directory '%s'" % localpdfpath

    for pdffilesrc in open(pdf_file_list):
        pdffilesrc = pdffilesrc.strip()
        if not file_exists(pdffilesrc):
            print "File '%s' doesn't exist" % pdffilesrc
            continue
        basename = os.path.basename(pdffilesrc)
        pdffiledst = os.path.join(localpdfpath, basename)
        if pdffiledst != pdffilesrc:
            if not file_exists(pdffiledst):
                print "Copying '%s' to '%s'" % (pdffilesrc, pdffiledst)
                shutil.copyfile(pdffilesrc, pdffiledst)
        do_one_pdf(pdftool, pdffiledst, logfile)

if __name__ == "__main__":
    main()

