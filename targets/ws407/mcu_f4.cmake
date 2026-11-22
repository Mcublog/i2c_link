include(${PROJECT_SOURCE_DIR}/cmake/options.cmake)

# region HAL
set(WS407_FUNTIK_HAL_INCLUDE_DIR
    ${PROJECT_SOURCE_DIR}/targets/ws407/Core/Inc
    ${PROJECT_SOURCE_DIR}/targets/ws407/Drivers/STM32F4xx_HAL_Driver/Inc
    ${PROJECT_SOURCE_DIR}/targets/ws407/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    ${PROJECT_SOURCE_DIR}/targets/ws407/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${PROJECT_SOURCE_DIR}/targets/ws407/Drivers/CMSIS/Include
)
# endregion

# Put here your symbols (preprocessor defines), one in each line
# Encapsulate them with double quotes for safety purpose
set(FUNTIK_FW_DEFINES
    $<$<CONFIG:DEBUG>:"DEBUG">
    $<$<CONFIG:RELASE>:"NDEBUG">
    $<$<CONFIG:RelWithDebInfo>:"FORCE_LOG">
    "USE_HAL_DRIVER"
    "STM32F407xx"
    "USE_STDPERIPH_DRIVER"
)

set(CPU_PARAMETERS -mthumb
    # This needs attention to properly set for used MCU
    -mcpu=cortex-m4
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)

set(ARM_NONE_EABI_FLAGS
    ${CPU_PARAMETERS}
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
    -Wno-pointer-arith
    -Wno-variadic-macros
    -fdata-sections
    -ffunction-sections
    -fdiagnostics-color=always
    $<$<COMPILE_LANGUAGE:CXX>: -Wno-register>
    # Full debug configuration
    $<$<CONFIG:DEBUG>: -Og -g3 -ggdb>
    # Release
    $<$<CONFIG:RELEASE>:-Os -s>
    # Release with DEBUG
    $<$<CONFIG:RelWithDebInfo>:-Os -g3 -ggdb>
)

# Set linker script
set(WS407_LINKER_SCRIPT ${PROJECT_SOURCE_DIR}/targets/ws407/STM32F407VETx_FLASH.ld)


set(WS407_LINKER_OPTION
    -T${WS407_LINKER_SCRIPT}
    ${CPU_PARAMETERS}
    -Wl,-Map=${CMAKE_PROJECT_NAME}.map
    -specs=nano.specs
    -specs=nosys.specs
    # -u _printf_float                # STDIO float formatting support
    -Wl,--start-group
    -lc
    -lm
    -fno-exceptions
    -fno-rtti
    -Wl,--end-group
    -Wl,--gc-sections # removed unused functions
    -Wl,--print-memory-usage
)
# endregion