/*====================================================================*
 -  Copyright (C) 2001 Leptonica.  All rights reserved.
 -
 -  Redistribution and use in source and binary forms, with or without
 -  modification, are permitted provided that the following conditions
 -  are met:
 -  1. Redistributions of source code must retain the above copyright
 -     notice, this list of conditions and the following disclaimer.
 -  2. Redistributions in binary form must reproduce the above
 -     copyright notice, this list of conditions and the following
 -     disclaimer in the documentation and/or other materials
 -     provided with the distribution.
 -
 -  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 -  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 -  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 -  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ANY
 -  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 -  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 -  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 -  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 -  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 -  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 -  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *====================================================================*/

/*
 * Modified from the excellent code here:
 *     http://en.literateprograms.org/Red-black_tree_(C)?oldid=19567
 * which has been placed in the public domain under the Creative Commons
 * CC0 1.0 waiver (http://creativecommons.org/publicdomain/zero/1.0/).
 */

/*!
 * \file rbtree.c
 * <pre>
 *
 *  Basic functions for using red-black trees.  These are "nearly" balanced
 *  sorted trees with ordering by key that allows insertion, lookup and
 *  deletion of key/value pairs in log(n) time.
 *
 *  We use red-black trees to implement our version of:
 *    * a map: a function that maps keys to values (e.g., int64 --> int64).
 *    * a set: a collection that is sorted by unique keys (without
 *      associated values)
 *
 *  There are 5 invariant properties of RB trees:
 *  (1) Each node is either red or black.
 *  (2) The root node is black.
 *  (3) All leaves are black and contain no data (null).
 *  (4) Every red node has two children and both are black.  This is
 *      equivalent to requiring the parent of every red node to be black.
 *  (5) All paths from any given node to its leaf nodes contain the
 *      same number of black nodes.
 *
 *  Interface to red-black tree
 *           L_RBTREE       *l_rbtreeCreate()
 *           RB_TYPE        *l_rbtreeLookup()
 *           void            l_rbtreeInsert()
 *           void            l_rbtreeDelete()
 *           void            l_rbtreeDestroy()
 *           L_RBTREE_NODE  *l_rbtreeGetFirst()
 *           L_RBTREE_NODE  *l_rbtreeGetNext()
 *           L_RBTREE_NODE  *l_rbtreeGetLast()
 *           L_RBTREE_NODE  *l_rbtreeGetPrev()
 *           l_int32         l_rbtreeGetCount()
 *           void            l_rbtreePrint()
 *
 *  General comparison function
 *           static l_int32  compareKeys()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

    /* The node color enum is only needed in the rbtree implementation */
enum {
    L_RED_NODE = 1,
    L_BLACK_NODE = 2
};

    /* This makes it simpler to read the code */
typedef L_RBTREE_NODE node;

    /* Lots of static helper functions */
static void destroy_helper(node *n);
static void count_helper(node *n, l_int32 *pcount);
static void print_tree_helper(FILE *fp, node *n, l_int32 keytype,
                              l_int32 indent);

static l_int32 compareKeys(l_int32 keytype, RB_TYPE left, RB_TYPE right);

static node *grandparent(node *n);
static node *sibling(node *n);
static node *uncle(node *n);
static l_int32 node_color(node *n);
static node *new_node(RB_TYPE key, RB_TYPE value, l_int32 node_color,
                      node *left, node *right);
static node *lookup_node(L_RBTREE *t, RB_TYPE key);
static void rotate_left(L_RBTREE *t, node *n);
static void rotate_right(L_RBTREE *t, node *n);
static void replace_node(L_RBTREE *t, node *oldn, node *newn);
static void insert_case1(L_RBTREE *t, node *n);
static void insert_case2(L_RBTREE *t, node *n);
static void insert_case3(L_RBTREE *t, node *n);
static void insert_case4(L_RBTREE *t, node *n);
static void insert_case5(L_RBTREE *t, node *n);
static node *maximum_node(node *root);
static void delete_case1(L_RBTREE *t, node *n);
static void delete_case2(L_RBTREE *t, node *n);
static void delete_case3(L_RBTREE *t, node *n);
static void delete_case4(L_RBTREE *t, node *n);
static void delete_case5(L_RBTREE *t, node *n);
static void delete_case6(L_RBTREE *t, node *n);
static void verify_properties(L_RBTREE *t);

#ifndef  NO_CONSOLE_IO
#define  VERIFY_RBTREE     0   /* only for debugging */
#endif  /* ~NO_CONSOLE_IO */

/* ------------------------------------------------------------- *
 *                   Interface to Red-black Tree                 *
 * ------------------------------------------------------------- */
/*!
 * \brief   l_rbtreeCreate()
 *
 * \param[in]   keytype   defined by an enum for an RB_TYPE union
 * \return      rbtree    container with empty ptr to the root
 */
L_RBTREE *
l_rbtreeCreate(l_int32  keytype)
{
L_RBTREE  *t;

    if (keytype != L_INT_TYPE && keytype != L_UINT_TYPE &&
        keytype != L_FLOAT_TYPE && keytype)
        return (L_RBTREE *)ERROR_PTR("invalid keytype", __func__, NULL);

    t = (L_RBTREE *)LEPT_CALLOC(1, sizeof(L_RBTREE));
    t->keytype = keytype;
    verify_properties(t);
    return t;
}

/*!
 * \brief   l_rbtreeLookup()
 *
 * \param[in]   t        rbtree, including root node
 * \param[in]   key      find a node with this key
 * \return    &value     a pointer to a union, if the node exists; else NULL
 */
RB_TYPE *
l_rbtreeLookup(L_RBTREE  *t,
               RB_TYPE    key)
{
node  *n;

    if (!t)
        return (RB_TYPE *)ERROR_PTR("tree is null\n", __func__, NULL);

    n = lookup_node(t, key);
    return n == NULL ? NULL : &n->value;
}

/*!
 * \brief   l_rbtreeInsert()
 *
 * \param[in]   t         rbtree, including root node
 * \param[in]   key       insert a node with this key, if the key does not
 *                        already exist in the tree
 * \param[in]   value     typically an int, used for an index
 * \return     void
 *
 * <pre>
 * Notes:
 *      (1) If a node with the key already exists, this just updates the value.
 * </pre>
 */
void
l_rbtreeInsert(L_RBTREE     *t,
               RB_TYPE       key,
               RB_TYPE       value)
{
node  *n, *inserted_node;

    if (!t) {
        L_ERROR("tree is null\n", __func__);
        return;
    }

    inserted_node = new_node(key, value, L_RED_NODE, NULL, NULL);
    if (t->root == NULL) {
        t->root = inserted_node;
    } else {
        n = t->root;
        while (1) {
            int comp_result = compareKeys(t->keytype, key, n->key);
            if (comp_result == 0) {
                n->value = value;
                LEPT_FREE(inserted_node);
                return;
            } else if (comp_result < 0) {
                if (n->left == NULL) {
                    n->left = inserted_node;
                    break;
                } else {
                    n = n->left;
                }
            } else {  /* comp_result > 0 */
                if (n->right == NULL) {
                    n->right = inserted_node;
                    break;
                } else {
                    n = n->right;
                }
            }
        }
        inserted_node->parent = n;
    }
    insert_case1(t, inserted_node);
    verify_properties(t);
}

/*!
 * \brief   l_rbtreeDelete()
 *
 * \param[in]   t     rbtree, including root node
 * \param[in]   key   delete the node with this key
 * \return      void
 */
void
l_rbtreeDelete(L_RBTREE  *t,
               RB_TYPE    key)
{
node  *n, *child;

    if (!t) {
        L_ERROR("tree is null\n", __func__);
        return;
    }

    n = lookup_node(t, key);
    if (n == NULL) return;  /* Key not found, do nothing */
    if (n->left != NULL && n->right != NULL) {
            /* Copy key/value from predecessor and then delete it instead */
        node *pred = maximum_node(n->left);
        n->key   = pred->key;
        n->value = pred->value;
        n = pred;
    }

        /* n->left == NULL || n->right == NULL */
    child = n->right == NULL ? n->left  : n->right;
    if (node_color(n) == L_BLACK_NODE) {
        n->color = node_color(child);
        delete_case1(t, n);
    }
    replace_node(t, n, child);
    if (n->parent == NULL && child != NULL)  /* root should be black */
        child->color = L_BLACK_NODE;
    LEPT_FREE(n);

    verify_properties(t);
}

/*!
 * \brief   l_rbtreeDestroy()
 *
 * \param[in]   pt     pointer to tree; will be wet to null before returning
 * \return      void
 *
 * <pre>
 * Notes:
 *      (1) Destroys the tree and nulls the input tree ptr.
 * </pre>
 */
void
l_rbtreeDestroy(L_RBTREE  **pt)
{
node    *n;

    if (!pt) return;
    if (*pt == NULL) return;
    n = (*pt)->root;
    destroy_helper(n);
    LEPT_FREE(*pt);
    *pt = NULL;
}

    /* postorder DFS */
static void
destroy_helper(node  *n)
{
    if (!n) return;
    destroy_helper(n->left);
    destroy_helper(n->right);
    LEPT_FREE(n);
}

/*!
 * \brief   l_rbtreeGetFirst()
 *
 * \param[in]    t    rbtree, including root node
 * \return       first node, or NULL on error or if the tree is empty
 *
 * <pre>
 * Notes:
 *      (1) This is the first node in an in-order traversal.
 * </pre>
 */
L_RBTREE_NODE *
l_rbtreeGetFirst(L_RBTREE  *t)
{
node  *n;

    if (!t)
        return (L_RBTREE_NODE *)ERROR_PTR("tree is null", __func__, NULL);
    if (t->root == NULL) {
        L_INFO("tree is empty\n", __func__);
        return NULL;
    }

        /* Just go down the left side as far as possible */
    n = t->root;
    while (n && n->left)
        n = n->left;
    return n;
}

/*!
 * \brief   l_rbtreeGetNext()
 *
 * \param[in]    n     current node
 * \return       next node, or NULL if it's the last node
 *
 * <pre>
 * Notes:
 *      (1) This finds the next node, in an in-order traversal, from
 *          the current node.
 *      (2) It is useful as an iterator for a map.
 *      (3) Call l_rbtreeGetFirst() to get the first node.
 * </pre>
 */
L_RBTREE_NODE *
l_rbtreeGetNext(L_RBTREE_NODE  *n)
{
    if (!n)
        return (L_RBTREE_NODE *)ERROR_PTR("n not defined", __func__, NULL);

        /* If there is a right child, go to it, and then go left all the
         * way to the end.  Otherwise go up to the parent; continue upward
         * as long as you're on the right branch, but stop at the parent
         * when you hit it from the left branch. */
    if (n->right) {
        n = n->right;
        while (n->left)
            n = n->left;
        return n;
    } else {
        while (n->parent && n->parent->right == n)
            n = n->parent;
        return n->parent;
    }
}

/*!
 * \brief   l_rbtreeGetLast()
 *
 * \param[in]   t      rbtree, including root node
 * \return      last node, or NULL on error or if the tree is empty
 *
 * <pre>
 * Notes:
 *      (1) This is the last node in an in-order traversal.
 * </pre>
 */
L_RBTREE_NODE *
l_rbtreeGetLast(L_RBTREE  *t)
{
node  *n;

    if (!t)
        return (L_RBTREE_NODE *)ERROR_PTR("tree is null", __func__, NULL);
    if (t->root == NULL) {
        L_INFO("tree is empty\n", __func__);
        return NULL;
    }

        /* Just go down the right side as far as possible */
    n = t->root;
    while (n && n->right)
        n = n->right;
    return n;
}

/*!
 * \brief   l_rbtreeGetPrev()
 *
 * \param[in]    n     current node
 * \return       next node, or NULL if it's the first node
 *
 * <pre>
 * Notes:
 *      (1) This finds the previous node, in an in-order traversal, from
 *          the current node.
 *      (2) It is useful as an iterator for a map.
 *      (3) Call l_rbtreeGetLast() to get the last node.
 * </pre>
 */
L_RBTREE_NODE *
l_rbtreeGetPrev(L_RBTREE_NODE  *n)
{
    if (!n)
        return (L_RBTREE_NODE *)ERROR_PTR("n not defined", __func__, NULL);

        /* If there is a left child, go to it, and then go right all the
         * way to the end.  Otherwise go up to the parent; continue upward
         * as long as you're on the left branch, but stop at the parent
         * when you hit it from the right branch. */
    if (n->left) {
        n = n->left;
        while (n->right)
            n = n->right;
        return n;
    } else {
        while (n->parent && n->parent->left == n)
            n = n->parent;
        return n->parent;
    }
}

/*!
 * \brief   l_rbtreeGetCount()
 *
 * \param[in]  t      rbtree
 * \return     count  the number of nodes in the tree, or 0 on error
 */
l_int32
l_rbtreeGetCount(L_RBTREE  *t)
{
l_int32  count = 0;
node    *n;

    if (!t) return 0;
    n = t->root;
    count_helper(n, &count);
    return count;
}

    /* preorder DFS */
static void
count_helper(node  *n, l_int32  *pcount)
{
    if (n)
        (*pcount)++;
    else
        return;

    count_helper(n->left, pcount);
    count_helper(n->right, pcount);
}


/*!
 * \brief   l_rbtreePrint()
 *
 * \param[in]    fp    file stream
 * \param[in]    t     rbtree
 * \return       void
 */
void
l_rbtreePrint(FILE      *fp,
              L_RBTREE  *t)
{
    if (!fp) {
        L_ERROR("stream not defined\n", __func__);
        return;
    }
    if (!t) {
        L_ERROR("tree not defined\n", __func__);
        return;
    }

    print_tree_helper(fp, t->root, t->keytype, 0);
    fprintf(fp, "\n");
}

#define INDENT_STEP  4

static void
print_tree_helper(FILE    *fp,
                  node    *n,
                  l_int32  keytype,
                  l_int32  indent)
{
l_int32  i;

    if (n == NULL) {
        fprintf(fp, "<empty tree>");
        return;
    }
    if (n->right != NULL) {
        print_tree_helper(fp, n->right, keytype, indent + INDENT_STEP);
    }
    for (i = 0; i < indent; i++)
        fprintf(fp, " ");
    if (n->color == L_BLACK_NODE) {
        if (keytype == L_INT_TYPE)
            fprintf(fp, "%lld\n", n->key.itype);
        else if (keytype == L_UINT_TYPE)
            fprintf(fp, "%llx\n", n->key.utype);
        else if (keytype == L_FLOAT_TYPE)
            fprintf(fp, "%f\n", n->key.ftype);
    } else {
        if (keytype == L_INT_TYPE)
            fprintf(fp, "<%lld>\n", n->key.itype);
        else if (keytype == L_UINT_TYPE)
            fprintf(fp, "<%llx>\n", n->key.utype);
        else if (keytype == L_FLOAT_TYPE)
            fprintf(fp, "<%f>\n", n->key.ftype);
    }
    if (n->left != NULL) {
        print_tree_helper(fp, n->left, keytype, indent + INDENT_STEP);
    }
}


/* ------------------------------------------------------------- *
 *                Static key comparison function                 *
 * ------------------------------------------------------------- */
static l_int32
compareKeys(l_int32  keytype,
            RB_TYPE  left,
            RB_TYPE  right)
{
    if (keytype == L_INT_TYPE) {
        if (left.itype < right.itype)
            return -1;
        else if (left.itype > right.itype)
            return 1;
        else {  /* equality */
            return 0;
        }
    } else if (keytype == L_UINT_TYPE) {
        if (left.utype < right.utype)
            return -1;
        else if (left.utype > right.utype)
            return 1;
        else {  /* equality */
            return 0;
        }
    } else if (keytype == L_FLOAT_TYPE) {
        if (left.ftype < right.ftype)
            return -1;
        else if (left.ftype > right.ftype)
            return 1;
        else {  /* equality */
            return 0;
        }
    } else {
        L_ERROR("unknown keytype %d\n", __func__, keytype);
        return 0;
    }
}


/* ------------------------------------------------------------- *
 *                  Static red-black tree helpers                *
 * ------------------------------------------------------------- */
static node *grandparent(node *n) {
    if (!n || !n->parent || !n->parent->parent) {
        L_ERROR("root and child of root have no grandparent\n", "grandparent");
        return NULL;
    }
    return n->parent->parent;
}

static node *sibling(node *n) {
    if (!n || !n->parent) {
        L_ERROR("root has no sibling\n", "sibling");
        return NULL;
    }
    if (n == n->parent->left)
        return n->parent->right;
    else
        return n->parent->left;
}

static node *uncle(node *n) {
    if (!n || !n->parent || !n->parent->parent) {
        L_ERROR("root and child of root have no uncle\n", "uncle");
        return NULL;
    }
    return sibling(n->parent);
}

static l_int32 node_color(node *n) {
    return n == NULL ? L_BLACK_NODE : n->color;
}


static node *new_node(RB_TYPE key, RB_TYPE value, l_int32 node_color,
                      node *left, node *right) {
    node *result = (node *)LEPT_CALLOC(1, sizeof(node));
    result->key = key;
    result->value = value;
    result->color = node_color;
    result->left = left;
    result->right = right;
    if (left != NULL) left->parent = result;
    if (right != NULL) right->parent = result;
    result->parent = NULL;
    return result;
}

static node *lookup_node(L_RBTREE *t, RB_TYPE key) {
    node *n = t->root;
    while (n != NULL) {
        int comp_result = compareKeys(t->keytype, key, n->key);
        if (comp_result == 0) {
            return n;
        } else if (comp_result < 0) {
            n = n->left;
        } else {  /* comp_result > 0 */
            n = n->right;
        }
    }
    return n;
}

static void rotate_left(L_RBTREE *t, node *n) {
    node *r = n->right;
    replace_node(t, n, r);
    n->right = r->left;
    if (r->left != NULL) {
        r->left->parent = n;
    }
    r->left = n;
    n->parent = r;
}

static void rotate_right(L_RBTREE *t, node *n) {
    node *L = n->left;
    replace_node(t, n, L);
    n->left = L->right;
    if (L->right != NULL) {
        L->right->parent = n;
    }
    L->right = n;
    n->parent = L;
}

static void replace_node(L_RBTREE *t, node *oldn, node *newn) {
    if (oldn->parent == NULL) {
        t->root = newn;
    } else {
        if (oldn == oldn->parent->left)
            oldn->parent->left = newn;
        else
            oldn->parent->right = newn;
    }
    if (newn != NULL) {
        newn->parent = oldn->parent;
    }
}

static void insert_case1(L_RBTREE *t, node *n) {
    if (n->parent == NULL)
        n->color = L_BLACK_NODE;
    else
        insert_case2(t, n);
}

static void insert_case2(L_RBTREE *t, node *n) {
    if (node_color(n->parent) == L_BLACK_NODE)
        return; /* Tree is still valid */
    else
        insert_case3(t, n);
}

static void insert_case3(L_RBTREE *t, node *n) {
    if (node_color(uncle(n)) == L_RED_NODE) {
        n->parent->color = L_BLACK_NODE;
        uncle(n)->color = L_BLACK_NODE;
        grandparent(n)->color = L_RED_NODE;
        insert_case1(t, grandparent(n));
    } else {
        insert_case4(t, n);
    }
}

static void insert_case4(L_RBTREE *t, node *n) {
    if (n == n->parent->right && n->parent == grandparent(n)->left) {
        rotate_left(t, n->parent);
        n = n->left;
    } else if (n == n->parent->left && n->parent == grandparent(n)->right) {
        rotate_right(t, n->parent);
        n = n->right;
    }
    insert_case5(t, n);
}

static void insert_case5(L_RBTREE *t, node *n) {
    n->parent->color = L_BLACK_NODE;
    grandparent(n)->color = L_RED_NODE;
    if (n == n->parent->left && n->parent == grandparent(n)->left) {
        rotate_right(t, grandparent(n));
    } else if (n == n->parent->right && n->parent == grandparent(n)->right) {
        rotate_left(t, grandparent(n));
    } else {
        L_ERROR("identity confusion\n", "insert_case5");
    }
}

static node *maximum_node(node *n) {
    if (!n) {
        L_ERROR("n not defined\n", "maximum_node");
        return NULL;
    }
    while (n->right != NULL) {
        n = n->right;
    }
    return n;
}

static void delete_case1(L_RBTREE *t, node *n) {
    if (n->parent == NULL)
        return;
    else
        delete_case2(t, n);
}

static void delete_case2(L_RBTREE *t, node *n) {
    if (node_color(sibling(n)) == L_RED_NODE) {
        n->parent->color = L_RED_NODE;
        sibling(n)->color = L_BLACK_NODE;
        if (n == n->parent->left)
            rotate_left(t, n->parent);
        else
            rotate_right(t, n->parent);
    }
    delete_case3(t, n);
}

static void delete_case3(L_RBTREE *t, node *n) {
    if (node_color(n->parent) == L_BLACK_NODE &&
        node_color(sibling(n)) == L_BLACK_NODE &&
        node_color(sibling(n)->left) == L_BLACK_NODE &&
        node_color(sibling(n)->right) == L_BLACK_NODE) {
        sibling(n)->color = L_RED_NODE;
        delete_case1(t, n->parent);
    } else {
        delete_case4(t, n);
    }
}

static void delete_case4(L_RBTREE *t, node *n) {
    if (node_color(n->parent) == L_RED_NODE &&
        node_color(sibling(n)) == L_BLACK_NODE &&
        node_color(sibling(n)->left) == L_BLACK_NODE &&
        node_color(sibling(n)->right) == L_BLACK_NODE) {
        sibling(n)->color = L_RED_NODE;
        n->parent->color = L_BLACK_NODE;
    } else {
        delete_case5(t, n);
    }
}

static void delete_case5(L_RBTREE *t, node *n) {
    if (n == n->parent->left &&
        node_color(sibling(n)) == L_BLACK_NODE &&
        node_color(sibling(n)->left) == L_RED_NODE &&
        node_color(sibling(n)->right) == L_BLACK_NODE) {
        sibling(n)->color = L_RED_NODE;
        sibling(n)->left->color = L_BLACK_NODE;
        rotate_right(t, sibling(n));
    } else if (n == n->parent->right &&
               node_color(sibling(n)) == L_BLACK_NODE &&
               node_color(sibling(n)->right) == L_RED_NODE &&
               node_color(sibling(n)->left) == L_BLACK_NODE) {
        sibling(n)->color = L_RED_NODE;
        sibling(n)->right->color = L_BLACK_NODE;
        rotate_left(t, sibling(n));
    }
    delete_case6(t, n);
}

static void delete_case6(L_RBTREE *t, node *n) {
    sibling(n)->color = node_color(n->parent);
    n->parent->color = L_BLACK_NODE;
    if (n == n->parent->left) {
        if (node_color(sibling(n)->right) != L_RED_NODE) {
            L_ERROR("right sibling is not RED", "delete_case6");
            return;
        }
        sibling(n)->right->color = L_BLACK_NODE;
        rotate_left(t, n->parent);
    } else {
        if (node_color(sibling(n)->left) != L_RED_NODE) {
            L_ERROR("left sibling is not RED", "delete_case6");
            return;
        }
        sibling(n)->left->color = L_BLACK_NODE;
        rotate_right(t, n->parent);
    }
}


/* ------------------------------------------------------------- *
 *               Debugging: verify if tree is valid              *
 * ------------------------------------------------------------- */
#if VERIFY_RBTREE
static void verify_property_1(node *root);
static void verify_property_2(node *root);
static void verify_property_4(node *root);
static void verify_property_5(node *root);
static void verify_property_5_helper(node *n, int black_count,
                                     int* black_count_path);
#endif

static void verify_properties(L_RBTREE *t) {
#if VERIFY_RBTREE
    verify_property_1(t->root);
    verify_property_2(t->root);
    /* Property 3 is implicit */
    verify_property_4(t->root);
    verify_property_5(t->root);
#endif
}

#if VERIFY_RBTREE
static void verify_property_1(node *n) {
    if (node_color(n) != L_RED_NODE && node_color(n) != L_BLACK_NODE) {
        L_ERROR("color neither RED nor BLACK\n", "verify_property_1");
        return;
    }
    if (n == NULL) return;
    verify_property_1(n->left);
    verify_property_1(n->right);
}

static void verify_property_2(node *root) {
    if (node_color(root) != L_BLACK_NODE)
        L_ERROR("root is not black!\n", "verify_property_2");
}

static void verify_property_4(node *n) {
    if (node_color(n) == L_RED_NODE) {
        if (node_color(n->left) != L_BLACK_NODE ||
            node_color(n->right) != L_BLACK_NODE ||
            node_color(n->parent) != L_BLACK_NODE) {
            L_ERROR("children & parent not all BLACK", "verify_property_4");
            return;
        }
    }
    if (n == NULL) return;
    verify_property_4(n->left);
    verify_property_4(n->right);
}

static void verify_property_5(node *root) {
    int black_count_path = -1;
    verify_property_5_helper(root, 0, &black_count_path);
}

static void verify_property_5_helper(node *n, int black_count,
                                     int* path_black_count) {
    if (node_color(n) == L_BLACK_NODE) {
        black_count++;
    }
    if (n == NULL) {
        if (*path_black_count == -1) {
            *path_black_count = black_count;
        } else if (*path_black_count != black_count) {
            L_ERROR("incorrect black count", "verify_property_5_helper");
        }
        return;
    }
    verify_property_5_helper(n->left,  black_count, path_black_count);
    verify_property_5_helper(n->right, black_count, path_black_count);
}
#endif
