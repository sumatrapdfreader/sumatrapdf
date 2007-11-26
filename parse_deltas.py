# parses log from fz_scanconvert in pathscan.c to determine
# good buffer size for deltas_static_buf (good means: small
# and covers as many cases as possible)
import sys

def dump_stats(stats):
  total = 0
  for count in stats.values(): total += count
  #print "total: %d" % total
  sorted_by_count = stats.items()
  sorted_by_count.sort(lambda y,x: cmp(x[1], y[1]))
  print sorted_by_count
  total_so_far = 0
  max_so_far = 0
  for (len, count) in sorted_by_count:
    total_so_far += count
    if len > max_so_far: max_so_far = len
    print "%4d, %5d, %5d, %4d" % (len, count, total_so_far, max_so_far)

def dump_stats_2(stats):
  total = 0
  for val in stats.values(): total += val
  sorted_by_len = stats.items()
  #sorted_by_len.sort(lambda x,y: cmp(x[0], y[0]))
  sorted_by_len.sort()
  #print sorted_by_len
  total_so_far = 0
  print "total: %d" % total
  for (len, count) in sorted_by_len:
    total_so_far += count
    percent = float(total_so_far) * 100.0 / float(total)
    print "%4d, %5d, %5d, %.2f" % (len, count, total_so_far, percent)

def main():
  if len(sys.argv) != 2:
    usage_and_exit();
  stats = {}
  fo = open(sys.argv[1], "rb")
  for line in fo:
    if line.startswith("dc:"):
      c = int(line.split()[1])
      #print c
      if stats.has_key(c):
        stats[c] = stats[c] + 1
      else:
        stats[c] = 1
  fo.close()
  dump_stats_2(stats)
  
if __name__ == "__main__":
  main()
