include(${PROJECT_SOURCE_DIR}/test/build.cmake)

# Using desktop gcc compiler
set(CMAKE_C_COMPILER    gcc)
set(CMAKE_CXX_COMPILER  g++)

test_target_with_main_entry("utest_activity" "" "" "")
