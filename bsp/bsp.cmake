file(GLOB_RECURSE BSP_FNTESTER_PCB_SOURCES
    ${PROJECT_SOURCE_DIR}/bsp/fntester_pcb/*.c
    ${PROJECT_SOURCE_DIR}/bsp/fntester_pcb/*.cpp
)

set(BSP_FNTESTER_PCB_INCLUDES
    ${PROJECT_SOURCE_DIR}/bsp/fntester_pcb
    ${PROJECT_SOURCE_DIR}/bsp/fntester_pcb/usb
)

file(GLOB_RECURSE BSP_DESKTOP_SOURCES
    ${PROJECT_SOURCE_DIR}/bsp/desktop/impl/*.c
    ${PROJECT_SOURCE_DIR}/bsp/desktop/impl/*.cpp
)

set(BSP_DESKTOP_INCLUDES ${PROJECT_SOURCE_DIR}/bsp/desktop/impl)