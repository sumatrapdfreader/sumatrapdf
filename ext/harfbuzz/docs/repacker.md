# Introduction

Several tables in the opentype format are formed internally by a graph of subtables. Parent node's
reference their children through the use of positive offsets, which are typically 16 bits wide.
Since offsets are always positive this forms a directed acyclic graph. For storage in the font file
the graph must be given a topological ordering and then the subtables packed in serial according to
that ordering. Since 16 bit offsets have a maximum value of 65,535 if the distance between a parent
subtable and a child is more then 65,535 bytes then it's not possible for the offset to encode that
edge.

For many fonts with complex layout rules (such as Arabic) it's not unusual for the tables containing
layout rules ([GSUB/GPOS](https://docs.microsoft.com/en-us/typography/opentype/spec/gsub)) to be
larger than 65kb. As a result these types of fonts are susceptible to offset overflows when
serializing to the binary font format.

Offset overflows can happen for a variety of reasons and require different strategies to resolve:
*  Simple overflows can often be resolved with a different topological ordering.
*  If a subtable has many parents this can result in the link from furthest parent(s)
   being at risk for overflows. In these cases it's possible to duplicate the shared subtable which
   allows it to be placed closer to it's parent.
*  If subtables exist which are themselves larger than 65kb it's not possible for any offsets to point
   past them. In these cases the subtable can usually be split into two smaller subtables to allow
   for more flexibility in the ordering.
*  In GSUB/GPOS overflows from Lookup subtables can be resolved by changing the Lookup to an extension
   lookup which uses a 32 bit offset instead of 16 bit offset.

In general there isn't a simple solution to produce an optimal topological ordering for a given graph.
Finding an ordering which doesn't overflow is a NP hard problem. Existing solutions use heuristics
which attempt a combination of the above strategies to attempt to find a non-overflowing configuration.

The harfbuzz subsetting library
[includes a repacking algorithm](https://github.com/harfbuzz/harfbuzz/blob/main/src/hb-repacker.hh)
which is used to resolve offset overflows that are present in the subsetted tables it produces. This
document provides a deep dive into how the harfbuzz repacking algorithm works.

Other implementations exist, such as in
[fontTools](https://github.com/fonttools/fonttools/blob/7af43123d49c188fcef4e540fa94796b3b44e858/Lib/fontTools/ttLib/tables/otBase.py#L72), however these are not covered in this document.

# Foundations

There's four key pieces to the harfbuzz approach:

*  Subtable Graph: a table's internal structure is abstracted out into a lightweight graph
   representation where each subtable is a node and each offset forms an edge. The nodes only need
   to know how many bytes the corresponding subtable occupies. This lightweight representation can
   be easily modified to test new ordering's and strategies as the repacking algorithm iterates.

*  [Topological sorting algorithm](https://en.wikipedia.org/wiki/Topological_sorting): an algorithm
   which given a graph gives a linear sorting of the nodes such that all offsets will be positive.

*  Overflow check: given a graph and a topological sorting it checks if there will be any overflows
   in any of the offsets. If there are overflows it returns a list of (parent, child) tuples that
   will overflow. Since the graph has information on the size of each subtable it's straightforward
   to calculate the final position of each subtable and then check if any offsets to it will
   overflow.

*  Content Aware Preprocessing: if the overflow resolver is aware of the format of the underlying
   tables (eg. GSUB, GPOS) then in some cases preprocessing can be done to increase the chance of
   successfully packing the graph. For example for GSUB and GPOS we can preprocess the graph and
   promote lookups to extension lookups (upgrades a 16 bit offset to 32 bits) or split large lookup
   subtables into two or more pieces.

*  Offset resolution strategies: given a particular occurrence of an overflow these strategies
   modify the graph to attempt to resolve the overflow.

# High Level Algorithm

```
def repack(graph):
  graph.topological_sort()

  if (graph.will_overflow())
    preprocess(graph)
    assign_spaces(graph)
    graph.topological_sort()

  while (overflows = graph.will_overflow()):
    for overflow in overflows:
      apply_offset_resolution_strategy (overflow, graph)
    graph.topological_sort()
```

The actual code for this processing loop can be found in the function hb_resolve_overflows () of
[hb-repacker.hh](https://github.com/harfbuzz/harfbuzz/blob/main/src/hb-repacker.hh).

# Topological Sorting Algorithms

The harfbuzz repacker uses two different algorithms for topological sorting:
*  [Kahn's Algorithm](https://en.wikipedia.org/wiki/Topological_sorting#Kahn's_algorithm)
*  Sorting by shortest distance

Kahn's algorithm is approximately twice as fast as the shortest distance sort so that is attempted
first (only on the first topological sort). If it fails to eliminate overflows then shortest distance
sort will be used for all subsequent topological sorting operations.

## Shortest Distance Sort

This algorithm orders the nodes based on total distance to each node. Nodes with a shorter distance
are ordered first.

The "weight" of an edge is the sum of the size of the sub-table being pointed to plus 2^16 for a 16 bit
offset and 2^32 for a 32 bit offset.

The distance of a node is the sum of all weights along the shortest path from the root to that node
plus a priority modifier (used to change where nodes are placed by moving increasing or
decreasing the effective distance). Ties between nodes with the same distance are broken based
on the order of the offset in the sub table bytes.

The shortest distance to each node is determined using
[Djikstra's algorithm](https://en.wikipedia.org/wiki/Dijkstra%27s_algorithm). Then the topological
ordering is produce by applying a modified version of Kahn's algorithm that uses a priority queue
based on the shortest distance to each node.

## Optimizing the Sorting

The topological sorting operation is the core of the repacker and is run on each iteration so it needs
to be as fast as possible. There's a few things that are done to speed up subsequent sorting
operations:

*  The number of incoming edges to each node is cached. This is required by the Kahn's algorithm
   portion of both sorts. Where possible when the graph is modified we manually update the cached
   edge counts of affected nodes.

*  The distance to each node is cached. Where possible when the graph is modified we manually update
   the cached distances of any affected nodes.

Caching these values allows the repacker to avoid recalculating them for the full graph on each
iteration.

The other important factor to speed is a fast priority queue which is a core datastructure to
the topological sorting algorithm. Currently a basic heap based queue is used. Heap based queue's
don't support fast priority decreases, but that can be worked around by just adding redundant entries
to the priority queue and filtering the older ones out when poppping off entries. This is based
on the recommendations in
[a study of the practical performance of priority queues in Dijkstra's algorithm](https://www3.cs.stonybrook.edu/~rezaul/papers/TR-07-54.pdf)

## Special Handling of 32 bit Offsets

If a graph contains multiple 32 bit offsets then the shortest distance sorting will be likely be
suboptimal. For example consider the case where a graph contains two 32 bit offsets that each point
to a subgraph which are not connected to each other. The shortest distance sort will interleave the
subtables of the two subgraphs, potentially resulting in overflows. Since each of these subgraphs are
independent of each other, and 32 bit offsets can point extremely long distances a better strategy is
to pack the first subgraph in it's entirety and then have the second subgraph packed after with the 32
bit offset pointing over the first subgraph. For example given the graph:


```
a--- b -- d -- f
 \
  \_ c -- e -- g
```

Where the links from a to b and a to c are 32 bit offsets, the shortest distance sort would be:

```
a, b, c, d, e, f, g

```

If nodes d and e have a combined size greater than 65kb then the offset from d to f will overflow.
A better ordering is:

```
a, b, d, f, c, e, g
```

The ability for 32 bit offsets to point long distances is utilized to jump over the subgraph of
b which gives the remaining 16 bit offsets a better chance of not overflowing.

The above is an ideal situation where the subgraphs are disconnected from each other, in practice
this is often not this case. So this idea can be generalized as follows:

If there is a subgraph that is only reachable from one or more 32 bit offsets, then:
*  That subgraph can be treated as an independent unit and all nodes of the subgraph packed in isolation
   from the rest of the graph.
*  In a table that occupies less than 4gb of space (in practice all fonts), that packed independent
   subgraph can be placed anywhere after the parent nodes without overflowing the 32 bit offsets from
   the parent nodes.

The sorting algorithm incorporates this via a "space" modifier that can be applied to nodes in the
graph. By default all nodes are treated as being in space zero. If a node is given a non-zero space, n,
then the computed distance to the node will be modified by adding `n * 2^32`. This will cause that
node and it's descendants to be packed between all nodes in space n-1 and space n+1. Resulting in a
topological sort like:

```
| space 0 subtables | space 1 subtables | .... | space n subtables |
```

The assign_spaces() step in the high level algorithm is responsible for identifying independent
subgraphs and assigning unique spaces to each one. More information on the space assignment can be
found in the next section.

# Graph Preprocessing

For certain table types we can preprocess and modify the graph structure to reduce the occurences
of overflows. Currently the repacker implements preprocessing only for GPOS and GSUB tables.

## GSUB/GPOS Table Splitting

The GSUB/GPOS preprocessor scans each lookup subtable and determines if the subtable's children are
so large that no overflow resolution is possible (for example a single subtable that exceeds 65kb
cannot be pointed over). When such cases are detected table splitting is invoked:

* The subtable is first analyzed to determine the smallest number of split points that will allow
  for successful offset overflow resolution.

* Then the subtable in the graph representation is modified to actually perform the split at the
  previously computed split points. At a high level splits are done by inserting new subtables
  which contain a subset of the data of the original subtable and then shrinking the original subtable.

Table splitting must be aware of the underlying format of each subtable type and thus needs custom
code for each subtable type. Currently subtable splitting is only supported for GPOS subtable types.

## GSUB/GPOS Extension Lookup Promotion

In GSUB/GPOS tables lookups can be regular lookups which use 16 bit offsets to the children subtables
or extension lookups which use 32 bit offsets to the children subtables. If the sub graph of all
regular lookups is too large then it can be difficult to find an overflow free configuration. This
can be remedied by promoting one or more regular lookups to extension lookups.

During preprocessing the graph is scanned to determine the size of the subgraph of regular lookups.
If the graph is found to be too big then the analysis finds a set of lookups to promote to reduce
the subgraph size. Lastly the graph is modified to convert those lookups to extension lookups.

# Offset Resolution Strategies

## Space Assignment

The goal of space assignment is to find connected subgraphs that are only reachable via 32 bit offsets
and then assign each such subgraph to a unique non-zero space. The algorithm is roughly:

1.  Collect the set, `S`, of nodes that are children of 32 bit offsets.

2.  Do a directed traversal from each node in `S` and collect all encountered nodes into set `T`.
    Mark all nodes in the graph that are not in `T` as being in space 0.

3.  Set `next_space = 1`.

4.  While set `S` is not empty:

    a.  Pick a node `n` in set `S` then perform an undirected graph traversal and find the set `Q` of
        nodes that are reachable from `n`.

    b.  During traversal if a node, `m`, has a edge to a node in space 0 then `m` must be duplicated
        to disconnect it from space 0.

    d.  Remove all nodes in `Q` from `S` and assign all nodes in `Q` to `next_space`.


    c.  Increment `next_space` by one.


## Manual Iterative Resolutions

For each overflow in each iteration the algorithm will attempt to apply offset overflow resolution
strategies to eliminate the overflow. The type of strategy applied is dependent on the characteristics
of the overflowing link:

*  If the overflowing offset is inside a space other than space 0 and the subgraph space has more
   than one 32 bit offset pointing into the subgraph then subdivide the space by moving subgraph
   from one of the 32 bit offsets into a new space via the duplication of shared nodes.

*  If the overflowing offset is pointing to a subtable with more than one incoming edge: duplicate
   the node so that the overflowing offset is pointing at it's own copy of that node.

*  Otherwise, attempt to move the child subtable closer to it's parent. This is accomplished by
   raising the priority of all children of the parent. Next time the topological sort is run the
   children will be ordered closer to the parent.

# Test Cases

The harfbuzz repacker has tests defined using generic graphs: https://github.com/harfbuzz/harfbuzz/blob/main/src/test-repacker.cc

# Future Improvements

Currently for GPOS tables the repacker implementation is sufficient to handle both subsetting and the
general case of font compilation repacking. However for GSUB the repacker is only sufficient for
subsetting related overflows. To enable general case repacking of GSUB, support for splitting of
GSUB subtables will need to be added. Other table types such as COLRv1 shouldn't require table
splitting due to the wide use of 24 bit offsets throughout the table.

Beyond subtable splitting there are a couple of "nice to have" improvements, but these are not required
to support the general case:

*  Extension demotion: currently extension promotion is supported but in some cases if the non-extension
   subgraph is underfilled then packed size can be reduced by demoting extension lookups back to regular
   lookups.

*  Currently only children nodes are moved to resolve offsets. However, in many cases moving a parent
   node closer to it's children will have less impact on the size of other offsets. Thus the algorithm
   should use a heuristic (based on parent and child subtable sizes) to decide if the children's
   priority should be increased or the parent's priority decreased.
