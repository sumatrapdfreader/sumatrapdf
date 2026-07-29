#ifndef OPJ_CIO_H
#define OPJ_CIO_H

#include "opj_config_private.h"

#if defined(OPJ_BIG_ENDIAN)
#define opj_write_bytes     opj_write_bytes_BE
#define opj_read_bytes      opj_read_bytes_BE
#define opj_write_double    opj_write_double_BE
#define opj_read_double     opj_read_double_BE
#define opj_write_float     opj_write_float_BE
#define opj_read_float      opj_read_float_BE
#else
#define opj_write_bytes     opj_write_bytes_LE
#define opj_read_bytes      opj_read_bytes_LE
#define opj_write_double    opj_write_double_LE
#define opj_read_double     opj_read_double_LE
#define opj_write_float     opj_write_float_LE
#define opj_read_float      opj_read_float_LE
#endif

#define OPJ_STREAM_STATUS_OUTPUT  0x1U
#define OPJ_STREAM_STATUS_INPUT   0x2U
#define OPJ_STREAM_STATUS_END     0x4U
#define OPJ_STREAM_STATUS_ERROR   0x8U

typedef struct opj_stream_private {

    void *                  m_user_data;

    opj_stream_free_user_data_fn        m_free_user_data_fn;

    OPJ_UINT64              m_user_data_length;

    opj_stream_read_fn      m_read_fn;

    opj_stream_write_fn     m_write_fn;

    opj_stream_skip_fn      m_skip_fn;

    opj_stream_seek_fn      m_seek_fn;

    OPJ_BYTE *                  m_stored_data;

    OPJ_BYTE *                  m_current_data;

    OPJ_OFF_T(* m_opj_skip)(struct opj_stream_private *, OPJ_OFF_T,
                            struct opj_event_mgr *);

    OPJ_BOOL(* m_opj_seek)(struct opj_stream_private *, OPJ_OFF_T,
                           struct opj_event_mgr *);

    OPJ_SIZE_T          m_bytes_in_buffer;

    OPJ_OFF_T           m_byte_offset;

    OPJ_SIZE_T          m_buffer_size;

    OPJ_UINT32 m_status;

}
opj_stream_private_t;

void opj_write_bytes_BE(OPJ_BYTE * p_buffer, OPJ_UINT32 p_value,
                        OPJ_UINT32 p_nb_bytes);

void opj_read_bytes_BE(const OPJ_BYTE * p_buffer, OPJ_UINT32 * p_value,
                       OPJ_UINT32 p_nb_bytes);

void opj_write_bytes_LE(OPJ_BYTE * p_buffer, OPJ_UINT32 p_value,
                        OPJ_UINT32 p_nb_bytes);

void opj_read_bytes_LE(const OPJ_BYTE * p_buffer, OPJ_UINT32 * p_value,
                       OPJ_UINT32 p_nb_bytes);

void opj_write_double_LE(OPJ_BYTE * p_buffer, OPJ_FLOAT64 p_value);

void opj_write_double_BE(OPJ_BYTE * p_buffer, OPJ_FLOAT64 p_value);

void opj_read_double_LE(const OPJ_BYTE * p_buffer, OPJ_FLOAT64 * p_value);

void opj_read_double_BE(const OPJ_BYTE * p_buffer, OPJ_FLOAT64 * p_value);

void opj_read_float_LE(const OPJ_BYTE * p_buffer, OPJ_FLOAT32 * p_value);

void opj_read_float_BE(const OPJ_BYTE * p_buffer, OPJ_FLOAT32 * p_value);

void opj_write_float_LE(OPJ_BYTE * p_buffer, OPJ_FLOAT32 p_value);

void opj_write_float_BE(OPJ_BYTE * p_buffer, OPJ_FLOAT32 p_value);

OPJ_SIZE_T opj_stream_read_data(opj_stream_private_t * p_stream,
                                OPJ_BYTE * p_buffer, OPJ_SIZE_T p_size, struct opj_event_mgr * p_event_mgr);

OPJ_SIZE_T opj_stream_write_data(opj_stream_private_t * p_stream,
                                 const OPJ_BYTE * p_buffer, OPJ_SIZE_T p_size,
                                 struct opj_event_mgr * p_event_mgr);

OPJ_BOOL opj_stream_flush(opj_stream_private_t * p_stream,
                          struct opj_event_mgr * p_event_mgr);

OPJ_OFF_T opj_stream_skip(opj_stream_private_t * p_stream, OPJ_OFF_T p_size,
                          struct opj_event_mgr * p_event_mgr);

OPJ_OFF_T opj_stream_tell(const opj_stream_private_t * p_stream);

OPJ_OFF_T opj_stream_get_number_byte_left(const opj_stream_private_t *
        p_stream);

OPJ_OFF_T opj_stream_write_skip(opj_stream_private_t * p_stream,
                                OPJ_OFF_T p_size, struct opj_event_mgr * p_event_mgr);

OPJ_OFF_T opj_stream_read_skip(opj_stream_private_t * p_stream,
                               OPJ_OFF_T p_size, struct opj_event_mgr * p_event_mgr);

OPJ_BOOL opj_stream_read_seek(opj_stream_private_t * p_stream, OPJ_OFF_T p_size,
                              struct opj_event_mgr * p_event_mgr);

OPJ_BOOL opj_stream_write_seek(opj_stream_private_t * p_stream,
                               OPJ_OFF_T p_size, struct opj_event_mgr * p_event_mgr);

OPJ_BOOL opj_stream_seek(opj_stream_private_t * p_stream, OPJ_OFF_T p_size,
                         struct opj_event_mgr * p_event_mgr);

OPJ_BOOL opj_stream_has_seek(const opj_stream_private_t * p_stream);

OPJ_SIZE_T opj_stream_default_read(void * p_buffer, OPJ_SIZE_T p_nb_bytes,
                                   void * p_user_data);

OPJ_SIZE_T opj_stream_default_write(void * p_buffer, OPJ_SIZE_T p_nb_bytes,
                                    void * p_user_data);

OPJ_OFF_T opj_stream_default_skip(OPJ_OFF_T p_nb_bytes, void * p_user_data);

OPJ_BOOL opj_stream_default_seek(OPJ_OFF_T p_nb_bytes, void * p_user_data);

#endif
