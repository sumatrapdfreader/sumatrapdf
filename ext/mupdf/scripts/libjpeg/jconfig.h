/* jconfig.vc --- jconfig.h for Microsoft Visual C++ on Windows 95 or NT. */
/* see jconfig.txt for explanations */

#define HAVE_PROTOTYPES
#define HAVE_UNSIGNED_CHAR
#define HAVE_UNSIGNED_SHORT
/* #define void char */
/* #define const */
#undef CHAR_IS_UNSIGNED
#define HAVE_STDDEF_H
#define HAVE_STDLIB_H
#undef NEED_BSD_STRINGS
#undef NEED_SYS_TYPES_H
#undef NEED_FAR_POINTERS	/* we presume a 32-bit flat memory model */
#undef NEED_SHORT_EXTERNAL_NAMES
#undef INCOMPLETE_TYPES_BROKEN

/* Define "boolean" as unsigned char, not int, per Windows custom */
#ifndef __RPCNDR_H__		/* don't conflict if rpcndr.h already read */
typedef unsigned char boolean;
#endif
#ifndef FALSE			/* in case these macros already exist */
#define FALSE	0		/* values of boolean */
#endif
#ifndef TRUE
#define TRUE	1
#endif
#define HAVE_BOOLEAN		/* prevent jmorecfg.h from redefining it */

#ifdef FZ_HIDE_INTERNAL_JPEG
/* The first list is culled from NEED_SHORT_EXTERNAL_NAMES */
#define jpeg_std_error                FZjpeg_std_error
#define jpeg_CreateCompress           FZjpeg_CreateCompress
#define jpeg_CreateDecompress         FZjpeg_CreateDecompress
#define jpeg_destroy_compress         FZjpeg_destroy_compress
#define jpeg_destroy_decompress       FZjpeg_destroy_decompress
#define jpeg_stdio_dest               FZjpeg_stdio_dest
#define jpeg_stdio_src                FZjpeg_stdio_src
#define jpeg_mem_dest                 FZjpeg_mem_dest
#define jpeg_mem_src                  FZjpeg_mem_src
#define jpeg_set_defaults             FZjpeg_set_defaults
#define jpeg_set_colorspace           FZjpeg_set_colorspace
#define jpeg_default_colorspace       FZjpeg_default_colorspace
#define jpeg_set_quality              FZjpeg_set_quality
#define jpeg_set_linear_quality       FZjpeg_set_linear_quality
#define jpeg_default_qtables          FZjpeg_default_qtables
#define jpeg_add_quant_table          FZjpeg_add_quant_table
#define jpeg_quality_scaling          FZjpeg_quality_scaling
#define jpeg_simple_progression       FZjpeg_simple_progression
#define jpeg_suppress_tables          FZjpeg_suppress_tables
#define jpeg_alloc_quant_table        FZjpeg_alloc_quant_table
#define jpeg_alloc_huff_table         FZjpeg_alloc_huff_table
#define jpeg_start_compress           FZjpeg_start_compress
#define jpeg_write_scanlines          FZjpeg_write_scanlines
#define jpeg_finish_compress          FZjpeg_finish_compress
#define jpeg_calc_jpeg_dimensions     FZjpeg_calc_jpeg_dimensions
#define jpeg_write_raw_data           FZjpeg_write_raw_data
#define jpeg_write_marker             FZjpeg_write_marker
#define jpeg_write_m_header           FZjpeg_write_m_header
#define jpeg_write_m_byte             FZjpeg_write_m_byte
#define jpeg_write_tables             FZjpeg_write_tables
#define jpeg_read_header              FZjpeg_read_header
#define jpeg_start_decompress         FZjpeg_start_decompress
#define jpeg_read_scanlines           FZjpeg_read_scanlines
#define jpeg_finish_decompress        FZjpeg_finish_decompress
#define jpeg_read_raw_data            FZjpeg_read_raw_data
#define jpeg_has_multiple_scans       FZjpeg_has_multiple_scans
#define jpeg_start_output             FZjpeg_start_output
#define jpeg_finish_output            FZjpeg_finish_output
#define jpeg_input_complete           FZjpeg_input_complete
#define jpeg_new_colormap             FZjpeg_new_colormap
#define jpeg_consume_input            FZjpeg_consume_input
#define jpeg_core_output_dimensions   FZjpeg_core_output_dimensions
#define jpeg_calc_output_dimensions   FZjpeg_calc_output_dimensions
#define jpeg_save_markers             FZjpeg_save_markers
#define jpeg_set_marker_processor     FZjpeg_set_marker_processor
#define jpeg_read_coefficients        FZjpeg_read_coefficients
#define jpeg_write_coefficients       FZjpeg_write_coefficients
#define jpeg_copy_critical_parameters FZjpeg_copy_critical_parameters
#define jpeg_abort_compress           FZjpeg_abort_compress
#define jpeg_abort_decompress         FZjpeg_abort_decompress
#define jpeg_abort                    FZjpeg_abort
#define jpeg_destroy                  FZjpeg_destroy
#define jpeg_resync_to_restart        FZjpeg_resync_to_restart

/* This second list comes from examination of symbols in the lib */
#define jpeg_free_small               FZjpeg_free_small
#define jpeg_get_small                FZjpeg_get_small
#define jpeg_get_large                FZjpeg_get_large
#define jpeg_free_large               FZjpeg_free_large
#define jpeg_mem_available            FZjpeg_mem_available
#define jpeg_open_backing_store       FZjpeg_open_backing_store
#define jpeg_mem_init                 FZjpeg_mem_init
#define jpeg_mem_term                 FZjpeg_mem_term
#define jpeg_natural_order            FZjpeg_natural_order
#define jpeg_natural_order2           FZjpeg_natural_order2
#define jpeg_natural_order3           FZjpeg_natural_order3
#define jpeg_natural_order4           FZjpeg_natural_order4
#define jpeg_natural_order5           FZjpeg_natural_order5
#define jpeg_natural_order6           FZjpeg_natural_order6
#define jpeg_natural_order7           FZjpeg_natural_order7
#define jpeg_fdct_10x10               FZjpeg_fdct_10x10
#define jpeg_fdct_10x5                FZjpeg_fdct_10x5
#define jpeg_fdct_11x11               FZjpeg_fdct_11x11
#define jpeg_fdct_12x12               FZjpeg_fdct_12x12
#define jpeg_fdct_12x6                FZjpeg_fdct_12x6
#define jpeg_fdct_13x13               FZjpeg_fdct_13x13
#define jpeg_fdct_14x14               FZjpeg_fdct_14x14
#define jpeg_fdct_14x7                FZjpeg_fdct_14x7
#define jpeg_fdct_15x15               FZjpeg_fdct_15x15
#define jpeg_fdct_16x16               FZjpeg_fdct_16x16
#define jpeg_fdct_16x8                FZjpeg_fdct_16x8
#define jpeg_fdct_1x1                 FZjpeg_fdct_1x1
#define jpeg_fdct_1x2                 FZjpeg_fdct_1x2
#define jpeg_fdct_2x1                 FZjpeg_fdct_2x1
#define jpeg_fdct_2x2                 FZjpeg_fdct_2x2
#define jpeg_fdct_2x4                 FZjpeg_fdct_2x4
#define jpeg_fdct_3x3                 FZjpeg_fdct_3x3
#define jpeg_fdct_3x6                 FZjpeg_fdct_3x6
#define jpeg_fdct_4x2                 FZjpeg_fdct_4x2
#define jpeg_fdct_4x4                 FZjpeg_fdct_4x4
#define jpeg_fdct_4x8                 FZjpeg_fdct_4x8
#define jpeg_fdct_5x10                FZjpeg_fdct_5x10
#define jpeg_fdct_5x5                 FZjpeg_fdct_5x5
#define jpeg_fdct_6x12                FZjpeg_fdct_6x12
#define jpeg_fdct_6x3                 FZjpeg_fdct_6x3
#define jpeg_fdct_6x6                 FZjpeg_fdct_6x6
#define jpeg_fdct_7x14                FZjpeg_fdct_7x14
#define jpeg_fdct_7x7                 FZjpeg_fdct_7x7
#define jpeg_fdct_8x16                FZjpeg_fdct_8x16
#define jpeg_fdct_8x4                 FZjpeg_fdct_8x4
#define jpeg_fdct_9x9                 FZjpeg_fdct_9x9
#define jpeg_cust_mem_init            FZjpeg_cust_mem_init
#define jpeg_cust_mem_set_private     FZjpeg_cust_mem_set_private
#define jpeg_fill_bit_buffer          FZjpeg_fill_bit_buffer
#define jpeg_huff_decode              FZjpeg_huff_decode
#define jpeg_make_c_derived_tbl       FZjpeg_make_c_derived_tbl
#define jpeg_make_d_derived_tbl       FZjpeg_make_d_derived_tbl
#define jpeg_zigzag_order             FZjpeg_zigzag_order
#define jpeg_zigzag_order2            FZjpeg_zigzag_order2
#define jpeg_zigzag_order3            FZjpeg_zigzag_order3
#define jpeg_zigzag_order4            FZjpeg_zigzag_order4
#define jpeg_zigzag_order5            FZjpeg_zigzag_order5
#define jpeg_zigzag_order6            FZjpeg_zigzag_order6
#define jpeg_zigzag_order7            FZjpeg_zigzag_order7
#define jpeg_std_message_table        FZjpeg_std_message_table
#define jpeg_aritab                   FZjpeg_aritab
#define jpeg_idct_islow               FZjpeg_idct_islow
#define jpeg_fdct_islow               FZjpeg_fdct_islow
#define jpeg_aritab                   FZjpeg_aritab
#define jpeg_gen_optimal_table        FZjpeg_gen_optimal_table
#define jinit_marker_reader           FZinit_marker_reader
#define jdiv_round_up                 FZdiv_round_up
#define jround_up                     FZround_up
#define jcopy_block_row               FZcopy_block_row
#define jcopy_sample_rows             FZcopy_sample_rows
#define jinit_input_controller        FZinit_input_controller
#define jinit_memory_mgr              FZinit_memory_mgr
#define jinit_master_decompress       FZinit_master_decompress
#define jinit_huff_decoder            FZinit_huff_decoder
#define jinit_d_coef_controller       FZinit_d_coef_controller
#define jinit_color_deconverter       FZinit_color_deconverter
#define jinit_inverse_dct             FZinit_inverse_dct
#define jinit_d_main_controller       FZinit_d_main_controller
#define jinit_arith_decoder           FZinit_arith_decoder
#define jinit_color_deconverter       FZinit_color_deconverter
#define jinit_d_coef_controller       FZinit_d_coef_controller
#define jinit_d_main_controller       FZinit_d_main_controller
#define jinit_d_post_controller       FZinit_d_post_controller
#define jinit_huff_decoder            FZinit_huff_decoder
#define jinit_master_decompress       FZinit_master_decompress
#define jinit_upsampler               FZinit_upsampler
#define jinit_d_post_controller       FZinit_d_post_controller
#define jinit_downsampler             FZinit_downsampler
#define jinit_arith_decoder           FZinit_arith_decoder
#define jinit_marker_writer           FZinit_marker_writer
#define jinit_marker_mgr              FZinit_marker_mgr
#define jinit_compress_master         FZinit_compress_master
#define jinit_c_coef_controller       FZinit_c_coef_controller
#define jinit_color_converter         FZinit_color_converter
#define jinit_forward_dct             FZinit_forward_dct
#define jinit_arith_encoder           FZinit_arith_encoder
#define jinit_huff_encoder            FZinit_huff_encoder
#define jinit_c_main_controller       FZinit_c_main_controller
#define jinit_marker_writer           FZinit_marker_writer
#define jinit_c_master_control        FZinit_c_master_control
#define jinit_c_prep_controller       FZinit_c_prep_controller
#define jpeg_std_huff_table           FZjpeg_std_huff_table
#define jinit_merged_upsampler        FZjinit_merged_upsampler
#define jpeg_fdct_float               FZjpeg_fdct_float
#define jpeg_fdct_ifast               FZjpeg_fdct_ifast
#define jpeg_idct_float               FZjpeg_idct_float
#define jpeg_idct_ifast               FZjpeg_idct_ifast
#define jpeg_idct_10x10               FZjpeg_idct_10x10
#define jpeg_idct_10x5                FZjpeg_idct_10x5
#define jpeg_idct_11x11               FZjpeg_idct_11x11
#define jpeg_idct_12x12               FZjpeg_idct_12x12
#define jpeg_idct_12x6                FZjpeg_idct_12x6
#define jpeg_idct_13x13               FZjpeg_idct_13x13
#define jpeg_idct_14x14               FZjpeg_idct_14x14
#define jpeg_idct_14x7                FZjpeg_idct_14x7
#define jpeg_idct_15x15               FZjpeg_idct_15x15
#define jpeg_idct_16x16               FZjpeg_idct_16x16
#define jpeg_idct_16x8                FZjpeg_idct_16x8
#define jpeg_idct_1x1                 FZjpeg_idct_1x1
#define jpeg_idct_1x2                 FZjpeg_idct_1x2
#define jpeg_idct_2x1                 FZjpeg_idct_2x1
#define jpeg_idct_2x2                 FZjpeg_idct_2x2
#define jpeg_idct_2x4                 FZjpeg_idct_2x4
#define jpeg_idct_3x3                 FZjpeg_idct_3x3
#define jpeg_idct_3x6                 FZjpeg_idct_3x6
#define jpeg_idct_4x2                 FZjpeg_idct_4x2
#define jpeg_idct_4x4                 FZjpeg_idct_4x4
#define jpeg_idct_4x8                 FZjpeg_idct_4x8
#define jpeg_idct_5x10                FZjpeg_idct_5x10
#define jpeg_idct_5x5                 FZjpeg_idct_5x5
#define jpeg_idct_6x12                FZjpeg_idct_6x12
#define jpeg_idct_6x3                 FZjpeg_idct_6x3
#define jpeg_idct_6x6                 FZjpeg_idct_6x6
#define jpeg_idct_7x14                FZjpeg_idct_7x14
#define jpeg_idct_7x7                 FZjpeg_idct_7x7
#define jpeg_idct_8x16                FZjpeg_idct_8x16
#define jpeg_idct_8x4                 FZjpeg_idct_8x4
#define jpeg_idct_9x9                 FZjpeg_idct_9x9
#define jinit_1pass_quantizer         FZjinit_1pass_quantizer
#define jinit_2pass_quantizer         FZjinit_2pass_quantizer
#endif

#ifdef JPEG_INTERNALS

#undef RIGHT_SHIFT_IS_UNSIGNED

#endif /* JPEG_INTERNALS */

#ifdef JPEG_CJPEG_DJPEG

#define BMP_SUPPORTED		/* BMP image file format */
#define GIF_SUPPORTED		/* GIF image file format */
#define PPM_SUPPORTED		/* PBMPLUS PPM/PGM image file format */
#undef RLE_SUPPORTED		/* Utah RLE image file format */
#define TARGA_SUPPORTED		/* Targa image file format */

#define TWO_FILE_COMMANDLINE	/* optional */
#define USE_SETMODE		/* Microsoft has setmode() */
#undef NEED_SIGNAL_CATCHER
#undef DONT_USE_B_MODE
#undef PROGRESS_REPORT		/* optional */

#endif /* JPEG_CJPEG_DJPEG */
