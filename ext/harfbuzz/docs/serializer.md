# Introduction

In hb-subset serialization is the process of writing the subsetted font
tables out to actual bytes in the final format. All serialization works
through an object called the serialize context
([hb_serialize_context_t](https://github.com/harfbuzz/harfbuzz/blob/main/src/hb-serialize.hh)).

Internally the serialize context holds a fixed size memory buffer. For simple
tables the final bytes are written into the buffer sequentially to produce
the final serialized bytes.

## Simple Tables

Simple tables are tables that do not use offset graphs.

To write a struct into the serialization context, first you call an
allocation method on the context which requests a writable array of bytes of
a fixed size. If the requested array will not exceed the bounds of the fixed
buffer the serializer will return a pointer to the next unwritten portion
of the buffer. Then the struct is cast onto the returned pointer and values
are written to the structs fields.

Internally the serialization context ends up looking like:

```
+-------+-------+-----+-------+--------------+
| Obj 1 | Obj 2 | ... | Obj N | Unused Space |
+-------+-------+-----+-------+--------------+
```

Here Obj N, is the object currently being written.

## Complex Tables

Complex tables are made up of graphs of objects, where offset's are used
to form the edges of the graphs. Each object is a continuous slice of bytes
that contains zero or more offsets pointing to more objects.

In this case the serialization buffer has a different layout:

```
|- in progress objects -|              |--- packed objects --|
+-----------+-----------+--------------+-------+-----+-------+
|  Obj n+2  |  Obj n+1  | Unused Space | Obj n | ... | Obj 0 |
+-----------+-----------+--------------+-------+-----+-------+
|----------------------->              <---------------------|
```

The buffer holds two stacks:

1. In progress objects are held in a stack starting from the start of buffer
   that grows towards the end of the buffer.

2. Packed objects are held in a stack that starts at the end of the buffer
   and grows towards the start of the buffer.

Once the object on the top of the in progress stack is finished being written
its bytes are popped from the in progress stack and copied to the top of
the packed objects stack. In the example above, finalizing Obj n+1
would result in the following state:

```
+---------+--------------+---------+-------+-----+-------+
| Obj n+2 | Unused Space | Obj n+1 | Obj n | ... | Obj 0 |
+---------+--------------+---------+-------+-----+-------+
```

Each packed object is associated with an ID, it's zero based position in the packed
objects stack. In this example Obj 0, would have an ID of 0.

During serialization offsets that link from one object to another are stored
using object ids. The serialize context maintains a list of links between
objects. Each link records the parent object id, the child object id, the position
of the offset field within the parent object, and the width of the offset.

Links are always added to the current in progress object and you can only link too
objects that have been packed and thus have an ID.

### Object De-duplication

An important optimization in packing offset graphs is de-duplicating equivalent objects. If you
have two or more parent objects that point to child objects that are equivalent then you only need
to encode the child once and can have the parents point to the same child. This can significantly
reduce the final size of a serialized graph.

During packing of an inprogress object the serialization context checks if any existing packed
objects are equivalent to the object being packed. Here equivalence means the object has the
exact same bytes and all of it's links are equivalent. If an equivalent object is found the
in progress object is discarded and not copied to the packed object stack. The object id of
the equivalent object is instead returned. Thus parent objects will then link to the existing
equivalent object.

To find equivalent objects the serialization context maintains a hashmap from object to the canonical
object id.

### Link Resolution

Once all objects have been packed the next step is to assign actual values to all of the offset
fields. Prior to this point all links in the graph have been recorded using object id's. For each
link the resolver computes the offset between the parent and child and writes the offset into
the serialization buffer at the appropriate location.

### Offset Overflow Resolution

If during link resolution the resolver finds that an offsets value would exceed what can be encoded
in that offset field link resolution is aborted and the offset overflow resolver is invoked.
That process is documented [here](reapcker.md).


### Example of Complex Serialization


If we wanted to serialize the following graph:

```
a--b--d
 \   /
   c
```

Serializer would be called like this:

```c++
hb_serialize_context_t ctx;

struct root {
  char name;
  Offset16To<child> child_1;
  Offset16To<child> child_2;
}

struct child {
  char name;
  Offset16To<char> leaf;
}

// Object A.
ctx->push();
root* a = ctx->start_embed<root> ();
ctx->extend_min (a);
a->name = 'a';

// Object B.
ctx->push();
child* b = ctx->start_embed<child> ();
ctx->extend_min (b);
b->name = 'b';

// Object D.
ctx->push();
*ctx->allocate_size<char> (1) = 'd';
unsigned d_id = ctx->pop_pack ();

ctx->add_link (b->leaf, d_id);
unsigned b_id = ctx->pop_pack ();

// Object C
ctx->push();
child* c = ctx->start_embed<child> ();
ctx->extend_min (c);
c->name = 'c';

// Object D.
ctx->push();
*ctx->allocate_size<char> (1) = 'd';
d_id = ctx->pop_pack (); // Serializer will automatically de-dup this with the previous 'd'

ctx->add_link (c->leaf, d_id);
unsigned c_id = ctx->pop_pack ();

// Object A's links:
ctx->add_link (a->child_1, b_id);
ctx->add_link (a->child_2, c_id);
ctx->pop_pack ();

ctx->end_serialize ();

```
