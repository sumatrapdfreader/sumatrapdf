"""
Looks for accesskey collisions in translations.

Groups of menu or dialog items which appear together can be marked as

//[ ACCESSKEY_GROUP <name>
... _TRN("Menu &Item") ... _TRN("&Another Menu Item") ...
//] ACCESSKEY_GROUP <name>

All accesskeys used within a group are compared per translation and
items which share the same accesskey are reported so that they could
be changed to unique accesskeys.
"""

import re, trans_download, trans_gen, trans_langs, util2

def extract_accesskey_groups(path):
	groups = {}
	group, group_name = None, None
	
	for line in open(path, "r").readlines():
		if line.startswith("//[ ACCESSKEY_GROUP ") or line.startswith("//] ACCESSKEY_GROUP "):
			new_name = line[20:].strip()
			if line[2] == '[':
				assert group is None, "Group '%s' doesn't end before group '%s' starts" % (group_name, new_name)
				group_name = new_name
				group = groups[group_name] = groups.get(group_name, [])
			else:
				assert group is not None, "Unexpected group end ('%s')" % new_name
				assert group_name == new_name, "Group end mismatch: '%s' != '%s'" (new_name, group_name)
				group = None
		elif group is not None:
			strings = re.findall(trans_gen.TRANSLATION_PATTERN, line)
			for string in strings:
				assert len(re.findall("&", string)) <= 1, "TODO: handle multiple '&' in strings"
				group.append(string)
	
	return groups

def detect_accesskey_clashes(groups, translations):
	for lang in trans_langs.g_langs:
		print "Checking language '%s'" % lang[0]
		print "=" * (20 + len(lang[0]))
		for (name, strings) in groups.items():
			used_keys, duplicates = {}, []
			
			for string in strings:
				trans = ([item[1] for item in translations[string] if item[0] == lang[0]] + [string])[0]
				ix = trans.find("&")
				if ix == -1:
					continue
				if ix == len(trans) - 1:
					print "ERROR: '&' must be followed by a letter (\"%s\")" % trans
					continue
				key = trans[ix + 1].upper()
				if key in used_keys.keys():
					# TODO: allow marking items which never appear together
					if string != "Open in &Microsoft HTML Help" or "XPS" not in used_keys[key].replace("&", ""):
						duplicates.append((key, trans))
				else:
					if not key.isalnum():
						print "WARNING: Access key '%s' might not work on all keyboards (\"%s\")" % (key, trans)
					used_keys[key] = trans
			
			if duplicates:
				print "Duplicates for accesskey group '%s':" % name
				for item in duplicates:
					print " * %s: \"%s\" and \"%s\"" % (item[0].upper(), item[1], used_keys[item[0].upper()])
				available = [chr(i) for i in range(ord("A"), ord("Z") + 1) if chr(i) not in used_keys.keys()]
				print "   (available keys: %s)" % "".join(available)
				print
		print

def main():
	util2.chdir_top()
	
	groups = {}
	for file in trans_gen.C_FILES_TO_PROCESS:
		groups.update(extract_accesskey_groups(file))
	
	translations = open(trans_download.lastDownloadFilePath(), "rb").read()
	translations = trans_download.parseTranslations(translations)
	
	detect_accesskey_clashes(groups, translations)

if __name__ == "__main__":
	main()
