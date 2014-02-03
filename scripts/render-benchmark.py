"""
Runs a loading and rendering benchmark for a given number of files
(10 times each).

Note: If SumatraPDF.exe can't be found in either ..\obj-rel\ or %PATH%,
      pass a path to it as the first argument.

render-benchmark.py obj-dbg\SumatraPDF.exe file1.pdf file2.xps
"""

import os, re, sys
from subprocess import Popen, PIPE

def log(str):
	sys.stderr.write(str + "\n")

def runBenchmark(SumatraPDFExe, file, repeats):
	log("-> %s (%d times)" % (file, repeats))
	proc = Popen([SumatraPDFExe] + ["-bench", file] * repeats, stdout=PIPE, stderr=PIPE)
	return proc.communicate()[1]

def matchLine(line, regex, result=None):
	match = re.findall(regex, line)
	if match and type(match[0]) == str:
		match[0] = (match[0],)
	if match and result is not None:
		result.extend(match[0])
	return match

def parseBenchOutput(output):
	result = {}
	current, data = None, []
	
	for line in output.replace("\r", "\n").split("\n"):
		match = []
		if matchLine(line, r"Starting: (.*)", match):
			if current or data:
				log("Ignoring data for failed run for %s" % (current))
			current, data = match[0], []
			if not current in result:
				result[current] = []
		
		elif matchLine(line, r"load: (\d+(?:\.\d+)?) ms", match):
			assert not data
			data.append(float(match[0]))
		
		elif matchLine(line, r"Finished \(in (\d+(?:\.\d+)?) ms\): (.*)", match):
			assert len(data) == 1
			data.append(float(match[0]))
			result[current].append(data)
			current, data = None, []
	
	return result

def displayBenchResults(result):
	print "Filename\tLoad time (in ms)\tRender time (in ms)\tRuns"
	for (file, data) in result.items():
		count = len(data)
		
		loadMin = min([item[0] for item in data])
		renderMin = min([item[1] for item in data])
		
		print "%(file)s\t%(loadMin).2f\t%(renderMin).2f\t%(count)d" % locals()

def main():
	if not sys.argv[1:]:
		log("Usage: %s [<SumatraPDF.exe>] <file1.pdf> [<file2.pdf> ...]" % (os.path.split(sys.argv[0])[1]))
		sys.exit(0)
	
	if sys.argv[1].lower().endswith(".exe"):
		SumatraPDFExe = sys.argv.pop(1)
	else:
		SumatraPDFExe = os.path.join(os.path.dirname(__file__), "..", "obj-rel", "SumatraPDF.exe")
		if not os.path.exists(SumatraPDFExe):
			SumatraPDFExe = "SumatraPDF.exe"
	
	benchData = ""
	log("Running benchmark with %s..." % os.path.relpath(SumatraPDFExe))
	for file in sys.argv[1:]:
		try:
			benchData += runBenchmark(SumatraPDFExe, file, 10) + "\n"
		except:
			log("Error: %s not found" % os.path.relpath(SumatraPDFExe))
			return
	log("")
	
	displayBenchResults(parseBenchOutput(benchData))

if __name__ == "__main__":
	main()
