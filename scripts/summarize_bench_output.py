"""
Takes files containing output from SumatraPDF.exe's -bench command and
summarizes the data in tabular form, calculating average load and render
times over several runs for the same file(s).
"""

import os, re, sys

def matchLine(line, regex, result=None):
	match = re.findall(regex, line)
	if match and type(match[0]) == str:
		match[0] = (match[0],)
	if match and result is not None:
		result.extend(match[0])
	return match

def parseBenchOutput(file, result):
	current, data = None, []
	for line in open(file).xreadlines():
		match = []
		if matchLine(line, r"Starting: (.*)", match):
			if current or data:
				print "Ignoring data for failed run for %s" % (current)
			current, data = match[0], []
			if not current in result:
				result[current] = []
		
		elif matchLine(line, r"load: (\d+(?:\.\d+)?) ms", match):
			assert not data
			data.append((float(match[0]), None))
		
		elif matchLine(line, r"page count: (\d+)", match):
			assert len(data) == 1
			data += [(0, 0)] * int(match[0])
		
		elif matchLine(line, r"pageload +(\d+): (\d+(?:\.\d+)?) ms", match):
			pageNo = int(match[0])
			assert 0 < pageNo <= len(data)
			data[pageNo] = (float(match[1]), data[pageNo][1])
		
		elif matchLine(line, r"pagerender +(\d+): (\d+(?:\.\d+)?) ms", match):
			pageNo = int(match[0])
			assert 0 < pageNo <= len(data)
			data[pageNo] = (data[pageNo][0], float(match[1]))
			
		elif matchLine(line, r"Finished \(in (\d+(?:\.\d+)?) ms\): (.*)", match):
			data[0] = (data[0][0], float(match[0]))
			result[current].append(data)
			current, data = None, []

def displayBenchResults(result):
	print "Filename\tRuns\tLoad time (in ms)\tRender time (in ms)"
	for (file, data) in result.items():
		count = len(data)
		
		loading = [item[0][0] for item in data]
		loadAvg = sum(loading) / count
		loadVar = (sum((time - loadAvg) ** 2 for time in loading) / count) ** 0.5
		
		rendering = [item[0][1] for item in data]
		renderAvg = sum(rendering) / count
		renderVar = (sum((time - renderAvg) ** 2 for time in rendering) / count) ** 0.5
		
		print "%(file)s\t%(count)d\t%(loadAvg).2f +/- %(loadVar).2f\t%(renderAvg).2f +/- %(renderVar).2f" % locals()

def main():
	if not sys.argv[1:]:
		print "Usage: %s <bench1.log> [<bench2.log> ...]" % (os.path.split(sys.argv[0])[1])
		sys.exit(0)
	
	result = { }
	for file in sys.argv[1:]:
		parseBenchOutput(file, result)
	displayBenchResults(result)

if __name__ == "__main__":
	main()
