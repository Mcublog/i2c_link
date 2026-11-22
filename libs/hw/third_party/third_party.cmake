file(GLOB_RECURSE GD32F3X0_FIRMWARE_LIBRARY_SOURCES
    ${PROJECT_SOURCE_DIR}/libs/hw/third_party/GD32F3x0_Firmware_Library/GD32F3x0_standard_peripheral/Source/*.c
    ${PROJECT_SOURCE_DIR}/libs/hw/third_party/GD32F3x0_Firmware_Library/CMSIS/GD/GD32F3x0/Source/system_gd32f3x0.c
)

set(GD32F3X0_FIRMWARE_LIBRARY_INCLUDES
    ${PROJECT_SOURCE_DIR}/libs/hw/third_party/GD32F3x0_Firmware_Library/GD32F3x0_standard_peripheral/Include
    ${PROJECT_SOURCE_DIR}/libs/hw/third_party/GD32F3x0_Firmware_Library/CMSIS
    ${PROJECT_SOURCE_DIR}/libs/hw/third_party/GD32F3x0_Firmware_Library/CMSIS/GD/GD32F3x0/Include
)

file(GLOB_RECURSE RTT_LIBRARY_SOURCES
    ${PROJECT_SOURCE_DIR}/libs/hw/third_party/SEGGER_RTT_V762c/RTT/*.c
    ${PROJECT_SOURCE_DIR}/libs/hw/third_party/SEGGER_RTT_V762c/RTT/*.S
    ${PROJECT_SOURCE_DIR}/libs/hw/third_party/SEGGER_RTT_V762c/Syscalls/SEGGER_RTT_Syscalls_GCC.c
)

set(RTT_LIBRARY_INCLUDE_DIR
   ${PROJECT_SOURCE_DIR}/libs/hw/third_party/SEGGER_RTT_V762c/Config
   ${PROJECT_SOURCE_DIR}/libs/hw/third_party/SEGGER_RTT_V762c/RTT)
# Add sources
