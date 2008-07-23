# this is an SCons build specification file

env = Environment(CPPDEFINES = {'HAVE_STDINT_H' : None,
				'PACKAGE' : '\\"jbig2dec\\"',
				'VERSION' : '\\"0.8\\"',
				'JBIG2_DEBUG' : None,
				'JBIG2_HALFTONE' : None})
env.Append(CCFLAGS = ' -g -Wall')

lib_sources = Split("""jbig2.c 
        jbig2_arith.c jbig2_arith_int.c jbig2_arith_iaid.c 
	jbig2_huffman.c jbig2_metadata.c
        jbig2_segment.c jbig2_page.c 
        jbig2_symbol_dict.c jbig2_text.c 
	jbig2_halftone.c
        jbig2_generic.c jbig2_refinement.c jbig2_mmr.c 
        jbig2_image.c jbig2_image_pbm.c""")

lib_headers = Split("""
        os_types.h config_types.h config_win32.h 
        jbig2.h jbig2_priv.h jbig2_image.h 
        jbig2_arith.h jbig2_arith_iaid.h jbig2_arith_int.h 
        jbig2_huffman.h jbig2_hufftab.h jbig2_mmr.h 
        jbig2_generic.h jbig2_symbol_dict.h 
        jbig2_metadata.h""")

env.Library('jbig2dec', lib_sources)

jbig2dec_sources = Split("""jbig2dec.c sha1.c""")

jbig2dec_headers = Split(""" sha1.h 
	jbig2.h jbig2_image.h getopt.h 
        os_types.h config_types.h config_win32.h""")

env.Program('jbig2dec', jbig2dec_sources, LIBS=['jbig2dec'], LIBPATH='.')

