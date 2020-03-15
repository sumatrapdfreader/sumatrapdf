#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "xmlparse.h"

void indent(int depth) { while (depth--) printf("  "); }

void dump_element(int depth, wdl_xml_element *elem)
{
  if (!elem) return;
  indent(depth); printf("element: %s (line %d col %d)\n",elem->name,elem->line,elem->col);
  if (elem->attributes.GetSize())
  {
    indent(depth+1);  printf("attributes:\n");
    int x;
    for(x=0;x<elem->attributes.GetSize();x++)
    {
      const char *key=NULL;
      const char *value=elem->attributes.Enumerate(x,(char **)&key);
      indent(depth+2); printf("%s=%s\n",key,value);
    }
  }
  if (elem->value.GetLength())
  {
    indent(depth+1); printf("value: %s\n",elem->value.Get());
  }
  if (elem->elements.GetSize())
  {
    indent(depth+1);  printf("elements:\n");
    int x;
    for(x=0;x<elem->elements.GetSize();x++)
    {
      dump_element(depth+2,elem->elements.Get(x));
    }
  }
}

int main(int argc, const char **argv)
{
  if (argc != 2)
  {
    printf("Usage: xmlparse_test filename.xml\n");
    return 1;
  }
  FILE *fp = fopen(argv[1],"r");
  wdl_xml_fileread fr(fp);
  const char *err = fr.parse();
  if (err)
  {
    printf("parse error line %d, col %d: %s\n", fr.getLine(),fr.getCol(), err);
    return 1;
  }
  printf("doctype: %s\n",fr.element_doctype_tokens.Get(0));
  dump_element(0, fr.element_xml);
  dump_element(0, fr.element_root);
  return 0;
}
