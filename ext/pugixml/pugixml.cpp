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

#include "pugixml.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>
#include <wchar.h>

// For placement new
#include <new>

#define DMC_VOLATILE volatile

#ifdef _MSC_VER
#	pragma warning(disable: 4127) // conditional expression is constant
#	pragma warning(disable: 4324) // structure was padded due to __declspec(align())
#	pragma warning(disable: 4611) // interaction between '_setjmp' and C++ object destruction is non-portable
#	pragma warning(disable: 4702) // unreachable code
#	pragma warning(disable: 4996) // this function or variable may be unsafe
#endif

#ifdef __INTEL_COMPILER
#	pragma warning(disable: 177) // function was declared but never referenced 
#	pragma warning(disable: 1478 1786) // function was declared "deprecated"
#endif

// uintptr_t
#if !defined(_MSC_VER) || _MSC_VER >= 1600
#	include <stdint.h>
#else
#	if _MSC_VER < 1300
// No native uintptr_t in MSVC6
typedef size_t uintptr_t;
#	endif
typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef __int32 int32_t;
#endif

// Inlining controls
#if defined(_MSC_VER) && _MSC_VER >= 1300
#	define PUGIXML_NO_INLINE __declspec(noinline)
#elif defined(__GNUC__)
#	define PUGIXML_NO_INLINE __attribute__((noinline))
#else
#	define PUGIXML_NO_INLINE 
#endif

// Simple static assertion
#define STATIC_ASSERT(cond) { static const char condition_failed[(cond) ? 1 : -1] = {0}; (void)condition_failed[0]; }

using namespace pugi;

// Memory allocation
namespace
{
	void* default_allocate(size_t size)
	{
		return malloc(size);
	}

	void default_deallocate(void* ptr)
	{
		free(ptr);
	}

	allocation_function global_allocate = default_allocate;
	deallocation_function global_deallocate = default_deallocate;
}

// String utilities
namespace
{
	// Get string length
	size_t strlength(const char_t* s)
	{
		assert(s);
		return strlen(s);
	}

	// Compare two strings
	bool strequal(const char_t* src, const char_t* dst)
	{
		assert(src && dst);
		return strcmp(src, dst) == 0;
	}

	// Compare lhs with [rhs_begin, rhs_end)
	bool strequalrange(const char_t* lhs, const char_t* rhs, size_t count)
	{
		for (size_t i = 0; i < count; ++i)
			if (lhs[i] != rhs[i])
				return false;
	
		return lhs[count] == 0;
	}
}

namespace
{
	static const size_t xml_memory_page_size = 32768;

	static const uintptr_t xml_memory_page_alignment = 32;
	static const uintptr_t xml_memory_page_pointer_mask = ~(xml_memory_page_alignment - 1);
	static const uintptr_t xml_memory_page_name_allocated_mask = 16;
	static const uintptr_t xml_memory_page_value_allocated_mask = 8;
	static const uintptr_t xml_memory_page_type_mask = 7;

	struct xml_allocator;

	struct xml_memory_page
	{
		static xml_memory_page* construct(void* memory)
		{
			if (!memory) return 0; //$ redundant, left for performance

			xml_memory_page* result = static_cast<xml_memory_page*>(memory);

			result->allocator = 0;
			result->memory = 0;
			result->prev = 0;
			result->next = 0;
			result->busy_size = 0;
			result->freed_size = 0;

			return result;
		}

		xml_allocator* allocator;

		void* memory;

		xml_memory_page* prev;
		xml_memory_page* next;

		size_t busy_size;
		size_t freed_size;

		char data[1];
	};

	struct xml_memory_string_header
	{
		uint16_t page_offset; // offset from page->data
		uint16_t full_size; // 0 if string occupies whole page
	};

	struct xml_allocator
	{
		xml_allocator(xml_memory_page* root): _root(root), _busy_size(root->busy_size)
		{
		}

		xml_memory_page* allocate_page(size_t data_size)
		{
			size_t size = offsetof(xml_memory_page, data) + data_size;

			// allocate block with some alignment, leaving memory for worst-case padding
			void* memory = global_allocate(size + xml_memory_page_alignment);
			if (!memory) return 0;

			// align upwards to page boundary
			void* page_memory = reinterpret_cast<void*>((reinterpret_cast<uintptr_t>(memory) + (xml_memory_page_alignment - 1)) & ~(xml_memory_page_alignment - 1));

			// prepare page structure
			xml_memory_page* page = xml_memory_page::construct(page_memory);

			page->memory = memory;
			page->allocator = _root->allocator;

			return page;
		}

		static void deallocate_page(xml_memory_page* page)
		{
			global_deallocate(page->memory);
		}

		void* allocate_memory_oob(size_t size, xml_memory_page*& out_page);

		void* allocate_memory(size_t size, xml_memory_page*& out_page)
		{
			if (_busy_size + size > xml_memory_page_size) return allocate_memory_oob(size, out_page);

			void* buf = _root->data + _busy_size;

			_busy_size += size;

			out_page = _root;

			return buf;
		}

		void deallocate_memory(void* ptr, size_t size, xml_memory_page* page)
		{
			if (page == _root) page->busy_size = _busy_size;

			assert(ptr >= page->data && ptr < page->data + page->busy_size);
			(void)!ptr;

			page->freed_size += size;
			assert(page->freed_size <= page->busy_size);

			if (page->freed_size == page->busy_size)
			{
				if (page->next == 0)
				{
					assert(_root == page);

					// top page freed, just reset sizes
					page->busy_size = page->freed_size = 0;
					_busy_size = 0;
				}
				else
				{
					assert(_root != page);
					assert(page->prev);

					// remove from the list
					page->prev->next = page->next;
					page->next->prev = page->prev;

					// deallocate
					deallocate_page(page);
				}
			}
		}

		char_t* allocate_string(size_t length)
		{
			// allocate memory for string and header block
			size_t size = sizeof(xml_memory_string_header) + length * sizeof(char_t);
			
			// round size up to pointer alignment boundary
			size_t full_size = (size + (sizeof(void*) - 1)) & ~(sizeof(void*) - 1);

			xml_memory_page* page;
			xml_memory_string_header* header = static_cast<xml_memory_string_header*>(allocate_memory(full_size, page));

			if (!header) return 0;

			// setup header
			ptrdiff_t page_offset = reinterpret_cast<char*>(header) - page->data;

			assert(page_offset >= 0 && page_offset < (1 << 16));
			header->page_offset = static_cast<uint16_t>(page_offset);

			// full_size == 0 for large strings that occupy the whole page
			assert(full_size < (1 << 16) || (page->busy_size == full_size && page_offset == 0));
			header->full_size = static_cast<uint16_t>(full_size < (1 << 16) ? full_size : 0);

			return reinterpret_cast<char_t*>(header + 1);
		}

		void deallocate_string(char_t* string)
		{
			// get header
			xml_memory_string_header* header = reinterpret_cast<xml_memory_string_header*>(string) - 1;

			// deallocate
			size_t page_offset = offsetof(xml_memory_page, data) + header->page_offset;
			xml_memory_page* page = reinterpret_cast<xml_memory_page*>(reinterpret_cast<char*>(header) - page_offset);

			// if full_size == 0 then this string occupies the whole page
			size_t full_size = header->full_size == 0 ? page->busy_size : header->full_size;

			deallocate_memory(header, full_size, page);
		}

		xml_memory_page* _root;
		size_t _busy_size;
	};

	PUGIXML_NO_INLINE void* xml_allocator::allocate_memory_oob(size_t size, xml_memory_page*& out_page)
	{
		const size_t large_allocation_threshold = xml_memory_page_size / 4;

		xml_memory_page* page = allocate_page(size <= large_allocation_threshold ? xml_memory_page_size : size);
		if (!page) return 0;

		if (size <= large_allocation_threshold)
		{
			_root->busy_size = _busy_size;

			// insert page at the end of linked list
			page->prev = _root;
			_root->next = page;
			_root = page;

			_busy_size = size;
		}
		else
		{
			// insert page before the end of linked list, so that it is deleted as soon as possible
			// the last page is not deleted even if it's empty (see deallocate_memory)
			assert(_root->prev);

			page->prev = _root->prev;
			page->next = _root;

			_root->prev->next = page;
			_root->prev = page;
		}

		// allocate inside page
		page->busy_size = size;

		out_page = page;
		return page->data;
	}
}

namespace pugi
{
	/// A 'name=value' XML attribute structure.
	struct xml_attribute_struct
	{
		/// Default ctor
		xml_attribute_struct(xml_memory_page* page): header(reinterpret_cast<uintptr_t>(page)), name(0), value(0), prev_attribute_c(0), next_attribute(0)
		{
		}

		uintptr_t header;

		char_t* name;	///< Pointer to attribute name.
		char_t*	value;	///< Pointer to attribute value.

		xml_attribute_struct* prev_attribute_c;	///< Previous attribute (cyclic list)
		xml_attribute_struct* next_attribute;	///< Next attribute
	};

	/// An XML document tree node.
	struct xml_node_struct
	{
		/// Default ctor
		/// \param type - node type
		xml_node_struct(xml_memory_page* page, xml_node_type type): header(reinterpret_cast<uintptr_t>(page) | (type - 1)), parent(0), name(0), value(0), first_child(0), prev_sibling_c(0), next_sibling(0), first_attribute(0)
		{
		}

		uintptr_t header;

		xml_node_struct*		parent;					///< Pointer to parent

		char_t*					name;					///< Pointer to element name.
		char_t*					value;					///< Pointer to any associated string data.

		xml_node_struct*		first_child;			///< First child
		
		xml_node_struct*		prev_sibling_c;			///< Left brother (cyclic list)
		xml_node_struct*		next_sibling;			///< Right brother
		
		xml_attribute_struct*	first_attribute;		///< First attribute
	};
}

namespace
{
	struct xml_document_struct: public xml_node_struct, public xml_allocator
	{
		xml_document_struct(xml_memory_page* page): xml_node_struct(page, node_document), xml_allocator(page), buffer(0)
		{
		}

		const char_t* buffer;
	};

	static inline xml_allocator& get_allocator(const xml_node_struct* node)
	{
		assert(node);

		return *reinterpret_cast<xml_memory_page*>(node->header & xml_memory_page_pointer_mask)->allocator;
	}
}

// Low-level DOM operations
namespace
{
	inline xml_attribute_struct* allocate_attribute(xml_allocator& alloc)
	{
		xml_memory_page* page;
		void* memory = alloc.allocate_memory(sizeof(xml_attribute_struct), page);

		return new (memory) xml_attribute_struct(page);
	}

	inline xml_node_struct* allocate_node(xml_allocator& alloc, xml_node_type type)
	{
		xml_memory_page* page;
		void* memory = alloc.allocate_memory(sizeof(xml_node_struct), page);

		return new (memory) xml_node_struct(page, type);
	}

	inline void destroy_attribute(xml_attribute_struct* a, xml_allocator& alloc)
	{
		uintptr_t header = a->header;

		if (header & xml_memory_page_name_allocated_mask) alloc.deallocate_string(a->name);
		if (header & xml_memory_page_value_allocated_mask) alloc.deallocate_string(a->value);

		alloc.deallocate_memory(a, sizeof(xml_attribute_struct), reinterpret_cast<xml_memory_page*>(header & xml_memory_page_pointer_mask));
	}

	inline void destroy_node(xml_node_struct* n, xml_allocator& alloc)
	{
		uintptr_t header = n->header;

		if (header & xml_memory_page_name_allocated_mask) alloc.deallocate_string(n->name);
		if (header & xml_memory_page_value_allocated_mask) alloc.deallocate_string(n->value);

		for (xml_attribute_struct* attr = n->first_attribute; attr; )
		{
			xml_attribute_struct* next = attr->next_attribute;

			destroy_attribute(attr, alloc);

			attr = next;
		}

		for (xml_node_struct* child = n->first_child; child; )
		{
			xml_node_struct* next = child->next_sibling;

			destroy_node(child, alloc);

			child = next;
		}

		alloc.deallocate_memory(n, sizeof(xml_node_struct), reinterpret_cast<xml_memory_page*>(header & xml_memory_page_pointer_mask));
	}

	PUGIXML_NO_INLINE xml_node_struct* append_node(xml_node_struct* node, xml_allocator& alloc, xml_node_type type = node_element)
	{
		xml_node_struct* child = allocate_node(alloc, type);
		if (!child) return 0;

		child->parent = node;

		xml_node_struct* first_child = node->first_child;
			
		if (first_child)
		{
			xml_node_struct* last_child = first_child->prev_sibling_c;

			last_child->next_sibling = child;
			child->prev_sibling_c = last_child;
			first_child->prev_sibling_c = child;
		}
		else
		{
			node->first_child = child;
			child->prev_sibling_c = child;
		}
			
		return child;
	}

	PUGIXML_NO_INLINE xml_attribute_struct* append_attribute_ll(xml_node_struct* node, xml_allocator& alloc)
	{
		xml_attribute_struct* a = allocate_attribute(alloc);
		if (!a) return 0;

		xml_attribute_struct* first_attribute = node->first_attribute;

		if (first_attribute)
		{
			xml_attribute_struct* last_attribute = first_attribute->prev_attribute_c;

			last_attribute->next_attribute = a;
			a->prev_attribute_c = last_attribute;
			first_attribute->prev_attribute_c = a;
		}
		else
		{
			node->first_attribute = a;
			a->prev_attribute_c = a;
		}
			
		return a;
	}
}

// Helper classes for code generation
namespace
{
	struct opt_false
	{
		enum { value = 0 };
	};

	struct opt_true
	{
		enum { value = 1 };
	};
}

// Unicode utilities
namespace
{
	inline uint16_t endian_swap(uint16_t value)
	{
		return static_cast<uint16_t>(((value & 0xff) << 8) | (value >> 8));
	}

	inline uint32_t endian_swap(uint32_t value)
	{
		return ((value & 0xff) << 24) | ((value & 0xff00) << 8) | ((value & 0xff0000) >> 8) | (value >> 24);
	}

	struct utf8_counter
	{
		typedef size_t value_type;

		static value_type low(value_type result, uint32_t ch)
		{
			// U+0000..U+007F
			if (ch < 0x80) return result + 1;
			// U+0080..U+07FF
			else if (ch < 0x800) return result + 2;
			// U+0800..U+FFFF
			else return result + 3;
		}

		static value_type high(value_type result, uint32_t)
		{
			// U+10000..U+10FFFF
			return result + 4;
		}
	};

	struct utf8_writer
	{
		typedef uint8_t* value_type;

		static value_type low(value_type result, uint32_t ch)
		{
			// U+0000..U+007F
			if (ch < 0x80)
			{
				*result = static_cast<uint8_t>(ch);
				return result + 1;
			}
			// U+0080..U+07FF
			else if (ch < 0x800)
			{
				result[0] = static_cast<uint8_t>(0xC0 | (ch >> 6));
				result[1] = static_cast<uint8_t>(0x80 | (ch & 0x3F));
				return result + 2;
			}
			// U+0800..U+FFFF
			else
			{
				result[0] = static_cast<uint8_t>(0xE0 | (ch >> 12));
				result[1] = static_cast<uint8_t>(0x80 | ((ch >> 6) & 0x3F));
				result[2] = static_cast<uint8_t>(0x80 | (ch & 0x3F));
				return result + 3;
			}
		}

		static value_type high(value_type result, uint32_t ch)
		{
			// U+10000..U+10FFFF
			result[0] = static_cast<uint8_t>(0xF0 | (ch >> 18));
			result[1] = static_cast<uint8_t>(0x80 | ((ch >> 12) & 0x3F));
			result[2] = static_cast<uint8_t>(0x80 | ((ch >> 6) & 0x3F));
			result[3] = static_cast<uint8_t>(0x80 | (ch & 0x3F));
			return result + 4;
		}

		static value_type any(value_type result, uint32_t ch)
		{
			return (ch < 0x10000) ? low(result, ch) : high(result, ch);
		}
	};

	struct utf16_counter
	{
		typedef size_t value_type;

		static value_type low(value_type result, uint32_t)
		{
			return result + 1;
		}

		static value_type high(value_type result, uint32_t)
		{
			return result + 2;
		}
	};

	struct utf16_writer
	{
		typedef uint16_t* value_type;

		static value_type low(value_type result, uint32_t ch)
		{
			*result = static_cast<uint16_t>(ch);

			return result + 1;
		}

		static value_type high(value_type result, uint32_t ch)
		{
			uint32_t msh = (uint32_t)(ch - 0x10000) >> 10;
			uint32_t lsh = (uint32_t)(ch - 0x10000) & 0x3ff;

			result[0] = static_cast<uint16_t>(0xD800 + msh);
			result[1] = static_cast<uint16_t>(0xDC00 + lsh);

			return result + 2;
		}

		static value_type any(value_type result, uint32_t ch)
		{
			return (ch < 0x10000) ? low(result, ch) : high(result, ch);
		}
	};

	struct utf32_counter
	{
		typedef size_t value_type;

		static value_type low(value_type result, uint32_t)
		{
			return result + 1;
		}

		static value_type high(value_type result, uint32_t)
		{
			return result + 1;
		}
	};

	struct utf32_writer
	{
		typedef uint32_t* value_type;

		static value_type low(value_type result, uint32_t ch)
		{
			*result = ch;

			return result + 1;
		}

		static value_type high(value_type result, uint32_t ch)
		{
			*result = ch;

			return result + 1;
		}

		static value_type any(value_type result, uint32_t ch)
		{
			*result = ch;

			return result + 1;
		}
	};

	template <size_t size> struct wchar_selector;

	template <> struct wchar_selector<2>
	{
		typedef uint16_t type;
		typedef utf16_counter counter;
		typedef utf16_writer writer;
	};

	template <> struct wchar_selector<4>
	{
		typedef uint32_t type;
		typedef utf32_counter counter;
		typedef utf32_writer writer;
	};

	typedef wchar_selector<sizeof(wchar_t)>::counter wchar_counter;
	typedef wchar_selector<sizeof(wchar_t)>::writer wchar_writer;

	template <typename Traits, typename opt_swap = opt_false> struct utf_decoder
	{
		static inline typename Traits::value_type decode_utf8_block(const uint8_t* data, size_t size, typename Traits::value_type result)
		{
			const uint8_t utf8_byte_mask = 0x3f;

			while (size)
			{
				uint8_t lead = *data;

				// 0xxxxxxx -> U+0000..U+007F
				if (lead < 0x80)
				{
					result = Traits::low(result, lead);
					data += 1;
					size -= 1;

					// process aligned single-byte (ascii) blocks
					if ((reinterpret_cast<uintptr_t>(data) & 3) == 0)
					{
						while (size >= 4 && (*reinterpret_cast<const uint32_t*>(data) & 0x80808080) == 0)
						{
							result = Traits::low(result, data[0]);
							result = Traits::low(result, data[1]);
							result = Traits::low(result, data[2]);
							result = Traits::low(result, data[3]);
							data += 4;
							size -= 4;
						}
					}
				}
				// 110xxxxx -> U+0080..U+07FF
				else if ((unsigned)(lead - 0xC0) < 0x20 && size >= 2 && (data[1] & 0xc0) == 0x80)
				{
					result = Traits::low(result, ((lead & ~0xC0) << 6) | (data[1] & utf8_byte_mask));
					data += 2;
					size -= 2;
				}
				// 1110xxxx -> U+0800-U+FFFF
				else if ((unsigned)(lead - 0xE0) < 0x10 && size >= 3 && (data[1] & 0xc0) == 0x80 && (data[2] & 0xc0) == 0x80)
				{
					result = Traits::low(result, ((lead & ~0xE0) << 12) | ((data[1] & utf8_byte_mask) << 6) | (data[2] & utf8_byte_mask));
					data += 3;
					size -= 3;
				}
				// 11110xxx -> U+10000..U+10FFFF
				else if ((unsigned)(lead - 0xF0) < 0x08 && size >= 4 && (data[1] & 0xc0) == 0x80 && (data[2] & 0xc0) == 0x80 && (data[3] & 0xc0) == 0x80)
				{
					result = Traits::high(result, ((lead & ~0xF0) << 18) | ((data[1] & utf8_byte_mask) << 12) | ((data[2] & utf8_byte_mask) << 6) | (data[3] & utf8_byte_mask));
					data += 4;
					size -= 4;
				}
				// 10xxxxxx or 11111xxx -> invalid
				else
				{
					data += 1;
					size -= 1;
				}
			}

			return result;
		}

		static inline typename Traits::value_type decode_utf16_block(const uint16_t* data, size_t size, typename Traits::value_type result)
		{
			const uint16_t* end = data + size;

			while (data < end)
			{
				uint16_t lead = opt_swap::value ? endian_swap(*data) : *data;

				// U+0000..U+D7FF
				if (lead < 0xD800)
				{
					result = Traits::low(result, lead);
					data += 1;
				}
				// U+E000..U+FFFF
				else if ((unsigned)(lead - 0xE000) < 0x2000)
				{
					result = Traits::low(result, lead);
					data += 1;
				}
				// surrogate pair lead
				else if ((unsigned)(lead - 0xD800) < 0x400 && data + 1 < end)
				{
					uint16_t next = opt_swap::value ? endian_swap(data[1]) : data[1];

					if ((unsigned)(next - 0xDC00) < 0x400)
					{
						result = Traits::high(result, 0x10000 + ((lead & 0x3ff) << 10) + (next & 0x3ff));
						data += 2;
					}
					else
					{
						data += 1;
					}
				}
				else
				{
					data += 1;
				}
			}

			return result;
		}

		static inline typename Traits::value_type decode_utf32_block(const uint32_t* data, size_t size, typename Traits::value_type result)
		{
			const uint32_t* end = data + size;

			while (data < end)
			{
				uint32_t lead = opt_swap::value ? endian_swap(*data) : *data;

				// U+0000..U+FFFF
				if (lead < 0x10000)
				{
					result = Traits::low(result, lead);
					data += 1;
				}
				// U+10000..U+10FFFF
				else
				{
					result = Traits::high(result, lead);
					data += 1;
				}
			}

			return result;
		}
	};

	template <typename T> inline void convert_utf_endian_swap(T* result, const T* data, size_t length)
	{
		for (size_t i = 0; i < length; ++i) result[i] = endian_swap(data[i]);
	}

	inline void convert_wchar_endian_swap(wchar_t* result, const wchar_t* data, size_t length)
	{
		for (size_t i = 0; i < length; ++i) result[i] = static_cast<wchar_t>(endian_swap(static_cast<wchar_selector<sizeof(wchar_t)>::type>(data[i])));
	}
}

namespace
{	
	enum chartype_t
	{
		ct_parse_pcdata = 1,	// \0, &, \r, <
		ct_parse_attr = 2,		// \0, &, \r, ', "
		ct_parse_attr_ws = 4,	// \0, &, \r, ', ", \n, tab
		ct_space = 8,			// \r, \n, space, tab
		ct_parse_cdata = 16,	// \0, ], >, \r
		ct_parse_comment = 32,	// \0, -, >, \r
		ct_symbol = 64,			// Any symbol > 127, a-z, A-Z, 0-9, _, :, -, .
		ct_start_symbol = 128	// Any symbol > 127, a-z, A-Z, _, :
	};

	const unsigned char chartype_table[256] =
	{
		55,  0,   0,   0,   0,   0,   0,   0,      0,   12,  12,  0,   0,   63,  0,   0,   // 0-15
		0,   0,   0,   0,   0,   0,   0,   0,      0,   0,   0,   0,   0,   0,   0,   0,   // 16-31
		8,   0,   6,   0,   0,   0,   7,   6,      0,   0,   0,   0,   0,   96,  64,  0,   // 32-47
		64,  64,  64,  64,  64,  64,  64,  64,     64,  64,  192, 0,   1,   0,   48,  0,   // 48-63
		0,   192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 192, 192, 192, 192, 192, // 64-79
		192, 192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 0,   0,   16,  0,   192, // 80-95
		0,   192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 192, 192, 192, 192, 192, // 96-111
		192, 192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 0, 0, 0, 0, 0,           // 112-127

		192, 192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 192, 192, 192, 192, 192, // 128+
		192, 192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 192, 192, 192, 192, 192,
		192, 192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 192, 192, 192, 192, 192,
		192, 192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 192, 192, 192, 192, 192,
		192, 192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 192, 192, 192, 192, 192,
		192, 192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 192, 192, 192, 192, 192,
		192, 192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 192, 192, 192, 192, 192,
		192, 192, 192, 192, 192, 192, 192, 192,    192, 192, 192, 192, 192, 192, 192, 192
	};

	enum chartypex_t
	{
		ctx_special_pcdata = 1,   // Any symbol >= 0 and < 32 (except \t, \r, \n), &, <, >
		ctx_special_attr = 2,     // Any symbol >= 0 and < 32 (except \t), &, <, >, "
		ctx_start_symbol = 4,	  // Any symbol > 127, a-z, A-Z, _
		ctx_digit = 8,			  // 0-9
		ctx_symbol = 16			  // Any symbol > 127, a-z, A-Z, 0-9, _, -, .
	};
	
	const unsigned char chartypex_table[256] =
	{
		3,  3,  3,  3,  3,  3,  3,  3,     3,  0,  2,  3,  3,  2,  3,  3,     // 0-15
		3,  3,  3,  3,  3,  3,  3,  3,     3,  3,  3,  3,  3,  3,  3,  3,     // 16-31
		0,  0,  2,  0,  0,  0,  3,  0,     0,  0,  0,  0,  0, 16, 16,  0,     // 32-47
		24, 24, 24, 24, 24, 24, 24, 24,    24, 24, 0,  0,  3,  0,  3,  0,     // 48-63

		0,  20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 20, 20, 20, 20, 20,    // 64-79
		20, 20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 0,  0,  0,  0,  20,    // 80-95
		0,  20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 20, 20, 20, 20, 20,    // 96-111
		20, 20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 0,  0,  0,  0,  0,     // 112-127

		20, 20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 20, 20, 20, 20, 20,    // 128+
		20, 20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 20, 20, 20, 20, 20,
		20, 20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 20, 20, 20, 20, 20,
		20, 20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 20, 20, 20, 20, 20,
		20, 20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 20, 20, 20, 20, 20,
		20, 20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 20, 20, 20, 20, 20,
		20, 20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 20, 20, 20, 20, 20,
		20, 20, 20, 20, 20, 20, 20, 20,    20, 20, 20, 20, 20, 20, 20, 20
	};
	
	#define IS_CHARTYPE_IMPL(c, ct, table) (table[static_cast<unsigned char>(c)] & (ct))

	#define IS_CHARTYPE(c, ct) IS_CHARTYPE_IMPL(c, ct, chartype_table)
	#define IS_CHARTYPEX(c, ct) IS_CHARTYPE_IMPL(c, ct, chartypex_table)

	bool is_little_endian()
	{
		unsigned int ui = 1;

		return *reinterpret_cast<unsigned char*>(&ui) == 1;
	}

	xml_encoding get_wchar_encoding()
	{
		STATIC_ASSERT(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4);

		if (sizeof(wchar_t) == 2)
			return is_little_endian() ? encoding_utf16_le : encoding_utf16_be;
		else 
			return is_little_endian() ? encoding_utf32_le : encoding_utf32_be;
	}

	xml_encoding guess_buffer_encoding(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3)
	{
		// look for BOM in first few bytes
		if (d0 == 0 && d1 == 0 && d2 == 0xfe && d3 == 0xff) return encoding_utf32_be;
		if (d0 == 0xff && d1 == 0xfe && d2 == 0 && d3 == 0) return encoding_utf32_le;
		if (d0 == 0xfe && d1 == 0xff) return encoding_utf16_be;
		if (d0 == 0xff && d1 == 0xfe) return encoding_utf16_le;
		if (d0 == 0xef && d1 == 0xbb && d2 == 0xbf) return encoding_utf8;

		// look for <, <? or <?xm in various encodings
		if (d0 == 0 && d1 == 0 && d2 == 0 && d3 == 0x3c) return encoding_utf32_be;
		if (d0 == 0x3c && d1 == 0 && d2 == 0 && d3 == 0) return encoding_utf32_le;
		if (d0 == 0 && d1 == 0x3c && d2 == 0 && d3 == 0x3f) return encoding_utf16_be;
		if (d0 == 0x3c && d1 == 0 && d2 == 0x3f && d3 == 0) return encoding_utf16_le;
		if (d0 == 0x3c && d1 == 0x3f && d2 == 0x78 && d3 == 0x6d) return encoding_utf8;

		// look for utf16 < followed by node name (this may fail, but is better than utf8 since it's zero terminated so early)
		if (d0 == 0 && d1 == 0x3c) return encoding_utf16_be;
		if (d0 == 0x3c && d1 == 0) return encoding_utf16_le;

		// no known BOM detected, assume utf8
		return encoding_utf8;
	}

	xml_encoding get_buffer_encoding(xml_encoding encoding, const void* contents, size_t size)
	{
		// replace wchar encoding with utf implementation
		if (encoding == encoding_wchar) return get_wchar_encoding();

		// replace utf16 encoding with utf16 with specific endianness
		if (encoding == encoding_utf16) return is_little_endian() ? encoding_utf16_le : encoding_utf16_be;

		// replace utf32 encoding with utf32 with specific endianness
		if (encoding == encoding_utf32) return is_little_endian() ? encoding_utf32_le : encoding_utf32_be;

		// only do autodetection if no explicit encoding is requested
		if (encoding != encoding_auto) return encoding;

		// skip encoding autodetection if input buffer is too small
		if (size < 4) return encoding_utf8;

		// try to guess encoding (based on XML specification, Appendix F.1)
		const uint8_t* data = static_cast<const uint8_t*>(contents);

		DMC_VOLATILE uint8_t d0 = data[0], d1 = data[1], d2 = data[2], d3 = data[3];

		return guess_buffer_encoding(d0, d1, d2, d3);
	}

	bool get_mutable_buffer(char_t*& out_buffer, size_t& out_length, const void* contents, size_t size, bool is_mutable)
	{
		if (is_mutable)
		{
			out_buffer = static_cast<char_t*>(const_cast<void*>(contents));
		}
		else
		{
			void* buffer = global_allocate(size > 0 ? size : 1);
			if (!buffer) return false;

			memcpy(buffer, contents, size);

			out_buffer = static_cast<char_t*>(buffer);
		}

		out_length = size / sizeof(char_t);

		return true;
	}

	template <typename opt_swap> bool convert_buffer_utf16(char_t*& out_buffer, size_t& out_length, const void* contents, size_t size, opt_swap)
	{
		const uint16_t* data = static_cast<const uint16_t*>(contents);
		size_t length = size / sizeof(uint16_t);

		// first pass: get length in utf8 units
		out_length = utf_decoder<utf8_counter, opt_swap>::decode_utf16_block(data, length, 0);

		// allocate buffer of suitable length
		out_buffer = static_cast<char_t*>(global_allocate((out_length > 0 ? out_length : 1) * sizeof(char_t)));
		if (!out_buffer) return false;

		// second pass: convert utf16 input to utf8
		uint8_t* out_begin = reinterpret_cast<uint8_t*>(out_buffer);
		uint8_t* out_end = utf_decoder<utf8_writer, opt_swap>::decode_utf16_block(data, length, out_begin);

		assert(out_end == out_begin + out_length);
		(void)!out_end;

		return true;
	}

	template <typename opt_swap> bool convert_buffer_utf32(char_t*& out_buffer, size_t& out_length, const void* contents, size_t size, opt_swap)
	{
		const uint32_t* data = static_cast<const uint32_t*>(contents);
		size_t length = size / sizeof(uint32_t);

		// first pass: get length in utf8 units
		out_length = utf_decoder<utf8_counter, opt_swap>::decode_utf32_block(data, length, 0);

		// allocate buffer of suitable length
		out_buffer = static_cast<char_t*>(global_allocate((out_length > 0 ? out_length : 1) * sizeof(char_t)));
		if (!out_buffer) return false;

		// second pass: convert utf32 input to utf8
		uint8_t* out_begin = reinterpret_cast<uint8_t*>(out_buffer);
		uint8_t* out_end = utf_decoder<utf8_writer, opt_swap>::decode_utf32_block(data, length, out_begin);

		assert(out_end == out_begin + out_length);
		(void)!out_end;

		return true;
	}

	bool convert_buffer(char_t*& out_buffer, size_t& out_length, xml_encoding encoding, const void* contents, size_t size, bool is_mutable)
	{
		// fast path: no conversion required
		if (encoding == encoding_utf8) return get_mutable_buffer(out_buffer, out_length, contents, size, is_mutable);

		// source encoding is utf16
		if (encoding == encoding_utf16_be || encoding == encoding_utf16_le)
		{
			xml_encoding native_encoding = is_little_endian() ? encoding_utf16_le : encoding_utf16_be;

			return (native_encoding == encoding) ?
				convert_buffer_utf16(out_buffer, out_length, contents, size, opt_false()) :
				convert_buffer_utf16(out_buffer, out_length, contents, size, opt_true());
		}

		// source encoding is utf32
		if (encoding == encoding_utf32_be || encoding == encoding_utf32_le)
		{
			xml_encoding native_encoding = is_little_endian() ? encoding_utf32_le : encoding_utf32_be;

			return (native_encoding == encoding) ?
				convert_buffer_utf32(out_buffer, out_length, contents, size, opt_false()) :
				convert_buffer_utf32(out_buffer, out_length, contents, size, opt_true());
		}

		assert(!"Invalid encoding");
		return false;
	}

	size_t as_utf8_begin(const wchar_t* str, size_t length)
	{
		STATIC_ASSERT(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4);

		// get length in utf8 characters
		return sizeof(wchar_t) == 2 ?
			utf_decoder<utf8_counter>::decode_utf16_block(reinterpret_cast<const uint16_t*>(str), length, 0) :
			utf_decoder<utf8_counter>::decode_utf32_block(reinterpret_cast<const uint32_t*>(str), length, 0);
    }

    void as_utf8_end(char* buffer, size_t size, const wchar_t* str, size_t length)
    {
		STATIC_ASSERT(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4);

        // convert to utf8
        uint8_t* begin = reinterpret_cast<uint8_t*>(buffer);
        uint8_t* end = sizeof(wchar_t) == 2 ?
            utf_decoder<utf8_writer>::decode_utf16_block(reinterpret_cast<const uint16_t*>(str), length, begin) :
            utf_decoder<utf8_writer>::decode_utf32_block(reinterpret_cast<const uint32_t*>(str), length, begin);
    
        assert(begin + size == end);
        (void)!end;

		// zero-terminate
		buffer[size] = 0;
	}

	inline bool strcpy_insitu_allow(size_t length, uintptr_t allocated, char_t* target)
	{
		assert(target);
		size_t target_length = strlength(target);

		// always reuse document buffer memory if possible
		if (!allocated) return target_length >= length;

		// reuse heap memory if waste is not too great
		const size_t reuse_threshold = 32;

		return target_length >= length && (target_length < reuse_threshold || target_length - length < target_length / 2);
	}

	bool strcpy_insitu(char_t*& dest, uintptr_t& header, uintptr_t header_mask, const char_t* source)
	{
		size_t source_length = strlength(source);

		if (source_length == 0)
		{
			// empty string and null pointer are equivalent, so just deallocate old memory
			xml_allocator* alloc = reinterpret_cast<xml_memory_page*>(header & xml_memory_page_pointer_mask)->allocator;

			if (header & header_mask) alloc->deallocate_string(dest);
			
			// mark the string as not allocated
			dest = 0;
			header &= ~header_mask;

			return true;
		}
		else if (dest && strcpy_insitu_allow(source_length, header & header_mask, dest))
		{
			// we can reuse old buffer, so just copy the new data (including zero terminator)
			memcpy(dest, source, (source_length + 1) * sizeof(char_t));
			
			return true;
		}
		else
		{
			xml_allocator* alloc = reinterpret_cast<xml_memory_page*>(header & xml_memory_page_pointer_mask)->allocator;

			// allocate new buffer
			char_t* buf = alloc->allocate_string(source_length + 1);
			if (!buf) return false;

			// copy the string (including zero terminator)
			memcpy(buf, source, (source_length + 1) * sizeof(char_t));

			// deallocate old buffer (*after* the above to protect against overlapping memory and/or allocation failures)
			if (header & header_mask) alloc->deallocate_string(dest);
			
			// the string is now allocated, so set the flag
			dest = buf;
			header |= header_mask;

			return true;
		}
	}

	struct gap
	{
		char_t* end;
		size_t size;
			
		gap(): end(0), size(0)
		{
		}
			
		// Push new gap, move s count bytes further (skipping the gap).
		// Collapse previous gap.
		void push(char_t*& s, size_t count)
		{
			if (end) // there was a gap already; collapse it
			{
				// Move [old_gap_end, new_gap_start) to [old_gap_start, ...)
				assert(s >= end);
				memmove(end - size, end, reinterpret_cast<char*>(s) - reinterpret_cast<char*>(end));
			}
				
			s += count; // end of current gap
				
			// "merge" two gaps
			end = s;
			size += count;
		}
			
		// Collapse all gaps, return past-the-end pointer
		char_t* flush(char_t* s)
		{
			if (end)
			{
				// Move [old_gap_end, current_pos) to [old_gap_start, ...)
				assert(s >= end);
				memmove(end - size, end, reinterpret_cast<char*>(s) - reinterpret_cast<char*>(end));

				return s - size;
			}
			else return s;
		}
	};
	
	char_t* strconv_escape(char_t* s, gap& g)
	{
		char_t* stre = s + 1;

		switch (*stre)
		{
			case '#':	// &#...
			{
				unsigned int ucsc = 0;

				if (stre[1] == 'x') // &#x... (hex code)
				{
					stre += 2;

					char_t ch = *stre;

					if (ch == ';') return stre;

					for (;;)
					{
						if (static_cast<unsigned int>(ch - '0') <= 9)
							ucsc = 16 * ucsc + (ch - '0');
						else if (static_cast<unsigned int>((ch | ' ') - 'a') <= 5)
							ucsc = 16 * ucsc + ((ch | ' ') - 'a' + 10);
						else if (ch == ';')
							break;
						else // cancel
							return stre;

						ch = *++stre;
					}
					
					++stre;
				}
				else	// &#... (dec code)
				{
					char_t ch = *++stre;

					if (ch == ';') return stre;

					for (;;)
					{
						if (static_cast<unsigned int>(ch - '0') <= 9)
							ucsc = 10 * ucsc + (ch - '0');
						else if (ch == ';')
							break;
						else // cancel
							return stre;

						ch = *++stre;
					}
					
					++stre;
				}

    			s = reinterpret_cast<char_t*>(utf8_writer::any(reinterpret_cast<uint8_t*>(s), ucsc));

				g.push(s, stre - s);
				return stre;
			}
			case 'a':	// &a
			{
				++stre;

				if (*stre == 'm') // &am
				{
					if (*++stre == 'p' && *++stre == ';') // &amp;
					{
						*s++ = '&';
						++stre;
							
						g.push(s, stre - s);
						return stre;
					}
				}
				else if (*stre == 'p') // &ap
				{
					if (*++stre == 'o' && *++stre == 's' && *++stre == ';') // &apos;
					{
						*s++ = '\'';
						++stre;

						g.push(s, stre - s);
						return stre;
					}
				}
				break;
			}
			case 'g': // &g
			{
				if (*++stre == 't' && *++stre == ';') // &gt;
				{
					*s++ = '>';
					++stre;
					
					g.push(s, stre - s);
					return stre;
				}
				break;
			}
			case 'l': // &l
			{
				if (*++stre == 't' && *++stre == ';') // &lt;
				{
					*s++ = '<';
					++stre;
						
					g.push(s, stre - s);
					return stre;
				}
				break;
			}
			case 'q': // &q
			{
				if (*++stre == 'u' && *++stre == 'o' && *++stre == 't' && *++stre == ';') // &quot;
				{
					*s++ = '"';
					++stre;
					
					g.push(s, stre - s);
					return stre;
				}
				break;
			}
		}
		
		return stre;
	}

	// Utility macro for last character handling
	#define ENDSWITH(c, e) ((c) == (e) || ((c) == 0 && endch == (e)))

	char_t* strconv_comment(char_t* s, char_t endch)
	{
		gap g;
		
		while (true)
		{
			while (!IS_CHARTYPE(*s, ct_parse_comment)) ++s;
		
			if (*s == '\r') // Either a single 0x0d or 0x0d 0x0a pair
			{
				*s++ = '\n'; // replace first one with 0x0a
				
				if (*s == '\n') g.push(s, 1);
			}
			else if (s[0] == '-' && s[1] == '-' && ENDSWITH(s[2], '>')) // comment ends here
			{
				*g.flush(s) = 0;
				
				return s + (s[2] == '>' ? 3 : 2);
			}
			else if (*s == 0)
			{
				return 0;
			}
			else ++s;
		}
	}

	char_t* strconv_cdata(char_t* s, char_t endch)
	{
		gap g;
			
		while (true)
		{
			while (!IS_CHARTYPE(*s, ct_parse_cdata)) ++s;
			
			if (*s == '\r') // Either a single 0x0d or 0x0d 0x0a pair
			{
				*s++ = '\n'; // replace first one with 0x0a
				
				if (*s == '\n') g.push(s, 1);
			}
			else if (s[0] == ']' && s[1] == ']' && ENDSWITH(s[2], '>')) // CDATA ends here
			{
				*g.flush(s) = 0;
				
				return s + 1;
			}
			else if (*s == 0)
			{
				return 0;
			}
			else ++s;
		}
	}
	
	typedef char_t* (*strconv_pcdata_t)(char_t*);
		
	template <typename opt_eol, typename opt_escape> struct strconv_pcdata_impl
	{
		static char_t* parse(char_t* s)
		{
			gap g;
			
			while (true)
			{
				while (!IS_CHARTYPE(*s, ct_parse_pcdata)) ++s;
					
				if (*s == '<') // PCDATA ends here
				{
					*g.flush(s) = 0;
					
					return s + 1;
				}
				else if (opt_eol::value && *s == '\r') // Either a single 0x0d or 0x0d 0x0a pair
				{
					*s++ = '\n'; // replace first one with 0x0a
					
					if (*s == '\n') g.push(s, 1);
				}
				else if (opt_escape::value && *s == '&')
				{
					s = strconv_escape(s, g);
				}
				else if (*s == 0)
				{
					return s;
				}
				else ++s;
			}
		}
	};
	
	strconv_pcdata_t get_strconv_pcdata(unsigned int optmask)
	{
		STATIC_ASSERT(parse_escapes == 0x10 && parse_eol == 0x20);

		switch ((optmask >> 4) & 3) // get bitmask for flags (eol escapes)
		{
		case 0: return strconv_pcdata_impl<opt_false, opt_false>::parse;
		case 1: return strconv_pcdata_impl<opt_false, opt_true>::parse;
		case 2: return strconv_pcdata_impl<opt_true, opt_false>::parse;
		case 3: return strconv_pcdata_impl<opt_true, opt_true>::parse;
		default: return 0; // should not get here
		}
	}

	typedef char_t* (*strconv_attribute_t)(char_t*, char_t);
	
	template <typename opt_escape> struct strconv_attribute_impl
	{
		static char_t* parse_wnorm(char_t* s, char_t end_quote)
		{
			gap g;

			// trim leading whitespaces
			if (IS_CHARTYPE(*s, ct_space))
			{
				char_t* str = s;
				
				do ++str;
				while (IS_CHARTYPE(*str, ct_space));
				
				g.push(s, str - s);
			}

			while (true)
			{
				while (!IS_CHARTYPE(*s, ct_parse_attr_ws | ct_space)) ++s;
				
				if (*s == end_quote)
				{
					char_t* str = g.flush(s);
					
					do *str-- = 0;
					while (IS_CHARTYPE(*str, ct_space));
				
					return s + 1;
				}
				else if (IS_CHARTYPE(*s, ct_space))
				{
					*s++ = ' ';
		
					if (IS_CHARTYPE(*s, ct_space))
					{
						char_t* str = s + 1;
						while (IS_CHARTYPE(*str, ct_space)) ++str;
						
						g.push(s, str - s);
					}
				}
				else if (opt_escape::value && *s == '&')
				{
					s = strconv_escape(s, g);
				}
				else if (!*s)
				{
					return 0;
				}
				else ++s;
			}
		}

		static char_t* parse_wconv(char_t* s, char_t end_quote)
		{
			gap g;

			while (true)
			{
				while (!IS_CHARTYPE(*s, ct_parse_attr_ws)) ++s;
				
				if (*s == end_quote)
				{
					*g.flush(s) = 0;
				
					return s + 1;
				}
				else if (IS_CHARTYPE(*s, ct_space))
				{
					if (*s == '\r')
					{
						*s++ = ' ';
				
						if (*s == '\n') g.push(s, 1);
					}
					else *s++ = ' ';
				}
				else if (opt_escape::value && *s == '&')
				{
					s = strconv_escape(s, g);
				}
				else if (!*s)
				{
					return 0;
				}
				else ++s;
			}
		}

		static char_t* parse_eol(char_t* s, char_t end_quote)
		{
			gap g;

			while (true)
			{
				while (!IS_CHARTYPE(*s, ct_parse_attr)) ++s;
				
				if (*s == end_quote)
				{
					*g.flush(s) = 0;
				
					return s + 1;
				}
				else if (*s == '\r')
				{
					*s++ = '\n';
					
					if (*s == '\n') g.push(s, 1);
				}
				else if (opt_escape::value && *s == '&')
				{
					s = strconv_escape(s, g);
				}
				else if (!*s)
				{
					return 0;
				}
				else ++s;
			}
		}

		static char_t* parse_simple(char_t* s, char_t end_quote)
		{
			gap g;

			while (true)
			{
				while (!IS_CHARTYPE(*s, ct_parse_attr)) ++s;
				
				if (*s == end_quote)
				{
					*g.flush(s) = 0;
				
					return s + 1;
				}
				else if (opt_escape::value && *s == '&')
				{
					s = strconv_escape(s, g);
				}
				else if (!*s)
				{
					return 0;
				}
				else ++s;
			}
		}
	};

	strconv_attribute_t get_strconv_attribute(unsigned int optmask)
	{
		STATIC_ASSERT(parse_escapes == 0x10 && parse_eol == 0x20 && parse_wconv_attribute == 0x40 && parse_wnorm_attribute == 0x80);
		
		switch ((optmask >> 4) & 15) // get bitmask for flags (wconv wnorm eol escapes)
		{
		case 0:  return strconv_attribute_impl<opt_false>::parse_simple;
		case 1:  return strconv_attribute_impl<opt_true>::parse_simple;
		case 2:  return strconv_attribute_impl<opt_false>::parse_eol;
		case 3:  return strconv_attribute_impl<opt_true>::parse_eol;
		case 4:  return strconv_attribute_impl<opt_false>::parse_wconv;
		case 5:  return strconv_attribute_impl<opt_true>::parse_wconv;
		case 6:  return strconv_attribute_impl<opt_false>::parse_wconv;
		case 7:  return strconv_attribute_impl<opt_true>::parse_wconv;
		case 8:  return strconv_attribute_impl<opt_false>::parse_wnorm;
		case 9:  return strconv_attribute_impl<opt_true>::parse_wnorm;
		case 10: return strconv_attribute_impl<opt_false>::parse_wnorm;
		case 11: return strconv_attribute_impl<opt_true>::parse_wnorm;
		case 12: return strconv_attribute_impl<opt_false>::parse_wnorm;
		case 13: return strconv_attribute_impl<opt_true>::parse_wnorm;
		case 14: return strconv_attribute_impl<opt_false>::parse_wnorm;
		case 15: return strconv_attribute_impl<opt_true>::parse_wnorm;
		default: return 0; // should not get here
		}
	}

	inline xml_parse_result make_parse_result(xml_parse_status status, ptrdiff_t offset = 0)
	{
		xml_parse_result result;
		result.status = status;
		result.offset = offset;

		return result;
	}

	struct xml_parser
	{
		xml_allocator alloc;
		char_t* error_offset;
		jmp_buf error_handler;
		
		// Parser utilities.
		#define SKIPWS()			{ while (IS_CHARTYPE(*s, ct_space)) ++s; }
		#define OPTSET(OPT)			( optmsk & OPT )
		#define PUSHNODE(TYPE)		{ cursor = append_node(cursor, alloc, TYPE); if (!cursor) THROW_ERROR(status_out_of_memory, s); }
		#define POPNODE()			{ cursor = cursor->parent; }
		#define SCANFOR(X)			{ while (*s != 0 && !(X)) ++s; }
		#define SCANWHILE(X)		{ while ((X)) ++s; }
		#define ENDSEG()			{ ch = *s; *s = 0; ++s; }
		#define THROW_ERROR(err, m)	error_offset = m, longjmp(error_handler, err)
		#define CHECK_ERROR(err, m)	{ if (*s == 0) THROW_ERROR(err, m); }
		
		xml_parser(const xml_allocator& alloc): alloc(alloc), error_offset(0)
		{
		}

		// DOCTYPE consists of nested sections of the following possible types:
		// <!-- ... -->, <? ... ?>, "...", '...'
		// <![...]]>
		// <!...>
		// First group can not contain nested groups
		// Second group can contain nested groups of the same type
		// Third group can contain all other groups
		char_t* parse_doctype_primitive(char_t* s)
		{
			if (*s == '"' || *s == '\'')
			{
				// quoted string
				char_t ch = *s++;
				SCANFOR(*s == ch);
				if (!*s) THROW_ERROR(status_bad_doctype, s);

				s++;
			}
			else if (s[0] == '<' && s[1] == '?')
			{
				// <? ... ?>
				s += 2;
				SCANFOR(s[0] == '?' && s[1] == '>'); // no need for ENDSWITH because ?> can't terminate proper doctype
				if (!*s) THROW_ERROR(status_bad_doctype, s);

				s += 2;
			}
			else if (s[0] == '<' && s[1] == '!' && s[2] == '-' && s[3] == '-')
			{
				s += 4;
				SCANFOR(s[0] == '-' && s[1] == '-' && s[2] == '>'); // no need for ENDSWITH because --> can't terminate proper doctype
				if (!*s) THROW_ERROR(status_bad_doctype, s);

				s += 4;
			}
			else THROW_ERROR(status_bad_doctype, s);

			return s;
		}

		char_t* parse_doctype_ignore(char_t* s)
		{
			assert(s[0] == '<' && s[1] == '!' && s[2] == '[');
			s++;

			while (*s)
			{
				if (s[0] == '<' && s[1] == '!' && s[2] == '[')
				{
					// nested ignore section
					s = parse_doctype_ignore(s);
				}
				else if (s[0] == ']' && s[1] == ']' && s[2] == '>')
				{
					// ignore section end
					s += 3;

					return s;
				}
				else s++;
			}

			THROW_ERROR(status_bad_doctype, s);

			return s;
		}

		char_t* parse_doctype_group(char_t* s, char_t endch, bool toplevel)
		{
			assert(s[0] == '<' && s[1] == '!');
			s++;

			while (*s)
			{
				if (s[0] == '<' && s[1] == '!' && s[2] != '-')
				{
					if (s[2] == '[')
					{
						// ignore
						s = parse_doctype_ignore(s);
					}
					else
					{
						// some control group
						s = parse_doctype_group(s, endch, false);
					}
				}
				else if (s[0] == '<' || s[0] == '"' || s[0] == '\'')
				{
					// unknown tag (forbidden), or some primitive group
					s = parse_doctype_primitive(s);
				}
				else if (*s == '>')
				{
					s++;

					return s;
				}
				else s++;
			}

			if (!toplevel || endch != '>') THROW_ERROR(status_bad_doctype, s);

			return s;
		}

		char_t* parse_exclamation(char_t* s, xml_node_struct* cursor, unsigned int optmsk, char_t endch)
		{
			// parse node contents, starting with exclamation mark
			++s;

			if (*s == '-') // '<!-...'
			{
				++s;

				if (*s == '-') // '<!--...'
				{
					++s;

					if (OPTSET(parse_comments))
					{
						PUSHNODE(node_comment); // Append a new node on the tree.
						cursor->value = s; // Save the offset.
					}

					if (OPTSET(parse_eol) && OPTSET(parse_comments))
					{
						s = strconv_comment(s, endch);

						if (!s) THROW_ERROR(status_bad_comment, cursor->value);
					}
					else
					{
						// Scan for terminating '-->'.
						SCANFOR(s[0] == '-' && s[1] == '-' && ENDSWITH(s[2], '>'));
						CHECK_ERROR(status_bad_comment, s);

						if (OPTSET(parse_comments))
							*s = 0; // Zero-terminate this segment at the first terminating '-'.

						s += (s[2] == '>' ? 3 : 2); // Step over the '\0->'.
					}
				}
				else THROW_ERROR(status_bad_comment, s);
			}
			else if (*s == '[')
			{
				// '<![CDATA[...'
				if (*++s=='C' && *++s=='D' && *++s=='A' && *++s=='T' && *++s=='A' && *++s == '[')
				{
					++s;

					if (OPTSET(parse_cdata))
					{
						PUSHNODE(node_cdata); // Append a new node on the tree.
						cursor->value = s; // Save the offset.

						if (OPTSET(parse_eol))
						{
							s = strconv_cdata(s, endch);

							if (!s) THROW_ERROR(status_bad_cdata, cursor->value);
						}
						else
						{
							// Scan for terminating ']]>'.
							SCANFOR(s[0] == ']' && s[1] == ']' && ENDSWITH(s[2], '>'));
							CHECK_ERROR(status_bad_cdata, s);

							*s++ = 0; // Zero-terminate this segment.
						}
					}
					else // Flagged for discard, but we still have to scan for the terminator.
					{
						// Scan for terminating ']]>'.
						SCANFOR(s[0] == ']' && s[1] == ']' && ENDSWITH(s[2], '>'));
						CHECK_ERROR(status_bad_cdata, s);

						++s;
					}

					s += (s[1] == '>' ? 2 : 1); // Step over the last ']>'.
				}
				else THROW_ERROR(status_bad_cdata, s);
			}
			else if (s[0] == 'D' && s[1] == 'O' && s[2] == 'C' && s[3] == 'T' && s[4] == 'Y' && s[5] == 'P' && ENDSWITH(s[6], 'E'))
			{
				s -= 2;

                if (cursor->parent) THROW_ERROR(status_bad_doctype, s);

                char_t* mark = s + 9;

				s = parse_doctype_group(s, endch, true);

                if (OPTSET(parse_doctype))
                {
                    while (IS_CHARTYPE(*mark, ct_space)) ++mark;

                    PUSHNODE(node_doctype);

                    cursor->value = mark;

                    assert((s[0] == 0 && endch == '>') || s[-1] == '>');
                    s[*s == 0 ? 0 : -1] = 0;

                    POPNODE();
                }
			}
			else if (*s == 0 && endch == '-') THROW_ERROR(status_bad_comment, s);
			else if (*s == 0 && endch == '[') THROW_ERROR(status_bad_cdata, s);
			else THROW_ERROR(status_unrecognized_tag, s);

			return s;
		}

		char_t* parse_question(char_t* s, xml_node_struct*& ref_cursor, unsigned int optmsk, char_t endch)
		{
			// load into registers
			xml_node_struct* cursor = ref_cursor;
			char_t ch = 0;

			// parse node contents, starting with question mark
			++s;

			// read PI target
			char_t* target = s;

			if (!IS_CHARTYPE(*s, ct_start_symbol)) THROW_ERROR(status_bad_pi, s);

			SCANWHILE(IS_CHARTYPE(*s, ct_symbol));
			CHECK_ERROR(status_bad_pi, s);

			// determine node type; stricmp / strcasecmp is not portable
			bool declaration = (target[0] | ' ') == 'x' && (target[1] | ' ') == 'm' && (target[2] | ' ') == 'l' && target + 3 == s;

			if (declaration ? OPTSET(parse_declaration) : OPTSET(parse_pi))
			{
				if (declaration)
				{
					// disallow non top-level declarations
					if (cursor->parent) THROW_ERROR(status_bad_pi, s);

					PUSHNODE(node_declaration);
				}
				else
				{
					PUSHNODE(node_pi);
				}

				cursor->name = target;

				ENDSEG();

				// parse value/attributes
				if (ch == '?')
				{
					// empty node
					if (!ENDSWITH(*s, '>')) THROW_ERROR(status_bad_pi, s);
					s += (*s == '>');

					POPNODE();
				}
				else if (IS_CHARTYPE(ch, ct_space))
				{
					SKIPWS();

					// scan for tag end
					char_t* value = s;

					SCANFOR(s[0] == '?' && ENDSWITH(s[1], '>'));
					CHECK_ERROR(status_bad_pi, s);

					if (declaration)
					{
						// replace ending ? with / so that 'element' terminates properly
						*s = '/';

						// we exit from this function with cursor at node_declaration, which is a signal to parse() to go to LOC_ATTRIBUTES
						s = value;
					}
					else
					{
						// store value and step over >
						cursor->value = value;
						POPNODE();

						ENDSEG();

						s += (*s == '>');
					}
				}
				else THROW_ERROR(status_bad_pi, s);
			}
			else
			{
				// scan for tag end
				SCANFOR(s[0] == '?' && ENDSWITH(s[1], '>'));
				CHECK_ERROR(status_bad_pi, s);

				s += (s[1] == '>' ? 2 : 1);
			}

			// store from registers
			ref_cursor = cursor;

			return s;
		}

		void parse(char_t* s, xml_node_struct* xmldoc, unsigned int optmsk, char_t endch)
		{
			strconv_attribute_t strconv_attribute = get_strconv_attribute(optmsk);
			strconv_pcdata_t strconv_pcdata = get_strconv_pcdata(optmsk);
			
			char_t ch = 0;
			xml_node_struct* cursor = xmldoc;
			char_t* mark = s;

			while (*s != 0)
			{
				if (*s == '<')
				{
					++s;

				LOC_TAG:
					if (IS_CHARTYPE(*s, ct_start_symbol)) // '<#...'
					{
						PUSHNODE(node_element); // Append a new node to the tree.

						cursor->name = s;

						SCANWHILE(IS_CHARTYPE(*s, ct_symbol)); // Scan for a terminator.
						ENDSEG(); // Save char in 'ch', terminate & step over.

						if (ch == '>')
						{
							// end of tag
						}
						else if (IS_CHARTYPE(ch, ct_space))
						{
						LOC_ATTRIBUTES:
						    while (true)
						    {
								SKIPWS(); // Eat any whitespace.
						
								if (IS_CHARTYPE(*s, ct_start_symbol)) // <... #...
								{
									xml_attribute_struct* a = append_attribute_ll(cursor, alloc); // Make space for this attribute.
									if (!a) THROW_ERROR(status_out_of_memory, s);

									a->name = s; // Save the offset.

									SCANWHILE(IS_CHARTYPE(*s, ct_symbol)); // Scan for a terminator.
									CHECK_ERROR(status_bad_attribute, s); //$ redundant, left for performance

									ENDSEG(); // Save char in 'ch', terminate & step over.
									CHECK_ERROR(status_bad_attribute, s); //$ redundant, left for performance

									if (IS_CHARTYPE(ch, ct_space))
									{
										SKIPWS(); // Eat any whitespace.
										CHECK_ERROR(status_bad_attribute, s); //$ redundant, left for performance

										ch = *s;
										++s;
									}
									
									if (ch == '=') // '<... #=...'
									{
										SKIPWS(); // Eat any whitespace.

										if (*s == '"' || *s == '\'') // '<... #="...'
										{
											ch = *s; // Save quote char to avoid breaking on "''" -or- '""'.
											++s; // Step over the quote.
											a->value = s; // Save the offset.

											s = strconv_attribute(s, ch);
										
											if (!s) THROW_ERROR(status_bad_attribute, a->value);

											// After this line the loop continues from the start;
											// Whitespaces, / and > are ok, symbols and EOF are wrong,
											// everything else will be detected
											if (IS_CHARTYPE(*s, ct_start_symbol)) THROW_ERROR(status_bad_attribute, s);
										}
										else THROW_ERROR(status_bad_attribute, s);
									}
									else THROW_ERROR(status_bad_attribute, s);
								}
								else if (*s == '/')
								{
									++s;
									
									if (*s == '>')
									{
										POPNODE();
										s++;
										break;
									}
									else if (*s == 0 && endch == '>')
									{
										POPNODE();
										break;
									}
									else THROW_ERROR(status_bad_start_element, s);
								}
								else if (*s == '>')
								{
									++s;

									break;
								}
								else if (*s == 0 && endch == '>')
								{
									break;
								}
								else THROW_ERROR(status_bad_start_element, s);
							}

							// !!!
						}
						else if (ch == '/') // '<#.../'
						{
							if (!ENDSWITH(*s, '>')) THROW_ERROR(status_bad_start_element, s);

							POPNODE(); // Pop.

							s += (*s == '>');
						}
						else if (ch == 0)
						{
							// we stepped over null terminator, backtrack & handle closing tag
							--s;
							
							if (endch != '>') THROW_ERROR(status_bad_start_element, s);
						}
						else THROW_ERROR(status_bad_start_element, s);
					}
					else if (*s == '/')
					{
						++s;

						char_t* name = cursor->name;
						if (!name) THROW_ERROR(status_end_element_mismatch, s);
						
						while (IS_CHARTYPE(*s, ct_symbol))
						{
							if (*s++ != *name++) THROW_ERROR(status_end_element_mismatch, s);
						}

						if (*name)
						{
							if (*s == 0 && name[0] == endch && name[1] == 0) THROW_ERROR(status_bad_end_element, s);
							else THROW_ERROR(status_end_element_mismatch, s);
						}
							
						POPNODE(); // Pop.

						SKIPWS();

						if (*s == 0)
						{
							if (endch != '>') THROW_ERROR(status_bad_end_element, s);
						}
						else
						{
							if (*s != '>') THROW_ERROR(status_bad_end_element, s);
							++s;
						}
					}
					else if (*s == '?') // '<?...'
					{
						s = parse_question(s, cursor, optmsk, endch);

						assert(cursor);
						if ((cursor->header & xml_memory_page_type_mask) + 1 == node_declaration) goto LOC_ATTRIBUTES;
					}
					else if (*s == '!') // '<!...'
					{
						s = parse_exclamation(s, cursor, optmsk, endch);
					}
					else if (*s == 0 && endch == '?') THROW_ERROR(status_bad_pi, s);
					else THROW_ERROR(status_unrecognized_tag, s);
				}
				else
				{
					mark = s; // Save this offset while searching for a terminator.

					SKIPWS(); // Eat whitespace if no genuine PCDATA here.

					if ((!OPTSET(parse_ws_pcdata) || mark == s) && (*s == '<' || !*s))
					{
						continue;
					}

					s = mark;
							
					if (cursor->parent)
					{
						PUSHNODE(node_pcdata); // Append a new node on the tree.
						cursor->value = s; // Save the offset.

						s = strconv_pcdata(s);
								
						POPNODE(); // Pop since this is a standalone.
						
						if (!*s) break;
					}
					else
					{
						SCANFOR(*s == '<'); // '...<'
						if (!*s) break;
						
						++s;
					}

					// We're after '<'
					goto LOC_TAG;
				}
			}

			// check that last tag is closed
			if (cursor != xmldoc) THROW_ERROR(status_end_element_mismatch, s);
		}

		static xml_parse_result parse(char_t* buffer, size_t length, xml_node_struct* root, unsigned int optmsk)
		{
			xml_document_struct* xmldoc = static_cast<xml_document_struct*>(root);

			// store buffer for offset_debug
			xmldoc->buffer = buffer;

			// early-out for empty documents
			if (length == 0) return make_parse_result(status_ok);

			// create parser on stack
			xml_parser parser(*xmldoc);

			// save last character and make buffer zero-terminated (speeds up parsing)
			char_t endch = buffer[length - 1];
			buffer[length - 1] = 0;
			
			// perform actual parsing
			int error = setjmp(parser.error_handler);

			if (error == 0)
			{
				parser.parse(buffer, xmldoc, optmsk, endch);
			}

			xml_parse_result result = make_parse_result(static_cast<xml_parse_status>(error), parser.error_offset ? parser.error_offset - buffer : 0);
			assert(result.offset >= 0 && static_cast<size_t>(result.offset) <= length);

			// update allocator state
			*static_cast<xml_allocator*>(xmldoc) = parser.alloc;

			// since we removed last character, we have to handle the only possible false positive
			if (result && endch == '<')
			{
				// there's no possible well-formed document with < at the end
				return make_parse_result(status_unrecognized_tag, length);
			}

			return result;
		}
	};

	// Output facilities
	xml_encoding get_write_native_encoding()
	{
		return encoding_utf8;
	}

	xml_encoding get_write_encoding(xml_encoding encoding)
	{
		// replace wchar encoding with utf implementation
		if (encoding == encoding_wchar) return get_wchar_encoding();

		// replace utf16 encoding with utf16 with specific endianness
		if (encoding == encoding_utf16) return is_little_endian() ? encoding_utf16_le : encoding_utf16_be;

		// replace utf32 encoding with utf32 with specific endianness
		if (encoding == encoding_utf32) return is_little_endian() ? encoding_utf32_le : encoding_utf32_be;

		// only do autodetection if no explicit encoding is requested
		if (encoding != encoding_auto) return encoding;

		// assume utf8 encoding
		return encoding_utf8;
	}

	size_t get_valid_length(const char_t* data, size_t length)
	{
		assert(length > 4);

		for (size_t i = 1; i <= 4; ++i)
		{
			uint8_t ch = static_cast<uint8_t>(data[length - i]);

			// either a standalone character or a leading one
			if ((ch & 0xc0) != 0x80) return length - i;
		}

		// there are four non-leading characters at the end, sequence tail is broken so might as well process the whole chunk
		return length;
	}

	size_t convert_buffer(char* result, const char_t* data, size_t length, xml_encoding encoding)
	{
		if (encoding == encoding_utf16_be || encoding == encoding_utf16_le)
		{
			uint16_t* dest = reinterpret_cast<uint16_t*>(result);

			// convert to native utf16
			uint16_t* end = utf_decoder<utf16_writer>::decode_utf8_block(reinterpret_cast<const uint8_t*>(data), length, dest);

			// swap if necessary
			xml_encoding native_encoding = is_little_endian() ? encoding_utf16_le : encoding_utf16_be;

			if (native_encoding != encoding) convert_utf_endian_swap(dest, dest, static_cast<size_t>(end - dest));

			return static_cast<size_t>(end - dest) * sizeof(uint16_t);
		}

		if (encoding == encoding_utf32_be || encoding == encoding_utf32_le)
		{
			uint32_t* dest = reinterpret_cast<uint32_t*>(result);

			// convert to native utf32
			uint32_t* end = utf_decoder<utf32_writer>::decode_utf8_block(reinterpret_cast<const uint8_t*>(data), length, dest);

			// swap if necessary
			xml_encoding native_encoding = is_little_endian() ? encoding_utf32_le : encoding_utf32_be;

			if (native_encoding != encoding) convert_utf_endian_swap(dest, dest, static_cast<size_t>(end - dest));

			return static_cast<size_t>(end - dest) * sizeof(uint32_t);
		}

		assert(!"Invalid encoding");
		return 0;
	}

	inline bool has_declaration(const xml_node& node)
	{
		for (xml_node child = node.first_child(); child; child = child.next_sibling())
		{
			xml_node_type type = child.type();

			if (type == node_declaration) return true;
			if (type == node_element) return false;
		}

		return false;
	}

	inline bool allow_insert_child(xml_node_type parent, xml_node_type child)
	{
		if (parent != node_document && parent != node_element) return false;
		if (child == node_document || child == node_null) return false;
		if (parent != node_document && (child == node_declaration || child == node_doctype)) return false;

		return true;
	}

	void recursive_copy_skip(xml_node& dest, const xml_node& source, const xml_node& skip)
	{
		assert(dest.type() == source.type());

		switch (source.type())
		{
		case node_element:
		{
			dest.set_name(source.name());

			for (xml_attribute a = source.first_attribute(); a; a = a.next_attribute())
				dest.append_attribute(a.name()).set_value(a.value());

			for (xml_node c = source.first_child(); c; c = c.next_sibling())
			{
				if (c == skip) continue;

				xml_node cc = dest.append_child(c.type());
				assert(cc);

				recursive_copy_skip(cc, c, skip);
			}

			break;
		}

		case node_pcdata:
		case node_cdata:
		case node_comment:
        case node_doctype:
			dest.set_value(source.value());
			break;

		case node_pi:
			dest.set_name(source.name());
			dest.set_value(source.value());
			break;

		case node_declaration:
		{
			dest.set_name(source.name());

			for (xml_attribute a = source.first_attribute(); a; a = a.next_attribute())
				dest.append_attribute(a.name()).set_value(a.value());

			break;
		}

		default:
			assert(!"Invalid node type");
		}
	}
}

namespace pugi
{

	xml_tree_walker::xml_tree_walker(): _depth(0)
	{
	}
	
	xml_tree_walker::~xml_tree_walker()
	{
	}

	int xml_tree_walker::depth() const
	{
		return _depth;
	}

	bool xml_tree_walker::begin(xml_node&)
	{
		return true;
	}

	bool xml_tree_walker::end(xml_node&)
	{
		return true;
	}

	xml_attribute::xml_attribute(): _attr(0)
	{
	}

	xml_attribute::xml_attribute(xml_attribute_struct* attr): _attr(attr)
	{
	}

	xml_attribute::operator xml_attribute::unspecified_bool_type() const
	{
      	return _attr ? &xml_attribute::_attr : 0;
   	}

   	bool xml_attribute::operator!() const
   	{
   		return !_attr;
   	}

	bool xml_attribute::operator==(const xml_attribute& r) const
	{
		return (_attr == r._attr);
	}
	
	bool xml_attribute::operator!=(const xml_attribute& r) const
	{
		return (_attr != r._attr);
	}

	bool xml_attribute::operator<(const xml_attribute& r) const
	{
		return (_attr < r._attr);
	}
	
	bool xml_attribute::operator>(const xml_attribute& r) const
	{
		return (_attr > r._attr);
	}
	
	bool xml_attribute::operator<=(const xml_attribute& r) const
	{
		return (_attr <= r._attr);
	}
	
	bool xml_attribute::operator>=(const xml_attribute& r) const
	{
		return (_attr >= r._attr);
	}

   	xml_attribute xml_attribute::next_attribute() const
   	{
    	return _attr ? xml_attribute(_attr->next_attribute) : xml_attribute();
   	}

    xml_attribute xml_attribute::previous_attribute() const
    {
    	return _attr && _attr->prev_attribute_c->next_attribute ? xml_attribute(_attr->prev_attribute_c) : xml_attribute();
    }

	int xml_attribute::as_int() const
	{
		if (!_attr || !_attr->value) return 0;

	#ifdef PUGIXML_WCHAR_MODE
		return (int)wcstol(_attr->value, 0, 10);
	#else
		return (int)strtol(_attr->value, 0, 10);
	#endif
	}

	unsigned int xml_attribute::as_uint() const
	{
		if (!_attr || !_attr->value) return 0;

	#ifdef PUGIXML_WCHAR_MODE
		return (unsigned int)wcstoul(_attr->value, 0, 10);
	#else
		return (unsigned int)strtoul(_attr->value, 0, 10);
	#endif
	}

	double xml_attribute::as_double() const
	{
		if (!_attr || !_attr->value) return 0;

	#ifdef PUGIXML_WCHAR_MODE
		return wcstod(_attr->value, 0);
	#else
		return strtod(_attr->value, 0);
	#endif
	}

	float xml_attribute::as_float() const
	{
		if (!_attr || !_attr->value) return 0;

	#ifdef PUGIXML_WCHAR_MODE
		return (float)wcstod(_attr->value, 0);
	#else
		return (float)strtod(_attr->value, 0);
	#endif
	}

	bool xml_attribute::as_bool() const
	{
		if (!_attr || !_attr->value) return false;

		// only look at first char
		char_t first = *_attr->value;

		// 1*, t* (true), T* (True), y* (yes), Y* (YES)
		return (first == '1' || first == 't' || first == 'T' || first == 'y' || first == 'Y');
	}

	bool xml_attribute::empty() const
	{
		return !_attr;
	}

	const char_t* xml_attribute::name() const
	{
		return (_attr && _attr->name) ? _attr->name : PUGIXML_TEXT("");
	}

	const char_t* xml_attribute::value() const
	{
		return (_attr && _attr->value) ? _attr->value : PUGIXML_TEXT("");
	}

    size_t xml_attribute::hash_value() const
    {
        return static_cast<size_t>(reinterpret_cast<uintptr_t>(_attr) / sizeof(xml_attribute_struct));
    }

	xml_attribute_struct* xml_attribute::internal_object() const
	{
        return _attr;
	}

	xml_attribute& xml_attribute::operator=(const char_t* rhs)
	{
		set_value(rhs);
		return *this;
	}
	
	xml_attribute& xml_attribute::operator=(int rhs)
	{
		set_value(rhs);
		return *this;
	}

	xml_attribute& xml_attribute::operator=(unsigned int rhs)
	{
		set_value(rhs);
		return *this;
	}

	xml_attribute& xml_attribute::operator=(double rhs)
	{
		set_value(rhs);
		return *this;
	}
	
	xml_attribute& xml_attribute::operator=(bool rhs)
	{
		set_value(rhs);
		return *this;
	}

	bool xml_attribute::set_name(const char_t* rhs)
	{
		if (!_attr) return false;
		
		return strcpy_insitu(_attr->name, _attr->header, xml_memory_page_name_allocated_mask, rhs);
	}
		
	bool xml_attribute::set_value(const char_t* rhs)
	{
		if (!_attr) return false;

		return strcpy_insitu(_attr->value, _attr->header, xml_memory_page_value_allocated_mask, rhs);
	}

	bool xml_attribute::set_value(int rhs)
	{
		char buf[128];
		sprintf(buf, "%d", rhs);
		return set_value(buf);
	}

	bool xml_attribute::set_value(unsigned int rhs)
	{
		char buf[128];
		sprintf(buf, "%u", rhs);
		return set_value(buf);
	}

	bool xml_attribute::set_value(double rhs)
	{
		char buf[128];
		sprintf(buf, "%g", rhs);
		return set_value(buf);
	}
	
	bool xml_attribute::set_value(bool rhs)
	{
		return set_value(rhs ? PUGIXML_TEXT("true") : PUGIXML_TEXT("false"));
	}

    xml_node::xml_node(): _root(0)
	{
	}

	xml_node::xml_node(xml_node_struct* p): _root(p)
	{
	}
	
	xml_node::operator xml_node::unspecified_bool_type() const
	{
      	return _root ? &xml_node::_root : 0;
   	}

   	bool xml_node::operator!() const
   	{
   		return !_root;
   	}

	xml_node::iterator xml_node::begin() const
	{
		return iterator(_root ? _root->first_child : 0, _root);
	}

	xml_node::iterator xml_node::end() const
	{
		return iterator(0, _root);
	}
	
	xml_node::attribute_iterator xml_node::attributes_begin() const
	{
		return attribute_iterator(_root ? _root->first_attribute : 0, _root);
	}

	xml_node::attribute_iterator xml_node::attributes_end() const
	{
		return attribute_iterator(0, _root);
	}

	bool xml_node::operator==(const xml_node& r) const
	{
		return (_root == r._root);
	}

	bool xml_node::operator!=(const xml_node& r) const
	{
		return (_root != r._root);
	}

	bool xml_node::operator<(const xml_node& r) const
	{
		return (_root < r._root);
	}
	
	bool xml_node::operator>(const xml_node& r) const
	{
		return (_root > r._root);
	}
	
	bool xml_node::operator<=(const xml_node& r) const
	{
		return (_root <= r._root);
	}
	
	bool xml_node::operator>=(const xml_node& r) const
	{
		return (_root >= r._root);
	}

	bool xml_node::empty() const
	{
		return !_root;
	}
	
	const char_t* xml_node::name() const
	{
		return (_root && _root->name) ? _root->name : PUGIXML_TEXT("");
	}

	xml_node_type xml_node::type() const
	{
		return _root ? static_cast<xml_node_type>((_root->header & xml_memory_page_type_mask) + 1) : node_null;
	}
	
	const char_t* xml_node::value() const
	{
		return (_root && _root->value) ? _root->value : PUGIXML_TEXT("");
	}
	
	xml_node xml_node::child(const char_t* name) const
	{
		if (!_root) return xml_node();

		for (xml_node_struct* i = _root->first_child; i; i = i->next_sibling)
			if (i->name && strequal(name, i->name)) return xml_node(i);

		return xml_node();
	}

	xml_attribute xml_node::attribute(const char_t* name) const
	{
		if (!_root) return xml_attribute();

		for (xml_attribute_struct* i = _root->first_attribute; i; i = i->next_attribute)
			if (i->name && strequal(name, i->name))
				return xml_attribute(i);
		
		return xml_attribute();
	}
	
	xml_node xml_node::next_sibling(const char_t* name) const
	{
		if (!_root) return xml_node();
		
		for (xml_node_struct* i = _root->next_sibling; i; i = i->next_sibling)
			if (i->name && strequal(name, i->name)) return xml_node(i);

		return xml_node();
	}

	xml_node xml_node::next_sibling() const
	{
		if (!_root) return xml_node();
		
		if (_root->next_sibling) return xml_node(_root->next_sibling);
		else return xml_node();
	}

	xml_node xml_node::previous_sibling(const char_t* name) const
	{
		if (!_root) return xml_node();
		
		for (xml_node_struct* i = _root->prev_sibling_c; i->next_sibling; i = i->prev_sibling_c)
			if (i->name && strequal(name, i->name)) return xml_node(i);

		return xml_node();
	}

	xml_node xml_node::previous_sibling() const
	{
		if (!_root) return xml_node();
		
		if (_root->prev_sibling_c->next_sibling) return xml_node(_root->prev_sibling_c);
		else return xml_node();
	}

	xml_node xml_node::parent() const
	{
		return _root ? xml_node(_root->parent) : xml_node();
	}

	xml_node xml_node::root() const
	{
		if (!_root) return xml_node();

		xml_memory_page* page = reinterpret_cast<xml_memory_page*>(_root->header & xml_memory_page_pointer_mask);

		return xml_node(static_cast<xml_document_struct*>(page->allocator));
	}

	const char_t* xml_node::child_value() const
	{
		if (!_root) return PUGIXML_TEXT("");
		
		for (xml_node_struct* i = _root->first_child; i; i = i->next_sibling)
		{
			xml_node_type type = static_cast<xml_node_type>((i->header & xml_memory_page_type_mask) + 1);

			if (i->value && (type == node_pcdata || type == node_cdata))
				return i->value;
		}

		return PUGIXML_TEXT("");
	}

	const char_t* xml_node::child_value(const char_t* name) const
	{
		return child(name).child_value();
	}

	xml_attribute xml_node::first_attribute() const
	{
		return _root ? xml_attribute(_root->first_attribute) : xml_attribute();
	}

	xml_attribute xml_node::last_attribute() const
	{
		return _root && _root->first_attribute ? xml_attribute(_root->first_attribute->prev_attribute_c) : xml_attribute();
	}

	xml_node xml_node::first_child() const
	{
		return _root ? xml_node(_root->first_child) : xml_node();
	}

	xml_node xml_node::last_child() const
	{
		return _root && _root->first_child ? xml_node(_root->first_child->prev_sibling_c) : xml_node();
	}

	bool xml_node::set_name(const char_t* rhs)
	{
		switch (type())
		{
		case node_pi:
		case node_declaration:
		case node_element:
			return strcpy_insitu(_root->name, _root->header, xml_memory_page_name_allocated_mask, rhs);

		default:
			return false;
		}
	}
		
	bool xml_node::set_value(const char_t* rhs)
	{
		switch (type())
		{
		case node_pi:
		case node_cdata:
		case node_pcdata:
		case node_comment:
        case node_doctype:
			return strcpy_insitu(_root->value, _root->header, xml_memory_page_value_allocated_mask, rhs);

		default:
			return false;
		}
	}

	xml_attribute xml_node::append_attribute(const char_t* name)
	{
		if (type() != node_element && type() != node_declaration) return xml_attribute();
		
		xml_attribute a(append_attribute_ll(_root, get_allocator(_root)));
		a.set_name(name);
		
		return a;
	}

	xml_attribute xml_node::prepend_attribute(const char_t* name)
	{
		if (type() != node_element && type() != node_declaration) return xml_attribute();
		
		xml_attribute a(allocate_attribute(get_allocator(_root)));
		if (!a) return xml_attribute();

		a.set_name(name);
		
        xml_attribute_struct* head = _root->first_attribute;

		if (head)
        {
            a._attr->prev_attribute_c = head->prev_attribute_c;
            head->prev_attribute_c = a._attr;
        }
        else
            a._attr->prev_attribute_c = a._attr;
		
		a._attr->next_attribute = head;
        _root->first_attribute = a._attr;
				
		return a;
	}

	xml_attribute xml_node::insert_attribute_before(const char_t* name, const xml_attribute& attr)
	{
		if ((type() != node_element && type() != node_declaration) || attr.empty()) return xml_attribute();
		
		// check that attribute belongs to *this
		xml_attribute_struct* cur = attr._attr;

		while (cur->prev_attribute_c->next_attribute) cur = cur->prev_attribute_c;

		if (cur != _root->first_attribute) return xml_attribute();

		xml_attribute a(allocate_attribute(get_allocator(_root)));
		if (!a) return xml_attribute();

		a.set_name(name);

		if (attr._attr->prev_attribute_c->next_attribute)
			attr._attr->prev_attribute_c->next_attribute = a._attr;
		else
			_root->first_attribute = a._attr;
		
		a._attr->prev_attribute_c = attr._attr->prev_attribute_c;
		a._attr->next_attribute = attr._attr;
		attr._attr->prev_attribute_c = a._attr;
				
		return a;
	}

	xml_attribute xml_node::insert_attribute_after(const char_t* name, const xml_attribute& attr)
	{
		if ((type() != node_element && type() != node_declaration) || attr.empty()) return xml_attribute();
		
		// check that attribute belongs to *this
		xml_attribute_struct* cur = attr._attr;

		while (cur->prev_attribute_c->next_attribute) cur = cur->prev_attribute_c;

		if (cur != _root->first_attribute) return xml_attribute();

		xml_attribute a(allocate_attribute(get_allocator(_root)));
		if (!a) return xml_attribute();

		a.set_name(name);

		if (attr._attr->next_attribute)
			attr._attr->next_attribute->prev_attribute_c = a._attr;
		else
			_root->first_attribute->prev_attribute_c = a._attr;
		
		a._attr->next_attribute = attr._attr->next_attribute;
		a._attr->prev_attribute_c = attr._attr;
		attr._attr->next_attribute = a._attr;

		return a;
	}

	xml_attribute xml_node::append_copy(const xml_attribute& proto)
	{
		if (!proto) return xml_attribute();

		xml_attribute result = append_attribute(proto.name());
		result.set_value(proto.value());

		return result;
	}

	xml_attribute xml_node::prepend_copy(const xml_attribute& proto)
	{
		if (!proto) return xml_attribute();

		xml_attribute result = prepend_attribute(proto.name());
		result.set_value(proto.value());

		return result;
	}

	xml_attribute xml_node::insert_copy_after(const xml_attribute& proto, const xml_attribute& attr)
	{
		if (!proto) return xml_attribute();

		xml_attribute result = insert_attribute_after(proto.name(), attr);
		result.set_value(proto.value());

		return result;
	}

	xml_attribute xml_node::insert_copy_before(const xml_attribute& proto, const xml_attribute& attr)
	{
		if (!proto) return xml_attribute();

		xml_attribute result = insert_attribute_before(proto.name(), attr);
		result.set_value(proto.value());

		return result;
	}

	xml_node xml_node::append_child(xml_node_type type)
	{
		if (!allow_insert_child(this->type(), type)) return xml_node();
		
		xml_node n(append_node(_root, get_allocator(_root), type));

		if (type == node_declaration) n.set_name(PUGIXML_TEXT("xml"));

		return n;
	}

	xml_node xml_node::prepend_child(xml_node_type type)
	{
		if (!allow_insert_child(this->type(), type)) return xml_node();
		
		xml_node n(allocate_node(get_allocator(_root), type));
		if (!n) return xml_node();

        n._root->parent = _root;

        xml_node_struct* head = _root->first_child;

		if (head)
        {
            n._root->prev_sibling_c = head->prev_sibling_c;
            head->prev_sibling_c = n._root;
        }
        else
            n._root->prev_sibling_c = n._root;
		
		n._root->next_sibling = head;
        _root->first_child = n._root;
				
		if (type == node_declaration) n.set_name(PUGIXML_TEXT("xml"));

		return n;
	}

	xml_node xml_node::insert_child_before(xml_node_type type, const xml_node& node)
	{
		if (!allow_insert_child(this->type(), type)) return xml_node();
		if (!node._root || node._root->parent != _root) return xml_node();
	
		xml_node n(allocate_node(get_allocator(_root), type));
		if (!n) return xml_node();

		n._root->parent = _root;
		
		if (node._root->prev_sibling_c->next_sibling)
			node._root->prev_sibling_c->next_sibling = n._root;
		else
			_root->first_child = n._root;
		
		n._root->prev_sibling_c = node._root->prev_sibling_c;
		n._root->next_sibling = node._root;
		node._root->prev_sibling_c = n._root;

		if (type == node_declaration) n.set_name(PUGIXML_TEXT("xml"));

		return n;
	}

	xml_node xml_node::insert_child_after(xml_node_type type, const xml_node& node)
	{
		if (!allow_insert_child(this->type(), type)) return xml_node();
		if (!node._root || node._root->parent != _root) return xml_node();
	
		xml_node n(allocate_node(get_allocator(_root), type));
		if (!n) return xml_node();

		n._root->parent = _root;
	
		if (node._root->next_sibling)
			node._root->next_sibling->prev_sibling_c = n._root;
		else
			_root->first_child->prev_sibling_c = n._root;
		
		n._root->next_sibling = node._root->next_sibling;
		n._root->prev_sibling_c = node._root;
		node._root->next_sibling = n._root;

		if (type == node_declaration) n.set_name(PUGIXML_TEXT("xml"));

		return n;
	}

    xml_node xml_node::append_child(const char_t* name)
    {
        xml_node result = append_child(node_element);

        result.set_name(name);

        return result;
    }

    xml_node xml_node::prepend_child(const char_t* name)
    {
        xml_node result = prepend_child(node_element);

        result.set_name(name);

        return result;
    }

    xml_node xml_node::insert_child_after(const char_t* name, const xml_node& node)
    {
        xml_node result = insert_child_after(node_element, node);

        result.set_name(name);

        return result;
    }

    xml_node xml_node::insert_child_before(const char_t* name, const xml_node& node)
    {
        xml_node result = insert_child_before(node_element, node);

        result.set_name(name);

        return result;
    }

	xml_node xml_node::append_copy(const xml_node& proto)
	{
		xml_node result = append_child(proto.type());

		if (result) recursive_copy_skip(result, proto, result);

		return result;
	}

	xml_node xml_node::prepend_copy(const xml_node& proto)
	{
		xml_node result = prepend_child(proto.type());

		if (result) recursive_copy_skip(result, proto, result);

		return result;
	}

	xml_node xml_node::insert_copy_after(const xml_node& proto, const xml_node& node)
	{
		xml_node result = insert_child_after(proto.type(), node);

		if (result) recursive_copy_skip(result, proto, result);

		return result;
	}

	xml_node xml_node::insert_copy_before(const xml_node& proto, const xml_node& node)
	{
		xml_node result = insert_child_before(proto.type(), node);

		if (result) recursive_copy_skip(result, proto, result);

		return result;
	}

	bool xml_node::remove_attribute(const char_t* name)
	{
		return remove_attribute(attribute(name));
	}

	bool xml_node::remove_attribute(const xml_attribute& a)
	{
		if (!_root || !a._attr) return false;

		// check that attribute belongs to *this
		xml_attribute_struct* attr = a._attr;

		while (attr->prev_attribute_c->next_attribute) attr = attr->prev_attribute_c;

		if (attr != _root->first_attribute) return false;

		if (a._attr->next_attribute) a._attr->next_attribute->prev_attribute_c = a._attr->prev_attribute_c;
		else if (_root->first_attribute) _root->first_attribute->prev_attribute_c = a._attr->prev_attribute_c;
		
		if (a._attr->prev_attribute_c->next_attribute) a._attr->prev_attribute_c->next_attribute = a._attr->next_attribute;
		else _root->first_attribute = a._attr->next_attribute;

		destroy_attribute(a._attr, get_allocator(_root));

		return true;
	}

	bool xml_node::remove_child(const char_t* name)
	{
		return remove_child(child(name));
	}

	bool xml_node::remove_child(const xml_node& n)
	{
		if (!_root || !n._root || n._root->parent != _root) return false;

		if (n._root->next_sibling) n._root->next_sibling->prev_sibling_c = n._root->prev_sibling_c;
		else if (_root->first_child) _root->first_child->prev_sibling_c = n._root->prev_sibling_c;
		
		if (n._root->prev_sibling_c->next_sibling) n._root->prev_sibling_c->next_sibling = n._root->next_sibling;
		else _root->first_child = n._root->next_sibling;
        
        destroy_node(n._root, get_allocator(_root));

		return true;
	}

	xml_node xml_node::find_child_by_attribute(const char_t* name, const char_t* attr_name, const char_t* attr_value) const
	{
		if (!_root) return xml_node();
		
		for (xml_node_struct* i = _root->first_child; i; i = i->next_sibling)
			if (i->name && strequal(name, i->name))
			{
				for (xml_attribute_struct* a = i->first_attribute; a; a = a->next_attribute)
					if (strequal(attr_name, a->name) && strequal(attr_value, a->value))
						return xml_node(i);
			}

		return xml_node();
	}

	xml_node xml_node::find_child_by_attribute(const char_t* attr_name, const char_t* attr_value) const
	{
		if (!_root) return xml_node();
		
		for (xml_node_struct* i = _root->first_child; i; i = i->next_sibling)
			for (xml_attribute_struct* a = i->first_attribute; a; a = a->next_attribute)
				if (strequal(attr_name, a->name) && strequal(attr_value, a->value))
					return xml_node(i);

		return xml_node();
	}

	xml_node xml_node::first_element_by_path(const char_t* path, char_t delimiter) const
	{
		xml_node found = *this; // Current search context.

		if (!_root || !path || !path[0]) return found;

		if (path[0] == delimiter)
		{
			// Absolute path; e.g. '/foo/bar'
			found = found.root();
			++path;
		}

		const char_t* path_segment = path;

		while (*path_segment == delimiter) ++path_segment;

		const char_t* path_segment_end = path_segment;

		while (*path_segment_end && *path_segment_end != delimiter) ++path_segment_end;

		if (path_segment == path_segment_end) return found;

		const char_t* next_segment = path_segment_end;

		while (*next_segment == delimiter) ++next_segment;

		if (*path_segment == '.' && path_segment + 1 == path_segment_end)
			return found.first_element_by_path(next_segment, delimiter);
		else if (*path_segment == '.' && *(path_segment+1) == '.' && path_segment + 2 == path_segment_end)
			return found.parent().first_element_by_path(next_segment, delimiter);
		else
		{
			for (xml_node_struct* j = found._root->first_child; j; j = j->next_sibling)
			{
				if (j->name && strequalrange(j->name, path_segment, static_cast<size_t>(path_segment_end - path_segment)))
				{
					xml_node subsearch = xml_node(j).first_element_by_path(next_segment, delimiter);

					if (subsearch) return subsearch;
				}
			}

			return xml_node();
		}
	}

	bool xml_node::traverse(xml_tree_walker& walker)
	{
		walker._depth = -1;
		
		xml_node arg_begin = *this;
		if (!walker.begin(arg_begin)) return false;

		xml_node cur = first_child();
				
		if (cur)
		{
			++walker._depth;

			do 
			{
				xml_node arg_for_each = cur;
				if (!walker.for_each(arg_for_each))
					return false;
						
				if (cur.first_child())
				{
					++walker._depth;
					cur = cur.first_child();
				}
				else if (cur.next_sibling())
					cur = cur.next_sibling();
				else
				{
					// Borland C++ workaround
					while (!cur.next_sibling() && cur != *this && (bool)cur.parent())
					{
						--walker._depth;
						cur = cur.parent();
					}
						
					if (cur != *this)
						cur = cur.next_sibling();
				}
			}
			while (cur && cur != *this);
		}

		assert(walker._depth == -1);

		xml_node arg_end = *this;
		return walker.end(arg_end);
	}

    size_t xml_node::hash_value() const
    {
        return static_cast<size_t>(reinterpret_cast<uintptr_t>(_root) / sizeof(xml_node_struct));
    }

	xml_node_struct* xml_node::internal_object() const
	{
        return _root;
	}

	ptrdiff_t xml_node::offset_debug() const
	{
		xml_node_struct* r = root()._root;

		if (!r) return -1;

		const char_t* buffer = static_cast<xml_document_struct*>(r)->buffer;

		if (!buffer) return -1;

		switch (type())
		{
		case node_document:
			return 0;

		case node_element:
		case node_declaration:
		case node_pi:
			return (_root->header & xml_memory_page_name_allocated_mask) ? -1 : _root->name - buffer;

		case node_pcdata:
		case node_cdata:
		case node_comment:
		case node_doctype:
			return (_root->header & xml_memory_page_value_allocated_mask) ? -1 : _root->value - buffer;

		default:
			return -1;
		}
	}

	xml_node_iterator::xml_node_iterator()
	{
	}

	xml_node_iterator::xml_node_iterator(const xml_node& node): _wrap(node), _parent(node.parent())
	{
	}

	xml_node_iterator::xml_node_iterator(xml_node_struct* ref, xml_node_struct* parent): _wrap(ref), _parent(parent)
	{
	}

	bool xml_node_iterator::operator==(const xml_node_iterator& rhs) const
	{
		return _wrap._root == rhs._wrap._root && _parent._root == rhs._parent._root;
	}
	
	bool xml_node_iterator::operator!=(const xml_node_iterator& rhs) const
	{
		return _wrap._root != rhs._wrap._root || _parent._root != rhs._parent._root;
	}

	xml_node& xml_node_iterator::operator*()
	{
		assert(_wrap._root);
		return _wrap;
	}

	xml_node* xml_node_iterator::operator->()
	{
		assert(_wrap._root);
		return &_wrap;
	}

	const xml_node_iterator& xml_node_iterator::operator++()
	{
		assert(_wrap._root);
		_wrap._root = _wrap._root->next_sibling;
		return *this;
	}

	xml_node_iterator xml_node_iterator::operator++(int)
	{
		xml_node_iterator temp = *this;
		++*this;
		return temp;
	}

	const xml_node_iterator& xml_node_iterator::operator--()
	{
		_wrap = _wrap._root ? _wrap.previous_sibling() : _parent.last_child();
		return *this;
	}

	xml_node_iterator xml_node_iterator::operator--(int)
	{
		xml_node_iterator temp = *this;
		--*this;
		return temp;
	}

	xml_attribute_iterator::xml_attribute_iterator()
	{
	}

	xml_attribute_iterator::xml_attribute_iterator(const xml_attribute& attr, const xml_node& parent): _wrap(attr), _parent(parent)
	{
	}

	xml_attribute_iterator::xml_attribute_iterator(xml_attribute_struct* ref, xml_node_struct* parent): _wrap(ref), _parent(parent)
	{
	}

	bool xml_attribute_iterator::operator==(const xml_attribute_iterator& rhs) const
	{
		return _wrap._attr == rhs._wrap._attr && _parent._root == rhs._parent._root;
	}
	
	bool xml_attribute_iterator::operator!=(const xml_attribute_iterator& rhs) const
	{
		return _wrap._attr != rhs._wrap._attr || _parent._root != rhs._parent._root;
	}

	xml_attribute& xml_attribute_iterator::operator*()
	{
		assert(_wrap._attr);
		return _wrap;
	}

	xml_attribute* xml_attribute_iterator::operator->()
	{
		assert(_wrap._attr);
		return &_wrap;
	}

	const xml_attribute_iterator& xml_attribute_iterator::operator++()
	{
		assert(_wrap._attr);
		_wrap._attr = _wrap._attr->next_attribute;
		return *this;
	}

	xml_attribute_iterator xml_attribute_iterator::operator++(int)
	{
		xml_attribute_iterator temp = *this;
		++*this;
		return temp;
	}

	const xml_attribute_iterator& xml_attribute_iterator::operator--()
	{
		_wrap = _wrap._attr ? _wrap.previous_attribute() : _parent.last_attribute();
		return *this;
	}

	xml_attribute_iterator xml_attribute_iterator::operator--(int)
	{
		xml_attribute_iterator temp = *this;
		--*this;
		return temp;
	}

    xml_parse_result::xml_parse_result(): status(status_internal_error), offset(0), encoding(encoding_auto)
    {
    }

    xml_parse_result::operator bool() const
    {
        return status == status_ok;
    }

	const char* xml_parse_result::description() const
	{
		switch (status)
		{
		case status_ok: return "No error";

		case status_file_not_found: return "File was not found";
		case status_io_error: return "Error reading from file/stream";
		case status_out_of_memory: return "Could not allocate memory";
		case status_internal_error: return "Internal error occurred";

		case status_unrecognized_tag: return "Could not determine tag type";

		case status_bad_pi: return "Error parsing document declaration/processing instruction";
		case status_bad_comment: return "Error parsing comment";
		case status_bad_cdata: return "Error parsing CDATA section";
		case status_bad_doctype: return "Error parsing document type declaration";
		case status_bad_pcdata: return "Error parsing PCDATA section";
		case status_bad_start_element: return "Error parsing start element tag";
		case status_bad_attribute: return "Error parsing element attribute";
		case status_bad_end_element: return "Error parsing end element tag";
		case status_end_element_mismatch: return "Start-end tags mismatch";

		default: return "Unknown error";
		}
	}

	xml_document::xml_document(): _buffer(0)
	{
		create();
	}

	xml_document::~xml_document()
	{
		destroy();
	}

	void xml_document::reset()
	{
		destroy();
		create();
	}

    void xml_document::reset(const xml_document& proto)
    {
        reset();

        for (xml_node cur = proto.first_child(); cur; cur = cur.next_sibling())
            append_copy(cur);
    }

	void xml_document::create()
	{
		// initialize sentinel page
		STATIC_ASSERT(offsetof(xml_memory_page, data) + sizeof(xml_document_struct) + xml_memory_page_alignment <= sizeof(_memory));

		// align upwards to page boundary
		void* page_memory = reinterpret_cast<void*>((reinterpret_cast<uintptr_t>(_memory) + (xml_memory_page_alignment - 1)) & ~(xml_memory_page_alignment - 1));

		// prepare page structure
		xml_memory_page* page = xml_memory_page::construct(page_memory);

		page->busy_size = xml_memory_page_size;

		// allocate new root
		_root = new (page->data) xml_document_struct(page);
		_root->prev_sibling_c = _root;

		// setup sentinel page
		page->allocator = static_cast<xml_document_struct*>(_root);
	}

	void xml_document::destroy()
	{
		// destroy static storage
		if (_buffer)
		{
			global_deallocate(_buffer);
			_buffer = 0;
		}

		// destroy dynamic storage, leave sentinel page (it's in static memory)
		if (_root)
		{
			xml_memory_page* root_page = reinterpret_cast<xml_memory_page*>(_root->header & xml_memory_page_pointer_mask);
			assert(root_page && !root_page->prev && !root_page->memory);

			// destroy all pages
			for (xml_memory_page* page = root_page->next; page; )
			{
				xml_memory_page* next = page->next;

				xml_allocator::deallocate_page(page);

				page = next;
			}

			// cleanup root page
			root_page->allocator = 0;
			root_page->next = 0;
			root_page->busy_size = root_page->freed_size = 0;

			_root = 0;
		}
	}

	xml_parse_result xml_document::load(const char_t* contents, unsigned int options)
	{
		// Force native encoding (skip autodetection)
	#ifdef PUGIXML_WCHAR_MODE
		xml_encoding encoding = encoding_wchar;
	#else
		xml_encoding encoding = encoding_utf8;
	#endif

		return load_buffer(contents, strlength(contents) * sizeof(char_t), options, encoding);
	}

	xml_parse_result xml_document::load_buffer_impl(void* contents, size_t size, unsigned int options, xml_encoding encoding, bool is_mutable, bool own)
	{
		reset();

		// check input buffer
		assert(contents || size == 0);

		// get actual encoding
		xml_encoding buffer_encoding = get_buffer_encoding(encoding, contents, size);

		// get private buffer
		char_t* buffer = 0;
		size_t length = 0;

		if (!convert_buffer(buffer, length, buffer_encoding, contents, size, is_mutable)) return make_parse_result(status_out_of_memory);
		
		// delete original buffer if we performed a conversion
		if (own && buffer != contents && contents) global_deallocate(contents);

		// parse
		xml_parse_result res = xml_parser::parse(buffer, length, _root, options);

		// remember encoding
		res.encoding = buffer_encoding;

		// grab onto buffer if it's our buffer, user is responsible for deallocating contens himself
		if (own || buffer != contents) _buffer = buffer;

		return res;
	}

	xml_parse_result xml_document::load_buffer(const void* contents, size_t size, unsigned int options, xml_encoding encoding)
	{
		return load_buffer_impl(const_cast<void*>(contents), size, options, encoding, false, false);
	}

	xml_parse_result xml_document::load_buffer_inplace(void* contents, size_t size, unsigned int options, xml_encoding encoding)
	{
		return load_buffer_impl(contents, size, options, encoding, true, false);
	}
		
	xml_parse_result xml_document::load_buffer_inplace_own(void* contents, size_t size, unsigned int options, xml_encoding encoding)
	{
		return load_buffer_impl(contents, size, options, encoding, true, true);
	}

    xml_node xml_document::document_element() const
    {
		for (xml_node_struct* i = _root->first_child; i; i = i->next_sibling)
			if ((i->header & xml_memory_page_type_mask) + 1 == node_element)
                return xml_node(i);

        return xml_node();
    }

    void PUGIXML_FUNCTION set_memory_management_functions(allocation_function allocate, deallocation_function deallocate)
    {
    	global_allocate = allocate;
    	global_deallocate = deallocate;
    }

    allocation_function PUGIXML_FUNCTION get_memory_allocation_function()
    {
    	return global_allocate;
    }

    deallocation_function PUGIXML_FUNCTION get_memory_deallocation_function()
    {
    	return global_deallocate;
    }
}

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
