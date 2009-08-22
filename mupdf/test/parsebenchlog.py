#!/usr/bin/env python
import sys
import string
import os.path

# TODO: show slowest pages to load (in addition to slowest pdfs to load)

def usage():
    print "Usage: %s [-dump-failing] <log-file>" % sys.argv[0]
    sys.exit(1)

class PdfInfo(object):
    def __init__(self):
        self.path = None
        self.loadtime = None
        self.pageloadtimes = {}
        self.pagerendertimes = {}
        self.pagecount = None
        self.crashed = False
        self.errors = []
        self.txt = ""
        self.lastpage = 0

    def add_page_load_time(self, pageno, loadtime):
        self.pageloadtimes[pageno] = loadtime
        assert pageno > self.lastpage
        self.lastpage = pageno

    def add_page_render_time(self, pageno, rendertime):
        self.pagerendertimes[pageno] = rendertime

    def failed(self): return len(self.errors) > 0

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

def extract_page_no(s):
    (tmp,loadtime) = s.split(": ")
    (tmp, page) = tmp.split()
    return int(page)

def extract_time_from_line(s, expected_start):
    if not s.startswith(expected_start):
        return None
    return extract_time(s)

def extract_pageno_time_from_line(s, expected_start):
    if not s.startswith(expected_start):
        return (None, None)
    pageno = extract_page_no(s)
    t = extract_time(s)
    return (pageno, t)

def load_line(s): return extract_time_from_line(s, "load:")
def pageload_line(s): return extract_pageno_time_from_line(s, "pageload ")
def pagerender_line(s): return extract_pageno_time_from_line(s, "pagerender ")

def is_pageload_line(s): return s.startswith("pageload")
def is_pagerender_line(s): return s.startswith("pagerender")
def is_starting_line(s): return s.startswith("Starting:")
def is_finished_line(s): return s.startswith("Finished:")
def is_error_line(s): return s.startswith("Error:")
def is_load_line(s): return s.startswith("load:")

def find(l,el):
    try:
        return l.index(el)
    except ValueError:
        return -1

def show_slowest_to_load(pdfs, count=20):
    slowest_to_load = [0.0] * count
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
        print("Slow to load, time: %.2f, name: %s" % (p.loadtime, p.path))

class RenderTimeInfo(object):
    def __init__(self, render_time, pageno, pdf):
        self.render_time = render_time
        self.pageno = pageno
        self.pdf = pdf

def show_slowest_to_render(pdfs, count=20):
    slowest_to_render = [RenderTimeInfo(0.0, -1, None)] * count
    fastest_time = slowest_to_render[0].render_time
    for p in pdfs:
        for (pageno, render_time) in p.pagerendertimes.items():
            if render_time > fastest_time:
                slowest_to_render.append(RenderTimeInfo(render_time, pageno, p))
                slowest_to_render.sort(lambda x, y: cmp(x.render_time, y.render_time))
                del slowest_to_render[0]
                fastest_time = slowest_to_render[0].render_time
                #print "New fastest_time: %.2f" % fastest_time
    for rti in reversed(slowest_to_render):
        print("Slow to render, time: %.2f, page: %d, name: %s" % (rti.render_time, rti.pageno, rti.pdf.path))

def parse_pdfbench_output():
    log_file = sys.argv[1]
    curr_pdf = None
    pdfs = []
    curr_lines = []
    line_no = -1
    for line in open(log_file):
        line_no += 1
        curr_lines.append(line)

        # as an optimization, check pageload and pagerender line since
        # they're most common
        if is_pageload_line(line):
            (pageno, pageload_time) = pageload_line(line)
            curr_pdf.add_page_load_time(pageno, pageload_time)
        elif is_pagerender_line(line):
            (pageno, pagerender_time) = pagerender_line(line)
            curr_pdf.add_page_render_time(pageno, pagerender_time)
        elif is_starting_line(line):
            start_name = starting_line(line)
            if curr_pdf != None: # means haven't seen finished line
                #print "Found crashed"
                curr_pdf.crashed = True
                pdfs.append(curr_pdf)
            curr_pdf = PdfInfo()
            curr_pdf.path = start_name
        elif is_finished_line(line):
            finish_name = finished_line(line)
            if curr_pdf.failed():
                curr_pdf.txt = string.join(curr_lines,"")
            pdfs.append(curr_pdf)
            if curr_pdf.path != finish_name:
                sys.stderr.write("start : %s\n" % curr_pdf.path)
                sys.stderr.write("finish: %s\n" % finish_name)
                sys.stderr.write("line  : %d\n\n" % line_no)
                assert(curr_pdf.path == finish_name)
            curr_pdf = None
            curr_lines = []
        elif is_error_line(line):
            if curr_pdf: curr_pdf.errors.append(line.strip())
        elif is_load_line(line):
            load_time = load_line(line)
            curr_pdf.loadtime = load_time
    return pdfs

def show_failing(pdfs, failed, crashed):
    failures = 0
    t = []
    t.append("total pdfs: %d" % len(pdfs))
    t.append("failed: %d" % len(failed))
    t.append("crashed: %d" % len(crashed))
    t.append("") # placeholder for failures
    t.append("Crashed:")
    for p in crashed:
        t.append(p.path + " on page " + str(p.lastpage + 1))
    t.append("Failed:")
    for p in failed:
        t.append(p.path)
        for e in p.errors:
            t.append(" " + e)
        failures += len(p.errors)
    t[3] = "failures: %d" % failures
    print("\n".join(t))

def cp_failing(failed, crashed):
    failpath = os.path.join("/Volumes", "Drobo1", "pdffail")
    crashdir = os.path.join(failpath, "crash")
    faildir = os.path.join(failpath, "fail")
    for p in crashed:
        pdf_src_path = p.path
        pdf_name = os.path.basename(pdf_src_path)
        pdf_dst_path = os.path.join(crashdir, pdf_name)
        print("cp %s %s" % (pdf_src_path, pdf_dst_path))
    for p in failed:
        pdf_src_path = p.path
        pdf_name = os.path.basename(pdf_src_path)
        pdf_dst_path = os.path.join(faildir, pdf_name)
        print("cp %s %s" % (pdf_src_path, pdf_dst_path))


def main():
    idx = find(sys.argv, "-dump-failing")
    dumpfailing = False
    if -1 != idx:
        dumpfailing = True
        del sys.argv[idx]

    if len(sys.argv) != 2:
        usage()

    pdfs = parse_pdfbench_output()

    failed = [p for p in pdfs if p.failed()]
    crashed = [p for p in pdfs if p.crashed]

    if dumpfailing:
        show_slowest_to_load(pdfs)
        show_slowest_to_render(pdfs)
        show_failing(pdfs, failed, crashed)
        return

    for p in failed:
        print("%s has %d errors" % (p.path, len(p.errors)))
        print(p.txt)


    print
    for p in crashed:
        print("Crashed: %s" % p.path)
    # TODO: find pages that are slowest to load
    # TODO: find pages that are slowest to render

    print("")
    print("total pdfs: %d" % len(pdfs))
    print("failed: %d" % len(failed))
    print("crashed: %d" % len(crashed))


if __name__ == "__main__":
    main()
