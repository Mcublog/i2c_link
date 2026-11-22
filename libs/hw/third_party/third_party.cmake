file(GLOB_RECURSE RTT_LIBRARY_SOURCES
    ${PROJECT_SOURCE_DIR}/libs/hw/third_party/SEGGER_RTT_V762c/RTT/*.c
    ${PROJECT_SOURCE_DIR}/libs/hw/third_party/SEGGER_RTT_V762c/RTT/*.S
    ${PROJECT_SOURCE_DIR}/libs/hw/third_party/SEGGER_RTT_V762c/Syscalls/SEGGER_RTT_Syscalls_GCC.c
)

set(RTT_LIBRARY_INCLUDE_DIR
   ${PROJECT_SOURCE_DIR}/libs/hw/third_party/SEGGER_RTT_V762c/Config
   ${PROJECT_SOURCE_DIR}/libs/hw/third_party/SEGGER_RTT_V762c/RTT)
# Add sources