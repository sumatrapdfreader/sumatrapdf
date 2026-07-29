#ifndef OPJ_FUNCTION_LIST_H
#define OPJ_FUNCTION_LIST_H

typedef void (*opj_procedure)(void);

typedef struct opj_procedure_list {

    OPJ_UINT32 m_nb_procedures;

    OPJ_UINT32 m_nb_max_procedures;

    opj_procedure * m_procedures;

} opj_procedure_list_t;

opj_procedure_list_t *  opj_procedure_list_create(void);

void  opj_procedure_list_destroy(opj_procedure_list_t * p_list);

OPJ_BOOL opj_procedure_list_add_procedure(opj_procedure_list_t *
        p_validation_list, opj_procedure p_procedure, opj_event_mgr_t* p_manager);

OPJ_UINT32 opj_procedure_list_get_nb_procedures(opj_procedure_list_t *
        p_validation_list);

opj_procedure* opj_procedure_list_get_first_procedure(opj_procedure_list_t *
        p_validation_list);

void opj_procedure_list_clear(opj_procedure_list_t * p_validation_list);

#endif
