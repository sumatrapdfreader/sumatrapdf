#include "BaseUtil.h"
#include "SerializeTxtParser.h"

/*
This is a parser for a tree-like text format:

foo [
  key: val
  k2 [
    another val
    and another
  ]
]

On purpose it doesn't try to always break things into key/values, just
to decode tree structure.
*/
bool ParseTxt(TxtParser& parser)
{

    return false;
}
