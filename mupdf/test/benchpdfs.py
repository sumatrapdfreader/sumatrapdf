#!/usr/bin/python
import sys, os, os.path, urllib2, gzip, bz2, traceback

# Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
# This code is in public domain.
#
# A regression testing script
#
# Given a list of urls to PDF files, it downloads them and runs
# pdftool draw -m $file-name
# on each file. This allows catching crashes e.g. on linux:
# python test/benchpdfs.py | grep Segmentation
# will produce an output if pdftool crashed on any of the pdfs
#
# Regression PDFs can be put anywhere. They can be gzipp'ed or bzip2'ed
# to save the bandwidth (in which case url must end in .gz or .bz2)
# 
# The script doesn't redownload the file if it has been downloaded before.
#
# Missing files are ignored
#
# By convetion names of PDF files are sha1 hash over the uncompressed content.
# They have a nice property of being unique for each file.
# To generate the name run sha1sum on an (uncompressed) pdf, rename
# the file to a result of that + .pdf and optionally compress with gzip or bzip2

# TODO:
# - run multiple pdfbench executables at the same time to see if it's faster
#   (by virtue of using multiple cpus if they're available)

pdfs_to_test = [
    "http://darcs.kowalczyk.info/testpdfs/293bcd6b00e006d66fdc62ea436508f3ebb30219.pdf.gz"
]

local_pdfs_dir = os.path.expanduser("~/testpdfs")

def dir_exists(path):
    if os.path.exists(path):
        return os.path.isdir(path)
    return False

def file_exists(path):
    if os.path.exists(path):
        return os.path.isfile(path)
    return False

# make a directory if doesn't exist yet. 
def make_dir(path):
    if not dir_exists(path): os.makedirs(path)

def write_to_file(path, data):
    fo = open(path, "wb")
    fo.write(data)
    fo.close()

# Does HTTP GET or POST (if data != None). Returns body of the response or 
# None if there was an error
# If username/pwd are provided, we assume it's for basic authentication
def do_http(url, data = None, dump_exception=False):
    body = None
    try:
        req = urllib2.Request(url, data)
        resp = urllib2.urlopen(req)
        body = resp.read()
    except:
        if dump_exception:
            print "do_http failed",url
            print '-'*60
            traceback.print_exc(file=sys.stdout)
            print '-'*60
    return body

# Tries to find root of the repository. Starts and pwd and goes up
# until can't go anymore or finds "mupdf" directory
def find_repo_root():
    curdir = os.getcwd()
    prevdir = None
    while curdir != prevdir:
        if dir_exists(os.path.join(curdir, "mupdf")):
            return curdir
        prevdir = curdir
        curdir = os.path.dirname(curdir)
    return None

def find_pdftool():
    root = find_repo_root()
    if root == None:
        print "Didn't find the root directory"
        print "Current directory: '%s'" % os.getcwd()
        sys.exit(1)
    print root
    # check build results for Jam and Makefile
    for f in [os.path.join("obj-rel", "pdftool"), os.path.join("obj-dbg", "pdftool"), 
              os.path.join("build", "relase", "pdftool"), os.path.join("build", "debug", "pdftool")]:
        path = os.path.join(root, f)
        if file_exists(path):
            return path
    print "Didn't find pdftool. Did you build it?"
    print "Root dir: '%s'" % root
    sys.exit(1)

def is_gzipped(filename): return filename.endswith(".gz")
def is_bzip2ed(filename): return filename.endswith(".bz2")

def uncompress_if_needed(filepath):
    if is_gzipped(filepath):
        finalpath = filepath[:-len(".gz")]
        print "Uncompressing '%s' to '%s'" % (filepath, finalpath)
        fin = gzip.open(filepath, "rb")
        fout = open(finalpath, "wb")
        data = fin.read()
        fout.write(data)
        fin.close()
        fout.close()
        os.remove(filepath)
    elif is_bzip2ed(filepath):
        finalpath = filepath[:-len(".bz2")]
        print "Uncompressing '%s' to '%s'" % (filepath, finalpath)
        fin = bz2.BZ2File(filepath, "r")
        fout = open(finalpath, "wb")
        data = fin.read()
        fout.write(data)
        fin.close()
        fout.close()
        os.remove(filepath)

def pdfname_from_url(url): return url.split("/")[-1]

def final_pdfname_from_url(url):
    potentially_compressed = pdfname_from_url(url)
    for suffix in [".gz", ".bz2"]:
        if potentially_compressed.endswith(suffix):
            return potentially_compressed[:-len(suffix)]
    return potentially_compressed

def main():
    print "Starting the test"
    pdftool = find_pdftool() # make sure to abort early if pdftool doesn't exist
    #print "pdftool: '%s'" % pdftool
    make_dir(local_pdfs_dir)
    for pdfurl in pdfs_to_test:
        pdfname = pdfname_from_url(pdfurl)
        local_pdf_path = os.path.join(local_pdfs_dir, pdfname)
        final_pdfname = final_pdfname_from_url(pdfurl)
        local_final_pdf_path = os.path.join(local_pdfs_dir, final_pdfname)
        # Download the file if not already downloaded
        if not os.path.exists(local_final_pdf_path):
            print "Downloading pdf file '%s' as '%s'" % (pdfurl, local_pdf_path)
            pdf_file_data = do_http(pdfurl)
            if None == pdf_file_data:
                print "Failed to download '%s'" % pdfurl
                continue # don't stop the test just because of that
            write_to_file(local_pdf_path, pdf_file_data)
            uncompress_if_needed(local_pdf_path)
        cmd = pdftool + " draw -m " + local_final_pdf_path
        print "Running '%s'" % cmd
        os.system(cmd)

if __name__ == "__main__":
    main()
