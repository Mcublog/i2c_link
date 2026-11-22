include(${PROJECT_SOURCE_DIR}/targets/emu/build.cmake)

# Using desktop gcc compiler
set(CMAKE_C_COMPILER    gcc)
set(CMAKE_CXX_COMPILER  g++)

emu_build(emu)
