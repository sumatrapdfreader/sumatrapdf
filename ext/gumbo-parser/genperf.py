import sys, re

gperf_output = sys.stdin.read()

hash_func = re.search(
    "static unsigned int\s+hash\s+\((.*?)\)\s{(.*?)\n}",
    gperf_output, re.DOTALL)

if not hash_func:
    raise "Failed to detect hash function in GPerf output"

wordlist = re.search(
    "wordlist\[\]\s+=\s+{(.*?)}",
    gperf_output, re.DOTALL)

if not wordlist:
    raise "Failed to detect word list in GPerf output"

def process_wordlist(text):
    wordlist = [w.strip().replace('"', '') for w in text.split(',')]
    taglist = [
        "\tGUMBO_TAG_" + (w.upper().replace('-', '_') if w else 'LAST')
        for w in wordlist]
    return taglist

print "static unsigned int tag_hash(%s)\n{%s\n}" % (
        hash_func.group(1), hash_func.group(2))
print ""
print "static const unsigned char kGumboTagMap[] = {\n%s\n};" % (
        ',\n'.join(process_wordlist(wordlist.group(1))))
