set(CMAKE_C_COMPILER    arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER  arm-none-eabi-g++)

include(${PROJECT_SOURCE_DIR}/targets/fntester_pcb/build.cmake)

fw_application(fntester_pcb)