#!/usr/bin/env python

import sys, string

def usage():
    print "Usage: %s [-dump-failing] <log-file>" % sys.argv[0]
    sys.exit(1)

class PdfInfo(object):
    def __init__(self):
        self.path = None
        self.loadtime = None
        self.pageloadtimes = []
        self.pagerendertimes = []
        self.pagecount = None
        self.crashed = False
        self.errors = 0
        self.txt = ""

    def failed(self): return self.errors > 0

def finished_line(s):
    if not s.startswith("Finished:"):
        return None
    (d,name) = s.split(": ")
    return name.strip()

def starting_line(s):
    if not s.startswith("Starting:"):
        return None
    (d,name) = s.split(": ")
    return name.strip()

def extract_time(s):
    try:
        (d,loadtime) = s.split(": ")
    except:
        print s,
        return None

    try:
        (timeinms, ms) = loadtime.split(" ")
    except:
        print s,
        return None
    assert(ms.strip() == "ms")
    return float(timeinms)

def extract_time_from_line(s, expected_start):
    if not s.startswith(expected_start):
        return None
    return extract_time(s)

def load_line(s): return extract_time_from_line(s, "load:")
def pageload_line(s): return extract_time_from_line(s, "pageload ")
def pagerender_line(s): return extract_time_from_line(s, "pagerender ")
def error_line(s): return s.startswith("Error:")

(ST_NONE, ST_READING) = range(2)

def find(l,el):
    try:
        return l.index(el)
    except ValueError:
        return -1

def main():
    idx = find(sys.argv, "-dump-failing")
    dumpfailing = False
    if -1 != idx:
        dumpfailing = True
        del sys.argv[idx]

    if len(sys.argv) != 2:
        usage()
    log_file = sys.argv[1]
    curr_pdf = None
    pdfs = []
    curr_lines = []
    state = ST_NONE
    for line in open(log_file):
        curr_lines.append(line)
        start_name = starting_line(line)
        finish_name = finished_line(line)
        error = error_line(line)
        load_time = load_line(line)
        pageload_time = pageload_line(line)
        pagerender_time = pagerender_line(line)
        if start_name:
            if curr_pdf != None: # means haven't seen finished line
                #print "Found crashed"
                curr_pdf.crashed = True
                pdfs.append(curr_pdf)
            curr_pdf = PdfInfo()
            curr_pdf.path = start_name
        elif finish_name:
            if curr_pdf.errors > 0:
                curr_pdf.txt = string.join(curr_lines,"")
            pdfs.append(curr_pdf)
            assert(curr_pdf.path == finish_name)
            curr_pdf = None
            curr_lines = []
        elif load_time != None:
            curr_pdf.loadtime = load_time
        elif pageload_time != None:
            curr_pdf.pageloadtimes.append(pageload_time)
        elif pagerender_time != None:
            curr_pdf.pagerendertimes.append(pagerender_time)
        elif error:
            if curr_pdf:
                curr_pdf.errors += 1

    crashed = [p for p in pdfs if p.crashed]
    failed = [p for p in pdfs if p.failed()]
    if dumpfailing:
        for p in crashed: print p.path
        for p in failed: print p.path
        return

    for p in failed:
        print "%s has %d errors" % (p.path, p.errors)
        print p.txt

    slowest_to_load = [0.0] * 20
    fastest_time = slowest_to_load[0]
    for p in pdfs:
        if p.loadtime == None: continue
        if p.loadtime > fastest_time:
            slowest_to_load.append(p.loadtime)
            slowest_to_load.sort()
            del slowest_to_load[0]
            fastest_time = slowest_to_load[0]
            #print "New fastest_time: %.2f" % fastest_time
    slow_to_load = [p for p in pdfs if p.loadtime in slowest_to_load]
    slow_to_load.sort(lambda x,y: cmp(y.loadtime, x.loadtime))
    for p in slow_to_load:
        print "Slow to load, time: %.2f, name: %s" % (p.loadtime, p.path)

    print
    for p in crashed:
        print "Crashed: %s" % p.path
    # TODO: find pages that are slowest to load
    # TODO: find pages that are slowest to render

    print
    print "total pdfs: %d" % len(pdfs)
    print "failed: %d" % len(failed)
    print "crashed: %d" % len(crashed)


if __name__ == "__main__":
    main()
