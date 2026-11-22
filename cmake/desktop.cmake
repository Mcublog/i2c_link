set(DESKTOP_INCLUDE_DIR
    ${DESKTOP_DIR}
    ${APP_DIRS}
)

file(GLOB_RECURSE BSP_SOURCES
    ${PROJECT_SOURCE_DIR}/bsp/desktop/*.c
    ${PROJECT_SOURCE_DIR}/bsp/desktop/*.cpp
)

file(GLOB_RECURSE ARCH_SOURCES
    ${PROJECT_SOURCE_DIR}/arch/desktop/*.c
    ${PROJECT_SOURCE_DIR}/arch/desktop/*.cpp
)

set(EMU_DEFINES
    $<$<CONFIG:DEBUG>:"DEBUG">
    $<$<CONFIG:RELASE>:"NDEBUG">
    "DESKTOP"
    "FORCE_LOG"
)
