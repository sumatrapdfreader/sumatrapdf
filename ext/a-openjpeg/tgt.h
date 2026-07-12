#ifndef OPJ_TGT_H
#define OPJ_TGT_H

typedef struct opj_tgt_node {
    struct opj_tgt_node *parent;
    OPJ_INT32 value;
    OPJ_INT32 low;
    OPJ_UINT32 known;
} opj_tgt_node_t;

typedef struct opj_tgt_tree {
    OPJ_UINT32  numleafsh;
    OPJ_UINT32  numleafsv;
    OPJ_UINT32 numnodes;
    opj_tgt_node_t *nodes;
    OPJ_UINT32  nodes_size;
} opj_tgt_tree_t;

opj_tgt_tree_t *opj_tgt_create(OPJ_UINT32 numleafsh, OPJ_UINT32 numleafsv,
                               opj_event_mgr_t *p_manager);

opj_tgt_tree_t *opj_tgt_init(opj_tgt_tree_t * p_tree,
                             OPJ_UINT32  p_num_leafs_h,
                             OPJ_UINT32  p_num_leafs_v, opj_event_mgr_t *p_manager);

void opj_tgt_destroy(opj_tgt_tree_t *tree);

void opj_tgt_reset(opj_tgt_tree_t *tree);

void opj_tgt_setvalue(opj_tgt_tree_t *tree,
                      OPJ_UINT32 leafno,
                      OPJ_INT32 value);

void opj_tgt_encode(opj_bio_t *bio,
                    opj_tgt_tree_t *tree,
                    OPJ_UINT32 leafno,
                    OPJ_INT32 threshold);

OPJ_UINT32 opj_tgt_decode(opj_bio_t *bio,
                          opj_tgt_tree_t *tree,
                          OPJ_UINT32 leafno,
                          OPJ_INT32 threshold);

#endif
