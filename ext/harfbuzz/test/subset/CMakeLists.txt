if (HB_BUILD_UTILS)
  file (READ "${CMAKE_CURRENT_SOURCE_DIR}/data/Makefile.sources" SOURCES)
  extract_make_variable (TESTS ${SOURCES})
  foreach (test IN ITEMS ${TESTS})
    add_test (NAME ${test}
      COMMAND "${PYTHON_EXECUTABLE}" run-tests.py $<TARGET_FILE:hb-subset> "data/${test}"
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    set_property(TEST ${test} PROPERTY SKIP_RETURN_CODE 77)
  endforeach ()
endif ()
