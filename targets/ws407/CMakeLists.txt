include(${PROJECT_SOURCE_DIR}/targets/ws407/build.cmake)

# Using desktop gcc compiler
set(CMAKE_C_COMPILER    arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER  arm-none-eabi-g++)

fw_application(mcu_fw)
