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

import re, trans_download, trans_gen, trans_langs, util

def extract_accesskey_groups(path):
	groups = {}
	group, group_name = None, None
	alt_group = None

	for line in open(path, "r").readlines():
		line = line.strip()
		if line.startswith("//[ ACCESSKEY_GROUP ") or line.startswith("//] ACCESSKEY_GROUP "):
			new_name = line[20:].strip()
			if line[2] == '[':
				assert group is None, "Group '%s' doesn't end before group '%s' starts" % (group_name, new_name)
				group_name = new_name
				group = groups[group_name] = groups.get(group_name, [[]])
			else:
				assert group is not None, "Unexpected group end ('%s')" % new_name
				assert group_name == new_name, "Group end mismatch: '%s' != '%s'" (new_name, group_name)
				group = None

		elif line.startswith("//[ ACCESSKEY_ALTERNATIVE") or  line.startswith("//| ACCESSKEY_ALTERNATIVE") or  line.startswith("//] ACCESSKEY_ALTERNATIVE"):
			assert group is not None, "Can't use ACCESSKEY_ALTERNATIVE outside of group"
			assert line[25].isspace(), "Typo?"
			if line[2] == '[':
				assert alt_group is None, "Nested ACCESSKEY_ALTERNATIVE isn't supported"
				alt_group = [[]]
				group[0].append(alt_group)
			elif line[2] == '|':
				assert alt_group is not None, "Unexpected ACCESSKEY_ALTERNATIVE alternative"
				alt_group.append([])
			else:
				assert alt_group is not None, "Unexpected ACCESSKEY_ALTERNATIVE end"
				alt_group = None

		elif group is not None:
			strings = re.findall(trans_gen.TRANSLATION_PATTERN, line)
			for string in strings:
				if string not in group:
					assert len(re.findall("&", string)) <= 1, "TODO: handle multiple '&' in strings"
					group.append(string)
				if alt_group is not None:
					alt_group[-1].append(string)

	return groups

def get_alternate_ix(alternates, string):
	for i in range(len(alternates)):
		for j in range(len(alternates[i])):
			if string in alternates[i][j]:
				return (i, j)
	return None

def detect_accesskey_clashes(groups, translations):
	for lang in trans_langs.g_langs:
		print "Accesskey issues for '%s'" % lang[1]
		print "=" * (23 + len(lang[1]))
		warnings = []

		for (name, strings) in groups.items():
			used_keys, duplicates, alternates = {}, [], {}

			for string in strings[1:]:
				trans = ([item[1] for item in translations[string] if item[0] == lang[0]] + [string])[0]
				ix = trans.find("&")
				if ix == -1:
					if "&" in string:
						warnings.append("WARNING: Translation has no accesskey where original does:")
						warnings.append("         \"%s\", \"%s\"" % (string, trans))
					continue
				if ix == len(trans) - 1:
					warnings.append("ERROR: '&' must be followed by a letter (\"%s\")" % trans)
					continue
				if "&" not in string:
					warnings.append("WARNING: Translation has accesskey where original doesn't:")
					warnings.append("         \"%s\", \"%s\"" % (string, trans))

				key = trans[ix + 1].upper()
				alternates[key] = alternates.get(key, [])
				if key in used_keys.keys():
					if None in alternates[key] or get_alternate_ix(strings[0], string) in alternates[key]:
						duplicates.append((key, trans))
					else:
						alternates[key].append(get_alternate_ix(strings[0], string))
				else:
					if not key.isalnum():
						warnings.append("WARNING: Access key '%s' might not work on all keyboards (\"%s\")" % (key, trans))
					used_keys[key] = trans
					alternates[key].append(get_alternate_ix(strings[0], string))

			if duplicates:
				print "Clashes in accesskey group '%s':" % name
				for item in duplicates:
					print " * %s: \"%s\" and \"%s\"" % (item[0].upper(), item[1], used_keys[item[0].upper()])
				available = [chr(i) for i in range(ord("A"), ord("Z") + 1) if chr(i) not in used_keys.keys()]
				print "   (available keys: %s)" % "".join(available)
				print

		print "\n".join(warnings)
		print

def main():
	util.chdir_top()

	groups = {}
	for file in trans_gen.C_FILES_TO_PROCESS:
		groups.update(extract_accesskey_groups(file))

	translations = open(trans_download.lastDownloadFilePath(), "rb").read()
	translations = trans_download.parseTranslations(translations)

	detect_accesskey_clashes(groups, translations)

if __name__ == "__main__":
	main()
