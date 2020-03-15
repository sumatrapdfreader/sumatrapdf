#include "libxml_tinyxml.h"
#include "tinyxml.h"

xmlAttr* MakeAttrList(const TiXmlAttribute* attr)
{
  xmlAttr* xattr = 0;
  if (attr)
  {
    xattr = new xmlAttr;
    memset(xattr, 0, sizeof(xmlAttr));

    xattr->name = attr->Name();
    xattr->content = attr->Value();
    
    // svgtiny expect xattr->children to be a pointer back to the attribute,
    // though in libxml it is supposed to always be NULL
    xattr->children = xattr;
    
    xattr->next = MakeAttrList(attr->Next());
  }
  return xattr;
}

xmlNode* MakeNodeTree(TiXmlNode* node)
{
  xmlNode* xnode = 0;
  if (node)
  {
    xnode = new xmlNode;
    memset(xnode, 0, sizeof(xmlNode));

    int type = node->Type();
    if (type == TiXmlNode::ELEMENT) 
    {
      xnode->type = XML_ELEMENT_NODE;
      xnode->name = node->Value();
      const TiXmlElement* elem = node->ToElement();
      if (elem) xnode->properties = MakeAttrList(elem->FirstAttribute());     
    }
    else if (type == TiXmlNode::TEXT) 
    {
      xnode->type = XML_TEXT_NODE;
      xnode->content = node->Value();
    }
    else xnode->type = 0;

    xnode->children = MakeNodeTree(node->FirstChild());
    xnode->next = MakeNodeTree(node->NextSibling());
  }
  return xnode;
}


xmlDoc* xmlReadMemory(const char* buffer, size_t size, const char* url, const char* encoding, int flags)
{
  if (buffer[size] != 0 || strlen(buffer) != size) return 0;

  if (!url) url = "";
  TiXmlDocument* doc = new TiXmlDocument(url);
  doc->Parse(buffer);
  if (doc->Error())
  {
    delete(doc);
    doc = 0;
  }

  xmlDoc* xdoc = 0;
  if (doc)
  {
    xdoc = new xmlDoc;
    memset(xdoc, 0, sizeof(xmlDoc));
    xdoc->tinyxml_obj = doc;
    xdoc->children = MakeNodeTree((TiXmlNode*) doc->RootElement());
  }
  return xdoc;
}

xmlNode* xmlDocGetRootElement(xmlDoc* xdoc)
{
  xmlNode* xnode = 0;
  if (xdoc) xnode = xdoc->children;
  return xnode;
}

xmlAttr* xmlHasProp(xmlNode* xnode, const xmlChar* name)
{
  xmlAttr* xattr = 0;
  if (xnode && name && name[0])
  {
    xattr = xnode->properties;
#ifdef _WIN32
    while (xattr && (!xattr->name || stricmp(xattr->name, name))) xattr = xattr->next;
#else
    while (xattr && (!xattr->name || strcasecmp(xattr->name, name))) xattr = xattr->next;
#endif
  }
  return xattr;
}

xmlChar* xmlGetProp(xmlNode* xnode, const xmlChar* name)
{
  xmlChar* xcontent = 0;
  xmlAttr* xattr = xmlHasProp(xnode, name);
  if (xattr) xcontent = strdup(xattr->content);
  return xcontent;
}

void xmlFree(xmlChar* str)
{
  free(str);
}

void xmlFreeDoc(xmlDoc* xdoc)
{
  if (xdoc)
  {
    delete((TiXmlDocument*)xdoc->tinyxml_obj);
    xdoc->tinyxml_obj = 0;
    // todo make sure tinyxmldoc owns all its nodes
    xdoc->children = 0;
  }
}


char* strndup(const char* s, size_t size)
{
  // no doubt the real strndup protects against s not being null-terminated
  if (size < strlen(s)) size = strlen(s);
  char* t = (char*) malloc(size+1);
  strncpy(t, s, size);  // strncpy should be ok since we checked size
  return t;
}