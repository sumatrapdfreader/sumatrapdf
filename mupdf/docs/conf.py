# see https://www.sphinx-doc.org/en/master/usage/configuration.html

import os
import datetime
import re

def get_mupdf_version_from_header():
	major = minor = patch = None
	_path = os.path.abspath(f'{__file__}/../../include/mupdf/fitz/version.h')
	with open(_path) as f:
		for line in f:
			if not major:
				major = re.search('#define FZ_VERSION_MAJOR ([0-9]+$)', line)
			if not minor:
				minor = re.search('#define FZ_VERSION_MINOR ([0-9]+$)', line)
			if not patch:
				patch = re.search('#define FZ_VERSION_PATCH ([0-9]+$)', line)
	if major and minor and patch:
		release = major.group(1) + "." + minor.group(1) + "." + patch.group(1)
	else:
		raise Exception(f'Failed to find MuPDF version in {_path}')
	return release

def get_mupdf_version_from_git():
	return os.popen("git describe --all").read().strip().replace("tags/", "").replace("heads/", "")

def get_mupdf_version():
	return get_mupdf_version_from_git() or get_mupdf_version_from_header()

project = "MuPDF"
copyright = "2004-" + str(datetime.date.today().year) + " Artifex"
release = version = get_mupdf_version()

extensions = [
	"myst_parser",
	"sphinx.ext.graphviz",
	"sphinxcontrib.imagesvg",
	"sphinxcontrib.googleanalytics",
]

toc_object_entries_show_parents = "hide"
highlight_language = "none"
default_role = "any"
add_function_parentheses = False
add_module_names = False
show_authors = False
keep_warnings = False

myst_heading_anchors = 2
myst_enable_extensions = [
	"deflist",
	"html_image",
	"linkify",
	"replacements",
	"smartquotes",
	"strikethrough",
	"tasklist",
]

googleanalytics_id = "G-JZTN4VTL9M"

rst_prolog = """

.. |no_new| replace:: *You cannot create instances of this class with the new operator!*

.. |interface_type| replace:: *This is an interface, not a concrete class!*

.. |only_mutool| raw:: html

   <span class="only_mutool">only&nbsp;mutool&nbsp;run</span>

.. |only_mupdfjs| raw:: html

   <span class="only_mupdfjs">only&nbsp;mupdf.js</span>

"""

# -- Options for HTML output ----------------------------------------------

html_theme = "furo"

html_title = "MuPDF " + version

html_use_smartypants = True

html_domain_indices = False
html_use_index = False
html_split_index = False

html_copy_source = False
html_show_sourcelink = False
html_show_sphinx = False
html_show_copyright = True

html_static_path = [ "_static" ]
html_css_files = [ "custom.css" ]
html_logo = "_static/mupdf-sidebar-logo.webp"
html_favicon = "_static/favicon.ico"

html_theme_options = {
	"footer_icons": [
		{
			"name": "Discord",
			"url": "https://discord.gg/DQjvZ6ERqH",
			"class": "discord-link",
			"html": """Find <b>#mupdf</b> on Discord <svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 127.14 96.36"><path fill="#5865f2" d="M107.7,8.07A105.15,105.15,0,0,0,81.47,0a72.06,72.06,0,0,0-3.36,6.83A97.68,97.68,0,0,0,49,6.83,72.37,72.37,0,0,0,45.64,0,105.89,105.89,0,0,0,19.39,8.09C2.79,32.65-1.71,56.6.54,80.21h0A105.73,105.73,0,0,0,32.71,96.36,77.7,77.7,0,0,0,39.6,85.25a68.42,68.42,0,0,1-10.85-5.18c.91-.66,1.8-1.34,2.66-2a75.57,75.57,0,0,0,64.32,0c.87.71,1.76,1.39,2.66,2a68.68,68.68,0,0,1-10.87,5.19,77,77,0,0,0,6.89,11.1A105.25,105.25,0,0,0,126.6,80.22h0C129.24,52.84,122.09,29.11,107.7,8.07ZM42.45,65.69C36.18,65.69,31,60,31,53s5-12.74,11.43-12.74S54,46,53.89,53,48.84,65.69,42.45,65.69Zm42.24,0C78.41,65.69,73.25,60,73.25,53s5-12.74,11.44-12.74S96.23,46,96.12,53,91.08,65.69,84.69,65.69Z"/></svg>""",
		},
	],
}

# -- Options for PDF output --------------------------------------------------

# (source start file, target name, title, author).
pdf_documents = [ ("index", "MuPDF", "MuPDF Manual", "Artifex") ]
pdf_compressed = True
pdf_language = "en_US"
pdf_use_index = True
pdf_use_modindex = True
pdf_use_coverpage = True
pdf_break_level = 2
pdf_verbosity = 0
pdf_invariant = True
