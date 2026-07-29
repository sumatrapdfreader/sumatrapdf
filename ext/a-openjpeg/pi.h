#ifndef OPJ_PI_H
#define OPJ_PI_H

typedef struct opj_pi_resolution {
    OPJ_UINT32 pdx, pdy;
    OPJ_UINT32 pw, ph;
} opj_pi_resolution_t;

typedef struct opj_pi_comp {
    OPJ_UINT32 dx, dy;

    OPJ_UINT32 numresolutions;
    opj_pi_resolution_t *resolutions;
} opj_pi_comp_t;

typedef struct opj_pi_iterator {

    OPJ_BYTE tp_on;

    OPJ_INT16 *include;

    OPJ_UINT32 include_size;

    OPJ_UINT32 step_l;

    OPJ_UINT32 step_r;

    OPJ_UINT32 step_c;

    OPJ_UINT32 step_p;

    OPJ_UINT32 compno;

    OPJ_UINT32 resno;

    OPJ_UINT32 precno;

    OPJ_UINT32 layno;

    OPJ_BOOL first;

    opj_poc_t poc;

    OPJ_UINT32 numcomps;

    opj_pi_comp_t *comps;

    OPJ_UINT32 tx0, ty0, tx1, ty1;

    OPJ_UINT32 x, y;

    OPJ_UINT32 dx, dy;

    opj_event_mgr_t* manager;
} opj_pi_iterator_t;

opj_pi_iterator_t *opj_pi_initialise_encode(const opj_image_t *image,
        opj_cp_t *cp,
        OPJ_UINT32 tileno,
        J2K_T2_MODE t2_mode,
        opj_event_mgr_t* manager);

void opj_pi_update_encoding_parameters(const opj_image_t *p_image,
                                       opj_cp_t *p_cp,
                                       OPJ_UINT32 p_tile_no);

void opj_pi_create_encode(opj_pi_iterator_t *pi,
                          opj_cp_t *cp,
                          OPJ_UINT32 tileno,
                          OPJ_UINT32 pino,
                          OPJ_UINT32 tpnum,
                          OPJ_INT32 tppos,
                          J2K_T2_MODE t2_mode);

opj_pi_iterator_t *opj_pi_create_decode(opj_image_t * image,
                                        opj_cp_t * cp,
                                        OPJ_UINT32 tileno,
                                        opj_event_mgr_t* manager);

void opj_pi_destroy(opj_pi_iterator_t *p_pi,
                    OPJ_UINT32 p_nb_elements);

OPJ_BOOL opj_pi_next(opj_pi_iterator_t * pi);

OPJ_UINT32 opj_get_encoding_packet_count(const opj_image_t *p_image,
        const opj_cp_t *p_cp,
        OPJ_UINT32 p_tile_no);

#endif
