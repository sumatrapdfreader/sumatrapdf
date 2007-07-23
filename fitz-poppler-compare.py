import math, sys, os, os.path, string

#files = ["timings-100scifi.txt", "timings-100scifi-2.txt", "timings-100scifi-3.txt", "timings-100scifi-4.txt", "timings-100scifi-5.txt"]
files = ["t-scifi-1.txt", "t-scifi-2.txt", "t-scifi-3.txt", "t-scifi-4.txt", "t-scifi-5.txt"]

def error_and_exit(str):
  print str
  sys.exit(1)

class FailedFitz: pass
class FailedPoppler: pass

class Stats:
  def __init__(self, file_name):
    self.file_name = file_name
    self.page_count = None
    self.fitz_load_time = None
    self.poppler_load_time = None
    self.fitz_times = []
    self.poppler_times = []
  def fitz_total_render_time(self):
    t = 0.0
    for tmp in self.fitz_times:
      t += tmp
    return t
  def poppler_total_render_time(self):
    t = 0.0
    for tmp in self.poppler_times:
      t += tmp
    return t

def ensure_starts_with(line, line_no, txt):
  if not line.startswith(txt):
    error_and_exit("Expected line %d '%s' to start with '%s'" % (line_no, line, txt))

def extract_file_name(line):
  TXT = "started both: "
  ensure_starts_with(line, 1, TXT)
  return line[len(TXT):]

def extract_load_time(line):
  (page, time_txt) = line.split(": ")
  (time_ms, rest) = time_txt.split(" ")
  ensure_starts_with(rest, 1, "ms")
  return float(time_ms)

def extract_page_count(line):
  TXT = "page count: "
  ensure_starts_with(line, 1, TXT)
  return int(line[len(TXT):])

def extract_render_time(line):
  (txt, time_txt) = line.split(": ")
  (time_ms, rest) = time_txt.split(" ")
  ensure_starts_with(rest, 1, "ms")
  return float(time_ms)

(ST_NONE, ST_EXPECT_FINISHED, ST_EXPECT_SPLASH_LOAD, ST_STARTED, ST_EXPECT_PAGE_COUNT, ST_EXPECT_PAGE_FITZ_OR_FINISHED, ST_EXPECT_PAGE_SPLASH) = range(7)

def state_name(state):
  if ST_NONE == state: return "ST_NONE"
  if ST_EXPECT_FINISHED == state: return "ST_EXPECT_FINISHED"
  if ST_EXPECT_SPLASH_LOAD == state: return "ST_EXPECT_SPLASH_LOAD"
  if ST_STARTED == state: return "ST_STARTED"
  if ST_EXPECT_PAGE_COUNT == state: return "ST_EXPECT_PAGE_COUNT"
  if ST_EXPECT_PAGE_FITZ_OR_FINISHED == state: return "ST_EXPECT_PAGE_FITZ_OR_FINISHED"
  if ST_EXPECT_PAGE_SPLASH == state: return "ST_EXPECT_PAGE_SPLASH"
  return "UNKNOWN STATE"

def parse_file(file_name):
  file_name_stats_map = {}
  fo = open(file_name, "rb")
  state = ST_NONE
  line_no = 1
  stats = None
  for line in fo:
    line = line.strip()
    if line.startswith("Error:"):
      line_no += 1
      continue
    #print "state = %s" % state_name(state)
    #print line
    if ST_NONE == state:
      ensure_starts_with(line, line_no, "started both")
      file_name = extract_file_name(line)
      state = ST_STARTED
    elif ST_EXPECT_FINISHED == state:
      ensure_starts_with(line, line_no, "finished both")
      state = ST_NONE
    elif ST_STARTED == state:
      if line.startswith("failed to load fitz"):
        file_name_stats_map[file_name] = FailedFitz()
        state = ST_EXPECT_FINISHED
      elif line.startswith("failed to load poppler"):
        file_name_stats_map[file_name] = FailedPoppler()
        state = ST_EXPECT_FINISHED
      else:
        ensure_starts_with(line, line_no, "load fitz")
        stats = Stats(file_name)
        stats.fitz_load_time = extract_load_time(line)
        file_name_stats_map[file_name] = stats
        state = ST_EXPECT_SPLASH_LOAD
    elif ST_EXPECT_SPLASH_LOAD == state:
      ensure_starts_with(line, line_no, "load splash")
      stats.poppler_load_time = extract_load_time(line)
      state = ST_EXPECT_PAGE_COUNT
    elif ST_EXPECT_PAGE_COUNT == state:
      ensure_starts_with(line, line_no, "page count:")
      stats.page_count = extract_page_count(line)
      state = ST_EXPECT_PAGE_FITZ_OR_FINISHED
    elif ST_EXPECT_PAGE_FITZ_OR_FINISHED == state:
      if line.startswith("finished both"):
        state = ST_NONE
      else:
        ensure_starts_with(line, line_no, "page fitz")
        stats.fitz_times.append(extract_render_time(line))
        state = ST_EXPECT_PAGE_SPLASH
    elif ST_EXPECT_PAGE_SPLASH == state:
      ensure_starts_with(line, line_no, "page splash")
      stats.poppler_times.append(extract_render_time(line))
      state = ST_EXPECT_PAGE_FITZ_OR_FINISHED
    else:
      assert False
    line_no += 1
  fo.close()
  return file_name_stats_map

def calc_stats_min(maps, file_name):
  stats_avg = Stats(file_name)
  stats_first = maps[0][file_name]
  maps = maps[1:]
  stats_avg.page_count = stats_first.page_count
  stats_avg.fitz_load_time = stats_first.fitz_load_time
  stats_avg.poppler_load_time = stats_first.poppler_load_time
  stats_avg.fitz_times = stats_first.fitz_times[:]
  stats_avg.poppler_times = stats_first.poppler_times[:]
  for map in maps:
    stats = map[file_name]
    page_count = stats_avg.page_count
    assert page_count == stats.page_count
    if stats_avg.fitz_load_time > stats.fitz_load_time:
      stats_avg.fitz_load_time = stats.fitz_load_time
    if stats_avg.poppler_load_time > stats.poppler_load_time:
      stats_avg.poppler_load_time > stats.poppler_load_time
    assert len(stats.fitz_times) == page_count
    assert len(stats.poppler_times) == page_count
    for n in range(page_count):
      if stats_avg.fitz_times[n] > stats.fitz_times[n]:
        stats_avg.fitz_times[n] > stats.fitz_times[n]
      if stats_avg.poppler_times[n] > stats.poppler_times[n]:
        stats_avg.poppler_times[n] > stats.poppler_times[n]
  return stats_avg

def percent(t1, t2):
  # t2 - 100%
  # t1 - x%
  x = ((t1 * 100.0) / t2) - 100.0
  return x

def calc_stats(maps):
  first_map = maps[0]
  all_stats = []
  print "total number of files: %d" % len(first_map.keys())
  fitz_failed = 0
  poppler_failed = 0
  for stats in first_map.values():
    if isinstance(stats, FailedFitz):
      fitz_failed += 1
      continue
    if isinstance(stats, FailedPoppler):
      poppler_failed += 1
      continue
    stats = calc_stats_min(maps, stats.file_name)
    all_stats.append(stats)
  print "fitz    failed: %d" % fitz_failed
  print "poppler failed: %d" % poppler_failed
  render_speedups = []
  for stat in all_stats:
    print "file: %s" % stat.file_name
    pt = stat.poppler_load_time
    ft = stat.fitz_load_time
    print "loading time     : %.2f, %.2f, %.2f%%" % (pt, ft, percent(pt, ft))
    pt = stat.poppler_total_render_time()
    ft = stat.fitz_total_render_time()
    print "total render time: %.2f, %.2f, %.2f%%" % (pt, ft, percent(pt, ft))
    render_speedups.append(percent(pt,ft))
    print
#  render_speedups.sort()
#  for rs in render_speedups:
#    print "%.2f%%" % rs
  rounded_speedups = {}
  for rs in render_speedups:
    rounded = int(math.floor(rs / 100.0))
    if rounded_speedups.has_key(rounded):
      rounded_speedups[rounded] = rounded_speedups[rounded] + 1
    else:
      rounded_speedups[rounded] = 1
  for k,v in rounded_speedups.items():
    print "%d, %d" % (k,v)

def main():
  maps = []
  for file in files:
    file_name_stats_map = parse_file(file)
    maps.append(file_name_stats_map)
  calc_stats(maps)

if "__main__" == __name__:
  main()
