xe_test_suite(xenia-kernel-tests ${CMAKE_CURRENT_SOURCE_DIR}
  LINKS fmt xenia-base
)

target_sources(xenia-kernel-tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../aot_runtime_sa2.cc
)
