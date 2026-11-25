include(${PROJECT_SOURCE_DIR}/cmake/options.cmake)

# region HAL
# Проект CubeMX для платы fntester_pcb (STM32F072CB) генерируется
# прямо в targets/fntester_pcb
set(FNTESTER_PCB_CUBE_DIR ${PROJECT_SOURCE_DIR}/targets/fntester_pcb)

set(FNTESTER_PCB_HAL_INCLUDE_DIR
    ${FNTESTER_PCB_CUBE_DIR}/Core/Inc
    ${FNTESTER_PCB_CUBE_DIR}/Drivers/STM32F0xx_HAL_Driver/Inc
    ${FNTESTER_PCB_CUBE_DIR}/Drivers/STM32F0xx_HAL_Driver/Inc/Legacy
    ${FNTESTER_PCB_CUBE_DIR}/Drivers/CMSIS/Device/ST/STM32F0xx/Include
    ${FNTESTER_PCB_CUBE_DIR}/Drivers/CMSIS/Include
)
# endregion

# Put here your symbols (preprocessor defines), one in each line
# Encapsulate them with double quotes for safety purpose
set(FW_DEFINES
    $<$<CONFIG:DEBUG>:"DEBUG">
    $<$<CONFIG:RELEASE>:"NDEBUG">
    $<$<CONFIG:RelWithDebInfo>:"FORCE_LOG">
    "USE_HAL_DRIVER"
    "STM32F072xB"
)

set(CPU_PARAMETERS -mthumb
    # STM32F072 - Cortex-M0, без FPU
    -mcpu=cortex-m0
    -mfloat-abi=soft
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

# Linker script генерируется CubeMX в корне каталога цели
file(GLOB FNTESTER_PCB_LINKER_SCRIPTS ${FNTESTER_PCB_CUBE_DIR}/*.ld)
list(LENGTH FNTESTER_PCB_LINKER_SCRIPTS FNTESTER_PCB_LINKER_SCRIPTS_COUNT)
if(FNTESTER_PCB_LINKER_SCRIPTS_COUNT GREATER 0)
    list(GET FNTESTER_PCB_LINKER_SCRIPTS 0 FNTESTER_PCB_LINKER_SCRIPT)
else()
    set(FNTESTER_PCB_LINKER_SCRIPT "")
endif()

set(FNTESTER_PCB_LINKER_OPTION
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