# Data Structures

MuPDF has implementations of many generally useful data structures and algorithms
such as hash tables and binary trees.

## Hash Table

We have a generic hash table structure with fixed length keys.

The keys and values are not reference counted by the hash table. Callers are responsible for manually taking care of reference counting when inserting and removing values from the table, should that be desired.

`fz_hash_table *fz_new_hash_table(fz_context *ctx, int initial_size, int key_length, int lock, void (*drop_value)(fz_context *ctx, void *value));`
:	The lock parameter should be zero, any other value will result in
	unpredictable behavior. The `drop_value` callback function to the
	constructor is only used to release values when the hash table is
	destroyed.

`void fz_drop_hash_table(fz_context *ctx, fz_hash_table *table);`
:	Free the hash table and call the `drop_value` function on all the
	values in the table.

`void *fz_hash_find(fz_context *ctx, fz_hash_table *table, const void *key);`
:	Find the value associated with the key. Returns `NULL` if not found.

`void *fz_hash_insert(fz_context *ctx, fz_hash_table *table, const void *key, void *value);`
:	Insert the value into the hash table. Inserting a duplicate entry will
	**not** overwrite the old value, it will return the old value instead.
	Return `NULL` if the value was inserted for the first time. Does not
	reference count the value!

`void fz_hash_remove(fz_context *ctx, fz_hash_table *table, const void *key);`
:	Remove the associated value from the hash table. This will not
	reference count the value!

`void fz_hash_for_each(fz_context *ctx, fz_hash_table *table, void *state, void (*callback)(fz_context *ctx, void *state, void *key, int key_length, void *value);`
:	Iterate and call a function for each key-value pair in the table.

## Binary Tree

The `fz_tree` structure is a self-balancing binary tree that maps text strings to values.

`void *fz_tree_lookup(fz_context *ctx, fz_tree *node, const char *key);`
:	Look up an entry in the tree. Returns `NULL` if not found.

`fz_tree *fz_tree_insert(fz_context *ctx, fz_tree *root, const char *key, void *value);`
:	Insert a new entry into the tree. Do not insert duplicate entries.
	Returns the new root object.

`void fz_drop_tree(fz_context *ctx, fz_tree *node, void (*dropfunc)(fz_context *ctx, void *value));`
:	Free the tree and all the values in it.

There is no constructor for this structure, since there is no containing root
structure. Instead, the insert function returns the new root node. Use `NULL`
for the initial empty tree.

	fz_tree *tree = NULL;
	tree = fz_tree_insert(ctx, tree, "A", my_a_obj);
	tree = fz_tree_insert(ctx, tree, "B", my_b_obj);
	tree = fz_tree_insert(ctx, tree, "C", my_c_obj);
	assert(fz_tree_lookup(ctx, tree, "B") == my_b_obj);
