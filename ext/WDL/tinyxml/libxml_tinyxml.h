#ifndef LIBXML_TINYXML
#define LIBXML_TINYXML

// libxml interface, tinyxml implementation

#include <stdlib.h>

enum { XML_PARSE_NONET=1, XML_PARSE_COMPACT=2 };
enum { XML_ELEMENT_NODE=1, XML_TEXT_NODE=2 };

#ifdef __cplusplus
extern "C" {
#endif 

typedef struct _xmlNode
{
  void* tinyxml_obj;
  unsigned short line;
  int type;
  const char* name; 
  const char* content; 
  struct _xmlNode* properties; 
  struct _xmlNode* children;
  struct _xmlNode* next;
} xmlNode;

typedef xmlNode xmlDoc;
typedef xmlNode xmlAttr;

typedef char xmlChar;


xmlDoc* xmlReadMemory(const char* buffer, size_t size, const char* url, const char* encoding, int flags);

xmlNode* xmlDocGetRootElement(xmlDoc* document);

xmlAttr* xmlHasProp(xmlNode* node, const xmlChar* name);

xmlChar* xmlGetProp(xmlNode* node, const xmlChar * name); // return is on the heap

void xmlFree(xmlChar* str);

void xmlFreeDoc(xmlDoc* document);


char* strndup(const char* s, size_t size);  // argh

#ifdef __cplusplus
}
#endif 

#endif