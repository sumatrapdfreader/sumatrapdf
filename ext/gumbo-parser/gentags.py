import sys

def open_and_write_header(filename, comment_prefix):
  f = open(filename, 'w')
  f.write(comment_prefix + ' Generated via `gentags.py src/tag.in`.\n')
  f.write(comment_prefix + ' Do not edit; edit src/tag.in instead.\n')
  f.write(comment_prefix + ' clang-format off\n')
  return f

tag_strings = open_and_write_header('src/tag_strings.h', '//')
tag_enum = open_and_write_header('src/tag_enum.h', '//')
tag_sizes = open_and_write_header('src/tag_sizes.h', '//')

tag_py = open_and_write_header('python/gumbo/gumboc_tags.py', '#')
tag_py.write('TagNames = [\n')

tagfile = open(sys.argv[1])

for tag in tagfile:
    tag = tag.strip()
    tag_upper = tag.upper().replace('-', '_')
    tag_strings.write('"%s",\n' % tag)
    tag_enum.write('GUMBO_TAG_%s,\n' % tag_upper)
    tag_sizes.write('%d, ' % len(tag))
    tag_py.write('  "%s",\n' % tag_upper)

tagfile.close()

tag_strings.close()
tag_enum.close()
tag_sizes.close()

tag_py.write(']\n')
tag_py.close()
