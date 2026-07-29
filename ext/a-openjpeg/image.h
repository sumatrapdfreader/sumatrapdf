#ifndef OPJ_IMAGE_H
#define OPJ_IMAGE_H

struct opj_image;
struct opj_cp;

opj_image_t* opj_image_create0(void);

void opj_image_comp_header_update(opj_image_t * p_image,
                                  const struct opj_cp* p_cp);

void opj_copy_image_header(const opj_image_t* p_image_src,
                           opj_image_t* p_image_dest);

#endif
