
macro(plugin_compilation name varName foundName optionName defineName)
    if (${foundName} AND WITH_${optionName})
        if (PLUGIN_LOADING_SUPPORTED_AND_ENABLED AND WITH_${optionName}_PLUGIN)
            message("Compiling '" ${name} "' as dynamic plugin")
            add_library(heif-${name}
                    MODULE ${${optionName}_sources} ${${optionName}_extra_plugin_sources}
                    ../api/libheif/heif_plugin.cc
                    )
            if (NOT APPLE)
                set_target_properties(heif-${name}
                        PROPERTIES
                        VERSION ${PROJECT_VERSION}
                        SOVERSION ${PROJECT_VERSION_MAJOR})
            endif ()
            target_compile_definitions(heif-${name}
                    PUBLIC
                    LIBHEIF_EXPORTS
                    HAVE_VISIBILITY)
            target_compile_definitions(heif-${name} PRIVATE PLUGIN_${defineName}=1)
            target_include_directories(heif-${name} PRIVATE ${PROJECT_SOURCE_DIR} ${libheif_BINARY_DIR} ${libheif_SOURCE_DIR}/libheif ${libheif_SOURCE_DIR}/libheif/api ${${varName}_INCLUDE_DIRS})
            target_link_libraries(heif-${name} PRIVATE ${${varName}_LIBRARIES} heif)

            install(TARGETS heif-${name}
                    LIBRARY DESTINATION ${COMPUTED_PLUGIN_INSTALL_DIRECTORY}
                    )
        else ()
            message("Compiling '" ${name} "' as built-in backend")
            target_sources(heif PRIVATE ${${optionName}_sources})
            target_compile_definitions(heif PRIVATE HAVE_${defineName}=1)
            target_include_directories(heif PRIVATE ${${varName}_INCLUDE_DIRS})
            target_link_directories(heif PRIVATE ${${varName}_LINK_DIRS})
            if (${varName}_DEBUG_LIBRARY)
                target_link_libraries(heif PRIVATE debug ${${varName}_DEBUG_LIBRARY})
                target_link_libraries(heif PRIVATE optimized ${${varName}_LIBRARIES})
            else()
                target_link_libraries(heif PRIVATE ${${varName}_LIBRARIES})
            endif()
        endif ()

        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${${varName}_CFLAGS}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${${varName}_CFLAGS}")
    else()
        message("Not compiling '" ${name} "' backend")
    endif ()
endmacro()

set(X265_sources encoder_x265.h encoder_x265.cc)
set(X265_extra_plugin_sources)
plugin_compilation(x265 X265 X265_FOUND X265 X265)

set(LIBDE265_sources decoder_libde265.cc decoder_libde265.h)
set(LIBDE265_extra_plugin_sources ../error.cc)
plugin_compilation(libde265 LIBDE265 LIBDE265_FOUND LIBDE265 LIBDE265)

set(DAV1D_sources decoder_dav1d.cc decoder_dav1d.h)
set(DAV1D_extra_plugin_sources ../error.cc ../common_utils.cc ../common_utils.h)
plugin_compilation(dav1d DAV1D DAV1D_FOUND DAV1D DAV1D)

set(AOM_DECODER_sources decoder_aom.cc decoder_aom.h)
set(AOM_DECODER_extra_plugin_sources)
plugin_compilation(aomdec AOM AOM_DECODER_FOUND AOM_DECODER AOM_DECODER)

set(AOM_ENCODER_sources encoder_aom.cc encoder_aom.h)
set(AOM_ENCODER_extra_plugin_sources ../error.cc ../common_utils.cc ../common_utils.h)
plugin_compilation(aomenc AOM AOM_ENCODER_FOUND AOM_ENCODER AOM_ENCODER)

set(SvtEnc_sources encoder_svt.cc encoder_svt.h)
set(SvtEnc_extra_plugin_sources ../error.cc ../common_utils.cc ../common_utils.h)
plugin_compilation(svtenc SvtEnc SvtEnc_FOUND SvtEnc SvtEnc)

set(RAV1E_sources encoder_rav1e.cc encoder_rav1e.h)
set(RAV1E_extra_plugin_sources ../error.cc)
plugin_compilation(rav1e RAV1E RAV1E_FOUND RAV1E RAV1E)

set(JPEG_DECODER_sources decoder_jpeg.cc decoder_jpeg.h)
set(JPEG_DECODER_extra_plugin_sources)
plugin_compilation(jpegdec JPEG JPEG_FOUND JPEG_DECODER JPEG_DECODER)

set(JPEG_ENCODER_sources encoder_jpeg.cc encoder_jpeg.h)
set(JPEG_ENCODER_extra_plugin_sources)
plugin_compilation(jpegenc JPEG JPEG_FOUND JPEG_ENCODER JPEG_ENCODER)

set(OpenJPEG_DECODER_sources decoder_openjpeg.cc decoder_openjpeg.h)
set(OpenJPEG_DECODER_extra_plugin_sources ) 
plugin_compilation(j2kdec OPENJPEG OPENJPEG_FOUND OpenJPEG_DECODER OPENJPEG_DECODER)

set(OpenJPEG_ENCODER_sources encoder_openjpeg.cc encoder_openjpeg.h)
set(OpenJPEG_ENCODER_extra_plugin_sources)
plugin_compilation(j2kenc OPENJPEG OPENJPEG_FOUND OpenJPEG_ENCODER OPENJPEG_ENCODER)

set(KVAZAAR_sources encoder_kvazaar.cc encoder_kvazaar.h)
set(KVAZAAR_extra_plugin_sources)
plugin_compilation(kvazaar KVAZAAR KVAZAAR_FOUND KVAZAAR KVAZAAR)

set(FFMPEG_DECODER_sources decoder_ffmpeg.cc decoder_ffmpeg.h)
set(FFMPEG_DECODER_extra_plugin_sources ../error.cc nalu_utils.cc)
plugin_compilation(ffmpegdec FFMPEG FFMPEG_FOUND FFMPEG_DECODER FFMPEG_DECODER)

set(OPENJPH_ENCODER_sources encoder_openjph.cc encoder_openjph.h)
set(OPENJPH_ENCODER_extra_plugin_sources)
plugin_compilation(jphenc OPENJPH OPENJPH_FOUND OPENJPH_ENCODER OPENJPH_ENCODER)

set(UVG266_sources encoder_uvg266.cc encoder_uvg266.h)
set(UVG266_extra_plugin_sources)
plugin_compilation(uvg266 UVG266 UVG266_FOUND UVG266 UVG266)

set(VVDEC_sources decoder_vvdec.cc decoder_vvdec.h)
set(VVDEC_extra_plugin_sources)
plugin_compilation(vvdec vvdec vvdec_FOUND VVDEC VVDEC)

set(VVENC_sources encoder_vvenc.cc encoder_vvenc.h)
set(VVENC_extra_plugin_sources)
plugin_compilation(vvenc vvenc vvenc_FOUND VVENC VVENC)

set(X264_sources encoder_x264.h encoder_x264.cc)
set(X264_extra_plugin_sources)
plugin_compilation(x264 X264 X264_FOUND X264 X264)

set(OpenH264_DECODER_sources decoder_openh264.cc decoder_openh264.h)
set(OpenH264_DECODER_extra_plugin_sources)
plugin_compilation(openh264dec OpenH264 OpenH264_DECODER_FOUND OpenH264_DECODER OpenH264_DECODER)

target_sources(heif PRIVATE
               encoder_mask.h
               encoder_mask.cc
               nalu_utils.h
               nalu_utils.cc)

if (WITH_UNCOMPRESSED_CODEC)
    target_sources(heif PRIVATE
            encoder_uncompressed.h
            encoder_uncompressed.cc
            decoder_uncompressed.h
            decoder_uncompressed.cc)
endif ()

if (WITH_WEBCODECS)
   target_sources(heif PRIVATE
   		  decoder_webcodecs.h
		  decoder_webcodecs.cc)
   target_compile_definitions(heif PRIVATE HAVE_WEBCODECS=1)
endif()


