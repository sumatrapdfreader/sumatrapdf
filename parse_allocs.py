import sys, re

allocs = {} # by addr
totalAllocs = 0
maxActiveAllocs = 0
currActiveAllocs = 0
currMemTaken = 0
maxMemTaken = 0
sizeHist = {}
freesWithoutAllocs = 0

currFrees = 0

def recordAction(isAlloc, addr, size = 0):
    global allocs, currMemTaken, maxMemTaken, sizeHist, freesWithoutAllocs
    global totalAllocs, maxActiveAllocs, currActiveAllocs, currFrees
    #print "isAlloc=%d, addr=%s, size=%d" % (isAlloc, addr, size)
    if isAlloc:
        currMemTaken += size
        if currMemTaken > maxMemTaken:
            maxMemTaken = currMemTaken
        currActiveAllocs += 1
        if currActiveAllocs > maxActiveAllocs:
            maxActiveAllocs = currActiveAllocs
        allocs[addr] = size
        if size in sizeHist:
            sizeHist[size] = sizeHist[size] + 1
        else:
            sizeHist[size] = 1
        totalAllocs += 1
    else:
        if "0x00000000" == addr:
            return
        currFrees += 1
        currActiveAllocs -= 1
        if addr in allocs:
            size = allocs[addr]
            currMemTaken -= size
            del allocs[addr]
        else:
            #print "  free without alloc"
            freesWithoutAllocs += 1

def recordAlloc(addr, size):
    recordAction(True, addr, size)

def recordFree(addr):
    recordAction(False, addr)

# sort by frequency, which is a second element of the tuple
def histSortFunc(e1, e2):
    return cmp(e1[1], e2[1])

def dumpStats():
    global allocs, maxMemTaken, sizeHist, freesWithougAllocs, totalAllocs, maxActiveAllocs
    hist = sizeHist.items()
    hist.sort(histSortFunc)
    print "allocation histogram (size,frequency)"
    for (size,freq) in hist:
        print "%d,%d" % (size,freq)
    print "total allocations: %d" % totalAllocs
    print "max active allocs: %d" % maxActiveAllocs
    print "max mem taken: %d" % maxMemTaken
    print "possible leaks: %d" % len(allocs)
    print "frees without allocs: %d" % freesWithoutAllocs

def parseLine(line):
    line = line.strip()
    lineParts = line.split()
    if lineParts[0] == '-':
        recordFree(lineParts[1].strip())
    elif lineParts[0] == '+':
        recordAlloc(lineParts[1].strip(), int(lineParts[2],16))

def usage():
    print "usage: parse_allocs.py file"
    sys.exit(0)

def parseFile(fileName):
    global currFrees
    fo = open(sys.argv[1], "rb")
    #maxFrees = 100
    for line in fo:
        parseLine(line)
        #if currFrees > maxFrees:
        #    break
    fo.close()

def main():
    if len(sys.argv) != 2:
        usage()
    parseFile(sys.argv[1])
    dumpStats()

if __name__ == "__main__":
    main()
