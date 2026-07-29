#ifndef OPJ_EVENT_H
#define OPJ_EVENT_H

typedef struct opj_event_mgr {

    void *          m_error_data;

    void *          m_warning_data;

    void *          m_info_data;

    opj_msg_callback error_handler;

    opj_msg_callback warning_handler;

    opj_msg_callback info_handler;
} opj_event_mgr_t;

#define EVT_ERROR   1
#define EVT_WARNING 2
#define EVT_INFO    4

OPJ_BOOL opj_event_msg(opj_event_mgr_t* event_mgr, OPJ_INT32 event_type,
                       const char *fmt, ...);

void opj_set_default_event_handler(opj_event_mgr_t * p_manager);

#endif
