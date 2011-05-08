"""
Runs a loading and rendering benchmark for a given number of files
(10 times each).

Note: If SumatraPDF.exe can't be found in the path, pass a path to
      it as the first argument.

render-benchmark.py obj-dbg\SumatraPDF.exe file1.pdf file2.xps
"""

import os, re, sys
import tempfile, subprocess

def log(str):
	sys.stderr.write(str + "\n")

def getTempFile():
	file = tempfile.NamedTemporaryFile(delete=False)
	name = file.name
	file.close()
	return name

def runBenchmark(SumatraPDF, file, logfile, repeats):
	try:
		log("-> %s (%d times)" % (file, repeats))
		subprocess.call([SumatraPDF, "-console-file", logfile] + ["-bench", file] * repeats)
		return True
	except WindowsError:
		print SumatraPDF, "not found!"
		return False

def matchLine(line, regex, result=None):
	match = re.findall(regex, line)
	if match and type(match[0]) == str:
		match[0] = (match[0],)
	if match and result is not None:
		result.extend(match[0])
	return match

def parseBenchOutput(file):
	result = {}
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
	
	return result

def displayBenchResults(result):
	print "Filename\tLoad time (in ms)\tRender time (in ms)\tRuns"
	for (file, data) in result.items():
		count = len(data)
		
		loading = [item[0][0] for item in data]
		loadMin = min(loading)
		
		rendering = [item[0][1] for item in data]
		renderMin = min(rendering)
		
		print "%(file)s\t%(loadMin).2f\t%(renderMin).2f\t%(count)d" % locals()

def main():
	if not sys.argv[1:]:
		print "Usage: %s [<SumatraPDF.exe>] <file1.pdf> [<file2.pdf> ...]" % (os.path.split(sys.argv[0])[1])
		sys.exit(0)
	
	if sys.argv[1].lower().endswith(".exe"):
		SumatraPDF = sys.argv.pop(1)
	else:
		SumatraPDF = "SumatraPDF.exe"
	
	log("Running benchmark with %s..." % SumatraPDF)
	logfile = getTempFile()
	for file in sys.argv[1:]:
		if not runBenchmark(SumatraPDF, file, logfile, 10):
			os.remove(logfile)
			return
	log("")
	displayBenchResults(parseBenchOutput(logfile))
	os.remove(logfile)

if __name__ == "__main__":
	main()
