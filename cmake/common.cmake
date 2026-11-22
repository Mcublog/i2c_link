set(APP     app)
set(APP_FW  app_fw)
# region GLOBAL PROPERTIES
set(STM32F407_TARGET_PATH           ${PROJECT_SOURCE_DIR}/targets/ws407)
set(DESKTOP_TARGET_PATH             ${PROJECT_SOURCE_DIR}/targets/emu)

set(APP_DIR     ${PROJECT_SOURCE_DIR}/app)
set(UTILS_DIR   ${APP_DIR}/utils)
set(IO_DIR      ${APP_DIR}/io)
set(LIBS_DIR    ${PROJECT_SOURCE_DIR}/libs)
set(DEBUG_DIR   ${LIBS_DIR}/debug)

set(APP_DIRS
    ${APP_DIR}
    ${DEBUG_DIR}
    ${UTILS_DIR}
    ${IO_DIR}
)

file(GLOB_RECURSE APP_SOURCES
    ${PROJECT_SOURCE_DIR}/app/*.c
    ${PROJECT_SOURCE_DIR}/app/*.cpp
)

set(COMMON_TYPES_TARGET common_types_test)
set(TEST_DIR            ${PROJECT_SOURCE_DIR}/test)

set(GCC_EMBEDDED    arm-none-eabi-gcc)
set(GCC_DESKTOP     gcc)
