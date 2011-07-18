/**
 * pugixml parser - version 1.0
 * --------------------------------------------------------
 * Copyright (C) 2006-2010, by Arseny Kapoulkine (arseny.kapoulkine@gmail.com)
 * Report bugs and download new versions at http://pugixml.org/
 *
 * This library is distributed under the MIT License. See notice at the end
 * of this file.
 *
 * This work is based on the pugxml parser, which is:
 * Copyright (C) 2003, by Kristen Wegner (kristen@tima.net)
 */

#ifndef HEADER_PUGIXML_HPP
#define HEADER_PUGIXML_HPP

#include "pugiconfig.hpp"

// Macro for deprecated features
#ifndef PUGIXML_DEPRECATED
#	if defined(__GNUC__)
#		define PUGIXML_DEPRECATED __attribute__((deprecated))
#	elif defined(_MSC_VER) && _MSC_VER >= 1300
#		define PUGIXML_DEPRECATED __declspec(deprecated)
#	else
#		define PUGIXML_DEPRECATED
#	endif
#endif

// If no API is defined, assume default
#ifndef PUGIXML_API
#   define PUGIXML_API
#endif

// If no API for classes is defined, assume default
#ifndef PUGIXML_CLASS
#   define PUGIXML_CLASS PUGIXML_API
#endif

// If no API for functions is defined, assume default
#ifndef PUGIXML_FUNCTION
#   define PUGIXML_FUNCTION PUGIXML_API
#endif

#include <stddef.h>

// Character interface macros
#ifdef PUGIXML_WCHAR_MODE
#	define PUGIXML_TEXT(t) L ## t
#	define PUGIXML_CHAR wchar_t
#else
#	define PUGIXML_TEXT(t) t
#	define PUGIXML_CHAR char
#endif

namespace pugi
{
	// Character type used for all internal storage and operations; depends on PUGIXML_WCHAR_MODE
	typedef PUGIXML_CHAR char_t;
}

// The PugiXML namespace
namespace pugi
{
	// Tree node types
	enum xml_node_type
	{
		node_null,          // Empty (null) node handle
		node_document,		// A document tree's absolute root
		node_element,		// Element tag, i.e. '<node/>'
		node_pcdata,		// Plain character data, i.e. 'text'
		node_cdata,			// Character data, i.e. '<![CDATA[text]]>'
		node_comment,		// Comment tag, i.e. '<!-- text -->'
		node_pi,			// Processing instruction, i.e. '<?name?>'
		node_declaration,	// Document declaration, i.e. '<?xml version="1.0"?>'
        node_doctype        // Document type declaration, i.e. '<!DOCTYPE doc>'
	};

	// Parsing options

	// Minimal parsing mode (equivalent to turning all other flags off).
    // Only elements and PCDATA sections are added to the DOM tree, no text conversions are performed.
	const unsigned int parse_minimal = 0x0000;

	// This flag determines if processing instructions (node_pi) are added to the DOM tree. This flag is off by default.
	const unsigned int parse_pi = 0x0001;

	// This flag determines if comments (node_comment) are added to the DOM tree. This flag is off by default.
	const unsigned int parse_comments = 0x0002;

	// This flag determines if CDATA sections (node_cdata) are added to the DOM tree. This flag is on by default.
	const unsigned int parse_cdata = 0x0004;

	// This flag determines if plain character data (node_pcdata) that consist only of whitespace are added to the DOM tree.
    // This flag is off by default; turning it on usually results in slower parsing and more memory consumption.
	const unsigned int parse_ws_pcdata = 0x0008;

	// This flag determines if character and entity references are expanded during parsing. This flag is on by default.
	const unsigned int parse_escapes = 0x0010;

	// This flag determines if EOL characters are normalized (converted to #xA) during parsing. This flag is on by default.
	const unsigned int parse_eol = 0x0020;
	
 	// This flag determines if attribute values are normalized using CDATA normalization rules during parsing. This flag is on by default.
 	const unsigned int parse_wconv_attribute = 0x0040;

 	// This flag determines if attribute values are normalized using NMTOKENS normalization rules during parsing. This flag is off by default.
 	const unsigned int parse_wnorm_attribute = 0x0080;
	
    // This flag determines if document declaration (node_declaration) is added to the DOM tree. This flag is off by default.
	const unsigned int parse_declaration = 0x0100;

    // This flag determines if document type declaration (node_doctype) is added to the DOM tree. This flag is off by default.
	const unsigned int parse_doctype = 0x0200;

	// The default parsing mode.
    // Elements, PCDATA and CDATA sections are added to the DOM tree, character/reference entities are expanded,
    // End-of-Line characters are normalized, attribute values are normalized using CDATA normalization rules.
	const unsigned int parse_default = parse_cdata | parse_escapes | parse_wconv_attribute | parse_eol;

    // The full parsing mode.
    // Nodes of all types are added to the DOM tree, character/reference entities are expanded,
    // End-of-Line characters are normalized, attribute values are normalized using CDATA normalization rules.
    const unsigned int parse_full = parse_default | parse_pi | parse_comments | parse_declaration | parse_doctype;

	// These flags determine the encoding of input data for XML document
	enum xml_encoding
	{
		encoding_auto,      // Auto-detect input encoding using BOM or < / <? detection; use UTF8 if BOM is not found
		encoding_utf8,      // UTF8 encoding
		encoding_utf16_le,  // Little-endian UTF16
		encoding_utf16_be,  // Big-endian UTF16
		encoding_utf16,     // UTF16 with native endianness
		encoding_utf32_le,  // Little-endian UTF32
		encoding_utf32_be,  // Big-endian UTF32
		encoding_utf32,     // UTF32 with native endianness
		encoding_wchar      // The same encoding wchar_t has (either UTF16 or UTF32)
	};

	// Formatting flags
	
	// Indent the nodes that are written to output stream with as many indentation strings as deep the node is in DOM tree. This flag is on by default.
	const unsigned int format_indent = 0x01;
	
	// Write encoding-specific BOM to the output stream. This flag is off by default.
	const unsigned int format_write_bom = 0x02;

	// Use raw output mode (no indentation and no line breaks are written). This flag is off by default.
	const unsigned int format_raw = 0x04;
	
	// Omit default XML declaration even if there is no declaration in the document. This flag is off by default.
	const unsigned int format_no_declaration = 0x08;

	// The default set of formatting flags.
    // Nodes are indented depending on their depth in DOM tree, a default declaration is output if document has none.
	const unsigned int format_default = format_indent;
		
	// Forward declarations
	struct xml_attribute_struct;
	struct xml_node_struct;

	class xml_node_iterator;
	class xml_attribute_iterator;

	class xml_tree_walker;
	
	class xml_node;

	// Writer interface for node printing (see xml_node::print)
	class PUGIXML_CLASS xml_writer
	{
	public:
		virtual ~xml_writer() {}

		// Write memory chunk into stream/file/whatever
		virtual void write(const void* data, size_t size) = 0;
	};

	// xml_writer implementation for FILE*
	class PUGIXML_CLASS xml_writer_file: public xml_writer
	{
	public:
        // Construct writer from a FILE* object; void* is used to avoid header dependencies on stdio
		xml_writer_file(void* file);

		virtual void write(const void* data, size_t size);

	private:
		void* file;
	};

	// A light-weight handle for manipulating attributes in DOM tree
	class PUGIXML_CLASS xml_attribute
	{
		friend class xml_attribute_iterator;
		friend class xml_node;

	private:
		xml_attribute_struct* _attr;
	
    	typedef xml_attribute_struct* xml_attribute::*unspecified_bool_type;

	public:
        // Default constructor. Constructs an empty attribute.
		xml_attribute();
		
        // Constructs attribute from internal pointer
		explicit xml_attribute(xml_attribute_struct* attr);

    	// Safe bool conversion operator
    	operator unspecified_bool_type() const;

    	// Borland C++ workaround
    	bool operator!() const;

		// Comparison operators (compares wrapped attribute pointers)
		bool operator==(const xml_attribute& r) const;
		bool operator!=(const xml_attribute& r) const;
		bool operator<(const xml_attribute& r) const;
		bool operator>(const xml_attribute& r) const;
		bool operator<=(const xml_attribute& r) const;
		bool operator>=(const xml_attribute& r) const;

		// Check if attribute is empty
		bool empty() const;

		// Get attribute name/value, or "" if attribute is empty
		const char_t* name() const;
		const char_t* value() const;

		// Get attribute value as a number, or 0 if conversion did not succeed or attribute is empty
		int as_int() const;
		unsigned int as_uint() const;
		double as_double() const;
		float as_float() const;

        // Get attribute value as bool (returns true if first character is in '1tTyY' set), or false if attribute is empty
		bool as_bool() const;

        // Set attribute name/value (returns false if attribute is empty or there is not enough memory)
		bool set_name(const char_t* rhs);
		bool set_value(const char_t* rhs);

        // Set attribute value with type conversion (numbers are converted to strings, boolean is converted to "true"/"false")
		bool set_value(int rhs);
		bool set_value(unsigned int rhs);
		bool set_value(double rhs);
		bool set_value(bool rhs);

		// Set attribute value (equivalent to set_value without error checking)
		xml_attribute& operator=(const char_t* rhs);
		xml_attribute& operator=(int rhs);
		xml_attribute& operator=(unsigned int rhs);
		xml_attribute& operator=(double rhs);
		xml_attribute& operator=(bool rhs);

        // Get next/previous attribute in the attribute list of the parent node
    	xml_attribute next_attribute() const;
    	xml_attribute previous_attribute() const;

        // Get hash value (unique for handles to the same object)
        size_t hash_value() const;

		// Get internal pointer
		xml_attribute_struct* internal_object() const;
	};

	// A light-weight handle for manipulating nodes in DOM tree
	class PUGIXML_CLASS xml_node
	{
		friend class xml_attribute_iterator;
		friend class xml_node_iterator;

	protected:
		xml_node_struct* _root;

    	typedef xml_node_struct* xml_node::*unspecified_bool_type;

	public:
		// Default constructor. Constructs an empty node.
		xml_node();

        // Constructs node from internal pointer
		explicit xml_node(xml_node_struct* p);

    	// Safe bool conversion operator
		operator unspecified_bool_type() const;

		// Borland C++ workaround
		bool operator!() const;
	
		// Comparison operators (compares wrapped node pointers)
		bool operator==(const xml_node& r) const;
		bool operator!=(const xml_node& r) const;
		bool operator<(const xml_node& r) const;
		bool operator>(const xml_node& r) const;
		bool operator<=(const xml_node& r) const;
		bool operator>=(const xml_node& r) const;

		// Check if node is empty.
		bool empty() const;

		// Get node type
		xml_node_type type() const;

		// Get node name/value, or "" if node is empty or it has no name/value
		const char_t* name() const;
		const char_t* value() const;
	
		// Get attribute list
		xml_attribute first_attribute() const;
        xml_attribute last_attribute() const;

        // Get children list
		xml_node first_child() const;
        xml_node last_child() const;

        // Get next/previous sibling in the children list of the parent node
		xml_node next_sibling() const;
		xml_node previous_sibling() const;
		
        // Get parent node
		xml_node parent() const;

		// Get root of DOM tree this node belongs to
		xml_node root() const;

		// Get child, attribute or next/previous sibling with the specified name
		xml_node child(const char_t* name) const;
		xml_attribute attribute(const char_t* name) const;
		xml_node next_sibling(const char_t* name) const;
		xml_node previous_sibling(const char_t* name) const;

		// Get child value of current node; that is, value of the first child node of type PCDATA/CDATA
		const char_t* child_value() const;

		// Get child value of child with specified name. Equivalent to child(name).child_value().
		const char_t* child_value(const char_t* name) const;

		// Set node name/value (returns false if node is empty, there is not enough memory, or node can not have name/value)
		bool set_name(const char_t* rhs);
		bool set_value(const char_t* rhs);
		
		// Add attribute with specified name. Returns added attribute, or empty attribute on errors.
		xml_attribute append_attribute(const char_t* name);
		xml_attribute prepend_attribute(const char_t* name);
		xml_attribute insert_attribute_after(const char_t* name, const xml_attribute& attr);
		xml_attribute insert_attribute_before(const char_t* name, const xml_attribute& attr);

		// Add a copy of the specified attribute. Returns added attribute, or empty attribute on errors.
		xml_attribute append_copy(const xml_attribute& proto);
		xml_attribute prepend_copy(const xml_attribute& proto);
		xml_attribute insert_copy_after(const xml_attribute& proto, const xml_attribute& attr);
		xml_attribute insert_copy_before(const xml_attribute& proto, const xml_attribute& attr);

		// Add child node with specified type. Returns added node, or empty node on errors.
		xml_node append_child(xml_node_type type = node_element);
		xml_node prepend_child(xml_node_type type = node_element);
		xml_node insert_child_after(xml_node_type type, const xml_node& node);
		xml_node insert_child_before(xml_node_type type, const xml_node& node);

		// Add child element with specified name. Returns added node, or empty node on errors.
		xml_node append_child(const char_t* name);
		xml_node prepend_child(const char_t* name);
		xml_node insert_child_after(const char_t* name, const xml_node& node);
		xml_node insert_child_before(const char_t* name, const xml_node& node);

		// Add a copy of the specified node as a child. Returns added node, or empty node on errors.
		xml_node append_copy(const xml_node& proto);
		xml_node prepend_copy(const xml_node& proto);
		xml_node insert_copy_after(const xml_node& proto, const xml_node& node);
		xml_node insert_copy_before(const xml_node& proto, const xml_node& node);

		// Remove specified attribute
		bool remove_attribute(const xml_attribute& a);
		bool remove_attribute(const char_t* name);

		// Remove specified child
		bool remove_child(const xml_node& n);
		bool remove_child(const char_t* name);

		// Find attribute using predicate. Returns first attribute for which predicate returned true.
		template <typename Predicate> xml_attribute find_attribute(Predicate pred) const
		{
			if (!_root) return xml_attribute();
			
			for (xml_attribute attrib = first_attribute(); attrib; attrib = attrib.next_attribute())
				if (pred(attrib))
					return attrib;
		
			return xml_attribute();
		}

		// Find child node using predicate. Returns first child for which predicate returned true.
		template <typename Predicate> xml_node find_child(Predicate pred) const
		{
			if (!_root) return xml_node();
	
			for (xml_node node = first_child(); node; node = node.next_sibling())
				if (pred(node))
					return node;
        
	        return xml_node();
		}

		// Find node from subtree using predicate. Returns first node from subtree (depth-first), for which predicate returned true.
		template <typename Predicate> xml_node find_node(Predicate pred) const
		{
			if (!_root) return xml_node();

			xml_node cur = first_child();
			
			while (cur._root && cur._root != _root)
			{
				if (pred(cur)) return cur;

				if (cur.first_child()) cur = cur.first_child();
				else if (cur.next_sibling()) cur = cur.next_sibling();
				else
				{
					while (!cur.next_sibling() && cur._root != _root) cur = cur.parent();

					if (cur._root != _root) cur = cur.next_sibling();
				}
			}

			return xml_node();
		}

		// Find child node by attribute name/value
		xml_node find_child_by_attribute(const char_t* name, const char_t* attr_name, const char_t* attr_value) const;
		xml_node find_child_by_attribute(const char_t* attr_name, const char_t* attr_value) const;

		// Search for a node by path consisting of node names and . or .. elements.
		xml_node first_element_by_path(const char_t* path, char_t delimiter = '/') const;

		// Recursively traverse subtree with xml_tree_walker
		bool traverse(xml_tree_walker& walker);
			
		// Print subtree using a writer object
		void print(xml_writer& writer, const char_t* indent = PUGIXML_TEXT("\t"), unsigned int flags = format_default, xml_encoding encoding = encoding_auto, unsigned int depth = 0) const;

		// Child nodes iterators
		typedef xml_node_iterator iterator;

		iterator begin() const;
		iterator end() const;

		// Attribute iterators
		typedef xml_attribute_iterator attribute_iterator;

		attribute_iterator attributes_begin() const;
		attribute_iterator attributes_end() const;

		// Get node offset in parsed file/string (in char_t units) for debugging purposes
		ptrdiff_t offset_debug() const;

        // Get hash value (unique for handles to the same object)
        size_t hash_value() const;

		// Get internal pointer
		xml_node_struct* internal_object() const;
	};

	// Child node iterator (a bidirectional iterator over a collection of xml_node)
	class PUGIXML_CLASS xml_node_iterator
	{
		friend class xml_node;

	private:
		xml_node _wrap;
		xml_node _parent;

		xml_node_iterator(xml_node_struct* ref, xml_node_struct* parent);

	public:
		// Iterator traits
		typedef ptrdiff_t difference_type;
		typedef xml_node value_type;
		typedef xml_node* pointer;
		typedef xml_node& reference;

        // Default constructor
		xml_node_iterator();

        // Construct an iterator which points to the specified node
		xml_node_iterator(const xml_node& node);

        // Iterator operators
		bool operator==(const xml_node_iterator& rhs) const;
		bool operator!=(const xml_node_iterator& rhs) const;

		xml_node& operator*();
		xml_node* operator->();

		const xml_node_iterator& operator++();
		xml_node_iterator operator++(int);

		const xml_node_iterator& operator--();
		xml_node_iterator operator--(int);
	};

	// Attribute iterator (a bidirectional iterator over a collection of xml_attribute)
	class PUGIXML_CLASS xml_attribute_iterator
	{
		friend class xml_node;

	private:
		xml_attribute _wrap;
		xml_node _parent;

		xml_attribute_iterator(xml_attribute_struct* ref, xml_node_struct* parent);

	public:
		// Iterator traits
		typedef ptrdiff_t difference_type;
		typedef xml_attribute value_type;
		typedef xml_attribute* pointer;
		typedef xml_attribute& reference;

        // Default constructor
		xml_attribute_iterator();

        // Construct an iterator which points to the specified attribute
		xml_attribute_iterator(const xml_attribute& attr, const xml_node& parent);

		// Iterator operators
		bool operator==(const xml_attribute_iterator& rhs) const;
		bool operator!=(const xml_attribute_iterator& rhs) const;

		xml_attribute& operator*();
		xml_attribute* operator->();

		const xml_attribute_iterator& operator++();
		xml_attribute_iterator operator++(int);

		const xml_attribute_iterator& operator--();
		xml_attribute_iterator operator--(int);
	};

	// Abstract tree walker class (see xml_node::traverse)
	class PUGIXML_CLASS xml_tree_walker
	{
		friend class xml_node;

	private:
		int _depth;
	
	protected:
		// Get current traversal depth
		int depth() const;
	
	public:
		xml_tree_walker();
		virtual ~xml_tree_walker();

		// Callback that is called when traversal begins
		virtual bool begin(xml_node& node);

		// Callback that is called for each node traversed
		virtual bool for_each(xml_node& node) = 0;

		// Callback that is called when traversal ends
		virtual bool end(xml_node& node);
	};

	// Parsing status, returned as part of xml_parse_result object
	enum xml_parse_status
	{
		status_ok = 0,              // No error

		status_file_not_found,      // File was not found during load_file()
		status_io_error,            // Error reading from file/stream
		status_out_of_memory,       // Could not allocate memory
		status_internal_error,      // Internal error occurred

		status_unrecognized_tag,    // Parser could not determine tag type

		status_bad_pi,              // Parsing error occurred while parsing document declaration/processing instruction
		status_bad_comment,         // Parsing error occurred while parsing comment
		status_bad_cdata,           // Parsing error occurred while parsing CDATA section
		status_bad_doctype,         // Parsing error occurred while parsing document type declaration
		status_bad_pcdata,          // Parsing error occurred while parsing PCDATA section
		status_bad_start_element,   // Parsing error occurred while parsing start element tag
		status_bad_attribute,       // Parsing error occurred while parsing element attribute
		status_bad_end_element,     // Parsing error occurred while parsing end element tag
		status_end_element_mismatch // There was a mismatch of start-end tags (closing tag had incorrect name, some tag was not closed or there was an excessive closing tag)
	};

	// Parsing result
	struct PUGIXML_CLASS xml_parse_result
	{
		// Parsing status (see xml_parse_status)
		xml_parse_status status;

		// Last parsed offset (in char_t units from start of input data)
		ptrdiff_t offset;

		// Source document encoding
		xml_encoding encoding;

        // Default constructor, initializes object to failed state
		xml_parse_result();

		// Cast to bool operator
		operator bool() const;

		// Get error description
		const char* description() const;
	};

	// Document class (DOM tree root)
	class PUGIXML_CLASS xml_document: public xml_node
	{
	private:
		char_t* _buffer;

		char _memory[192];
		
		// Non-copyable semantics
		xml_document(const xml_document&);
		const xml_document& operator=(const xml_document&);

		void create();
		void destroy();

		xml_parse_result load_buffer_impl(void* contents, size_t size, unsigned int options, xml_encoding encoding, bool is_mutable, bool own);

	public:
		// Default constructor, makes empty document
		xml_document();

		// Destructor, invalidates all node/attribute handles to this document
		~xml_document();

        // Removes all nodes, leaving the empty document
		void reset();

        // Removes all nodes, then copies the entire contents of the specified document
		void reset(const xml_document& proto);

		// Load document from zero-terminated string. No encoding conversions are applied.
		xml_parse_result load(const char_t* contents, unsigned int options = parse_default);

		// Load document from file
		xml_parse_result load_file(const char* path, unsigned int options = parse_default, xml_encoding encoding = encoding_auto);
		xml_parse_result load_file(const wchar_t* path, unsigned int options = parse_default, xml_encoding encoding = encoding_auto);

		// Load document from buffer. Copies/converts the buffer, so it may be deleted or changed after the function returns.
		xml_parse_result load_buffer(const void* contents, size_t size, unsigned int options = parse_default, xml_encoding encoding = encoding_auto);

		// Load document from buffer, using the buffer for in-place parsing (the buffer is modified and used for storage of document data).
        // You should ensure that buffer data will persist throughout the document's lifetime, and free the buffer memory manually once document is destroyed.
		xml_parse_result load_buffer_inplace(void* contents, size_t size, unsigned int options = parse_default, xml_encoding encoding = encoding_auto);

		// Load document from buffer, using the buffer for in-place parsing (the buffer is modified and used for storage of document data).
        // You should allocate the buffer with pugixml allocation function; document will free the buffer when it is no longer needed (you can't use it anymore).
		xml_parse_result load_buffer_inplace_own(void* contents, size_t size, unsigned int options = parse_default, xml_encoding encoding = encoding_auto);

		// Save XML document to writer (semantics is slightly different from xml_node::print, see documentation for details).
		void save(xml_writer& writer, const char_t* indent = PUGIXML_TEXT("\t"), unsigned int flags = format_default, xml_encoding encoding = encoding_auto) const;

		// Save XML to file
		bool save_file(const char* path, const char_t* indent = PUGIXML_TEXT("\t"), unsigned int flags = format_default, xml_encoding encoding = encoding_auto) const;
		bool save_file(const wchar_t* path, const char_t* indent = PUGIXML_TEXT("\t"), unsigned int flags = format_default, xml_encoding encoding = encoding_auto) const;

        // Get document element
        xml_node document_element() const;
	};


	// Memory allocation function interface; returns pointer to allocated memory or NULL on failure
	typedef void* (*allocation_function)(size_t size);
	
	// Memory deallocation function interface
    typedef void (*deallocation_function)(void* ptr);

    // Override default memory management functions. All subsequent allocations/deallocations will be performed via supplied functions.
    void PUGIXML_FUNCTION set_memory_management_functions(allocation_function allocate, deallocation_function deallocate);
    
    // Get current memory management functions
    allocation_function PUGIXML_FUNCTION get_memory_allocation_function();
    deallocation_function PUGIXML_FUNCTION get_memory_deallocation_function();
}

#endif

/**
 * Copyright (c) 2006-2010 Arseny Kapoulkine
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
