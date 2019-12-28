package main

import (
	"fmt"
	"strings"

	"github.com/kjk/u"
)

func isGroupStartOrEnd(s string) bool {
	if strings.HasPrefix(s, "//[ ACCESSKEY_GROUP ") {
		return true
	}
	if strings.HasPrefix(s, "//] ACCESSKEY_GROUP ") {
		return true
	}
	return false
}

func isAltGroupStartOrEnd(s string) bool {
	if strings.HasPrefix(s, "//[ ACCESSKEY_ALTERNATIVE") {
		return true
	}
	if strings.HasPrefix(s, "//| ACCESSKEY_ALTERNATIVE") {
		return true
	}
	if strings.HasPrefix(s, "//] ACCESSKEY_ALTERNATIVE") {
		return true
	}
	return false
}

type accessGroup struct {
	group      []string
	inAltGroup bool
	altGroup   []string
}

func extract_accesskey_groups(path string) map[string]*accessGroup {
	lines, err := u.ReadLinesFromFile(path)
	must(err)

	groups := map[string]*accessGroup{}
	group_name := ""
	var group *accessGroup

	for _, line := range lines {
		line = strings.TrimSpace(line)
		if isGroupStartOrEnd(line) {
			new_name := line[20:]
			if line[2] == '[' {
				// start of new group
				panicIf(group != nil, "Group '%s' doesn't end before group '%s' starts", group_name, new_name)
				group_name = new_name
				group = groups[group_name]
				if group == nil {
					group = &accessGroup{}
					groups[group_name] = group
				}
			} else {
				// end of group
				panicIf(group == nil, "Unexpected group end ('%s')", new_name)
				panicIf(group_name != new_name, "Group end mismatch: '%s' != '%s'", new_name, group_name)
				group = nil
			}
		} else if isAltGroupStartOrEnd(line) {
			panicIf(group == nil, "Can't use ACCESSKEY_ALTERNATIVE outside of group")
			if line[2] == '[' {
				panicIf(line[25] != ' ', "Typo?")
				panicIf(group.inAltGroup, "Nested ACCESSKEY_ALTERNATIVE isn't supported")
				group.inAltGroup = true
			} else if line[2] == '|' {
				panicIf(!group.inAltGroup, "Unexpected ACCESSKEY_ALTERNATIVE alternative")
			} else {
				panicIf(!group.inAltGroup, "Unexpected ACCESSKEY_ALTERNATIVE end")
				group.inAltGroup = false
			}
		} else if group != nil {
			strs := extractTranslations(line)
			for _, str := range strs {
				exists := u.StringInSlice(group.group, str)
				n := strings.Count(str, "&")
				panicIf(n > 1, "TODO: handle multiple '&' in strings")
				if exists {
					group.group = append(group.group, str)
				}
				if group.inAltGroup {
					group.altGroup = append(group.altGroup, str)
				}
			}
		}
	}
	return groups
}

func strMult(s string, n int) string {
	res := ""
	for i := 0; i < n; i++ {
		res += s
	}
	return res
}

func isAlnum(s string) bool {
	panicIf(len(s) != 1)
	c := s[0]
	if c >= 'a' && c <= 'z' {
		return true
	}
	if c >= 'A' && c <= 'Z' {
		return true
	}
	if c >= '0' && c <= '9' {
		return true
	}
	return false
}

func detect_accesskey_clashes(groups map[string]*accessGroup, translations map[string][]*Translation) {
	for _, lang := range g_langs {
		fmt.Printf("Accesskey issues for '%s'", lang[1])
		fmt.Printf("%s'\n", strMult("=", 23+len(lang[1])))
		warnings := []string{}
		for name, group := range groups {
			used_keys := map[string]string{}
			duplicates := map[string]bool{}
			alternates := map[string][]string{}

			_ = name
			_ = duplicates
			_ = alternates

			strs := group.group
			for _, str := range strs[1:] {
				trans := str
				for _, item := range translations[str] {
					if item.Lang == lang[0] {
						trans = item.Translation
						break
					}
				}
				ix := strings.Index(trans, "&")
				if ix == -1 {
					if strings.Contains(str, "&") {
						s := "WARNING: Translation has no accesskey where original does:"
						warnings = append(warnings, s)
						s = fmt.Sprintf("         \"%s\", \"%s\"", strs, trans)
						warnings = append(warnings, s)
						continue
					}
				}
				if ix == len(trans)-1 {
					s := fmt.Sprintf("ERROR: '&' must be followed by a letter (\"%s\")", trans)
					warnings = append(warnings, s)
					continue
				}
				if !strings.Contains(str, "&") {
					s := "WARNING: Translation has accesskey where original doesn't:"
					warnings = append(warnings, s)
					s = fmt.Sprintf("         \"%s\", \"%s\"", str, trans)
					warnings = append(warnings, s)
				}
				key := strings.ToUpper(string(trans[ix+1]))
				if _, ok := used_keys[key]; ok {
					/*
						if None in alternates[key] or get_alternate_ix(strings[0], string) in alternates[key]:
							duplicates.append((key, trans))
						else:
							alternates[key].append(get_alternate_ix(strings[0], string))
					*/
				} else {
					if !isAlnum(key) {
						s := fmt.Sprintf("WARNING: Access key '%s' might not work on all keyboards (\"%s\")", key, trans)
						warnings = append(warnings, s)
					}
					used_keys[key] = trans
					//alternates[key].append(get_alternate_ix(strings[0], string))
				}
			}
		}
		/*
				if duplicates:
					print "Clashes in accesskey group '%s':" % name
					for item in duplicates:
						print " * %s: \"%s\" and \"%s\"" % (item[0].upper(), item[1], used_keys[item[0].upper()])
					available = [chr(i) for i in range(ord("A"), ord("Z") + 1) if chr(i) not in used_keys.keys()]
					print "   (available keys: %s)" % "".join(available)
					print

			print "\n".join(warnings)
		*/
		fmt.Printf("\n")
	}
}

func printGroups(file string, groups map[string]*accessGroup) {
	if len(groups) == 0 {
		return
	}
	fmt.Printf("%s\n", file)
	for k, g := range groups {
		fmt.Printf("  %s\n", k)
		if len(g.group) > 0 {
			for _, s := range g.group {
				fmt.Printf("    %s\n", s)
			}
		}
		if len(g.altGroup) > 0 {
			fmt.Printf("    alt groups:\n")
			for _, s := range g.altGroup {
				fmt.Printf("    %s\n", s)
			}
		}
	}
}

func updateGroups(m1 map[string]*accessGroup, m2 map[string]*accessGroup) map[string]*accessGroup {
	if m1 == nil {
		return m2
	}

	for k, g2 := range m2 {
		g1 := m1[k]
		if g1 == nil {
			m1[k] = g2
			continue
		}
		g1.group = append(g1.group, g2.group...)
		g1.altGroup = append(g1.altGroup, g2.altGroup...)
	}
	return m1
}

func checkAccessKeys() {
	cFiles := getFilesToProcess()
	var allGroups map[string]*accessGroup
	for _, file := range cFiles {
		group := extract_accesskey_groups(file)
		printGroups(file, group)
		allGroups = updateGroups(allGroups, group)
	}
	d := u.ReadFileMust(lastDownloadFilePath())
	translations := parseTranslations(string(d))
	detect_accesskey_clashes(allGroups, translations)
}
