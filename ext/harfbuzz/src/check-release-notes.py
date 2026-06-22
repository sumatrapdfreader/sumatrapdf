#!/usr/bin/env python3
import sys


news_path = sys.argv[1]
release = sys.argv[2]

with open(news_path, "r", encoding="utf-8") as news_file:
    lines = news_file.readlines()

start = None
end = None
for i, line in enumerate(lines):
    line = line.rstrip()
    if line.startswith("Overview of changes leading to"):
        if start is not None:  # Start of next release
            end = i
            break
        if line.endswith(release):  # Start of the release
            start = i + 3  # Skip the header lines

assert start and end and end > start

print("".join(lines[start:end]))
