if (HB_CHECK)
  file (READ "${CMAKE_CURRENT_SOURCE_DIR}/Makefile.am" MAKEFILEAM)
  extract_make_variable (hb_shape_fuzzer_SOURCES ${MAKEFILEAM})
  extract_make_variable (hb_subset_fuzzer_SOURCES ${MAKEFILEAM})
  extract_make_variable (hb_subset_get_codepoints_fuzzer_SOURCES ${MAKEFILEAM})

  # TODO: enable these two
  #extract_make_variable (FUZZING_CPPFLAGS ${MAKEFILEAM}) # extracting regex fail
  #add_executable (hb-shape-fuzzer # it should be run only after ragel execution
  #  ${project_sources} ${project_extra_sources} ${project_headers}
  #  ${hb_shape_fuzzer_SOURCES})

  add_executable (hb-shape-fuzzer ${hb_shape_fuzzer_SOURCES})
  target_link_libraries (hb-shape-fuzzer harfbuzz)

  add_executable (hb-subset-fuzzer ${hb_subset_fuzzer_SOURCES})
  target_link_libraries (hb-subset-fuzzer harfbuzz-subset)

  add_executable (hb-subset-get-codepoints-fuzzer ${hb_subset_get_codepoints_fuzzer_SOURCES})
  target_link_libraries (hb-subset-get-codepoints-fuzzer harfbuzz-subset)

  target_compile_definitions(hb-shape-fuzzer PUBLIC ${FUZZING_CPPFLAGS})
  target_compile_definitions(hb-subset-fuzzer PUBLIC ${FUZZING_CPPFLAGS})
  target_compile_definitions(hb-subset-get-codepoints-fuzzer PUBLIC ${FUZZING_CPPFLAGS})

  add_test (NAME hb-shape-fuzzer
    COMMAND "${PYTHON_EXECUTABLE}" run-shape-fuzzer-tests.py $<TARGET_FILE:hb-shape-fuzzer>
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  add_test (NAME hb-subset-fuzzer
    COMMAND "${PYTHON_EXECUTABLE}" run-subset-fuzzer-tests.py $<TARGET_FILE:hb-subset-fuzzer> $<TARGET_FILE:hb-subset-get-codepoints-fuzzer>
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif ()
