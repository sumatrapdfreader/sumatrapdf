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

#ifndef PUGIXML_NO_XPATH
#	include <math.h>
#	include <float.h>
#endif

#ifndef PUGIXML_NO_STL
#	include <istream>
#	include <ostream>
#	include <string>
#endif

// For placement new
#include <new>

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

#ifdef __BORLANDC__
#	pragma warn -8008 // condition is always false
#	pragma warn -8066 // unreachable code
#endif

#ifdef __SNC__
#	pragma diag_suppress=178 // function was declared but never referenced
#	pragma diag_suppress=237 // controlling expression is constant
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

// Digital Mars C++ bug workaround for passing char loaded from memory via stack
#ifdef __DMC__
#	define DMC_VOLATILE volatile
#else
#	define DMC_VOLATILE
#endif

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

	#ifdef PUGIXML_WCHAR_MODE
		return wcslen(s);
	#else
		return strlen(s);
	#endif
	}

	// Compare two strings
	bool strequal(const char_t* src, const char_t* dst)
	{
		assert(src && dst);

	#ifdef PUGIXML_WCHAR_MODE
		return wcscmp(src, dst) == 0;
	#else
		return strcmp(src, dst) == 0;
	#endif
	}

	// Compare lhs with [rhs_begin, rhs_end)
	bool strequalrange(const char_t* lhs, const char_t* rhs, size_t count)
	{
		for (size_t i = 0; i < count; ++i)
			if (lhs[i] != rhs[i])
				return false;
	
		return lhs[count] == 0;
	}
	
#ifdef PUGIXML_WCHAR_MODE
	// Convert string to wide string, assuming all symbols are ASCII
	void widen_ascii(wchar_t* dest, const char* source)
	{
		for (const char* i = source; *i; ++i) *dest++ = *i;
		*dest = 0;
	}
#endif
}

#if !defined(PUGIXML_NO_STL) || !defined(PUGIXML_NO_XPATH)
// auto_ptr-like buffer holder for exception recovery
namespace
{
	struct buffer_holder
	{
		void* data;
		void (*deleter)(void*);

		buffer_holder(void* data, void (*deleter)(void*)): data(data), deleter(deleter)
		{
		}

		~buffer_holder()
		{
			if (data) deleter(data);
		}

		void* release()
		{
			void* result = data;
			data = 0;
			return result;
		}
	};
}
#endif

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
	
#ifdef PUGIXML_WCHAR_MODE
	#define IS_CHARTYPE_IMPL(c, ct, table) ((static_cast<unsigned int>(c) < 128 ? table[static_cast<unsigned int>(c)] : table[128]) & (ct))
#else
	#define IS_CHARTYPE_IMPL(c, ct, table) (table[static_cast<unsigned char>(c)] & (ct))
#endif

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

#ifdef PUGIXML_WCHAR_MODE
	inline bool need_endian_swap_utf(xml_encoding le, xml_encoding re)
	{
		return (le == encoding_utf16_be && re == encoding_utf16_le) || (le == encoding_utf16_le && re == encoding_utf16_be) ||
		       (le == encoding_utf32_be && re == encoding_utf32_le) || (le == encoding_utf32_le && re == encoding_utf32_be);
	}

	bool convert_buffer_endian_swap(char_t*& out_buffer, size_t& out_length, const void* contents, size_t size, bool is_mutable)
	{
		const char_t* data = static_cast<const char_t*>(contents);
	
		if (is_mutable)
		{
			out_buffer = const_cast<char_t*>(data);
		}
		else
		{
			out_buffer = static_cast<char_t*>(global_allocate(size > 0 ? size : 1));
			if (!out_buffer) return false;
		}

		out_length = size / sizeof(char_t);

		convert_wchar_endian_swap(out_buffer, data, out_length);

		return true;
	}

	bool convert_buffer_utf8(char_t*& out_buffer, size_t& out_length, const void* contents, size_t size)
	{
		const uint8_t* data = static_cast<const uint8_t*>(contents);

		// first pass: get length in wchar_t units
		out_length = utf_decoder<wchar_counter>::decode_utf8_block(data, size, 0);

		// allocate buffer of suitable length
		out_buffer = static_cast<char_t*>(global_allocate((out_length > 0 ? out_length : 1) * sizeof(char_t)));
		if (!out_buffer) return false;

		// second pass: convert utf8 input to wchar_t
		wchar_writer::value_type out_begin = reinterpret_cast<wchar_writer::value_type>(out_buffer);
		wchar_writer::value_type out_end = utf_decoder<wchar_writer>::decode_utf8_block(data, size, out_begin);

		assert(out_end == out_begin + out_length);
		(void)!out_end;

		return true;
	}

	template <typename opt_swap> bool convert_buffer_utf16(char_t*& out_buffer, size_t& out_length, const void* contents, size_t size, opt_swap)
	{
		const uint16_t* data = static_cast<const uint16_t*>(contents);
		size_t length = size / sizeof(uint16_t);

		// first pass: get length in wchar_t units
		out_length = utf_decoder<wchar_counter, opt_swap>::decode_utf16_block(data, length, 0);

		// allocate buffer of suitable length
		out_buffer = static_cast<char_t*>(global_allocate((out_length > 0 ? out_length : 1) * sizeof(char_t)));
		if (!out_buffer) return false;

		// second pass: convert utf16 input to wchar_t
		wchar_writer::value_type out_begin = reinterpret_cast<wchar_writer::value_type>(out_buffer);
		wchar_writer::value_type out_end = utf_decoder<wchar_writer, opt_swap>::decode_utf16_block(data, length, out_begin);

		assert(out_end == out_begin + out_length);
		(void)!out_end;

		return true;
	}

	template <typename opt_swap> bool convert_buffer_utf32(char_t*& out_buffer, size_t& out_length, const void* contents, size_t size, opt_swap)
	{
		const uint32_t* data = static_cast<const uint32_t*>(contents);
		size_t length = size / sizeof(uint32_t);

		// first pass: get length in wchar_t units
		out_length = utf_decoder<wchar_counter, opt_swap>::decode_utf32_block(data, length, 0);

		// allocate buffer of suitable length
		out_buffer = static_cast<char_t*>(global_allocate((out_length > 0 ? out_length : 1) * sizeof(char_t)));
		if (!out_buffer) return false;

		// second pass: convert utf32 input to wchar_t
		wchar_writer::value_type out_begin = reinterpret_cast<wchar_writer::value_type>(out_buffer);
		wchar_writer::value_type out_end = utf_decoder<wchar_writer, opt_swap>::decode_utf32_block(data, length, out_begin);

		assert(out_end == out_begin + out_length);
		(void)!out_end;

		return true;
	}

	bool convert_buffer(char_t*& out_buffer, size_t& out_length, xml_encoding encoding, const void* contents, size_t size, bool is_mutable)
	{
		// get native encoding
		xml_encoding wchar_encoding = get_wchar_encoding();

		// fast path: no conversion required
		if (encoding == wchar_encoding) return get_mutable_buffer(out_buffer, out_length, contents, size, is_mutable);

		// only endian-swapping is required
		if (need_endian_swap_utf(encoding, wchar_encoding)) return convert_buffer_endian_swap(out_buffer, out_length, contents, size, is_mutable);

		// source encoding is utf8
		if (encoding == encoding_utf8) return convert_buffer_utf8(out_buffer, out_length, contents, size);

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
#else
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
#endif

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
    
#ifndef PUGIXML_NO_STL
    std::string as_utf8_impl(const wchar_t* str, size_t length)
    {
		// first pass: get length in utf8 characters
        size_t size = as_utf8_begin(str, length);

		// allocate resulting string
		std::string result;
		result.resize(size);

		// second pass: convert to utf8
		if (size > 0) as_utf8_end(&result[0], size, str, length);

	  	return result;
    }

	std::wstring as_wide_impl(const char* str, size_t size)
	{
		const uint8_t* data = reinterpret_cast<const uint8_t*>(str);

		// first pass: get length in wchar_t units
		size_t length = utf_decoder<wchar_counter>::decode_utf8_block(data, size, 0);

		// allocate resulting string
		std::wstring result;
		result.resize(length);

		// second pass: convert to wchar_t
		if (length > 0)
		{
			wchar_writer::value_type begin = reinterpret_cast<wchar_writer::value_type>(&result[0]);
			wchar_writer::value_type end = utf_decoder<wchar_writer>::decode_utf8_block(data, size, begin);

			assert(begin + length == end);
			(void)!end;
		}

		return result;
	}
#endif

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

			#ifdef PUGIXML_WCHAR_MODE
				s = reinterpret_cast<char_t*>(wchar_writer::any(reinterpret_cast<wchar_writer::value_type>(s), ucsc));
			#else
				s = reinterpret_cast<char_t*>(utf8_writer::any(reinterpret_cast<uint8_t*>(s), ucsc));
			#endif
					
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
	#ifdef PUGIXML_WCHAR_MODE
		return get_wchar_encoding();
	#else
		return encoding_utf8;
	#endif
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

#ifdef PUGIXML_WCHAR_MODE
	size_t get_valid_length(const char_t* data, size_t length)
	{
		assert(length > 0);

		// discard last character if it's the lead of a surrogate pair 
		return (sizeof(wchar_t) == 2 && (unsigned)(static_cast<uint16_t>(data[length - 1]) - 0xD800) < 0x400) ? length - 1 : length;
	}

	size_t convert_buffer(char* result, const char_t* data, size_t length, xml_encoding encoding)
	{
		// only endian-swapping is required
		if (need_endian_swap_utf(encoding, get_wchar_encoding()))
		{
			convert_wchar_endian_swap(reinterpret_cast<char_t*>(result), data, length);

			return length * sizeof(char_t);
		}
	
		// convert to utf8
		if (encoding == encoding_utf8)
		{
			uint8_t* dest = reinterpret_cast<uint8_t*>(result);

			uint8_t* end = sizeof(wchar_t) == 2 ?
				utf_decoder<utf8_writer>::decode_utf16_block(reinterpret_cast<const uint16_t*>(data), length, dest) :
				utf_decoder<utf8_writer>::decode_utf32_block(reinterpret_cast<const uint32_t*>(data), length, dest);

			return static_cast<size_t>(end - dest);
		}

		// convert to utf16
		if (encoding == encoding_utf16_be || encoding == encoding_utf16_le)
		{
			uint16_t* dest = reinterpret_cast<uint16_t*>(result);

			// convert to native utf16
			uint16_t* end = utf_decoder<utf16_writer>::decode_utf32_block(reinterpret_cast<const uint32_t*>(data), length, dest);

			// swap if necessary
			xml_encoding native_encoding = is_little_endian() ? encoding_utf16_le : encoding_utf16_be;

			if (native_encoding != encoding) convert_utf_endian_swap(dest, dest, static_cast<size_t>(end - dest));

			return static_cast<size_t>(end - dest) * sizeof(uint16_t);
		}

		// convert to utf32
		if (encoding == encoding_utf32_be || encoding == encoding_utf32_le)
		{
			uint32_t* dest = reinterpret_cast<uint32_t*>(result);

			// convert to native utf32
			uint32_t* end = utf_decoder<utf32_writer>::decode_utf16_block(reinterpret_cast<const uint16_t*>(data), length, dest);

			// swap if necessary
			xml_encoding native_encoding = is_little_endian() ? encoding_utf32_le : encoding_utf32_be;

			if (native_encoding != encoding) convert_utf_endian_swap(dest, dest, static_cast<size_t>(end - dest));

			return static_cast<size_t>(end - dest) * sizeof(uint32_t);
		}

		assert(!"Invalid encoding");
		return 0;
	}
#else
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
#endif

	class xml_buffered_writer
	{
		xml_buffered_writer(const xml_buffered_writer&);
		xml_buffered_writer& operator=(const xml_buffered_writer&);

	public:
		xml_buffered_writer(xml_writer& writer, xml_encoding user_encoding): writer(writer), bufsize(0), encoding(get_write_encoding(user_encoding))
		{
		}

		~xml_buffered_writer()
		{
			flush();
		}

		void flush()
		{
			flush(buffer, bufsize);
			bufsize = 0;
		}

		void flush(const char_t* data, size_t size)
		{
			if (size == 0) return;

			// fast path, just write data
			if (encoding == get_write_native_encoding())
				writer.write(data, size * sizeof(char_t));
			else
			{
				// convert chunk
				size_t result = convert_buffer(scratch, data, size, encoding);
				assert(result <= sizeof(scratch));

				// write data
				writer.write(scratch, result);
			}
		}

		void write(const char_t* data, size_t length)
		{
			if (bufsize + length > bufcapacity)
			{
				// flush the remaining buffer contents
				flush();

				// handle large chunks
				if (length > bufcapacity)
				{
					if (encoding == get_write_native_encoding())
					{
						// fast path, can just write data chunk
						writer.write(data, length * sizeof(char_t));
						return;
					}

					// need to convert in suitable chunks
					while (length > bufcapacity)
					{
						// get chunk size by selecting such number of characters that are guaranteed to fit into scratch buffer
						// and form a complete codepoint sequence (i.e. discard start of last codepoint if necessary)
						size_t chunk_size = get_valid_length(data, bufcapacity);

						// convert chunk and write
						flush(data, chunk_size);

						// iterate
						data += chunk_size;
						length -= chunk_size;
					}

					// small tail is copied below
					bufsize = 0;
				}
			}

			memcpy(buffer + bufsize, data, length * sizeof(char_t));
			bufsize += length;
		}

		void write(const char_t* data)
		{
			write(data, strlength(data));
		}

		void write(char_t d0)
		{
			if (bufsize + 1 > bufcapacity) flush();

			buffer[bufsize + 0] = d0;
			bufsize += 1;
		}

		void write(char_t d0, char_t d1)
		{
			if (bufsize + 2 > bufcapacity) flush();

			buffer[bufsize + 0] = d0;
			buffer[bufsize + 1] = d1;
			bufsize += 2;
		}

		void write(char_t d0, char_t d1, char_t d2)
		{
			if (bufsize + 3 > bufcapacity) flush();

			buffer[bufsize + 0] = d0;
			buffer[bufsize + 1] = d1;
			buffer[bufsize + 2] = d2;
			bufsize += 3;
		}

		void write(char_t d0, char_t d1, char_t d2, char_t d3)
		{
			if (bufsize + 4 > bufcapacity) flush();

			buffer[bufsize + 0] = d0;
			buffer[bufsize + 1] = d1;
			buffer[bufsize + 2] = d2;
			buffer[bufsize + 3] = d3;
			bufsize += 4;
		}

		void write(char_t d0, char_t d1, char_t d2, char_t d3, char_t d4)
		{
			if (bufsize + 5 > bufcapacity) flush();

			buffer[bufsize + 0] = d0;
			buffer[bufsize + 1] = d1;
			buffer[bufsize + 2] = d2;
			buffer[bufsize + 3] = d3;
			buffer[bufsize + 4] = d4;
			bufsize += 5;
		}

		void write(char_t d0, char_t d1, char_t d2, char_t d3, char_t d4, char_t d5)
		{
			if (bufsize + 6 > bufcapacity) flush();

			buffer[bufsize + 0] = d0;
			buffer[bufsize + 1] = d1;
			buffer[bufsize + 2] = d2;
			buffer[bufsize + 3] = d3;
			buffer[bufsize + 4] = d4;
			buffer[bufsize + 5] = d5;
			bufsize += 6;
		}

		// utf8 maximum expansion: x4 (-> utf32)
		// utf16 maximum expansion: x2 (-> utf32)
		// utf32 maximum expansion: x1
		enum { bufcapacity = 2048 };

		char_t buffer[bufcapacity];
		char scratch[4 * bufcapacity];

		xml_writer& writer;
		size_t bufsize;
		xml_encoding encoding;
	};

	void write_bom(xml_writer& writer, xml_encoding encoding)
	{
		switch (encoding)
		{
		case encoding_utf8:
			writer.write("\xef\xbb\xbf", 3);
			break;

		case encoding_utf16_be:
			writer.write("\xfe\xff", 2);
			break;

		case encoding_utf16_le:
			writer.write("\xff\xfe", 2);
			break;

		case encoding_utf32_be:
			writer.write("\x00\x00\xfe\xff", 4);
			break;

		case encoding_utf32_le:
			writer.write("\xff\xfe\x00\x00", 4);
			break;

		default:
			assert(!"Invalid encoding");
		}
	}

	void text_output_escaped(xml_buffered_writer& writer, const char_t* s, chartypex_t type)
	{
		while (*s)
		{
			const char_t* prev = s;
			
			// While *s is a usual symbol
			while (!IS_CHARTYPEX(*s, type)) ++s;
		
			writer.write(prev, static_cast<size_t>(s - prev));

			switch (*s)
			{
				case 0: break;
				case '&':
					writer.write('&', 'a', 'm', 'p', ';');
					++s;
					break;
				case '<':
					writer.write('&', 'l', 't', ';');
					++s;
					break;
				case '>':
					writer.write('&', 'g', 't', ';');
					++s;
					break;
				case '"':
					writer.write('&', 'q', 'u', 'o', 't', ';');
					++s;
					break;
				default: // s is not a usual symbol
				{
					unsigned int ch = static_cast<unsigned int>(*s++);
					assert(ch < 32);

					writer.write('&', '#', static_cast<char_t>((ch / 10) + '0'), static_cast<char_t>((ch % 10) + '0'), ';');
				}
			}
		}
	}

	void text_output_cdata(xml_buffered_writer& writer, const char_t* s)
	{
		do
		{
			writer.write('<', '!', '[', 'C', 'D');
			writer.write('A', 'T', 'A', '[');

			const char_t* prev = s;

			// look for ]]> sequence - we can't output it as is since it terminates CDATA
			while (*s && !(s[0] == ']' && s[1] == ']' && s[2] == '>')) ++s;

			// skip ]] if we stopped at ]]>, > will go to the next CDATA section
			if (*s) s += 2;

			writer.write(prev, static_cast<size_t>(s - prev));

			writer.write(']', ']', '>');
		}
		while (*s);
	}

	void node_output_attributes(xml_buffered_writer& writer, const xml_node& node)
	{
		const char_t* default_name = PUGIXML_TEXT(":anonymous");

		for (xml_attribute a = node.first_attribute(); a; a = a.next_attribute())
		{
			writer.write(' ');
			writer.write(a.name()[0] ? a.name() : default_name);
			writer.write('=', '"');

			text_output_escaped(writer, a.value(), ctx_special_attr);

			writer.write('"');
		}
	}

	void node_output(xml_buffered_writer& writer, const xml_node& node, const char_t* indent, unsigned int flags, unsigned int depth)
	{
		const char_t* default_name = PUGIXML_TEXT(":anonymous");

		if ((flags & format_indent) != 0 && (flags & format_raw) == 0)
			for (unsigned int i = 0; i < depth; ++i) writer.write(indent);

		switch (node.type())
		{
		case node_document:
		{
			for (xml_node n = node.first_child(); n; n = n.next_sibling())
				node_output(writer, n, indent, flags, depth);
			break;
		}
			
		case node_element:
		{
			const char_t* name = node.name()[0] ? node.name() : default_name;

			writer.write('<');
			writer.write(name);

			node_output_attributes(writer, node);

			if (flags & format_raw)
			{
				if (!node.first_child())
					writer.write(' ', '/', '>');
				else
				{
					writer.write('>');

					for (xml_node n = node.first_child(); n; n = n.next_sibling())
						node_output(writer, n, indent, flags, depth + 1);

					writer.write('<', '/');
					writer.write(name);
					writer.write('>');
				}
			}
			else if (!node.first_child())
				writer.write(' ', '/', '>', '\n');
			else if (node.first_child() == node.last_child() && (node.first_child().type() == node_pcdata || node.first_child().type() == node_cdata))
			{
				writer.write('>');

                if (node.first_child().type() == node_pcdata)
                    text_output_escaped(writer, node.first_child().value(), ctx_special_pcdata);
                else
                    text_output_cdata(writer, node.first_child().value());

				writer.write('<', '/');
				writer.write(name);
				writer.write('>', '\n');
			}
			else
			{
				writer.write('>', '\n');
				
				for (xml_node n = node.first_child(); n; n = n.next_sibling())
					node_output(writer, n, indent, flags, depth + 1);

				if ((flags & format_indent) != 0 && (flags & format_raw) == 0)
					for (unsigned int i = 0; i < depth; ++i) writer.write(indent);
				
				writer.write('<', '/');
				writer.write(name);
				writer.write('>', '\n');
			}

			break;
		}
		
		case node_pcdata:
			text_output_escaped(writer, node.value(), ctx_special_pcdata);
			if ((flags & format_raw) == 0) writer.write('\n');
			break;

		case node_cdata:
			text_output_cdata(writer, node.value());
			if ((flags & format_raw) == 0) writer.write('\n');
			break;

		case node_comment:
			writer.write('<', '!', '-', '-');
			writer.write(node.value());
			writer.write('-', '-', '>');
			if ((flags & format_raw) == 0) writer.write('\n');
			break;

		case node_pi:
		case node_declaration:
			writer.write('<', '?');
			writer.write(node.name()[0] ? node.name() : default_name);

			if (node.type() == node_declaration)
			{
				node_output_attributes(writer, node);
			}
			else if (node.value()[0])
			{
				writer.write(' ');
				writer.write(node.value());
			}

			writer.write('?', '>');
			if ((flags & format_raw) == 0) writer.write('\n');
			break;

		case node_doctype:
			writer.write('<', '!', 'D', 'O', 'C');
			writer.write('T', 'Y', 'P', 'E');

            if (node.value()[0])
            {
                writer.write(' ');
                writer.write(node.value());
            }

            writer.write('>');
			if ((flags & format_raw) == 0) writer.write('\n');
			break;

		default:
			assert(!"Invalid node type");
		}
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

	// we need to get length of entire file to load it in memory; the only (relatively) sane way to do it is via seek/tell trick
	xml_parse_status get_file_size(FILE* file, size_t& out_result)
	{
	#if defined(_MSC_VER) && _MSC_VER >= 1400
		// there are 64-bit versions of fseek/ftell, let's use them
		typedef __int64 length_type;

		_fseeki64(file, 0, SEEK_END);
		length_type length = _ftelli64(file);
		_fseeki64(file, 0, SEEK_SET);
	#elif defined(__MINGW32__) && !defined(__NO_MINGW_LFS) && !defined(__STRICT_ANSI__)
		// there are 64-bit versions of fseek/ftell, let's use them
		typedef off64_t length_type;

		fseeko64(file, 0, SEEK_END);
		length_type length = ftello64(file);
		fseeko64(file, 0, SEEK_SET);
	#else
		// if this is a 32-bit OS, long is enough; if this is a unix system, long is 64-bit, which is enough; otherwise we can't do anything anyway.
		typedef long length_type;

		fseek(file, 0, SEEK_END);
		length_type length = ftell(file);
		fseek(file, 0, SEEK_SET);
	#endif

		// check for I/O errors
		if (length < 0) return status_io_error;
		
		// check for overflow
		size_t result = static_cast<size_t>(length);

		if (static_cast<length_type>(result) != length) return status_out_of_memory;

		// finalize
		out_result = result;

		return status_ok;
	}

	xml_parse_result load_file_impl(xml_document& doc, FILE* file, unsigned int options, xml_encoding encoding)
	{
		if (!file) return make_parse_result(status_file_not_found);

		// get file size (can result in I/O errors)
		size_t size = 0;
		xml_parse_status size_status = get_file_size(file, size);

		if (size_status != status_ok)
		{
			fclose(file);
			return make_parse_result(size_status);
		}
		
		// allocate buffer for the whole file
		char* contents = static_cast<char*>(global_allocate(size > 0 ? size : 1));

		if (!contents)
		{
			fclose(file);
			return make_parse_result(status_out_of_memory);
		}

		// read file in memory
		size_t read_size = fread(contents, 1, size, file);
		fclose(file);

		if (read_size != size)
		{
			global_deallocate(contents);
			return make_parse_result(status_io_error);
		}
		
		return doc.load_buffer_inplace_own(contents, size, options, encoding);
	}

#ifndef PUGIXML_NO_STL
	template <typename T> xml_parse_result load_stream_impl(xml_document& doc, std::basic_istream<T>& stream, unsigned int options, xml_encoding encoding)
	{
		// get length of remaining data in stream
		typename std::basic_istream<T>::pos_type pos = stream.tellg();
		stream.seekg(0, std::ios::end);
		std::streamoff length = stream.tellg() - pos;
		stream.seekg(pos);

		if (stream.fail() || pos < 0) return make_parse_result(status_io_error);

		// guard against huge files
		size_t read_length = static_cast<size_t>(length);

		if (static_cast<std::streamsize>(read_length) != length || length < 0) return make_parse_result(status_out_of_memory);

		// read stream data into memory (guard against stream exceptions with buffer holder)
		buffer_holder buffer(global_allocate((read_length > 0 ? read_length : 1) * sizeof(T)), global_deallocate);
		if (!buffer.data) return make_parse_result(status_out_of_memory);

		stream.read(static_cast<T*>(buffer.data), static_cast<std::streamsize>(read_length));

		// read may set failbit | eofbit in case gcount() is less than read_length (i.e. line ending conversion), so check for other I/O errors
		if (stream.bad()) return make_parse_result(status_io_error);

		// load data from buffer
		size_t actual_length = static_cast<size_t>(stream.gcount());
		assert(actual_length <= read_length);

		return doc.load_buffer_inplace_own(buffer.release(), actual_length * sizeof(T), options, encoding);
	}
#endif

#if defined(_MSC_VER) || defined(__BORLANDC__) || defined(__MINGW32__)
	FILE* open_file_wide(const wchar_t* path, const wchar_t* mode)
	{
		return _wfopen(path, mode);
	}
#else
	char* convert_path_heap(const wchar_t* str)
	{
		assert(str);

		// first pass: get length in utf8 characters
		size_t length = wcslen(str);
        size_t size = as_utf8_begin(str, length);

		// allocate resulting string
		char* result = static_cast<char*>(global_allocate(size + 1));
		if (!result) return 0;

		// second pass: convert to utf8
        as_utf8_end(result, size, str, length);

	  	return result;
	}

	FILE* open_file_wide(const wchar_t* path, const wchar_t* mode)
	{
		// there is no standard function to open wide paths, so our best bet is to try utf8 path
		char* path_utf8 = convert_path_heap(path);
		if (!path_utf8) return 0;

		// convert mode to ASCII (we mirror _wfopen interface)
		char mode_ascii[4] = {0};
		for (size_t i = 0; mode[i]; ++i) mode_ascii[i] = static_cast<char>(mode[i]);

		// try to open the utf8 path
		FILE* result = fopen(path_utf8, mode_ascii);

		// free dummy buffer
		global_deallocate(path_utf8);

		return result;
	}
#endif
}

namespace pugi
{
	xml_writer_file::xml_writer_file(void* file): file(file)
	{
	}

	void xml_writer_file::write(const void* data, size_t size)
	{
		fwrite(data, size, 1, static_cast<FILE*>(file));
	}

#ifndef PUGIXML_NO_STL
	xml_writer_stream::xml_writer_stream(std::basic_ostream<char, std::char_traits<char> >& stream): narrow_stream(&stream), wide_stream(0)
	{
	}

	xml_writer_stream::xml_writer_stream(std::basic_ostream<wchar_t, std::char_traits<wchar_t> >& stream): narrow_stream(0), wide_stream(&stream)
	{
	}

	void xml_writer_stream::write(const void* data, size_t size)
	{
		if (narrow_stream)
		{
			assert(!wide_stream);
			narrow_stream->write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
		}
		else
		{
			assert(wide_stream);
			assert(size % sizeof(wchar_t) == 0);

			wide_stream->write(reinterpret_cast<const wchar_t*>(data), static_cast<std::streamsize>(size / sizeof(wchar_t)));
		}
	}
#endif

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
	
	#ifdef PUGIXML_WCHAR_MODE
		char_t wbuf[128];
		widen_ascii(wbuf, buf);

		return set_value(wbuf);
	#else
		return set_value(buf);
	#endif
	}

	bool xml_attribute::set_value(unsigned int rhs)
	{
		char buf[128];
		sprintf(buf, "%u", rhs);

	#ifdef PUGIXML_WCHAR_MODE
		char_t wbuf[128];
		widen_ascii(wbuf, buf);

		return set_value(wbuf);
	#else
		return set_value(buf);
	#endif
	}

	bool xml_attribute::set_value(double rhs)
	{
		char buf[128];
		sprintf(buf, "%g", rhs);

	#ifdef PUGIXML_WCHAR_MODE
		char_t wbuf[128];
		widen_ascii(wbuf, buf);

		return set_value(wbuf);
	#else
		return set_value(buf);
	#endif
	}
	
	bool xml_attribute::set_value(bool rhs)
	{
		return set_value(rhs ? PUGIXML_TEXT("true") : PUGIXML_TEXT("false"));
	}

#ifdef __BORLANDC__
	bool operator&&(const xml_attribute& lhs, bool rhs)
	{
		return (bool)lhs && rhs;
	}

	bool operator||(const xml_attribute& lhs, bool rhs)
	{
		return (bool)lhs || rhs;
	}
#endif

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

#ifndef PUGIXML_NO_STL
	string_t xml_node::path(char_t delimiter) const
	{
		string_t path;

		xml_node cursor = *this; // Make a copy.
		
		path = cursor.name();

		while (cursor.parent())
		{
			cursor = cursor.parent();
			
			string_t temp = cursor.name();
			temp += delimiter;
			temp += path;
			path.swap(temp);
		}

		return path;
	}
#endif

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

	void xml_node::print(xml_writer& writer, const char_t* indent, unsigned int flags, xml_encoding encoding, unsigned int depth) const
	{
		if (!_root) return;

		xml_buffered_writer buffered_writer(writer, encoding);

		node_output(buffered_writer, *this, indent, flags, depth);
	}

#ifndef PUGIXML_NO_STL
	void xml_node::print(std::basic_ostream<char, std::char_traits<char> >& stream, const char_t* indent, unsigned int flags, xml_encoding encoding, unsigned int depth) const
	{
		xml_writer_stream writer(stream);

		print(writer, indent, flags, encoding, depth);
	}

	void xml_node::print(std::basic_ostream<wchar_t, std::char_traits<wchar_t> >& stream, const char_t* indent, unsigned int flags, unsigned int depth) const
	{
		xml_writer_stream writer(stream);

		print(writer, indent, flags, encoding_wchar, depth);
	}
#endif

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

#ifdef __BORLANDC__
	bool operator&&(const xml_node& lhs, bool rhs)
	{
		return (bool)lhs && rhs;
	}

	bool operator||(const xml_node& lhs, bool rhs)
	{
		return (bool)lhs || rhs;
	}
#endif

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

#ifndef PUGIXML_NO_STL
	xml_parse_result xml_document::load(std::basic_istream<char, std::char_traits<char> >& stream, unsigned int options, xml_encoding encoding)
	{
		reset();

		return load_stream_impl(*this, stream, options, encoding);
	}

	xml_parse_result xml_document::load(std::basic_istream<wchar_t, std::char_traits<wchar_t> >& stream, unsigned int options)
	{
		reset();

		return load_stream_impl(*this, stream, options, encoding_wchar);
	}
#endif

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

	xml_parse_result xml_document::load_file(const char* path, unsigned int options, xml_encoding encoding)
	{
		reset();

		FILE* file = fopen(path, "rb");

		return load_file_impl(*this, file, options, encoding);
	}

	xml_parse_result xml_document::load_file(const wchar_t* path, unsigned int options, xml_encoding encoding)
	{
		reset();

		FILE* file = open_file_wide(path, L"rb");

		return load_file_impl(*this, file, options, encoding);
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

	void xml_document::save(xml_writer& writer, const char_t* indent, unsigned int flags, xml_encoding encoding) const
	{
		if (flags & format_write_bom) write_bom(writer, get_write_encoding(encoding));

		xml_buffered_writer buffered_writer(writer, encoding);

		if (!(flags & format_no_declaration) && !has_declaration(*this))
		{
			buffered_writer.write(PUGIXML_TEXT("<?xml version=\"1.0\"?>"));
			if (!(flags & format_raw)) buffered_writer.write('\n');
		}

		node_output(buffered_writer, *this, indent, flags, 0);
	}

#ifndef PUGIXML_NO_STL
	void xml_document::save(std::basic_ostream<char, std::char_traits<char> >& stream, const char_t* indent, unsigned int flags, xml_encoding encoding) const
	{
		xml_writer_stream writer(stream);

		save(writer, indent, flags, encoding);
	}

	void xml_document::save(std::basic_ostream<wchar_t, std::char_traits<wchar_t> >& stream, const char_t* indent, unsigned int flags) const
	{
		xml_writer_stream writer(stream);

		save(writer, indent, flags, encoding_wchar);
	}
#endif

	bool xml_document::save_file(const char* path, const char_t* indent, unsigned int flags, xml_encoding encoding) const
	{
		FILE* file = fopen(path, "wb");
		if (!file) return false;

		xml_writer_file writer(file);
		save(writer, indent, flags, encoding);

		fclose(file);

		return true;
	}

	bool xml_document::save_file(const wchar_t* path, const char_t* indent, unsigned int flags, xml_encoding encoding) const
	{
		FILE* file = open_file_wide(path, L"wb");
		if (!file) return false;

		xml_writer_file writer(file);
		save(writer, indent, flags, encoding);

		fclose(file);

		return true;
	}

    xml_node xml_document::document_element() const
    {
		for (xml_node_struct* i = _root->first_child; i; i = i->next_sibling)
			if ((i->header & xml_memory_page_type_mask) + 1 == node_element)
                return xml_node(i);

        return xml_node();
    }

#ifndef PUGIXML_NO_STL
	std::string PUGIXML_FUNCTION as_utf8(const wchar_t* str)
	{
		assert(str);

        return as_utf8_impl(str, wcslen(str));
	}

	std::string PUGIXML_FUNCTION as_utf8(const std::wstring& str)
	{
        return as_utf8_impl(str.c_str(), str.size());
	}
	
	std::wstring PUGIXML_FUNCTION as_wide(const char* str)
	{
		assert(str);

        return as_wide_impl(str, strlen(str));
	}
	
	std::wstring PUGIXML_FUNCTION as_wide(const std::string& str)
	{
        return as_wide_impl(str.c_str(), str.size());
	}
#endif

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

#if !defined(PUGIXML_NO_STL) && (defined(_MSC_VER) || defined(__ICC))
namespace std
{
	// Workarounds for (non-standard) iterator category detection for older versions (MSVC7/IC8 and earlier)
	std::bidirectional_iterator_tag _Iter_cat(const xml_node_iterator&)
	{
		return std::bidirectional_iterator_tag();
	}

	std::bidirectional_iterator_tag _Iter_cat(const xml_attribute_iterator&)
	{
		return std::bidirectional_iterator_tag();
	}
}
#endif

#if !defined(PUGIXML_NO_STL) && defined(__SUNPRO_CC)
namespace std
{
	// Workarounds for (non-standard) iterator category detection
	std::bidirectional_iterator_tag __iterator_category(const xml_node_iterator&)
	{
		return std::bidirectional_iterator_tag();
	}

	std::bidirectional_iterator_tag __iterator_category(const xml_attribute_iterator&)
	{
		return std::bidirectional_iterator_tag();
	}
}
#endif

#ifndef PUGIXML_NO_XPATH

// STL replacements
namespace
{
	struct equal_to
	{
		template <typename T> bool operator()(const T& lhs, const T& rhs) const
		{
			return lhs == rhs;
		}
	};

	struct not_equal_to
	{
		template <typename T> bool operator()(const T& lhs, const T& rhs) const
		{
			return lhs != rhs;
		}
	};

	struct less
	{
		template <typename T> bool operator()(const T& lhs, const T& rhs) const
		{
			return lhs < rhs;
		}
	};

	struct less_equal
	{
		template <typename T> bool operator()(const T& lhs, const T& rhs) const
		{
			return lhs <= rhs;
		}
	};

	template <typename T> void swap(T& lhs, T& rhs)
	{
		T temp = lhs;
		lhs = rhs;
		rhs = temp;
	}

	template <typename I, typename Pred> I min_element(I begin, I end, const Pred& pred)
	{
		I result = begin;

		for (I it = begin + 1; it != end; ++it)
			if (pred(*it, *result))
				result = it;

		return result;
	}

	template <typename I> void reverse(I begin, I end)
	{
		while (begin + 1 < end) swap(*begin++, *--end);
	}

	template <typename I> I unique(I begin, I end)
	{
		// fast skip head
		while (begin + 1 < end && *begin != *(begin + 1)) begin++;

		if (begin == end) return begin;

		// last written element
		I write = begin++; 

		// merge unique elements
		while (begin != end)
		{
			if (*begin != *write)
				*++write = *begin++;
			else
				begin++;
		}

		// past-the-end (write points to live element)
		return write + 1;
	}

	template <typename I> void copy_backwards(I begin, I end, I target)
	{
		while (begin != end) *--target = *--end;
	}

	template <typename I, typename Pred, typename T> void insertion_sort(I begin, I end, const Pred& pred, T*)
	{
		assert(begin != end);

		for (I it = begin + 1; it != end; ++it)
		{
			T val = *it;

			if (pred(val, *begin))
			{
				// move to front
				copy_backwards(begin, it, it + 1);
				*begin = val;
			}
			else
			{
				I hole = it;

				// move hole backwards
				while (pred(val, *(hole - 1)))
				{
					*hole = *(hole - 1);
					hole--;
				}

				// fill hole with element
				*hole = val;
			}
		}
	}

	// std variant for elements with ==
	template <typename I, typename Pred> void partition(I begin, I middle, I end, const Pred& pred, I* out_eqbeg, I* out_eqend)
	{
		I eqbeg = middle, eqend = middle + 1;

		// expand equal range
		while (eqbeg != begin && *(eqbeg - 1) == *eqbeg) --eqbeg;
		while (eqend != end && *eqend == *eqbeg) ++eqend;

		// process outer elements
		I ltend = eqbeg, gtbeg = eqend;

		for (;;)
		{
			// find the element from the right side that belongs to the left one
			for (; gtbeg != end; ++gtbeg)
				if (!pred(*eqbeg, *gtbeg))
				{
					if (*gtbeg == *eqbeg) swap(*gtbeg, *eqend++);
					else break;
				}

			// find the element from the left side that belongs to the right one
			for (; ltend != begin; --ltend)
				if (!pred(*(ltend - 1), *eqbeg))
				{
					if (*eqbeg == *(ltend - 1)) swap(*(ltend - 1), *--eqbeg);
					else break;
				}

			// scanned all elements
			if (gtbeg == end && ltend == begin)
			{
				*out_eqbeg = eqbeg;
				*out_eqend = eqend;
				return;
			}

			// make room for elements by moving equal area
			if (gtbeg == end)
			{
				if (--ltend != --eqbeg) swap(*ltend, *eqbeg);
				swap(*eqbeg, *--eqend);
			}
			else if (ltend == begin)
			{
				if (eqend != gtbeg) swap(*eqbeg, *eqend);
				++eqend;
				swap(*gtbeg++, *eqbeg++);
			}
			else swap(*gtbeg++, *--ltend);
		}
	}

	template <typename I, typename Pred> void median3(I first, I middle, I last, const Pred& pred)
	{
		if (pred(*middle, *first)) swap(*middle, *first);
		if (pred(*last, *middle)) swap(*last, *middle);
		if (pred(*middle, *first)) swap(*middle, *first);
	}

	template <typename I, typename Pred> void median(I first, I middle, I last, const Pred& pred)
	{
		if (last - first <= 40)
		{
			// median of three for small chunks
			median3(first, middle, last, pred);
		}
		else
		{
			// median of nine
			size_t step = (last - first + 1) / 8;

			median3(first, first + step, first + 2 * step, pred);
			median3(middle - step, middle, middle + step, pred);
			median3(last - 2 * step, last - step, last, pred);
			median3(first + step, middle, last - step, pred);
		}
	}

	template <typename I, typename Pred> void sort(I begin, I end, const Pred& pred)
	{
		// sort large chunks
		while (end - begin > 32)
		{
			// find median element
			I middle = begin + (end - begin) / 2;
			median(begin, middle, end - 1, pred);

			// partition in three chunks (< = >)
			I eqbeg, eqend;
			partition(begin, middle, end, pred, &eqbeg, &eqend);

			// loop on larger half
			if (eqbeg - begin > end - eqend)
			{
				sort(eqend, end, pred);
				end = eqbeg;
			}
			else
			{
				sort(begin, eqbeg, pred);
				begin = eqend;
			}
		}

		// insertion sort small chunk
		if (begin != end) insertion_sort(begin, end, pred, &*begin);
	}
}

// Allocator used for AST and evaluation stacks
namespace
{
	struct xpath_memory_block
	{	
		xpath_memory_block* next;

		char data[4096];
	};
		
	class xpath_allocator
	{
		xpath_memory_block* _root;
		size_t _root_size;

	public:
	#ifdef PUGIXML_NO_EXCEPTIONS
		jmp_buf* error_handler;
	#endif

		xpath_allocator(xpath_memory_block* root, size_t root_size = 0): _root(root), _root_size(root_size)
		{
		#ifdef PUGIXML_NO_EXCEPTIONS
			error_handler = 0;
		#endif
		}
		
		void* allocate_nothrow(size_t size)
		{
			const size_t block_capacity = sizeof(_root->data);

			// align size so that we're able to store pointers in subsequent blocks
			size = (size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);

			if (_root_size + size <= block_capacity)
			{
				void* buf = _root->data + _root_size;
				_root_size += size;
				return buf;
			}
			else
			{
				size_t block_data_size = (size > block_capacity) ? size : block_capacity;
				size_t block_size = block_data_size + offsetof(xpath_memory_block, data);

				xpath_memory_block* block = static_cast<xpath_memory_block*>(global_allocate(block_size));
				if (!block) return 0;
				
				block->next = _root;
				
				_root = block;
				_root_size = size;
				
				return block->data;
			}
		}

		void* allocate(size_t size)
		{
			void* result = allocate_nothrow(size);

			if (!result)
			{
			#ifdef PUGIXML_NO_EXCEPTIONS
				assert(error_handler);
				longjmp(*error_handler, 1);
			#else
				throw std::bad_alloc();
			#endif
			}

			return result;
		}

		void* reallocate(void* ptr, size_t old_size, size_t new_size)
		{
			// align size so that we're able to store pointers in subsequent blocks
			old_size = (old_size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
			new_size = (new_size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);

			// we can only reallocate the last object
			assert(ptr == 0 || static_cast<char*>(ptr) + old_size == _root->data + _root_size);

			// adjust root size so that we have not allocated the object at all
			bool only_object = (_root_size == old_size);

			if (ptr) _root_size -= old_size;

			// allocate a new version (this will obviously reuse the memory if possible)
			void* result = allocate(new_size);
			assert(result);

			// we have a new block
			if (result != ptr && ptr)
			{
				// copy old data
				assert(new_size > old_size);
				memcpy(result, ptr, old_size);

				// free the previous page if it had no other objects
				if (only_object)
				{
					assert(_root->data == result);
					assert(_root->next);

					xpath_memory_block* next = _root->next->next;

					if (next)
					{
						// deallocate the whole page, unless it was the first one
						global_deallocate(_root->next);
						_root->next = next;
					}
				}
			}

			return result;
		}

		void revert(const xpath_allocator& state)
		{
			// free all new pages
			xpath_memory_block* cur = _root;

			while (cur != state._root)
			{
				xpath_memory_block* next = cur->next;

				global_deallocate(cur);

				cur = next;
			}

			// restore state
			_root = state._root;
			_root_size = state._root_size;
		}

		void release()
		{
			xpath_memory_block* cur = _root;
			assert(cur);

			while (cur->next)
			{
				xpath_memory_block* next = cur->next;

				global_deallocate(cur);

				cur = next;
			}
		}
	};

	struct xpath_allocator_capture
	{
		xpath_allocator_capture(xpath_allocator* alloc): _target(alloc), _state(*alloc)
		{
		}

		~xpath_allocator_capture()
		{
			_target->revert(_state);
		}

		xpath_allocator* _target;
		xpath_allocator _state;
	};

	struct xpath_stack
	{
		xpath_allocator* result;
		xpath_allocator* temp;
	};

	struct xpath_stack_data
	{
		xpath_memory_block blocks[2];
		xpath_allocator result;
		xpath_allocator temp;
		xpath_stack stack;

	#ifdef PUGIXML_NO_EXCEPTIONS
		jmp_buf error_handler;
	#endif

		xpath_stack_data(): result(blocks + 0), temp(blocks + 1)
		{
			blocks[0].next = blocks[1].next = 0;

			stack.result = &result;
			stack.temp = &temp;

		#ifdef PUGIXML_NO_EXCEPTIONS
			result.error_handler = temp.error_handler = &error_handler;
		#endif
		}

		~xpath_stack_data()
		{
			result.release();
			temp.release();
		}
	};
}

// String class
namespace
{
	class xpath_string
	{
		const char_t* _buffer;
		bool _uses_heap;

		static char_t* duplicate_string(const char_t* string, size_t length, xpath_allocator* alloc)
		{
			char_t* result = static_cast<char_t*>(alloc->allocate((length + 1) * sizeof(char_t)));
			assert(result);

			memcpy(result, string, length * sizeof(char_t));
			result[length] = 0;

			return result;
		}

		static char_t* duplicate_string(const char_t* string, xpath_allocator* alloc)
		{
			return duplicate_string(string, strlength(string), alloc);
		}

	public:
		xpath_string(): _buffer(PUGIXML_TEXT("")), _uses_heap(false)
		{
		}

		explicit xpath_string(const char_t* str, xpath_allocator* alloc)
		{
			bool empty = (*str == 0);

			_buffer = empty ? PUGIXML_TEXT("") : duplicate_string(str, alloc);
			_uses_heap = !empty;
		}

		explicit xpath_string(const char_t* str, bool use_heap): _buffer(str), _uses_heap(use_heap)
		{
		}

		xpath_string(const char_t* begin, const char_t* end, xpath_allocator* alloc)
		{
			assert(begin <= end);

			bool empty = (begin == end);

			_buffer = empty ? PUGIXML_TEXT("") : duplicate_string(begin, static_cast<size_t>(end - begin), alloc);
			_uses_heap = !empty;
		}

		void append(const xpath_string& o, xpath_allocator* alloc)
		{
			// skip empty sources
			if (!*o._buffer) return;

			// fast append for constant empty target and constant source
			if (!*_buffer && !_uses_heap && !o._uses_heap)
			{
				_buffer = o._buffer;
			}
			else
			{
				// need to make heap copy
				size_t target_length = strlength(_buffer);
				size_t source_length = strlength(o._buffer);
				size_t length = target_length + source_length;

				// allocate new buffer
				char_t* result = static_cast<char_t*>(alloc->reallocate(_uses_heap ? const_cast<char_t*>(_buffer) : 0, (target_length + 1) * sizeof(char_t), (length + 1) * sizeof(char_t)));
				assert(result);

				// append first string to the new buffer in case there was no reallocation
				if (!_uses_heap) memcpy(result, _buffer, target_length * sizeof(char_t));

				// append second string to the new buffer
				memcpy(result + target_length, o._buffer, source_length * sizeof(char_t));
				result[length] = 0;

				// finalize
				_buffer = result;
				_uses_heap = true;
			}
		}

		const char_t* c_str() const
		{
			return _buffer;
		}

		size_t length() const
		{
			return strlength(_buffer);
		}
		
		char_t* data(xpath_allocator* alloc)
		{
			// make private heap copy
			if (!_uses_heap)
			{
				_buffer = duplicate_string(_buffer, alloc);
				_uses_heap = true;
			}

			return const_cast<char_t*>(_buffer);
		}

		bool empty() const
		{
			return *_buffer == 0;
		}

		bool operator==(const xpath_string& o) const
		{
			return strequal(_buffer, o._buffer);
		}

		bool operator!=(const xpath_string& o) const
		{
			return !strequal(_buffer, o._buffer);
		}

		bool uses_heap() const
		{
			return _uses_heap;
		}
	};

	xpath_string xpath_string_const(const char_t* str)
	{
		return xpath_string(str, false);
	}
}

namespace
{
	bool starts_with(const char_t* string, const char_t* pattern)
	{
		while (*pattern && *string == *pattern)
		{
			string++;
			pattern++;
		}

		return *pattern == 0;
	}

	const char_t* find_char(const char_t* s, char_t c)
	{
	#ifdef PUGIXML_WCHAR_MODE
		return wcschr(s, c);
	#else
		return strchr(s, c);
	#endif
	}

	const char_t* find_substring(const char_t* s, const char_t* p)
	{
	#ifdef PUGIXML_WCHAR_MODE
		// MSVC6 wcsstr bug workaround (if s is empty it always returns 0)
		return (*p == 0) ? s : wcsstr(s, p);
	#else
		return strstr(s, p);
	#endif
	}

	// Converts symbol to lower case, if it is an ASCII one
	char_t tolower_ascii(char_t ch)
	{
		return static_cast<unsigned int>(ch - 'A') < 26 ? static_cast<char_t>(ch | ' ') : ch;
	}

	xpath_string string_value(const xpath_node& na, xpath_allocator* alloc)
	{
		if (na.attribute())
			return xpath_string_const(na.attribute().value());
		else
		{
			const xml_node& n = na.node();

			switch (n.type())
			{
			case node_pcdata:
			case node_cdata:
			case node_comment:
			case node_pi:
				return xpath_string_const(n.value());
			
			case node_document:
			case node_element:
			{
				xpath_string result;

				xml_node cur = n.first_child();
				
				while (cur && cur != n)
				{
					if (cur.type() == node_pcdata || cur.type() == node_cdata)
						result.append(xpath_string_const(cur.value()), alloc);

					if (cur.first_child())
						cur = cur.first_child();
					else if (cur.next_sibling())
						cur = cur.next_sibling();
					else
					{
						while (!cur.next_sibling() && cur != n)
							cur = cur.parent();

						if (cur != n) cur = cur.next_sibling();
					}
				}
				
				return result;
			}
			
			default:
				return xpath_string();
			}
		}
	}
	
	unsigned int node_height(xml_node n)
	{
	    unsigned int result = 0;
	    
	    while (n)
	    {
	        ++result;
	        n = n.parent();
	    }
	    
	    return result;
	}
	
	bool node_is_before(xml_node ln, unsigned int lh, xml_node rn, unsigned int rh)
	{
		// normalize heights
		for (unsigned int i = rh; i < lh; i++) ln = ln.parent();
		for (unsigned int j = lh; j < rh; j++) rn = rn.parent();
	    
		// one node is the ancestor of the other
	    if (ln == rn) return lh < rh;
	    
		// find common ancestor
	    while (ln.parent() != rn.parent())
	    {
	        ln = ln.parent();
	        rn = rn.parent();
	    }

		// there is no common ancestor (the shared parent is null), nodes are from different documents
		if (!ln.parent()) return ln < rn;

		// determine sibling order
        for (; ln; ln = ln.next_sibling())
            if (ln == rn)
                return true;
                
        return false;
    }

    bool node_is_ancestor(xml_node parent, xml_node node)
    {
    	while (node && node != parent) node = node.parent();

    	return parent && node == parent;
    }

    const void* document_order(const xpath_node& xnode)
    {
        xml_node_struct* node = xnode.node().internal_object();

        if (node)
        {
            if (node->name && (node->header & xml_memory_page_name_allocated_mask) == 0) return node->name;
            if (node->value && (node->header & xml_memory_page_value_allocated_mask) == 0) return node->value;
            return 0;
        }

        xml_attribute_struct* attr = xnode.attribute().internal_object();

        if (attr)
        {
            if ((attr->header & xml_memory_page_name_allocated_mask) == 0) return attr->name;
            if ((attr->header & xml_memory_page_value_allocated_mask) == 0) return attr->value;
            return 0;
        }

		return 0;
    }
    
	struct document_order_comparator
	{
		bool operator()(const xpath_node& lhs, const xpath_node& rhs) const
		{
			// optimized document order based check
			const void* lo = document_order(lhs);
			const void* ro = document_order(rhs);

			if (lo && ro) return lo < ro;

            // slow comparison
			xml_node ln = lhs.node(), rn = rhs.node();

			// compare attributes
			if (lhs.attribute() && rhs.attribute())
			{
				// shared parent
				if (lhs.parent() == rhs.parent())
				{
					// determine sibling order
				    for (xml_attribute a = lhs.attribute(); a; a = a.next_attribute())
				        if (a == rhs.attribute())
				            return true;
				    
				    return false;
				}
				
				// compare attribute parents
				ln = lhs.parent();
				rn = rhs.parent();
			}
			else if (lhs.attribute())
			{
				// attributes go after the parent element
				if (lhs.parent() == rhs.node()) return false;
				
				ln = lhs.parent();
			}
			else if (rhs.attribute())
			{
				// attributes go after the parent element
				if (rhs.parent() == lhs.node()) return true;
				
				rn = rhs.parent();
			}

			if (ln == rn) return false;
			
			unsigned int lh = node_height(ln);
			unsigned int rh = node_height(rn);
			
			return node_is_before(ln, lh, rn, rh);
		}
	};

	struct duplicate_comparator
	{
		bool operator()(const xpath_node& lhs, const xpath_node& rhs) const
		{
			if (lhs.attribute()) return rhs.attribute() ? lhs.attribute() < rhs.attribute() : true;
			else return rhs.attribute() ? false : lhs.node() < rhs.node();
		}
	};
	
	double gen_nan()
	{
	#if defined(__STDC_IEC_559__) || ((FLT_RADIX - 0 == 2) && (FLT_MAX_EXP - 0 == 128) && (FLT_MANT_DIG - 0 == 24))
		union { float f; int32_t i; } u[sizeof(float) == sizeof(int32_t) ? 1 : -1];
		u[0].i = 0x7fc00000;
		return u[0].f;
	#else
		// fallback
		const volatile double zero = 0.0;
		return zero / zero;
	#endif
	}
	
	bool is_nan(double value)
	{
	#if defined(_MSC_VER) || defined(__BORLANDC__)
		return !!_isnan(value);
	#elif defined(fpclassify) && defined(FP_NAN)
		return fpclassify(value) == FP_NAN;
	#else
		// fallback
		const volatile double v = value;
		return v != v;
	#endif
	}
	
	const char_t* convert_number_to_string_special(double value)
	{
	#if defined(_MSC_VER) || defined(__BORLANDC__)
		if (_finite(value)) return (value == 0) ? PUGIXML_TEXT("0") : 0;
		if (_isnan(value)) return PUGIXML_TEXT("NaN");
		return PUGIXML_TEXT("-Infinity") + (value > 0);
	#elif defined(fpclassify) && defined(FP_NAN) && defined(FP_INFINITE) && defined(FP_ZERO)
		switch (fpclassify(value))
		{
		case FP_NAN:
			return PUGIXML_TEXT("NaN");

		case FP_INFINITE:
			return PUGIXML_TEXT("-Infinity") + (value > 0);

		case FP_ZERO:
			return PUGIXML_TEXT("0");

		default:
			return 0;
		}
	#else
		// fallback
		const volatile double v = value;

		if (v == 0) return PUGIXML_TEXT("0");
		if (v != v) return PUGIXML_TEXT("NaN");
		if (v * 2 == v) return PUGIXML_TEXT("-Infinity") + (value > 0);
		return 0;
	#endif
	}
	
	bool convert_number_to_boolean(double value)
	{
		return (value != 0 && !is_nan(value));
	}
	
	void truncate_zeros(char* begin, char* end)
	{
		while (begin != end && end[-1] == '0') end--;

		*end = 0;
	}

	// gets mantissa digits in the form of 0.xxxxx with 0. implied and the exponent
#if defined(_MSC_VER) && _MSC_VER >= 1400
	void convert_number_to_mantissa_exponent(double value, char* buffer, size_t buffer_size, char** out_mantissa, int* out_exponent)
	{
		// get base values
		int sign, exponent;
		_ecvt_s(buffer, buffer_size, value, DBL_DIG + 1, &exponent, &sign);

		// truncate redundant zeros
		truncate_zeros(buffer, buffer + strlen(buffer));

		// fill results
		*out_mantissa = buffer;
		*out_exponent = exponent;
	}
#else
	void convert_number_to_mantissa_exponent(double value, char* buffer, size_t buffer_size, char** out_mantissa, int* out_exponent)
	{
		// get a scientific notation value with IEEE DBL_DIG decimals
		sprintf(buffer, "%.*e", DBL_DIG, value);
		assert(strlen(buffer) < buffer_size);
		(void)!buffer_size;

		// get the exponent (possibly negative)
		char* exponent_string = strchr(buffer, 'e');
		assert(exponent_string);

		int exponent = atoi(exponent_string + 1);

		// extract mantissa string: skip sign
		char* mantissa = buffer[0] == '-' ? buffer + 1 : buffer;
		assert(mantissa[0] != '0' && mantissa[1] == '.');

		// divide mantissa by 10 to eliminate integer part
		mantissa[1] = mantissa[0];
		mantissa++;
		exponent++;

		// remove extra mantissa digits and zero-terminate mantissa
		truncate_zeros(mantissa, exponent_string);

		// fill results
		*out_mantissa = mantissa;
		*out_exponent = exponent;
	}
#endif

	xpath_string convert_number_to_string(double value, xpath_allocator* alloc)
	{
		// try special number conversion
		const char_t* special = convert_number_to_string_special(value);
		if (special) return xpath_string_const(special);

		// get mantissa + exponent form
		char mantissa_buffer[64];

		char* mantissa;
		int exponent;
		convert_number_to_mantissa_exponent(value, mantissa_buffer, sizeof(mantissa_buffer), &mantissa, &exponent);

		// make the number!
		char_t result[512];
		char_t* s = result;

		// sign
		if (value < 0) *s++ = '-';

		// integer part
		if (exponent <= 0)
		{
			*s++ = '0';
		}
		else
		{
			while (exponent > 0)
			{
				assert(*mantissa == 0 || (unsigned)(*mantissa - '0') <= 9);
				*s++ = *mantissa ? *mantissa++ : '0';
				exponent--;
			}
		}

		// fractional part
		if (*mantissa)
		{
			// decimal point
			*s++ = '.';

			// extra zeroes from negative exponent
			while (exponent < 0)
			{
				*s++ = '0';
				exponent++;
			}

			// extra mantissa digits
			while (*mantissa)
			{
				assert((unsigned)(*mantissa - '0') <= 9);
				*s++ = *mantissa++;
			}
		}

		// zero-terminate
		assert(s < result + sizeof(result) / sizeof(result[0]));
		*s = 0;

		return xpath_string(result, alloc);
	}
	
	bool check_string_to_number_format(const char_t* string)
	{
		// parse leading whitespace
		while (IS_CHARTYPE(*string, ct_space)) ++string;

		// parse sign
		if (*string == '-') ++string;

		if (!*string) return false;

		// if there is no integer part, there should be a decimal part with at least one digit
		if (!IS_CHARTYPEX(string[0], ctx_digit) && (string[0] != '.' || !IS_CHARTYPEX(string[1], ctx_digit))) return false;

		// parse integer part
		while (IS_CHARTYPEX(*string, ctx_digit)) ++string;

		// parse decimal part
		if (*string == '.')
		{
			++string;

			while (IS_CHARTYPEX(*string, ctx_digit)) ++string;
		}

		// parse trailing whitespace
		while (IS_CHARTYPE(*string, ct_space)) ++string;

		return *string == 0;
	}

	double convert_string_to_number(const char_t* string)
	{
		// check string format
		if (!check_string_to_number_format(string)) return gen_nan();

		// parse string
	#ifdef PUGIXML_WCHAR_MODE
		return wcstod(string, 0);
	#else
		return atof(string);
	#endif
	}

	bool convert_string_to_number(const char_t* begin, const char_t* end, double* out_result)
	{
		char_t buffer[32];

		size_t length = static_cast<size_t>(end - begin);
		char_t* scratch = buffer;

		if (length >= sizeof(buffer) / sizeof(buffer[0]))
		{
			// need to make dummy on-heap copy
			scratch = static_cast<char_t*>(global_allocate((length + 1) * sizeof(char_t)));
			if (!scratch) return false;
		}

		// copy string to zero-terminated buffer and perform conversion
		memcpy(scratch, begin, length * sizeof(char_t));
		scratch[length] = 0;

		*out_result = convert_string_to_number(scratch);

		// free dummy buffer
		if (scratch != buffer) global_deallocate(scratch);

		return true;
	}
	
	double round_nearest(double value)
	{
		return floor(value + 0.5);
	}

	double round_nearest_nzero(double value)
	{
		// same as round_nearest, but returns -0 for [-0.5, -0]
		// ceil is used to differentiate between +0 and -0 (we return -0 for [-0.5, -0] and +0 for +0)
		return (value >= -0.5 && value <= 0) ? ceil(value) : floor(value + 0.5);
	}
	
	const char_t* qualified_name(const xpath_node& node)
	{
		return node.attribute() ? node.attribute().name() : node.node().name();
	}
	
	const char_t* local_name(const xpath_node& node)
	{
		const char_t* name = qualified_name(node);
		const char_t* p = find_char(name, ':');
		
		return p ? p + 1 : name;
	}

	struct namespace_uri_predicate
	{
		const char_t* prefix;
		size_t prefix_length;

		namespace_uri_predicate(const char_t* name)
		{
			const char_t* pos = find_char(name, ':');

			prefix = pos ? name : 0;
			prefix_length = pos ? static_cast<size_t>(pos - name) : 0;
		}

		bool operator()(const xml_attribute& a) const
		{
			const char_t* name = a.name();

			if (!starts_with(name, PUGIXML_TEXT("xmlns"))) return false;

			return prefix ? name[5] == ':' && strequalrange(name + 6, prefix, prefix_length) : name[5] == 0;
		}
	};

	const char_t* namespace_uri(const xml_node& node)
	{
		namespace_uri_predicate pred = node.name();
		
		xml_node p = node;
		
		while (p)
		{
			xml_attribute a = p.find_attribute(pred);
			
			if (a) return a.value();
			
			p = p.parent();
		}
		
		return PUGIXML_TEXT("");
	}

	const char_t* namespace_uri(const xml_attribute& attr, const xml_node& parent)
	{
		namespace_uri_predicate pred = attr.name();
		
		// Default namespace does not apply to attributes
		if (!pred.prefix) return PUGIXML_TEXT("");
		
		xml_node p = parent;
		
		while (p)
		{
			xml_attribute a = p.find_attribute(pred);
			
			if (a) return a.value();
			
			p = p.parent();
		}
		
		return PUGIXML_TEXT("");
	}

	const char_t* namespace_uri(const xpath_node& node)
	{
		return node.attribute() ? namespace_uri(node.attribute(), node.parent()) : namespace_uri(node.node());
	}

	void normalize_space(char_t* buffer)
	{
		char_t* write = buffer;

		for (char_t* it = buffer; *it; )
		{
			char_t ch = *it++;

			if (IS_CHARTYPE(ch, ct_space))
			{
				// replace whitespace sequence with single space
				while (IS_CHARTYPE(*it, ct_space)) it++;

				// avoid leading spaces
				if (write != buffer) *write++ = ' ';
			}
			else *write++ = ch;
		}

		// remove trailing space
		if (write != buffer && IS_CHARTYPE(write[-1], ct_space)) write--;

		// zero-terminate
		*write = 0;
	}

	void translate(char_t* buffer, const char_t* from, const char_t* to)
	{
		size_t to_length = strlength(to);

		char_t* write = buffer;

		while (*buffer)
		{
			DMC_VOLATILE char_t ch = *buffer++;

			const char_t* pos = find_char(from, ch);

			if (!pos)
				*write++ = ch; // do not process
			else if (static_cast<size_t>(pos - from) < to_length)
				*write++ = to[pos - from]; // replace
		}

		// zero-terminate
		*write = 0;
	}

	struct xpath_variable_boolean: xpath_variable
	{
		xpath_variable_boolean(): value(false)
		{
		}

		bool value;
		char_t name[1];
	};

	struct xpath_variable_number: xpath_variable
	{
		xpath_variable_number(): value(0)
		{
		}

		double value;
		char_t name[1];
	};

	struct xpath_variable_string: xpath_variable
	{
		xpath_variable_string(): value(0)
		{
		}

		~xpath_variable_string()
		{
			if (value) global_deallocate(value);
		}

		char_t* value;
		char_t name[1];
	};

	struct xpath_variable_node_set: xpath_variable
	{
		xpath_node_set value;
		char_t name[1];
	};

	const xpath_node_set dummy_node_set;

	unsigned int hash_string(const char_t* str)
	{
		// Jenkins one-at-a-time hash (http://en.wikipedia.org/wiki/Jenkins_hash_function#one-at-a-time)
		unsigned int result = 0;

		while (*str)
		{
			result += static_cast<unsigned int>(*str++);
			result += result << 10;
			result ^= result >> 6;
		}
	
		result += result << 3;
		result ^= result >> 11;
		result += result << 15;
	
		return result;
	}

	template <typename T> T* new_xpath_variable(const char_t* name)
	{
		size_t length = strlength(name);
		if (length == 0) return 0; // empty variable names are invalid

		// $$ we can't use offsetof(T, name) because T is non-POD, so we just allocate additional length characters
		void* memory = global_allocate(sizeof(T) + length * sizeof(char_t));
		if (!memory) return 0;

		T* result = new (memory) T();

		memcpy(result->name, name, (length + 1) * sizeof(char_t));

		return result;
	}

	xpath_variable* new_xpath_variable(xpath_value_type type, const char_t* name)
	{
		switch (type)
		{
		case xpath_type_node_set:
			return new_xpath_variable<xpath_variable_node_set>(name);

		case xpath_type_number:
			return new_xpath_variable<xpath_variable_number>(name);

		case xpath_type_string:
			return new_xpath_variable<xpath_variable_string>(name);

		case xpath_type_boolean:
			return new_xpath_variable<xpath_variable_boolean>(name);

		default:
			return 0;
		}
	}

	template <typename T> void delete_xpath_variable(T* var)
	{
		var->~T();
		global_deallocate(var);
	}

	void delete_xpath_variable(xpath_value_type type, xpath_variable* var)
	{
		switch (type)
		{
		case xpath_type_node_set:
			delete_xpath_variable(static_cast<xpath_variable_node_set*>(var));
			break;

		case xpath_type_number:
			delete_xpath_variable(static_cast<xpath_variable_number*>(var));
			break;

		case xpath_type_string:
			delete_xpath_variable(static_cast<xpath_variable_string*>(var));
			break;

		case xpath_type_boolean:
			delete_xpath_variable(static_cast<xpath_variable_boolean*>(var));
			break;

		default:
			assert(!"Invalid variable type");
		}
	}

	xpath_variable* get_variable(xpath_variable_set* set, const char_t* begin, const char_t* end)
	{
		char_t buffer[32];

		size_t length = static_cast<size_t>(end - begin);
		char_t* scratch = buffer;

		if (length >= sizeof(buffer) / sizeof(buffer[0]))
		{
			// need to make dummy on-heap copy
			scratch = static_cast<char_t*>(global_allocate((length + 1) * sizeof(char_t)));
			if (!scratch) return 0;
		}

		// copy string to zero-terminated buffer and perform lookup
		memcpy(scratch, begin, length * sizeof(char_t));
		scratch[length] = 0;

		xpath_variable* result = set->get(scratch);

		// free dummy buffer
		if (scratch != buffer) global_deallocate(scratch);

		return result;
	}
}

// Internal node set class
namespace
{
	xpath_node_set::type_t xpath_sort(xpath_node* begin, xpath_node* end, xpath_node_set::type_t type, bool rev)
	{
		xpath_node_set::type_t order = rev ? xpath_node_set::type_sorted_reverse : xpath_node_set::type_sorted;

		if (type == xpath_node_set::type_unsorted)
		{
			sort(begin, end, document_order_comparator());

			type = xpath_node_set::type_sorted;
		}
		
		if (type != order) reverse(begin, end);
			
		return order;
	}

	xpath_node xpath_first(const xpath_node* begin, const xpath_node* end, xpath_node_set::type_t type)
	{
		if (begin == end) return xpath_node();

		switch (type)
		{
		case xpath_node_set::type_sorted:
			return *begin;

		case xpath_node_set::type_sorted_reverse:
			return *(end - 1);

		case xpath_node_set::type_unsorted:
			return *min_element(begin, end, document_order_comparator());

		default:
			assert(!"Invalid node set type");
			return xpath_node();
		}
	}
	class xpath_node_set_raw
	{
		xpath_node_set::type_t _type;

		xpath_node* _begin;
		xpath_node* _end;
		xpath_node* _eos;

	public:
		xpath_node_set_raw(): _type(xpath_node_set::type_unsorted), _begin(0), _end(0), _eos(0)
		{
		}

		xpath_node* begin() const
		{
			return _begin;
		}

		xpath_node* end() const
		{
			return _end;
		}

		bool empty() const
		{
			return _begin == _end;
		}

		size_t size() const
		{
			return static_cast<size_t>(_end - _begin);
		}

		xpath_node first() const
		{
			return xpath_first(_begin, _end, _type);
		}

		void push_back(const xpath_node& node, xpath_allocator* alloc)
		{
			if (_end == _eos)
			{
				size_t capacity = static_cast<size_t>(_eos - _begin);

				// get new capacity (1.5x rule)
				size_t new_capacity = capacity + capacity / 2 + 1;

				// reallocate the old array or allocate a new one
				xpath_node* data = static_cast<xpath_node*>(alloc->reallocate(_begin, capacity * sizeof(xpath_node), new_capacity * sizeof(xpath_node)));
				assert(data);

				// finalize
				_begin = data;
				_end = data + capacity;
				_eos = data + new_capacity;
			}

			*_end++ = node;
		}

		void append(const xpath_node* begin, const xpath_node* end, xpath_allocator* alloc)
		{
			size_t size = static_cast<size_t>(_end - _begin);
			size_t capacity = static_cast<size_t>(_eos - _begin);
			size_t count = static_cast<size_t>(end - begin);

			if (size + count > capacity)
			{
				// reallocate the old array or allocate a new one
				xpath_node* data = static_cast<xpath_node*>(alloc->reallocate(_begin, capacity * sizeof(xpath_node), (size + count) * sizeof(xpath_node)));
				assert(data);

				// finalize
				_begin = data;
				_end = data + size;
				_eos = data + size + count;
			}

			memcpy(_end, begin, count * sizeof(xpath_node));
			_end += count;
		}

		void sort_do()
		{
			_type = xpath_sort(_begin, _end, _type, false);
		}

		void truncate(xpath_node* pos)
		{
			assert(_begin <= pos && pos <= _end);

			_end = pos;
		}

		void remove_duplicates()
		{
			if (_type == xpath_node_set::type_unsorted)
				sort(_begin, _end, duplicate_comparator());
		
			_end = unique(_begin, _end);
		}

		xpath_node_set::type_t type() const
		{
			return _type;
		}

		void set_type(xpath_node_set::type_t type)
		{
			_type = type;
		}
	};
}

namespace
{
	struct xpath_context
	{
		xpath_node n;
		size_t position, size;

		xpath_context(const xpath_node& n, size_t position, size_t size): n(n), position(position), size(size)
		{
		}
	};

	enum lexeme_t
	{
		lex_none = 0,
		lex_equal,
		lex_not_equal,
		lex_less,
		lex_greater,
		lex_less_or_equal,
		lex_greater_or_equal,
		lex_plus,
		lex_minus,
		lex_multiply,
		lex_union,
		lex_var_ref,
		lex_open_brace,
		lex_close_brace,
		lex_quoted_string,
		lex_number,
		lex_slash,
		lex_double_slash,
		lex_open_square_brace,
		lex_close_square_brace,
		lex_string,
		lex_comma,
		lex_axis_attribute,
		lex_dot,
		lex_double_dot,
		lex_double_colon,
		lex_eof
	};

	struct xpath_lexer_string
	{
		const char_t* begin;
		const char_t* end;

		xpath_lexer_string(): begin(0), end(0)
		{
		}

		bool operator==(const char_t* other) const
		{
			size_t length = static_cast<size_t>(end - begin);

			return strequalrange(other, begin, length);
		}
	};

	class xpath_lexer
	{
		const char_t* _cur;
		const char_t* _cur_lexeme_pos;
		xpath_lexer_string _cur_lexeme_contents;

		lexeme_t _cur_lexeme;

	public:
		explicit xpath_lexer(const char_t* query): _cur(query)
		{
			next();
		}
		
		const char_t* state() const
		{
			return _cur;
		}
		
		void next()
		{
			const char_t* cur = _cur;

			while (IS_CHARTYPE(*cur, ct_space)) ++cur;

			// save lexeme position for error reporting
			_cur_lexeme_pos = cur;

			switch (*cur)
			{
			case 0:
				_cur_lexeme = lex_eof;
				break;
			
			case '>':
				if (*(cur+1) == '=')
				{
					cur += 2;
					_cur_lexeme = lex_greater_or_equal;
				}
				else
				{
					cur += 1;
					_cur_lexeme = lex_greater;
				}
				break;

			case '<':
				if (*(cur+1) == '=')
				{
					cur += 2;
					_cur_lexeme = lex_less_or_equal;
				}
				else
				{
					cur += 1;
					_cur_lexeme = lex_less;
				}
				break;

			case '!':
				if (*(cur+1) == '=')
				{
					cur += 2;
					_cur_lexeme = lex_not_equal;
				}
				else
				{
					_cur_lexeme = lex_none;
				}
				break;

			case '=':
				cur += 1;
				_cur_lexeme = lex_equal;

				break;
			
			case '+':
				cur += 1;
				_cur_lexeme = lex_plus;

				break;

			case '-':
				cur += 1;
				_cur_lexeme = lex_minus;

				break;

			case '*':
				cur += 1;
				_cur_lexeme = lex_multiply;

				break;

			case '|':
				cur += 1;
				_cur_lexeme = lex_union;

				break;
			
			case '$':
				cur += 1;

				if (IS_CHARTYPEX(*cur, ctx_start_symbol))
				{
					_cur_lexeme_contents.begin = cur;

					while (IS_CHARTYPEX(*cur, ctx_symbol)) cur++;

					if (cur[0] == ':' && IS_CHARTYPEX(cur[1], ctx_symbol)) // qname
					{
						cur++; // :

						while (IS_CHARTYPEX(*cur, ctx_symbol)) cur++;
					}

					_cur_lexeme_contents.end = cur;
				
					_cur_lexeme = lex_var_ref;
				}
				else
				{
					_cur_lexeme = lex_none;
				}

				break;

			case '(':
				cur += 1;
				_cur_lexeme = lex_open_brace;

				break;

			case ')':
				cur += 1;
				_cur_lexeme = lex_close_brace;

				break;
			
			case '[':
				cur += 1;
				_cur_lexeme = lex_open_square_brace;

				break;

			case ']':
				cur += 1;
				_cur_lexeme = lex_close_square_brace;

				break;

			case ',':
				cur += 1;
				_cur_lexeme = lex_comma;

				break;

			case '/':
				if (*(cur+1) == '/')
				{
					cur += 2;
					_cur_lexeme = lex_double_slash;
				}
				else
				{
					cur += 1;
					_cur_lexeme = lex_slash;
				}
				break;
		
			case '.':
				if (*(cur+1) == '.')
				{
					cur += 2;
					_cur_lexeme = lex_double_dot;
				}
				else if (IS_CHARTYPEX(*(cur+1), ctx_digit))
				{
					_cur_lexeme_contents.begin = cur; // .

					++cur;

					while (IS_CHARTYPEX(*cur, ctx_digit)) cur++;

					_cur_lexeme_contents.end = cur;
					
					_cur_lexeme = lex_number;
				}
				else
				{
					cur += 1;
					_cur_lexeme = lex_dot;
				}
				break;

			case '@':
				cur += 1;
				_cur_lexeme = lex_axis_attribute;

				break;

			case '"':
			case '\'':
			{
				char_t terminator = *cur;

				++cur;

				_cur_lexeme_contents.begin = cur;
				while (*cur && *cur != terminator) cur++;
				_cur_lexeme_contents.end = cur;
				
				if (!*cur)
					_cur_lexeme = lex_none;
				else
				{
					cur += 1;
					_cur_lexeme = lex_quoted_string;
				}

				break;
			}

			case ':':
				if (*(cur+1) == ':')
				{
					cur += 2;
					_cur_lexeme = lex_double_colon;
				}
				else
				{
					_cur_lexeme = lex_none;
				}
				break;

			default:
				if (IS_CHARTYPEX(*cur, ctx_digit))
				{
					_cur_lexeme_contents.begin = cur;

					while (IS_CHARTYPEX(*cur, ctx_digit)) cur++;
				
					if (*cur == '.')
					{
						cur++;

						while (IS_CHARTYPEX(*cur, ctx_digit)) cur++;
					}

					_cur_lexeme_contents.end = cur;

					_cur_lexeme = lex_number;
				}
				else if (IS_CHARTYPEX(*cur, ctx_start_symbol))
				{
					_cur_lexeme_contents.begin = cur;

					while (IS_CHARTYPEX(*cur, ctx_symbol)) cur++;

					if (cur[0] == ':')
					{
						if (cur[1] == '*') // namespace test ncname:*
						{
							cur += 2; // :*
						}
						else if (IS_CHARTYPEX(cur[1], ctx_symbol)) // namespace test qname
						{
							cur++; // :

							while (IS_CHARTYPEX(*cur, ctx_symbol)) cur++;
						}
					}

					_cur_lexeme_contents.end = cur;
				
					_cur_lexeme = lex_string;
				}
				else
				{
					_cur_lexeme = lex_none;
				}
			}

			_cur = cur;
		}

		lexeme_t current() const
		{
			return _cur_lexeme;
		}

		const char_t* current_pos() const
		{
			return _cur_lexeme_pos;
		}

		const xpath_lexer_string& contents() const
		{
			assert(_cur_lexeme == lex_var_ref || _cur_lexeme == lex_number || _cur_lexeme == lex_string || _cur_lexeme == lex_quoted_string);

			return _cur_lexeme_contents;
		}
	};

	enum ast_type_t
	{
		ast_op_or,						// left or right
		ast_op_and,						// left and right
		ast_op_equal,					// left = right
		ast_op_not_equal, 				// left != right
		ast_op_less,					// left < right
		ast_op_greater,					// left > right
		ast_op_less_or_equal,			// left <= right
		ast_op_greater_or_equal,		// left >= right
		ast_op_add,						// left + right
		ast_op_subtract,				// left - right
		ast_op_multiply,				// left * right
		ast_op_divide,					// left / right
		ast_op_mod,						// left % right
		ast_op_negate,					// left - right
		ast_op_union,					// left | right
		ast_predicate,					// apply predicate to set; next points to next predicate
		ast_filter,						// select * from left where right
		ast_filter_posinv,				// select * from left where right; proximity position invariant
		ast_string_constant,			// string constant
		ast_number_constant,			// number constant
		ast_variable,					// variable
		ast_func_last,					// last()
		ast_func_position,				// position()
		ast_func_count,					// count(left)
		ast_func_id,					// id(left)
		ast_func_local_name_0,			// local-name()
		ast_func_local_name_1,			// local-name(left)
		ast_func_namespace_uri_0,		// namespace-uri()
		ast_func_namespace_uri_1,		// namespace-uri(left)
		ast_func_name_0,				// name()
		ast_func_name_1,				// name(left)
		ast_func_string_0,				// string()
		ast_func_string_1,				// string(left)
		ast_func_concat,				// concat(left, right, siblings)
		ast_func_starts_with,			// starts_with(left, right)
		ast_func_contains,				// contains(left, right)
		ast_func_substring_before,		// substring-before(left, right)
		ast_func_substring_after,		// substring-after(left, right)
		ast_func_substring_2,			// substring(left, right)
		ast_func_substring_3,			// substring(left, right, third)
		ast_func_string_length_0,		// string-length()
		ast_func_string_length_1,		// string-length(left)
		ast_func_normalize_space_0,		// normalize-space()
		ast_func_normalize_space_1,		// normalize-space(left)
		ast_func_translate,				// translate(left, right, third)
		ast_func_boolean,				// boolean(left)
		ast_func_not,					// not(left)
		ast_func_true,					// true()
		ast_func_false,					// false()
		ast_func_lang,					// lang(left)
		ast_func_number_0,				// number()
		ast_func_number_1,				// number(left)
		ast_func_sum,					// sum(left)
		ast_func_floor,					// floor(left)
		ast_func_ceiling,				// ceiling(left)
		ast_func_round,					// round(left)
		ast_step,						// process set left with step
		ast_step_root					// select root node
	};

	enum axis_t
	{
		axis_ancestor,
		axis_ancestor_or_self,
		axis_attribute,
		axis_child,
		axis_descendant,
		axis_descendant_or_self,
		axis_following,
		axis_following_sibling,
		axis_namespace,
		axis_parent,
		axis_preceding,
		axis_preceding_sibling,
		axis_self
	};
	
	enum nodetest_t
	{
		nodetest_none,
		nodetest_name,
		nodetest_type_node,
		nodetest_type_comment,
		nodetest_type_pi,
		nodetest_type_text,
		nodetest_pi,
		nodetest_all,
		nodetest_all_in_namespace
	};

	template <axis_t N> struct axis_to_type
	{
		static const axis_t axis;
	};

	template <axis_t N> const axis_t axis_to_type<N>::axis = N;
		
	class xpath_ast_node
	{
	private:
		// node type
		char _type;
		char _rettype;

		// for ast_step / ast_predicate
		char _axis;
		char _test;

		// tree node structure
		xpath_ast_node* _left;
		xpath_ast_node* _right;
		xpath_ast_node* _next;

		union
		{
			// value for ast_string_constant
			const char_t* string;
			// value for ast_number_constant
			double number;
			// variable for ast_variable
			xpath_variable* variable;
			// node test for ast_step (node name/namespace/node type/pi target)
			const char_t* nodetest;
		} _data;

		xpath_ast_node(const xpath_ast_node&);
		xpath_ast_node& operator=(const xpath_ast_node&);

		template <class Comp> static bool compare_eq(xpath_ast_node* lhs, xpath_ast_node* rhs, const xpath_context& c, const xpath_stack& stack, const Comp& comp)
		{
			xpath_value_type lt = lhs->rettype(), rt = rhs->rettype();

			if (lt != xpath_type_node_set && rt != xpath_type_node_set)
			{
				if (lt == xpath_type_boolean || rt == xpath_type_boolean)
					return comp(lhs->eval_boolean(c, stack), rhs->eval_boolean(c, stack));
				else if (lt == xpath_type_number || rt == xpath_type_number)
					return comp(lhs->eval_number(c, stack), rhs->eval_number(c, stack));
				else if (lt == xpath_type_string || rt == xpath_type_string)
				{
					xpath_allocator_capture cr(stack.result);

					xpath_string ls = lhs->eval_string(c, stack);
					xpath_string rs = rhs->eval_string(c, stack);

					return comp(ls, rs);
				}
			}
			else if (lt == xpath_type_node_set && rt == xpath_type_node_set)
			{
				xpath_allocator_capture cr(stack.result);

				xpath_node_set_raw ls = lhs->eval_node_set(c, stack);
				xpath_node_set_raw rs = rhs->eval_node_set(c, stack);

				for (const xpath_node* li = ls.begin(); li != ls.end(); ++li)
					for (const xpath_node* ri = rs.begin(); ri != rs.end(); ++ri)
					{
						xpath_allocator_capture cri(stack.result);

						if (comp(string_value(*li, stack.result), string_value(*ri, stack.result)))
							return true;
					}

				return false;
			}
			else
			{
				if (lt == xpath_type_node_set)
				{
					swap(lhs, rhs);
					swap(lt, rt);
				}

				if (lt == xpath_type_boolean)
					return comp(lhs->eval_boolean(c, stack), rhs->eval_boolean(c, stack));
				else if (lt == xpath_type_number)
				{
					xpath_allocator_capture cr(stack.result);

					double l = lhs->eval_number(c, stack);
					xpath_node_set_raw rs = rhs->eval_node_set(c, stack);

					for (const xpath_node* ri = rs.begin(); ri != rs.end(); ++ri)
					{
						xpath_allocator_capture cri(stack.result);

						if (comp(l, convert_string_to_number(string_value(*ri, stack.result).c_str())))
							return true;
					}

					return false;
				}
				else if (lt == xpath_type_string)
				{
					xpath_allocator_capture cr(stack.result);

					xpath_string l = lhs->eval_string(c, stack);
					xpath_node_set_raw rs = rhs->eval_node_set(c, stack);

					for (const xpath_node* ri = rs.begin(); ri != rs.end(); ++ri)
					{
						xpath_allocator_capture cri(stack.result);

						if (comp(l, string_value(*ri, stack.result)))
							return true;
					}

					return false;
				}
			}

			assert(!"Wrong types");
			return false;
		}

		template <class Comp> static bool compare_rel(xpath_ast_node* lhs, xpath_ast_node* rhs, const xpath_context& c, const xpath_stack& stack, const Comp& comp)
		{
			xpath_value_type lt = lhs->rettype(), rt = rhs->rettype();

			if (lt != xpath_type_node_set && rt != xpath_type_node_set)
				return comp(lhs->eval_number(c, stack), rhs->eval_number(c, stack));
			else if (lt == xpath_type_node_set && rt == xpath_type_node_set)
			{
				xpath_allocator_capture cr(stack.result);

				xpath_node_set_raw ls = lhs->eval_node_set(c, stack);
				xpath_node_set_raw rs = rhs->eval_node_set(c, stack);

				for (const xpath_node* li = ls.begin(); li != ls.end(); ++li)
				{
					xpath_allocator_capture cri(stack.result);

					double l = convert_string_to_number(string_value(*li, stack.result).c_str());

					for (const xpath_node* ri = rs.begin(); ri != rs.end(); ++ri)
					{
						xpath_allocator_capture crii(stack.result);

						if (comp(l, convert_string_to_number(string_value(*ri, stack.result).c_str())))
							return true;
					}
				}

				return false;
			}
			else if (lt != xpath_type_node_set && rt == xpath_type_node_set)
			{
				xpath_allocator_capture cr(stack.result);

				double l = lhs->eval_number(c, stack);
				xpath_node_set_raw rs = rhs->eval_node_set(c, stack);

				for (const xpath_node* ri = rs.begin(); ri != rs.end(); ++ri)
				{
					xpath_allocator_capture cri(stack.result);

					if (comp(l, convert_string_to_number(string_value(*ri, stack.result).c_str())))
						return true;
				}

				return false;
			}
			else if (lt == xpath_type_node_set && rt != xpath_type_node_set)
			{
				xpath_allocator_capture cr(stack.result);

				xpath_node_set_raw ls = lhs->eval_node_set(c, stack);
				double r = rhs->eval_number(c, stack);

				for (const xpath_node* li = ls.begin(); li != ls.end(); ++li)
				{
					xpath_allocator_capture cri(stack.result);

					if (comp(convert_string_to_number(string_value(*li, stack.result).c_str()), r))
						return true;
				}

				return false;
			}
			else
			{
				assert(!"Wrong types");
				return false;
			}
		}

		void apply_predicate(xpath_node_set_raw& ns, size_t first, xpath_ast_node* expr, const xpath_stack& stack)
		{
			assert(ns.size() >= first);

			size_t i = 1;
			size_t size = ns.size() - first;
				
			xpath_node* last = ns.begin() + first;
				
			// remove_if... or well, sort of
			for (xpath_node* it = last; it != ns.end(); ++it, ++i)
			{
				xpath_context c(*it, i, size);
			
				if (expr->rettype() == xpath_type_number)
				{
					if (expr->eval_number(c, stack) == i)
						*last++ = *it;
				}
				else if (expr->eval_boolean(c, stack))
					*last++ = *it;
			}
			
			ns.truncate(last);
		}

		void apply_predicates(xpath_node_set_raw& ns, size_t first, const xpath_stack& stack)
		{
			if (ns.size() == first) return;
			
			for (xpath_ast_node* pred = _right; pred; pred = pred->_next)
			{
				apply_predicate(ns, first, pred->_left, stack);
			}
		}

		void step_push(xpath_node_set_raw& ns, const xml_attribute& a, const xml_node& parent, xpath_allocator* alloc)
		{
			if (!a) return;

			const char_t* name = a.name();

			// There are no attribute nodes corresponding to attributes that declare namespaces
			// That is, "xmlns:..." or "xmlns"
			if (starts_with(name, PUGIXML_TEXT("xmlns")) && (name[5] == 0 || name[5] == ':')) return;
			
			switch (_test)
			{
			case nodetest_name:
				if (strequal(name, _data.nodetest)) ns.push_back(xpath_node(a, parent), alloc);
				break;
				
			case nodetest_type_node:
			case nodetest_all:
				ns.push_back(xpath_node(a, parent), alloc);
				break;
				
			case nodetest_all_in_namespace:
				if (starts_with(name, _data.nodetest))
					ns.push_back(xpath_node(a, parent), alloc);
				break;
			
			default:
				;
			}
		}
		
		void step_push(xpath_node_set_raw& ns, const xml_node& n, xpath_allocator* alloc)
		{
			if (!n) return;

			switch (_test)
			{
			case nodetest_name:
				if (n.type() == node_element && strequal(n.name(), _data.nodetest)) ns.push_back(n, alloc);
				break;
				
			case nodetest_type_node:
				ns.push_back(n, alloc);
				break;
				
			case nodetest_type_comment:
				if (n.type() == node_comment)
					ns.push_back(n, alloc);
				break;
				
			case nodetest_type_text:
				if (n.type() == node_pcdata || n.type() == node_cdata)
					ns.push_back(n, alloc);
				break;
				
			case nodetest_type_pi:
				if (n.type() == node_pi)
					ns.push_back(n, alloc);
				break;
									
			case nodetest_pi:
				if (n.type() == node_pi && strequal(n.name(), _data.nodetest))
					ns.push_back(n, alloc);
				break;
				
			case nodetest_all:
				if (n.type() == node_element)
					ns.push_back(n, alloc);
				break;
				
			case nodetest_all_in_namespace:
				if (n.type() == node_element && starts_with(n.name(), _data.nodetest))
					ns.push_back(n, alloc);
				break;

			default:
				assert(!"Unknown axis");
			} 
		}

		template <class T> void step_fill(xpath_node_set_raw& ns, const xml_node& n, xpath_allocator* alloc, T)
		{
			const axis_t axis = T::axis;

			switch (axis)
			{
			case axis_attribute:
			{
				for (xml_attribute a = n.first_attribute(); a; a = a.next_attribute())
					step_push(ns, a, n, alloc);
				
				break;
			}
			
			case axis_child:
			{
				for (xml_node c = n.first_child(); c; c = c.next_sibling())
					step_push(ns, c, alloc);
					
				break;
			}
			
			case axis_descendant:
			case axis_descendant_or_self:
			{
				if (axis == axis_descendant_or_self)
					step_push(ns, n, alloc);
					
				xml_node cur = n.first_child();
				
				while (cur && cur != n)
				{
					step_push(ns, cur, alloc);
					
					if (cur.first_child())
						cur = cur.first_child();
					else if (cur.next_sibling())
						cur = cur.next_sibling();
					else
					{
						while (!cur.next_sibling() && cur != n)
							cur = cur.parent();
					
						if (cur != n) cur = cur.next_sibling();
					}
				}
				
				break;
			}
			
			case axis_following_sibling:
			{
				for (xml_node c = n.next_sibling(); c; c = c.next_sibling())
					step_push(ns, c, alloc);
				
				break;
			}
			
			case axis_preceding_sibling:
			{
				for (xml_node c = n.previous_sibling(); c; c = c.previous_sibling())
					step_push(ns, c, alloc);
				
				break;
			}
			
			case axis_following:
			{
				xml_node cur = n;

				// exit from this node so that we don't include descendants
				while (cur && !cur.next_sibling()) cur = cur.parent();
				cur = cur.next_sibling();

				for (;;)
				{
					step_push(ns, cur, alloc);

					if (cur.first_child())
						cur = cur.first_child();
					else if (cur.next_sibling())
						cur = cur.next_sibling();
					else
					{
						while (cur && !cur.next_sibling()) cur = cur.parent();
						cur = cur.next_sibling();

						if (!cur) break;
					}
				}

				break;
			}

			case axis_preceding:
			{
				xml_node cur = n;

				while (cur && !cur.previous_sibling()) cur = cur.parent();
				cur = cur.previous_sibling();

				for (;;)
				{
					if (cur.last_child())
						cur = cur.last_child();
					else
					{
						// leaf node, can't be ancestor
						step_push(ns, cur, alloc);

						if (cur.previous_sibling())
							cur = cur.previous_sibling();
						else
						{
							do 
							{
								cur = cur.parent();
								if (!cur) break;

								if (!node_is_ancestor(cur, n)) step_push(ns, cur, alloc);
							}
							while (!cur.previous_sibling());

							cur = cur.previous_sibling();

							if (!cur) break;
						}
					}
				}

				break;
			}
			
			case axis_ancestor:
			case axis_ancestor_or_self:
			{
				if (axis == axis_ancestor_or_self)
					step_push(ns, n, alloc);

				xml_node cur = n.parent();
				
				while (cur)
				{
					step_push(ns, cur, alloc);
					
					cur = cur.parent();
				}
				
				break;
			}

			case axis_self:
			{
				step_push(ns, n, alloc);

				break;
			}

			case axis_parent:
			{
				if (n.parent()) step_push(ns, n.parent(), alloc);

				break;
			}
				
			default:
				assert(!"Unimplemented axis");
			}
		}
		
		template <class T> void step_fill(xpath_node_set_raw& ns, const xml_attribute& a, const xml_node& p, xpath_allocator* alloc, T v)
		{
			const axis_t axis = T::axis;

			switch (axis)
			{
			case axis_ancestor:
			case axis_ancestor_or_self:
			{
				if (axis == axis_ancestor_or_self && _test == nodetest_type_node) // reject attributes based on principal node type test
					step_push(ns, a, p, alloc);

				xml_node cur = p;
				
				while (cur)
				{
					step_push(ns, cur, alloc);
					
					cur = cur.parent();
				}
				
				break;
			}

			case axis_descendant_or_self:
			case axis_self:
			{
				if (_test == nodetest_type_node) // reject attributes based on principal node type test
					step_push(ns, a, p, alloc);

				break;
			}

			case axis_following:
			{
				xml_node cur = p;
				
				for (;;)
				{
					if (cur.first_child())
						cur = cur.first_child();
					else if (cur.next_sibling())
						cur = cur.next_sibling();
					else
					{
						while (cur && !cur.next_sibling()) cur = cur.parent();
						cur = cur.next_sibling();
						
						if (!cur) break;
					}

					step_push(ns, cur, alloc);
				}

				break;
			}

			case axis_parent:
			{
				step_push(ns, p, alloc);

				break;
			}

			case axis_preceding:
			{
				// preceding:: axis does not include attribute nodes and attribute ancestors (they are the same as parent's ancestors), so we can reuse node preceding
				step_fill(ns, p, alloc, v);
				break;
			}
			
			default:
				assert(!"Unimplemented axis");
			}
		}
		
		template <class T> xpath_node_set_raw step_do(const xpath_context& c, const xpath_stack& stack, T v)
		{
			const axis_t axis = T::axis;
			bool attributes = (axis == axis_ancestor || axis == axis_ancestor_or_self || axis == axis_descendant_or_self || axis == axis_following || axis == axis_parent || axis == axis_preceding || axis == axis_self);

			xpath_node_set_raw ns;
			ns.set_type((axis == axis_ancestor || axis == axis_ancestor_or_self || axis == axis_preceding || axis == axis_preceding_sibling) ? xpath_node_set::type_sorted_reverse : xpath_node_set::type_sorted);

			if (_left)
			{
				xpath_node_set_raw s = _left->eval_node_set(c, stack);

				// self axis preserves the original order
				if (axis == axis_self) ns.set_type(s.type());

				for (const xpath_node* it = s.begin(); it != s.end(); ++it)
				{
					size_t size = ns.size();

					// in general, all axes generate elements in a particular order, but there is no order guarantee if axis is applied to two nodes
					if (axis != axis_self && size != 0) ns.set_type(xpath_node_set::type_unsorted);
					
					if (it->node())
						step_fill(ns, it->node(), stack.result, v);
					else if (attributes)
						step_fill(ns, it->attribute(), it->parent(), stack.result, v);
						
					apply_predicates(ns, size, stack);
				}
			}
			else
			{
				if (c.n.node())
					step_fill(ns, c.n.node(), stack.result, v);
				else if (attributes)
					step_fill(ns, c.n.attribute(), c.n.parent(), stack.result, v);
				
				apply_predicates(ns, 0, stack);
			}

			// child, attribute and self axes always generate unique set of nodes
			// for other axis, if the set stayed sorted, it stayed unique because the traversal algorithms do not visit the same node twice
			if (axis != axis_child && axis != axis_attribute && axis != axis_self && ns.type() == xpath_node_set::type_unsorted)
				ns.remove_duplicates();

			return ns;
		}
		
	public:
		xpath_ast_node(ast_type_t type, xpath_value_type rettype, const char_t* value):
			_type((char)type), _rettype((char)rettype), _axis(0), _test(0), _left(0), _right(0), _next(0)
		{
			assert(type == ast_string_constant);
			_data.string = value;
		}

		xpath_ast_node(ast_type_t type, xpath_value_type rettype, double value):
			_type((char)type), _rettype((char)rettype), _axis(0), _test(0), _left(0), _right(0), _next(0)
		{
			assert(type == ast_number_constant);
			_data.number = value;
		}
		
		xpath_ast_node(ast_type_t type, xpath_value_type rettype, xpath_variable* value):
			_type((char)type), _rettype((char)rettype), _axis(0), _test(0), _left(0), _right(0), _next(0)
		{
			assert(type == ast_variable);
			_data.variable = value;
		}
		
		xpath_ast_node(ast_type_t type, xpath_value_type rettype, xpath_ast_node* left = 0, xpath_ast_node* right = 0):
			_type((char)type), _rettype((char)rettype), _axis(0), _test(0), _left(left), _right(right), _next(0)
		{
		}

		xpath_ast_node(ast_type_t type, xpath_ast_node* left, axis_t axis, nodetest_t test, const char_t* contents):
			_type((char)type), _rettype(xpath_type_node_set), _axis((char)axis), _test((char)test), _left(left), _right(0), _next(0)
		{
			_data.nodetest = contents;
		}

		void set_next(xpath_ast_node* value)
		{
			_next = value;
		}

		void set_right(xpath_ast_node* value)
		{
			_right = value;
		}

		bool eval_boolean(const xpath_context& c, const xpath_stack& stack)
		{
			switch (_type)
			{
			case ast_op_or:
				return _left->eval_boolean(c, stack) || _right->eval_boolean(c, stack);
				
			case ast_op_and:
				return _left->eval_boolean(c, stack) && _right->eval_boolean(c, stack);
				
			case ast_op_equal:
				return compare_eq(_left, _right, c, stack, equal_to());

			case ast_op_not_equal:
				return compare_eq(_left, _right, c, stack, not_equal_to());
	
			case ast_op_less:
				return compare_rel(_left, _right, c, stack, less());
			
			case ast_op_greater:
				return compare_rel(_right, _left, c, stack, less());

			case ast_op_less_or_equal:
				return compare_rel(_left, _right, c, stack, less_equal());
			
			case ast_op_greater_or_equal:
				return compare_rel(_right, _left, c, stack, less_equal());

			case ast_func_starts_with:
			{
				xpath_allocator_capture cr(stack.result);

				xpath_string lr = _left->eval_string(c, stack);
				xpath_string rr = _right->eval_string(c, stack);

				return starts_with(lr.c_str(), rr.c_str());
			}

			case ast_func_contains:
			{
				xpath_allocator_capture cr(stack.result);

				xpath_string lr = _left->eval_string(c, stack);
				xpath_string rr = _right->eval_string(c, stack);

				return find_substring(lr.c_str(), rr.c_str()) != 0;
			}

			case ast_func_boolean:
				return _left->eval_boolean(c, stack);
				
			case ast_func_not:
				return !_left->eval_boolean(c, stack);
				
			case ast_func_true:
				return true;
				
			case ast_func_false:
				return false;

			case ast_func_lang:
			{
				if (c.n.attribute()) return false;
				
				xpath_allocator_capture cr(stack.result);

				xpath_string lang = _left->eval_string(c, stack);
				
				for (xml_node n = c.n.node(); n; n = n.parent())
				{
					xml_attribute a = n.attribute(PUGIXML_TEXT("xml:lang"));
					
					if (a)
					{
						const char_t* value = a.value();
						
						// strnicmp / strncasecmp is not portable
						for (const char_t* lit = lang.c_str(); *lit; ++lit)
						{
							if (tolower_ascii(*lit) != tolower_ascii(*value)) return false;
							++value;
						}
						
						return *value == 0 || *value == '-';
					}
				}
				
				return false;
			}

			case ast_variable:
			{
				assert(_rettype == _data.variable->type());

				if (_rettype == xpath_type_boolean)
					return _data.variable->get_boolean();

				// fallthrough to type conversion
			}

			default:
			{
				switch (_rettype)
				{
				case xpath_type_number:
					return convert_number_to_boolean(eval_number(c, stack));
					
				case xpath_type_string:
				{
					xpath_allocator_capture cr(stack.result);

					return !eval_string(c, stack).empty();
				}
					
				case xpath_type_node_set:				
				{
					xpath_allocator_capture cr(stack.result);

					return !eval_node_set(c, stack).empty();
				}

				default:
					assert(!"Wrong expression for return type boolean");
					return false;
				}
			}
			}
		}

		double eval_number(const xpath_context& c, const xpath_stack& stack)
		{
			switch (_type)
			{
			case ast_op_add:
				return _left->eval_number(c, stack) + _right->eval_number(c, stack);
				
			case ast_op_subtract:
				return _left->eval_number(c, stack) - _right->eval_number(c, stack);

			case ast_op_multiply:
				return _left->eval_number(c, stack) * _right->eval_number(c, stack);

			case ast_op_divide:
				return _left->eval_number(c, stack) / _right->eval_number(c, stack);

			case ast_op_mod:
				return fmod(_left->eval_number(c, stack), _right->eval_number(c, stack));

			case ast_op_negate:
				return -_left->eval_number(c, stack);

			case ast_number_constant:
				return _data.number;

			case ast_func_last:
				return (double)c.size;
			
			case ast_func_position:
				return (double)c.position;

			case ast_func_count:
			{
				xpath_allocator_capture cr(stack.result);

				return (double)_left->eval_node_set(c, stack).size();
			}
			
			case ast_func_string_length_0:
			{
				xpath_allocator_capture cr(stack.result);

				return (double)string_value(c.n, stack.result).length();
			}
			
			case ast_func_string_length_1:
			{
				xpath_allocator_capture cr(stack.result);

				return (double)_left->eval_string(c, stack).length();
			}
			
			case ast_func_number_0:
			{
				xpath_allocator_capture cr(stack.result);

				return convert_string_to_number(string_value(c.n, stack.result).c_str());
			}
			
			case ast_func_number_1:
				return _left->eval_number(c, stack);

			case ast_func_sum:
			{
				xpath_allocator_capture cr(stack.result);

				double r = 0;
				
				xpath_node_set_raw ns = _left->eval_node_set(c, stack);
				
				for (const xpath_node* it = ns.begin(); it != ns.end(); ++it)
				{
					xpath_allocator_capture cri(stack.result);

					r += convert_string_to_number(string_value(*it, stack.result).c_str());
				}
			
				return r;
			}

			case ast_func_floor:
			{
				double r = _left->eval_number(c, stack);
				
				return r == r ? floor(r) : r;
			}

			case ast_func_ceiling:
			{
				double r = _left->eval_number(c, stack);
				
				return r == r ? ceil(r) : r;
			}

			case ast_func_round:
				return round_nearest_nzero(_left->eval_number(c, stack));
			
			case ast_variable:
			{
				assert(_rettype == _data.variable->type());

				if (_rettype == xpath_type_number)
					return _data.variable->get_number();

				// fallthrough to type conversion
			}

			default:
			{
				switch (_rettype)
				{
				case xpath_type_boolean:
					return eval_boolean(c, stack) ? 1 : 0;
					
				case xpath_type_string:
				{
					xpath_allocator_capture cr(stack.result);

					return convert_string_to_number(eval_string(c, stack).c_str());
				}
					
				case xpath_type_node_set:
				{
					xpath_allocator_capture cr(stack.result);

					return convert_string_to_number(eval_string(c, stack).c_str());
				}
					
				default:
					assert(!"Wrong expression for return type number");
					return 0;
				}
				
			}
			}
		}
		
		xpath_string eval_string_concat(const xpath_context& c, const xpath_stack& stack)
		{
			assert(_type == ast_func_concat);

			xpath_allocator_capture ct(stack.temp);

			// count the string number
			size_t count = 1;
			for (xpath_ast_node* nc = _right; nc; nc = nc->_next) count++;

			// gather all strings
			xpath_string static_buffer[4];
			xpath_string* buffer = static_buffer;

			// allocate on-heap for large concats
			if (count > sizeof(static_buffer) / sizeof(static_buffer[0]))
			{
				buffer = static_cast<xpath_string*>(stack.temp->allocate(count * sizeof(xpath_string)));
				assert(buffer);
			}

			// evaluate all strings to temporary stack
			xpath_stack swapped_stack = {stack.temp, stack.result};

			buffer[0] = _left->eval_string(c, swapped_stack);

			size_t pos = 1;
			for (xpath_ast_node* n = _right; n; n = n->_next, ++pos) buffer[pos] = n->eval_string(c, swapped_stack);
			assert(pos == count);

			// get total length
			size_t length = 0;
			for (size_t i = 0; i < count; ++i) length += buffer[i].length();

			// create final string
			char_t* result = static_cast<char_t*>(stack.result->allocate((length + 1) * sizeof(char_t)));
			assert(result);

			char_t* ri = result;

			for (size_t j = 0; j < count; ++j)
				for (const char_t* bi = buffer[j].c_str(); *bi; ++bi)
					*ri++ = *bi;

			*ri = 0;

			return xpath_string(result, true);
		}

		xpath_string eval_string(const xpath_context& c, const xpath_stack& stack)
		{
			switch (_type)
			{
			case ast_string_constant:
				return xpath_string_const(_data.string);
			
			case ast_func_local_name_0:
			{
				xpath_node na = c.n;
				
				return xpath_string_const(local_name(na));
			}

			case ast_func_local_name_1:
			{
				xpath_allocator_capture cr(stack.result);

				xpath_node_set_raw ns = _left->eval_node_set(c, stack);
				xpath_node na = ns.first();
				
				return xpath_string_const(local_name(na));
			}

			case ast_func_name_0:
			{
				xpath_node na = c.n;
				
				return xpath_string_const(qualified_name(na));
			}

			case ast_func_name_1:
			{
				xpath_allocator_capture cr(stack.result);

				xpath_node_set_raw ns = _left->eval_node_set(c, stack);
				xpath_node na = ns.first();
				
				return xpath_string_const(qualified_name(na));
			}

			case ast_func_namespace_uri_0:
			{
				xpath_node na = c.n;
				
				return xpath_string_const(namespace_uri(na));
			}

			case ast_func_namespace_uri_1:
			{
				xpath_allocator_capture cr(stack.result);

				xpath_node_set_raw ns = _left->eval_node_set(c, stack);
				xpath_node na = ns.first();
				
				return xpath_string_const(namespace_uri(na));
			}

			case ast_func_string_0:
				return string_value(c.n, stack.result);

			case ast_func_string_1:
				return _left->eval_string(c, stack);

			case ast_func_concat:
				return eval_string_concat(c, stack);

			case ast_func_substring_before:
			{
				xpath_allocator_capture cr(stack.temp);

				xpath_stack swapped_stack = {stack.temp, stack.result};

				xpath_string s = _left->eval_string(c, swapped_stack);
				xpath_string p = _right->eval_string(c, swapped_stack);

				const char_t* pos = find_substring(s.c_str(), p.c_str());
				
				return pos ? xpath_string(s.c_str(), pos, stack.result) : xpath_string();
			}
			
			case ast_func_substring_after:
			{
				xpath_allocator_capture cr(stack.temp);

				xpath_stack swapped_stack = {stack.temp, stack.result};

				xpath_string s = _left->eval_string(c, swapped_stack);
				xpath_string p = _right->eval_string(c, swapped_stack);
				
				const char_t* pos = find_substring(s.c_str(), p.c_str());
				if (!pos) return xpath_string();

				const char_t* result = pos + p.length();

				return s.uses_heap() ? xpath_string(result, stack.result) : xpath_string_const(result);
			}

			case ast_func_substring_2:
			{
				xpath_allocator_capture cr(stack.temp);

				xpath_stack swapped_stack = {stack.temp, stack.result};

				xpath_string s = _left->eval_string(c, swapped_stack);
				size_t s_length = s.length();

				double first = round_nearest(_right->eval_number(c, stack));
				
				if (is_nan(first)) return xpath_string(); // NaN
				else if (first >= s_length + 1) return xpath_string();
				
				size_t pos = first < 1 ? 1 : (size_t)first;
				assert(1 <= pos && pos <= s_length + 1);

				const char_t* rbegin = s.c_str() + (pos - 1);
				
				return s.uses_heap() ? xpath_string(rbegin, stack.result) : xpath_string_const(rbegin);
			}
			
			case ast_func_substring_3:
			{
				xpath_allocator_capture cr(stack.temp);

				xpath_stack swapped_stack = {stack.temp, stack.result};

				xpath_string s = _left->eval_string(c, swapped_stack);
				size_t s_length = s.length();

				double first = round_nearest(_right->eval_number(c, stack));
				double last = first + round_nearest(_right->_next->eval_number(c, stack));
				
				if (is_nan(first) || is_nan(last)) return xpath_string();
				else if (first >= s_length + 1) return xpath_string();
				else if (first >= last) return xpath_string();
				else if (last < 1) return xpath_string();
				
				size_t pos = first < 1 ? 1 : (size_t)first;
				size_t end = last >= s_length + 1 ? s_length + 1 : (size_t)last;

				assert(1 <= pos && pos <= end && end <= s_length + 1);
				const char_t* rbegin = s.c_str() + (pos - 1);
				const char_t* rend = s.c_str() + (end - 1);

				return (end == s_length + 1 && !s.uses_heap()) ? xpath_string_const(rbegin) : xpath_string(rbegin, rend, stack.result);
			}

			case ast_func_normalize_space_0:
			{
				xpath_string s = string_value(c.n, stack.result);

				normalize_space(s.data(stack.result));

				return s;
			}

			case ast_func_normalize_space_1:
			{
				xpath_string s = _left->eval_string(c, stack);

				normalize_space(s.data(stack.result));
			
				return s;
			}

			case ast_func_translate:
			{
				xpath_allocator_capture cr(stack.temp);

				xpath_stack swapped_stack = {stack.temp, stack.result};

				xpath_string s = _left->eval_string(c, stack);
				xpath_string from = _right->eval_string(c, swapped_stack);
				xpath_string to = _right->_next->eval_string(c, swapped_stack);

				translate(s.data(stack.result), from.c_str(), to.c_str());

				return s;
			}

			case ast_variable:
			{
				assert(_rettype == _data.variable->type());

				if (_rettype == xpath_type_string)
					return xpath_string_const(_data.variable->get_string());

				// fallthrough to type conversion
			}

			default:
			{
				switch (_rettype)
				{
				case xpath_type_boolean:
					return xpath_string_const(eval_boolean(c, stack) ? PUGIXML_TEXT("true") : PUGIXML_TEXT("false"));
					
				case xpath_type_number:
					return convert_number_to_string(eval_number(c, stack), stack.result);
					
				case xpath_type_node_set:
				{
					xpath_allocator_capture cr(stack.temp);

					xpath_stack swapped_stack = {stack.temp, stack.result};

					xpath_node_set_raw ns = eval_node_set(c, swapped_stack);
					return ns.empty() ? xpath_string() : string_value(ns.first(), stack.result);
				}
				
				default:
					assert(!"Wrong expression for return type string");
					return xpath_string();
				}
			}
			}
		}

		xpath_node_set_raw eval_node_set(const xpath_context& c, const xpath_stack& stack)
		{
			switch (_type)
			{
			case ast_op_union:
			{
				xpath_allocator_capture cr(stack.temp);

				xpath_stack swapped_stack = {stack.temp, stack.result};

				xpath_node_set_raw ls = _left->eval_node_set(c, swapped_stack);
				xpath_node_set_raw rs = _right->eval_node_set(c, stack);
				
				// we can optimize merging two sorted sets, but this is a very rare operation, so don't bother
  		        rs.set_type(xpath_node_set::type_unsorted);

				rs.append(ls.begin(), ls.end(), stack.result);
				rs.remove_duplicates();
				
				return rs;
			}

			case ast_filter:
			case ast_filter_posinv:
			{
				xpath_node_set_raw set = _left->eval_node_set(c, stack);

				// either expression is a number or it contains position() call; sort by document order
				if (_type == ast_filter) set.sort_do();

				apply_predicate(set, 0, _right, stack);
			
				return set;
			}
			
			case ast_func_id:
				return xpath_node_set_raw();
			
			case ast_step:
			{
				switch (_axis)
				{
				case axis_ancestor:
					return step_do(c, stack, axis_to_type<axis_ancestor>());
					
				case axis_ancestor_or_self:
					return step_do(c, stack, axis_to_type<axis_ancestor_or_self>());

				case axis_attribute:
					return step_do(c, stack, axis_to_type<axis_attribute>());

				case axis_child:
					return step_do(c, stack, axis_to_type<axis_child>());
				
				case axis_descendant:
					return step_do(c, stack, axis_to_type<axis_descendant>());

				case axis_descendant_or_self:
					return step_do(c, stack, axis_to_type<axis_descendant_or_self>());

				case axis_following:
					return step_do(c, stack, axis_to_type<axis_following>());
				
				case axis_following_sibling:
					return step_do(c, stack, axis_to_type<axis_following_sibling>());
				
				case axis_namespace:
					// namespaced axis is not supported
					return xpath_node_set_raw();
				
				case axis_parent:
					return step_do(c, stack, axis_to_type<axis_parent>());
				
				case axis_preceding:
					return step_do(c, stack, axis_to_type<axis_preceding>());

				case axis_preceding_sibling:
					return step_do(c, stack, axis_to_type<axis_preceding_sibling>());
				
				case axis_self:
					return step_do(c, stack, axis_to_type<axis_self>());
				}
			}

			case ast_step_root:
			{
				assert(!_right); // root step can't have any predicates

				xpath_node_set_raw ns;

				ns.set_type(xpath_node_set::type_sorted);

				if (c.n.node()) ns.push_back(c.n.node().root(), stack.result);
				else if (c.n.attribute()) ns.push_back(c.n.parent().root(), stack.result);

				return ns;
			}

			case ast_variable:
			{
				assert(_rettype == _data.variable->type());

				if (_rettype == xpath_type_node_set)
				{
					const xpath_node_set& s = _data.variable->get_node_set();

					xpath_node_set_raw ns;

					ns.set_type(s.type());
					ns.append(s.begin(), s.end(), stack.result);

					return ns;
				}

				// fallthrough to type conversion
			}

			default:
				assert(!"Wrong expression for return type node set");
				return xpath_node_set_raw();
			}
		}
		
		bool is_posinv()
		{
			switch (_type)
			{
			case ast_func_position:
				return false;

			case ast_string_constant:
			case ast_number_constant:
			case ast_variable:
				return true;

			case ast_step:
			case ast_step_root:
				return true;

			case ast_predicate:
			case ast_filter:
			case ast_filter_posinv:
				return true;

			default:
				if (_left && !_left->is_posinv()) return false;
				
				for (xpath_ast_node* n = _right; n; n = n->_next)
					if (!n->is_posinv()) return false;
					
				return true;
			}
		}

		xpath_value_type rettype() const
		{
			return static_cast<xpath_value_type>(_rettype);
		}
	};

	struct xpath_parser
	{
	    xpath_allocator* _alloc;
	    xpath_lexer _lexer;

		const char_t* _query;
		xpath_variable_set* _variables;

		xpath_parse_result* _result;

	#ifdef PUGIXML_NO_EXCEPTIONS
		jmp_buf _error_handler;
	#endif

		void throw_error(const char* message)
		{
			_result->error = message;
			_result->offset = _lexer.current_pos() - _query;

		#ifdef PUGIXML_NO_EXCEPTIONS
			longjmp(_error_handler, 1);
		#else
			throw xpath_exception(*_result);
		#endif
		}

		void throw_error_oom()
        {
        #ifdef PUGIXML_NO_EXCEPTIONS
            throw_error("Out of memory");
        #else
            throw std::bad_alloc();
        #endif
        }

		void* alloc_node()
		{
			void* result = _alloc->allocate_nothrow(sizeof(xpath_ast_node));

			if (!result) throw_error_oom();

			return result;
		}

		const char_t* alloc_string(const xpath_lexer_string& value)
		{
			if (value.begin)
			{
				size_t length = static_cast<size_t>(value.end - value.begin);

				char_t* c = static_cast<char_t*>(_alloc->allocate_nothrow((length + 1) * sizeof(char_t)));
				if (!c) throw_error_oom();

				memcpy(c, value.begin, length * sizeof(char_t));
				c[length] = 0;

				return c;
			}
			else return 0;
		}

		xpath_ast_node* parse_function_helper(ast_type_t type0, ast_type_t type1, size_t argc, xpath_ast_node* args[2])
		{
			assert(argc <= 1);

			if (argc == 1 && args[0]->rettype() != xpath_type_node_set) throw_error("Function has to be applied to node set");

			return new (alloc_node()) xpath_ast_node(argc == 0 ? type0 : type1, xpath_type_string, args[0]);
		}

		xpath_ast_node* parse_function(const xpath_lexer_string& name, size_t argc, xpath_ast_node* args[2])
		{
			switch (name.begin[0])
			{
			case 'b':
				if (name == PUGIXML_TEXT("boolean") && argc == 1)
					return new (alloc_node()) xpath_ast_node(ast_func_boolean, xpath_type_boolean, args[0]);
					
				break;
			
			case 'c':
				if (name == PUGIXML_TEXT("count") && argc == 1)
				{
					if (args[0]->rettype() != xpath_type_node_set) throw_error("Function has to be applied to node set");
					return new (alloc_node()) xpath_ast_node(ast_func_count, xpath_type_number, args[0]);
				}
				else if (name == PUGIXML_TEXT("contains") && argc == 2)
					return new (alloc_node()) xpath_ast_node(ast_func_contains, xpath_type_string, args[0], args[1]);
				else if (name == PUGIXML_TEXT("concat") && argc >= 2)
					return new (alloc_node()) xpath_ast_node(ast_func_concat, xpath_type_string, args[0], args[1]);
				else if (name == PUGIXML_TEXT("ceiling") && argc == 1)
					return new (alloc_node()) xpath_ast_node(ast_func_ceiling, xpath_type_number, args[0]);
					
				break;
			
			case 'f':
				if (name == PUGIXML_TEXT("false") && argc == 0)
					return new (alloc_node()) xpath_ast_node(ast_func_false, xpath_type_boolean);
				else if (name == PUGIXML_TEXT("floor") && argc == 1)
					return new (alloc_node()) xpath_ast_node(ast_func_floor, xpath_type_number, args[0]);
					
				break;
			
			case 'i':
				if (name == PUGIXML_TEXT("id") && argc == 1)
					return new (alloc_node()) xpath_ast_node(ast_func_id, xpath_type_node_set, args[0]);
					
				break;
			
			case 'l':
				if (name == PUGIXML_TEXT("last") && argc == 0)
					return new (alloc_node()) xpath_ast_node(ast_func_last, xpath_type_number);
				else if (name == PUGIXML_TEXT("lang") && argc == 1)
					return new (alloc_node()) xpath_ast_node(ast_func_lang, xpath_type_boolean, args[0]);
				else if (name == PUGIXML_TEXT("local-name") && argc <= 1)
					return parse_function_helper(ast_func_local_name_0, ast_func_local_name_1, argc, args);
			
				break;
			
			case 'n':
				if (name == PUGIXML_TEXT("name") && argc <= 1)
					return parse_function_helper(ast_func_name_0, ast_func_name_1, argc, args);
				else if (name == PUGIXML_TEXT("namespace-uri") && argc <= 1)
					return parse_function_helper(ast_func_namespace_uri_0, ast_func_namespace_uri_1, argc, args);
				else if (name == PUGIXML_TEXT("normalize-space") && argc <= 1)
					return new (alloc_node()) xpath_ast_node(argc == 0 ? ast_func_normalize_space_0 : ast_func_normalize_space_1, xpath_type_string, args[0], args[1]);
				else if (name == PUGIXML_TEXT("not") && argc == 1)
					return new (alloc_node()) xpath_ast_node(ast_func_not, xpath_type_boolean, args[0]);
				else if (name == PUGIXML_TEXT("number") && argc <= 1)
					return new (alloc_node()) xpath_ast_node(argc == 0 ? ast_func_number_0 : ast_func_number_1, xpath_type_number, args[0]);
			
				break;
			
			case 'p':
				if (name == PUGIXML_TEXT("position") && argc == 0)
					return new (alloc_node()) xpath_ast_node(ast_func_position, xpath_type_number);
				
				break;
			
			case 'r':
				if (name == PUGIXML_TEXT("round") && argc == 1)
					return new (alloc_node()) xpath_ast_node(ast_func_round, xpath_type_number, args[0]);

				break;
			
			case 's':
				if (name == PUGIXML_TEXT("string") && argc <= 1)
					return new (alloc_node()) xpath_ast_node(argc == 0 ? ast_func_string_0 : ast_func_string_1, xpath_type_string, args[0]);
				else if (name == PUGIXML_TEXT("string-length") && argc <= 1)
					return new (alloc_node()) xpath_ast_node(argc == 0 ? ast_func_string_length_0 : ast_func_string_length_1, xpath_type_string, args[0]);
				else if (name == PUGIXML_TEXT("starts-with") && argc == 2)
					return new (alloc_node()) xpath_ast_node(ast_func_starts_with, xpath_type_boolean, args[0], args[1]);
				else if (name == PUGIXML_TEXT("substring-before") && argc == 2)
					return new (alloc_node()) xpath_ast_node(ast_func_substring_before, xpath_type_string, args[0], args[1]);
				else if (name == PUGIXML_TEXT("substring-after") && argc == 2)
					return new (alloc_node()) xpath_ast_node(ast_func_substring_after, xpath_type_string, args[0], args[1]);
				else if (name == PUGIXML_TEXT("substring") && (argc == 2 || argc == 3))
					return new (alloc_node()) xpath_ast_node(argc == 2 ? ast_func_substring_2 : ast_func_substring_3, xpath_type_string, args[0], args[1]);
				else if (name == PUGIXML_TEXT("sum") && argc == 1)
				{
					if (args[0]->rettype() != xpath_type_node_set) throw_error("Function has to be applied to node set");
					return new (alloc_node()) xpath_ast_node(ast_func_sum, xpath_type_number, args[0]);
				}

				break;
			
			case 't':
				if (name == PUGIXML_TEXT("translate") && argc == 3)
					return new (alloc_node()) xpath_ast_node(ast_func_translate, xpath_type_string, args[0], args[1]);
				else if (name == PUGIXML_TEXT("true") && argc == 0)
					return new (alloc_node()) xpath_ast_node(ast_func_true, xpath_type_boolean);
					
				break;
			}

			throw_error("Unrecognized function or wrong parameter count");

			return 0;
		}

		axis_t parse_axis_name(const xpath_lexer_string& name, bool& specified)
		{
			specified = true;

			switch (name.begin[0])
			{
			case 'a':
				if (name == PUGIXML_TEXT("ancestor"))
					return axis_ancestor;
				else if (name == PUGIXML_TEXT("ancestor-or-self"))
					return axis_ancestor_or_self;
				else if (name == PUGIXML_TEXT("attribute"))
					return axis_attribute;
				
				break;
			
			case 'c':
				if (name == PUGIXML_TEXT("child"))
					return axis_child;
				
				break;
			
			case 'd':
				if (name == PUGIXML_TEXT("descendant"))
					return axis_descendant;
				else if (name == PUGIXML_TEXT("descendant-or-self"))
					return axis_descendant_or_self;
				
				break;
			
			case 'f':
				if (name == PUGIXML_TEXT("following"))
					return axis_following;
				else if (name == PUGIXML_TEXT("following-sibling"))
					return axis_following_sibling;
				
				break;
			
			case 'n':
				if (name == PUGIXML_TEXT("namespace"))
					return axis_namespace;
				
				break;
			
			case 'p':
				if (name == PUGIXML_TEXT("parent"))
					return axis_parent;
				else if (name == PUGIXML_TEXT("preceding"))
					return axis_preceding;
				else if (name == PUGIXML_TEXT("preceding-sibling"))
					return axis_preceding_sibling;
				
				break;
			
			case 's':
				if (name == PUGIXML_TEXT("self"))
					return axis_self;
				
				break;
			}

			specified = false;
			return axis_child;
		}

		nodetest_t parse_node_test_type(const xpath_lexer_string& name)
		{
			switch (name.begin[0])
			{
			case 'c':
				if (name == PUGIXML_TEXT("comment"))
					return nodetest_type_comment;

				break;

			case 'n':
				if (name == PUGIXML_TEXT("node"))
					return nodetest_type_node;

				break;

			case 'p':
				if (name == PUGIXML_TEXT("processing-instruction"))
					return nodetest_type_pi;

				break;

			case 't':
				if (name == PUGIXML_TEXT("text"))
					return nodetest_type_text;

				break;
			}

			return nodetest_none;
		}

	    // PrimaryExpr ::= VariableReference | '(' Expr ')' | Literal | Number | FunctionCall
	    xpath_ast_node* parse_primary_expression()
	    {
	    	switch (_lexer.current())
	    	{
	    	case lex_var_ref:
	    	{
				xpath_lexer_string name = _lexer.contents();

				if (!_variables)
					throw_error("Unknown variable: variable set is not provided");

				xpath_variable* var = get_variable(_variables, name.begin, name.end);

				if (!var)
					throw_error("Unknown variable: variable set does not contain the given name");

				_lexer.next();

	    		return new (alloc_node()) xpath_ast_node(ast_variable, var->type(), var);
			}

			case lex_open_brace:
			{
				_lexer.next();

				xpath_ast_node* n = parse_expression();

				if (_lexer.current() != lex_close_brace)
					throw_error("Unmatched braces");

				_lexer.next();

				return n;
			}

			case lex_quoted_string:
			{
				const char_t* value = alloc_string(_lexer.contents());

				xpath_ast_node* n = new (alloc_node()) xpath_ast_node(ast_string_constant, xpath_type_string, value);
				_lexer.next();

				return n;
			}

			case lex_number:
			{
				double value = 0;

				if (!convert_string_to_number(_lexer.contents().begin, _lexer.contents().end, &value))
					throw_error_oom();

				xpath_ast_node* n = new (alloc_node()) xpath_ast_node(ast_number_constant, xpath_type_number, value);
				_lexer.next();

				return n;
			}

			case lex_string:
			{
				xpath_ast_node* args[2] = {0};
				size_t argc = 0;
				
				xpath_lexer_string function = _lexer.contents();
				_lexer.next();
				
				xpath_ast_node* last_arg = 0;
				
				if (_lexer.current() != lex_open_brace)
					throw_error("Unrecognized function call");
				_lexer.next();

				if (_lexer.current() != lex_close_brace)
					args[argc++] = parse_expression();

				while (_lexer.current() != lex_close_brace)
				{
					if (_lexer.current() != lex_comma)
						throw_error("No comma between function arguments");
					_lexer.next();
					
					xpath_ast_node* n = parse_expression();
					
					if (argc < 2) args[argc] = n;
					else last_arg->set_next(n);

					argc++;
					last_arg = n;
				}
				
				_lexer.next();

				return parse_function(function, argc, args);
			}

	    	default:
	    		throw_error("Unrecognizable primary expression");

	    		return 0;
	    	}
	    }
	    
	    // FilterExpr ::= PrimaryExpr | FilterExpr Predicate
	    // Predicate ::= '[' PredicateExpr ']'
	    // PredicateExpr ::= Expr
	    xpath_ast_node* parse_filter_expression()
	    {
	    	xpath_ast_node* n = parse_primary_expression();

	    	while (_lexer.current() == lex_open_square_brace)
	    	{
	    		_lexer.next();

				xpath_ast_node* expr = parse_expression();

				if (n->rettype() != xpath_type_node_set) throw_error("Predicate has to be applied to node set");

				bool posinv = expr->rettype() != xpath_type_number && expr->is_posinv();

	    		n = new (alloc_node()) xpath_ast_node(posinv ? ast_filter_posinv : ast_filter, xpath_type_node_set, n, expr);

	    		if (_lexer.current() != lex_close_square_brace)
	    			throw_error("Unmatched square brace");
	    	
	    		_lexer.next();
	    	}
	    	
	    	return n;
	    }
	    
	    // Step ::= AxisSpecifier NodeTest Predicate* | AbbreviatedStep
	    // AxisSpecifier ::= AxisName '::' | '@'?
	    // NodeTest ::= NameTest | NodeType '(' ')' | 'processing-instruction' '(' Literal ')'
	    // NameTest ::= '*' | NCName ':' '*' | QName
	    // AbbreviatedStep ::= '.' | '..'
	    xpath_ast_node* parse_step(xpath_ast_node* set)
	    {
			if (set && set->rettype() != xpath_type_node_set)
				throw_error("Step has to be applied to node set");

			bool axis_specified = false;
			axis_t axis = axis_child; // implied child axis

			if (_lexer.current() == lex_axis_attribute)
			{
				axis = axis_attribute;
				axis_specified = true;
				
				_lexer.next();
			}
			else if (_lexer.current() == lex_dot)
			{
				_lexer.next();
				
				return new (alloc_node()) xpath_ast_node(ast_step, set, axis_self, nodetest_type_node, 0);
			}
			else if (_lexer.current() == lex_double_dot)
			{
				_lexer.next();
				
				return new (alloc_node()) xpath_ast_node(ast_step, set, axis_parent, nodetest_type_node, 0);
			}
	    
			nodetest_t nt_type = nodetest_none;
			xpath_lexer_string nt_name;
			
			if (_lexer.current() == lex_string)
			{
				// node name test
				nt_name = _lexer.contents();
				_lexer.next();

				// was it an axis name?
				if (_lexer.current() == lex_double_colon)
				{
					// parse axis name
					if (axis_specified) throw_error("Two axis specifiers in one step");

					axis = parse_axis_name(nt_name, axis_specified);

					if (!axis_specified) throw_error("Unknown axis");

					// read actual node test
					_lexer.next();

					if (_lexer.current() == lex_multiply)
					{
						nt_type = nodetest_all;
						nt_name = xpath_lexer_string();
						_lexer.next();
					}
					else if (_lexer.current() == lex_string)
					{
						nt_name = _lexer.contents();
						_lexer.next();
					}
					else throw_error("Unrecognized node test");
				}
				
				if (nt_type == nodetest_none)
				{
					// node type test or processing-instruction
					if (_lexer.current() == lex_open_brace)
					{
						_lexer.next();
						
						if (_lexer.current() == lex_close_brace)
						{
							_lexer.next();

							nt_type = parse_node_test_type(nt_name);

							if (nt_type == nodetest_none) throw_error("Unrecognized node type");
							
							nt_name = xpath_lexer_string();
						}
						else if (nt_name == PUGIXML_TEXT("processing-instruction"))
						{
							if (_lexer.current() != lex_quoted_string)
								throw_error("Only literals are allowed as arguments to processing-instruction()");
						
							nt_type = nodetest_pi;
							nt_name = _lexer.contents();
							_lexer.next();
							
							if (_lexer.current() != lex_close_brace)
								throw_error("Unmatched brace near processing-instruction()");
							_lexer.next();
						}
						else
							throw_error("Unmatched brace near node type test");

					}
					// QName or NCName:*
					else
					{
						if (nt_name.end - nt_name.begin > 2 && nt_name.end[-2] == ':' && nt_name.end[-1] == '*') // NCName:*
						{
							nt_name.end--; // erase *
							
							nt_type = nodetest_all_in_namespace;
						}
						else nt_type = nodetest_name;
					}
				}
			}
			else if (_lexer.current() == lex_multiply)
			{
				nt_type = nodetest_all;
				_lexer.next();
			}
			else throw_error("Unrecognized node test");
			
			xpath_ast_node* n = new (alloc_node()) xpath_ast_node(ast_step, set, axis, nt_type, alloc_string(nt_name));
			
			xpath_ast_node* last = 0;
			
			while (_lexer.current() == lex_open_square_brace)
			{
				_lexer.next();
				
				xpath_ast_node* expr = parse_expression();

				xpath_ast_node* pred = new (alloc_node()) xpath_ast_node(ast_predicate, xpath_type_node_set, expr);
				
				if (_lexer.current() != lex_close_square_brace)
	    			throw_error("Unmatched square brace");
				_lexer.next();
				
				if (last) last->set_next(pred);
				else n->set_right(pred);
				
				last = pred;
			}
			
			return n;
	    }
	    
	    // RelativeLocationPath ::= Step | RelativeLocationPath '/' Step | RelativeLocationPath '//' Step
	    xpath_ast_node* parse_relative_location_path(xpath_ast_node* set)
	    {
			xpath_ast_node* n = parse_step(set);
			
			while (_lexer.current() == lex_slash || _lexer.current() == lex_double_slash)
			{
				lexeme_t l = _lexer.current();
				_lexer.next();

				if (l == lex_double_slash)
					n = new (alloc_node()) xpath_ast_node(ast_step, n, axis_descendant_or_self, nodetest_type_node, 0);
				
				n = parse_step(n);
			}
			
			return n;
	    }
	    
	    // LocationPath ::= RelativeLocationPath | AbsoluteLocationPath
	    // AbsoluteLocationPath ::= '/' RelativeLocationPath? | '//' RelativeLocationPath
	    xpath_ast_node* parse_location_path()
	    {
			if (_lexer.current() == lex_slash)
			{
				_lexer.next();
				
				xpath_ast_node* n = new (alloc_node()) xpath_ast_node(ast_step_root, xpath_type_node_set);

				// relative location path can start from axis_attribute, dot, double_dot, multiply and string lexemes; any other lexeme means standalone root path
				lexeme_t l = _lexer.current();

				if (l == lex_string || l == lex_axis_attribute || l == lex_dot || l == lex_double_dot || l == lex_multiply)
					return parse_relative_location_path(n);
				else
					return n;
			}
			else if (_lexer.current() == lex_double_slash)
			{
				_lexer.next();
				
				xpath_ast_node* n = new (alloc_node()) xpath_ast_node(ast_step_root, xpath_type_node_set);
				n = new (alloc_node()) xpath_ast_node(ast_step, n, axis_descendant_or_self, nodetest_type_node, 0);
				
				return parse_relative_location_path(n);
			}

			// else clause moved outside of if because of bogus warning 'control may reach end of non-void function being inlined' in gcc 4.0.1
			return parse_relative_location_path(0);
	    }
	    
	    // PathExpr ::= LocationPath
	    //				| FilterExpr
	    //				| FilterExpr '/' RelativeLocationPath
	    //				| FilterExpr '//' RelativeLocationPath
	    xpath_ast_node* parse_path_expression()
	    {
			// Clarification.
			// PathExpr begins with either LocationPath or FilterExpr.
			// FilterExpr begins with PrimaryExpr
			// PrimaryExpr begins with '$' in case of it being a variable reference,
			// '(' in case of it being an expression, string literal, number constant or
			// function call.

			if (_lexer.current() == lex_var_ref || _lexer.current() == lex_open_brace || 
				_lexer.current() == lex_quoted_string || _lexer.current() == lex_number ||
				_lexer.current() == lex_string)
	    	{
	    		if (_lexer.current() == lex_string)
	    		{
	    			// This is either a function call, or not - if not, we shall proceed with location path
	    			const char_t* state = _lexer.state();
	    			
					while (IS_CHARTYPE(*state, ct_space)) ++state;
	    			
	    			if (*state != '(') return parse_location_path();

					// This looks like a function call; however this still can be a node-test. Check it.
					if (parse_node_test_type(_lexer.contents()) != nodetest_none) return parse_location_path();
	    		}
	    		
	    		xpath_ast_node* n = parse_filter_expression();

	    		if (_lexer.current() == lex_slash || _lexer.current() == lex_double_slash)
	    		{
					lexeme_t l = _lexer.current();
	    			_lexer.next();
	    			
					if (l == lex_double_slash)
					{
						if (n->rettype() != xpath_type_node_set) throw_error("Step has to be applied to node set");

						n = new (alloc_node()) xpath_ast_node(ast_step, n, axis_descendant_or_self, nodetest_type_node, 0);
					}
	
	    			// select from location path
	    			return parse_relative_location_path(n);
	    		}

	    		return n;
	    	}
	    	else return parse_location_path();
	    }

	    // UnionExpr ::= PathExpr | UnionExpr '|' PathExpr
	    xpath_ast_node* parse_union_expression()
	    {
	    	xpath_ast_node* n = parse_path_expression();

	    	while (_lexer.current() == lex_union)
	    	{
	    		_lexer.next();

				xpath_ast_node* expr = parse_union_expression();

				if (n->rettype() != xpath_type_node_set || expr->rettype() != xpath_type_node_set)
					throw_error("Union operator has to be applied to node sets");

	    		n = new (alloc_node()) xpath_ast_node(ast_op_union, xpath_type_node_set, n, expr);
	    	}

	    	return n;
	    }

	    // UnaryExpr ::= UnionExpr | '-' UnaryExpr
	    xpath_ast_node* parse_unary_expression()
	    {
	    	if (_lexer.current() == lex_minus)
	    	{
	    		_lexer.next();

				xpath_ast_node* expr = parse_unary_expression();

	    		return new (alloc_node()) xpath_ast_node(ast_op_negate, xpath_type_number, expr);
	    	}
	    	else return parse_union_expression();
	    }
	    
	    // MultiplicativeExpr ::= UnaryExpr
	    //						  | MultiplicativeExpr '*' UnaryExpr
	    //						  | MultiplicativeExpr 'div' UnaryExpr
	    //						  | MultiplicativeExpr 'mod' UnaryExpr
	    xpath_ast_node* parse_multiplicative_expression()
	    {
	    	xpath_ast_node* n = parse_unary_expression();

	    	while (_lexer.current() == lex_multiply || (_lexer.current() == lex_string &&
	    		   (_lexer.contents() == PUGIXML_TEXT("mod") || _lexer.contents() == PUGIXML_TEXT("div"))))
	    	{
	    		ast_type_t op = _lexer.current() == lex_multiply ? ast_op_multiply :
	    			_lexer.contents().begin[0] == 'd' ? ast_op_divide : ast_op_mod;
	    		_lexer.next();

				xpath_ast_node* expr = parse_unary_expression();

	    		n = new (alloc_node()) xpath_ast_node(op, xpath_type_number, n, expr);
	    	}

	    	return n;
	    }

	    // AdditiveExpr ::= MultiplicativeExpr
	    //					| AdditiveExpr '+' MultiplicativeExpr
	    //					| AdditiveExpr '-' MultiplicativeExpr
	    xpath_ast_node* parse_additive_expression()
	    {
	    	xpath_ast_node* n = parse_multiplicative_expression();

	    	while (_lexer.current() == lex_plus || _lexer.current() == lex_minus)
	    	{
	    		lexeme_t l = _lexer.current();

	    		_lexer.next();

				xpath_ast_node* expr = parse_multiplicative_expression();

	    		n = new (alloc_node()) xpath_ast_node(l == lex_plus ? ast_op_add : ast_op_subtract, xpath_type_number, n, expr);
	    	}

	    	return n;
	    }

	    // RelationalExpr ::= AdditiveExpr
	    //					  | RelationalExpr '<' AdditiveExpr
	    //					  | RelationalExpr '>' AdditiveExpr
	    //					  | RelationalExpr '<=' AdditiveExpr
	    //					  | RelationalExpr '>=' AdditiveExpr
	    xpath_ast_node* parse_relational_expression()
	    {
	    	xpath_ast_node* n = parse_additive_expression();

	    	while (_lexer.current() == lex_less || _lexer.current() == lex_less_or_equal || 
	    		   _lexer.current() == lex_greater || _lexer.current() == lex_greater_or_equal)
	    	{
	    		lexeme_t l = _lexer.current();
	    		_lexer.next();

				xpath_ast_node* expr = parse_additive_expression();

	    		n = new (alloc_node()) xpath_ast_node(l == lex_less ? ast_op_less : l == lex_greater ? ast_op_greater :
	    						l == lex_less_or_equal ? ast_op_less_or_equal : ast_op_greater_or_equal, xpath_type_boolean, n, expr);
	    	}

	    	return n;
	    }
	    
	    // EqualityExpr ::= RelationalExpr
	    //					| EqualityExpr '=' RelationalExpr
	    //					| EqualityExpr '!=' RelationalExpr
	    xpath_ast_node* parse_equality_expression()
	    {
	    	xpath_ast_node* n = parse_relational_expression();

	    	while (_lexer.current() == lex_equal || _lexer.current() == lex_not_equal)
	    	{
	    		lexeme_t l = _lexer.current();

	    		_lexer.next();

				xpath_ast_node* expr = parse_relational_expression();

	    		n = new (alloc_node()) xpath_ast_node(l == lex_equal ? ast_op_equal : ast_op_not_equal, xpath_type_boolean, n, expr);
	    	}

	    	return n;
	    }
	    
	    // AndExpr ::= EqualityExpr | AndExpr 'and' EqualityExpr
	    xpath_ast_node* parse_and_expression()
	    {
	    	xpath_ast_node* n = parse_equality_expression();

	    	while (_lexer.current() == lex_string && _lexer.contents() == PUGIXML_TEXT("and"))
	    	{
	    		_lexer.next();

				xpath_ast_node* expr = parse_equality_expression();

	    		n = new (alloc_node()) xpath_ast_node(ast_op_and, xpath_type_boolean, n, expr);
	    	}

	    	return n;
	    }

	    // OrExpr ::= AndExpr | OrExpr 'or' AndExpr
	    xpath_ast_node* parse_or_expression()
	    {
	    	xpath_ast_node* n = parse_and_expression();

	    	while (_lexer.current() == lex_string && _lexer.contents() == PUGIXML_TEXT("or"))
	    	{
	    		_lexer.next();

				xpath_ast_node* expr = parse_and_expression();

	    		n = new (alloc_node()) xpath_ast_node(ast_op_or, xpath_type_boolean, n, expr);
	    	}

	    	return n;
	    }
		
		// Expr ::= OrExpr
		xpath_ast_node* parse_expression()
		{
			return parse_or_expression();
		}

		xpath_parser(const char_t* query, xpath_variable_set* variables, xpath_allocator* alloc, xpath_parse_result* result): _alloc(alloc), _lexer(query), _query(query), _variables(variables), _result(result)
		{
		}

		xpath_ast_node* parse()
		{
			xpath_ast_node* result = parse_expression();
			
			if (_lexer.current() != lex_eof)
			{
				// there are still unparsed tokens left, error
				throw_error("Incorrect query");
			}
			
			return result;
		}

		static xpath_ast_node* parse(const char_t* query, xpath_variable_set* variables, xpath_allocator* alloc, xpath_parse_result* result)
		{
			xpath_parser parser(query, variables, alloc, result);

		#ifdef PUGIXML_NO_EXCEPTIONS
			int error = setjmp(parser._error_handler);

			return (error == 0) ? parser.parse() : 0;
		#else
			return parser.parse();
		#endif
		}
	};

    struct xpath_query_impl
    {
		static xpath_query_impl* create()
		{
			void* memory = global_allocate(sizeof(xpath_query_impl));

            return new (memory) xpath_query_impl();
		}

		static void destroy(void* ptr)
		{
			if (!ptr) return;
			
			// free all allocated pages
			static_cast<xpath_query_impl*>(ptr)->alloc.release();

			// free allocator memory (with the first page)
			global_deallocate(ptr);
		}

        xpath_query_impl(): root(0), alloc(&block)
        {
            block.next = 0;
        }

        xpath_ast_node* root;
        xpath_allocator alloc;
        xpath_memory_block block;
    };

	xpath_string evaluate_string_impl(xpath_query_impl* impl, const xpath_node& n, xpath_stack_data& sd)
	{
		if (!impl) return xpath_string();

	#ifdef PUGIXML_NO_EXCEPTIONS
		if (setjmp(sd.error_handler)) return xpath_string();
	#endif

		xpath_context c(n, 1, 1);

		return impl->root->eval_string(c, sd.stack);
	}
}

namespace pugi
{
#ifndef PUGIXML_NO_EXCEPTIONS
	xpath_exception::xpath_exception(const xpath_parse_result& result): _result(result)
	{
		assert(result.error);
	}
	
	const char* xpath_exception::what() const throw()
	{
		return _result.error;
	}

	const xpath_parse_result& xpath_exception::result() const
	{
		return _result;
	}
#endif
	
	xpath_node::xpath_node()
	{
	}
		
	xpath_node::xpath_node(const xml_node& node): _node(node)
	{
	}
		
	xpath_node::xpath_node(const xml_attribute& attribute, const xml_node& parent): _node(attribute ? parent : xml_node()), _attribute(attribute)
	{
	}

	xml_node xpath_node::node() const
	{
		return _attribute ? xml_node() : _node;
	}
		
	xml_attribute xpath_node::attribute() const
	{
		return _attribute;
	}
	
	xml_node xpath_node::parent() const
	{
		return _attribute ? _node : _node.parent();
	}

	xpath_node::operator xpath_node::unspecified_bool_type() const
	{
		return (_node || _attribute) ? &xpath_node::_node : 0;
	}
	
	bool xpath_node::operator!() const
	{
		return !(_node || _attribute);
	}

	bool xpath_node::operator==(const xpath_node& n) const
	{
		return _node == n._node && _attribute == n._attribute;
	}
	
	bool xpath_node::operator!=(const xpath_node& n) const
	{
		return _node != n._node || _attribute != n._attribute;
	}

#ifdef __BORLANDC__
	bool operator&&(const xpath_node& lhs, bool rhs)
	{
		return (bool)lhs && rhs;
	}

	bool operator||(const xpath_node& lhs, bool rhs)
	{
		return (bool)lhs || rhs;
	}
#endif

	void xpath_node_set::_assign(const_iterator begin, const_iterator end)
	{
		assert(begin <= end);

		size_t size = static_cast<size_t>(end - begin);

		if (size <= 1)
		{
			// deallocate old buffer
			if (_begin != &_storage) global_deallocate(_begin);

			// use internal buffer
			if (begin != end) _storage = *begin;

			_begin = &_storage;
			_end = &_storage + size;
		}
		else
		{
			// make heap copy
			xpath_node* storage = static_cast<xpath_node*>(global_allocate(size * sizeof(xpath_node)));

			if (!storage)
			{
			#ifdef PUGIXML_NO_EXCEPTIONS
				return;
			#else
				throw std::bad_alloc();
			#endif
			}

			memcpy(storage, begin, size * sizeof(xpath_node));
			
			// deallocate old buffer
			if (_begin != &_storage) global_deallocate(_begin);

			// finalize
			_begin = storage;
			_end = storage + size;
		}
	}

	xpath_node_set::xpath_node_set(): _type(type_unsorted), _begin(&_storage), _end(&_storage)
	{
	}

	xpath_node_set::xpath_node_set(const_iterator begin, const_iterator end, type_t type): _type(type), _begin(&_storage), _end(&_storage)
	{
		_assign(begin, end);
	}

	xpath_node_set::~xpath_node_set()
	{
		if (_begin != &_storage) global_deallocate(_begin);
	}
		
	xpath_node_set::xpath_node_set(const xpath_node_set& ns): _type(ns._type), _begin(&_storage), _end(&_storage)
	{
		_assign(ns._begin, ns._end);
	}
	
	xpath_node_set& xpath_node_set::operator=(const xpath_node_set& ns)
	{
		if (this == &ns) return *this;
		
		_type = ns._type;
		_assign(ns._begin, ns._end);

		return *this;
	}

	xpath_node_set::type_t xpath_node_set::type() const
	{
		return _type;
	}
		
	size_t xpath_node_set::size() const
	{
		return _end - _begin;
	}
		
	bool xpath_node_set::empty() const
	{
		return _begin == _end;
	}
		
	const xpath_node& xpath_node_set::operator[](size_t index) const
	{
		assert(index < size());
		return _begin[index];
	}

	xpath_node_set::const_iterator xpath_node_set::begin() const
	{
		return _begin;
	}
		
	xpath_node_set::const_iterator xpath_node_set::end() const
	{
		return _end;
	}
	
	void xpath_node_set::sort(bool reverse)
	{
		_type = xpath_sort(_begin, _end, _type, reverse);
	}

	xpath_node xpath_node_set::first() const
	{
		return xpath_first(_begin, _end, _type);
	}

    xpath_parse_result::xpath_parse_result(): error("Internal error"), offset(0)
    {
    }

    xpath_parse_result::operator bool() const
    {
        return error == 0;
    }
	const char* xpath_parse_result::description() const
	{
		return error ? error : "No error";
	}

	xpath_variable::xpath_variable()
    {
    }

	const char_t* xpath_variable::name() const
	{
		switch (_type)
		{
		case xpath_type_node_set:
			return static_cast<const xpath_variable_node_set*>(this)->name;

		case xpath_type_number:
			return static_cast<const xpath_variable_number*>(this)->name;

		case xpath_type_string:
			return static_cast<const xpath_variable_string*>(this)->name;

		case xpath_type_boolean:
			return static_cast<const xpath_variable_boolean*>(this)->name;

		default:
			assert(!"Invalid variable type");
			return 0;
		}
	}

	xpath_value_type xpath_variable::type() const
	{
		return _type;
	}

	bool xpath_variable::get_boolean() const
	{
		return (_type == xpath_type_boolean) ? static_cast<const xpath_variable_boolean*>(this)->value : false;
	}

	double xpath_variable::get_number() const
	{
		return (_type == xpath_type_number) ? static_cast<const xpath_variable_number*>(this)->value : gen_nan();
	}

	const char_t* xpath_variable::get_string() const
	{
		const char_t* value = (_type == xpath_type_string) ? static_cast<const xpath_variable_string*>(this)->value : 0;
		return value ? value : PUGIXML_TEXT("");
	}

	const xpath_node_set& xpath_variable::get_node_set() const
	{
		return (_type == xpath_type_node_set) ? static_cast<const xpath_variable_node_set*>(this)->value : dummy_node_set;
	}

	bool xpath_variable::set(bool value)
	{
		if (_type != xpath_type_boolean) return false;

		static_cast<xpath_variable_boolean*>(this)->value = value;
		return true;
	}

	bool xpath_variable::set(double value)
	{
		if (_type != xpath_type_number) return false;

		static_cast<xpath_variable_number*>(this)->value = value;
		return true;
	}

	bool xpath_variable::set(const char_t* value)
	{
		if (_type != xpath_type_string) return false;

		xpath_variable_string* var = static_cast<xpath_variable_string*>(this);

		// duplicate string
		size_t size = (strlength(value) + 1) * sizeof(char_t);

		char_t* copy = static_cast<char_t*>(global_allocate(size));
		if (!copy) return false;

		memcpy(copy, value, size);

		// replace old string
		if (var->value) global_deallocate(var->value);
		var->value = copy;

		return true;
	}

	bool xpath_variable::set(const xpath_node_set& value)
	{
		if (_type != xpath_type_node_set) return false;

		static_cast<xpath_variable_node_set*>(this)->value = value;
		return true;
	}

	xpath_variable_set::xpath_variable_set()
	{
		for (size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i) _data[i] = 0;
	}

	xpath_variable_set::~xpath_variable_set()
	{
		for (size_t i = 0; i < sizeof(_data) / sizeof(_data[0]); ++i)
		{
			xpath_variable* var = _data[i];

			while (var)
			{
				xpath_variable* next = var->_next;

				delete_xpath_variable(var->_type, var);

				var = next;
			}
		}
	}

	xpath_variable* xpath_variable_set::find(const char_t* name) const
	{
		const size_t hash_size = sizeof(_data) / sizeof(_data[0]);
		size_t hash = hash_string(name) % hash_size;

		// look for existing variable
		for (xpath_variable* var = _data[hash]; var; var = var->_next)
			if (strequal(var->name(), name))
				return var;

		return 0;
	}

	xpath_variable* xpath_variable_set::add(const char_t* name, xpath_value_type type)
	{
		const size_t hash_size = sizeof(_data) / sizeof(_data[0]);
		size_t hash = hash_string(name) % hash_size;

		// look for existing variable
		for (xpath_variable* var = _data[hash]; var; var = var->_next)
			if (strequal(var->name(), name))
				return var->type() == type ? var : 0;

		// add new variable
		xpath_variable* result = new_xpath_variable(type, name);

		if (result)
		{
			result->_type = type;
			result->_next = _data[hash];

			_data[hash] = result;
		}

		return result;
	}

	bool xpath_variable_set::set(const char_t* name, bool value)
	{
		xpath_variable* var = add(name, xpath_type_boolean);
		return var ? var->set(value) : false;
	}

	bool xpath_variable_set::set(const char_t* name, double value)
	{
		xpath_variable* var = add(name, xpath_type_number);
		return var ? var->set(value) : false;
	}

	bool xpath_variable_set::set(const char_t* name, const char_t* value)
	{
		xpath_variable* var = add(name, xpath_type_string);
		return var ? var->set(value) : false;
	}

	bool xpath_variable_set::set(const char_t* name, const xpath_node_set& value)
	{
		xpath_variable* var = add(name, xpath_type_node_set);
		return var ? var->set(value) : false;
	}

	xpath_variable* xpath_variable_set::get(const char_t* name)
	{
		return find(name);
	}

	const xpath_variable* xpath_variable_set::get(const char_t* name) const
	{
		return find(name);
	}

	xpath_query::xpath_query(const char_t* query, xpath_variable_set* variables): _impl(0)
	{
		xpath_query_impl* impl = xpath_query_impl::create();

		if (!impl)
		{
		#ifdef PUGIXML_NO_EXCEPTIONS
			_result.error = "Out of memory";
        #else
			throw std::bad_alloc();
		#endif
		}
		else
		{
			buffer_holder impl_holder(impl, xpath_query_impl::destroy);

			impl->root = xpath_parser::parse(query, variables, &impl->alloc, &_result);

			if (impl->root)
			{
                _impl = static_cast<xpath_query_impl*>(impl_holder.release());
				_result.error = 0;
			}
		}
	}

	xpath_query::~xpath_query()
	{
		xpath_query_impl::destroy(_impl);
	}

	xpath_value_type xpath_query::return_type() const
	{
		if (!_impl) return xpath_type_none;

		return static_cast<xpath_query_impl*>(_impl)->root->rettype();
	}

	bool xpath_query::evaluate_boolean(const xpath_node& n) const
	{
		if (!_impl) return false;
		
		xpath_context c(n, 1, 1);
		xpath_stack_data sd;

	#ifdef PUGIXML_NO_EXCEPTIONS
		if (setjmp(sd.error_handler)) return false;
	#endif
		
		return static_cast<xpath_query_impl*>(_impl)->root->eval_boolean(c, sd.stack);
	}
	
	double xpath_query::evaluate_number(const xpath_node& n) const
	{
		if (!_impl) return gen_nan();
		
		xpath_context c(n, 1, 1);
		xpath_stack_data sd;

	#ifdef PUGIXML_NO_EXCEPTIONS
		if (setjmp(sd.error_handler)) return gen_nan();
	#endif

		return static_cast<xpath_query_impl*>(_impl)->root->eval_number(c, sd.stack);
	}

#ifndef PUGIXML_NO_STL
	string_t xpath_query::evaluate_string(const xpath_node& n) const
	{
		xpath_stack_data sd;

		return evaluate_string_impl(static_cast<xpath_query_impl*>(_impl), n, sd).c_str();
	}
#endif

	size_t xpath_query::evaluate_string(char_t* buffer, size_t capacity, const xpath_node& n) const
	{
		xpath_stack_data sd;

		xpath_string r = evaluate_string_impl(static_cast<xpath_query_impl*>(_impl), n, sd);

		size_t full_size = r.length() + 1;
		
		if (capacity > 0)
        {
            size_t size = (full_size < capacity) ? full_size : capacity;
            assert(size > 0);

            memcpy(buffer, r.c_str(), (size - 1) * sizeof(char_t));
            buffer[size - 1] = 0;
        }
		
		return full_size;
	}

	xpath_node_set xpath_query::evaluate_node_set(const xpath_node& n) const
	{
		if (!_impl) return xpath_node_set();

        xpath_ast_node* root = static_cast<xpath_query_impl*>(_impl)->root;

		if (root->rettype() != xpath_type_node_set)
		{
		#ifdef PUGIXML_NO_EXCEPTIONS
			return xpath_node_set();
		#else
			xpath_parse_result result;
			result.error = "Expression does not evaluate to node set";

			throw xpath_exception(result);
		#endif
		}
		
		xpath_context c(n, 1, 1);
		xpath_stack_data sd;

	#ifdef PUGIXML_NO_EXCEPTIONS
		if (setjmp(sd.error_handler)) return xpath_node_set();
	#endif

		xpath_node_set_raw r = root->eval_node_set(c, sd.stack);

		return xpath_node_set(r.begin(), r.end(), r.type());
	}

	const xpath_parse_result& xpath_query::result() const
	{
		return _result;
	}

	xpath_query::operator xpath_query::unspecified_bool_type() const
	{
		return _impl ? &xpath_query::_impl : 0;
	}

	bool xpath_query::operator!() const
	{
		return !_impl;
	}

	xpath_node xml_node::select_single_node(const char_t* query, xpath_variable_set* variables) const
	{
		xpath_query q(query, variables);
		return select_single_node(q);
	}

	xpath_node xml_node::select_single_node(const xpath_query& query) const
	{
		xpath_node_set s = query.evaluate_node_set(*this);
		return s.empty() ? xpath_node() : s.first();
	}

	xpath_node_set xml_node::select_nodes(const char_t* query, xpath_variable_set* variables) const
	{
		xpath_query q(query, variables);
		return select_nodes(q);
	}

	xpath_node_set xml_node::select_nodes(const xpath_query& query) const
	{
		return query.evaluate_node_set(*this);
	}
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
