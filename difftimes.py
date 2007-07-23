# Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)

import sys, os, os.path, string

BIG_PERCENT_DIFF = 45.0
SMALL_PERCENT_DIFF = 10.0

def uage_and_exit():
  print "Usage: compare_times.py <stats-file> <stats-file>"
  sys.exit(1)

# return difference between <one> and <two> in percent
def percent_diff(one, two):
  return 100.0 - (two * float(100) / one)

def percent_diff_abs(one, two):
  return abs(percent_diff(one, two))

def get_time(txt):
  t = txt.split()
  assert 2 == len(t)
  assert "ms" == t[1]
  return float(t[0])

# given text in the form "page N", return N as an int
def get_page_no(txt):
  lp = txt.split()
  assert "page" == lp[0]
  assert 2 == len(lp)
  return int(lp[1])

class Stats:
  def __init__(self, file_name):
    self.file_name = file_name
    self.page_count = None
    self.timings = []
    self.load_time = None

def verify_stats(stats):
  if 0 == len(stats): return
  file_name = stats[0].file_name
  count = len(stats[0].timings)
  # if we have -loadonly flag, we don't have stats for pages
  if 0 == count: return
  for stat in stats:
    assert None != stat.page_count
    assert None != stat.load_time
    assert file_name == stat.file_name
    assert count == len(stat.timings)
    #assert count == stat.page_count

# given a list of float values, return their average
def get_avg(float_list):
  if 0 == len(float_list):
    return 0.0
  total = 0.0
  for el in float_list:
    total += el
  return total / float(len(float_list))

# given a list of float values, return a list with elements that differ by more
# than <max_diff> from average removed
def filter_avg(float_list, max_diff):
  avg = get_avg(float_list)
  return [el for el in float_list if percent_diff_abs(el, avg) <= max_diff]

def get_load_avg_time(stats):
  float_list = [stat.load_time for stat in stats]
  filtered = filter_avg(float_list, BIG_PERCENT_DIFF)
  filtered = filter_avg(filtered, SMALL_PERCENT_DIFF)
  return get_avg(filtered)

def get_page_avg_time(stats, page_no):
  float_list = [stat.timings[page_no] for stat in stats]
  filtered = filter_avg(float_list, BIG_PERCENT_DIFF)
  filtered = filter_avg(filtered, SMALL_PERCENT_DIFF)
  return get_avg(filtered)

def parse_stats_from_file(file_name):
  fo = open(file_name, "rb")
  txt = fo.read()
  fo.close()
  return parse_stats(txt)

def parse_stats(txt):
  stats = []
  curStat = None  
  lines = txt.split("\n")
  lines = [l.strip() for l in lines if len(l.strip()) > 0]
  for l in lines:
    #print l
    lp = l.split(":", 1)
    assert 2 == len(lp)
    key = lp[0]
    if "started" == key:
      assert None == curStat
      curStat = Stats(lp[1])
      continue
    if "finished" == key:
      if lp[1] != curStat.file_name:
        print "file names mismatch %s %s" % (curStat.file_name, lp[1])
      stats.append(curStat)
      curStat = None
      continue
    if "load" == key:
      curStat.load_time = get_time(lp[1])
      continue
    if "page count" == key:
      curStat.page_count = int(lp[1])
      continue
    if key.startswith("page"):
      pageNo = get_page_no(key)
      curStat.timings.append(get_time(lp[1]))
      continue

  return stats

def calc_avg(stats):
  avg = Stats(stats[0].file_name)
  avg.timings = []
  avg.page_count = len(stats[0].timings)
  avg.load_time = get_load_avg_time(stats)
  for page_no in range(avg.page_count):
    avg.timings.append(get_page_avg_time(stats, page_no))
  return avg

def dump_stats(file_one, avg_one, file_two, avg_two):
  print file_one + ", " + file_two
  one = avg_one.load_time
  two = avg_two.load_time
  d = percent_diff(one, two)
  print "loading time, %.2f, %.2f, %%%.2f" % (one, two, d)
  assert avg_one.page_count == avg_two.page_count
  for page_no in range(avg_one.page_count):
    one = avg_one.timings[page_no]
    two = avg_two.timings[page_no]
    d = percent_diff(one, two)
    print "page %d, %.2f, %.2f, %%%.2f" % (page_no+1, one, two, d)

def compare_stats(file_one, file_two):
  stats_one = parse_stats_from_file(file_one)
  verify_stats(stats_one)
  avg_one = calc_avg(stats_one)
  stats_two = parse_stats_from_file(file_two)  
  verify_stats(stats_two)
  avg_two = calc_avg(stats_two)
  dump_stats(file_one, avg_one, file_two, avg_two)

def main():
  if 3 != len(sys.argv):
    usage_and_exit()
  compare_stats(sys.argv[1], sys.argv[2])

if __name__ == "__main__":
  main()
