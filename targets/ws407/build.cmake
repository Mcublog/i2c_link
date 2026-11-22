include(${PROJECT_SOURCE_DIR}/targets/ws407/mcu_f4.cmake)
include(${PROJECT_SOURCE_DIR}/libs/libs.cmake)
include(${PROJECT_SOURCE_DIR}/libs/hw/third_party/third_party.cmake)
include(${PROJECT_SOURCE_DIR}/arch/arch.cmake)
include(${PROJECT_SOURCE_DIR}/bsp/bsp.cmake)

function(fw_application_extra_opt targetname extra_opt)
    set(FUNTIK_FW_DEFINES
        ${FUNTIK_FW_DEFINES}
        ${extra_opt}
    )

    file(GLOB_RECURSE TARGET_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/startup_stm32f407xx.s
    )

    file(GLOB_RECURSE APP_SOURCES
        ${PROJECT_SOURCE_DIR}/app/*.c
        ${PROJECT_SOURCE_DIR}/app/*.cpp
    )

    set(HW_TARGET ${targetname})

    # Build lib
    add_executable(${HW_TARGET}
        ${TARGET_SOURCES}
        ${ARCH_F407_SOURCES}
        ${BSP_WS407_SOURCES}
        ${LOG_DEBUG_SOURCES}
        ${APP_SOURCES}
        ${RTT_LIBRARY_SOURCES}
        ${RING_BUFFER_SOURCES}
        ${SOFT_TIMER_SOURCES}
        ${DWT_SOURCES}
        ${COBS_SOURCES}
        ${RINGFS_SOURCES}
    )

    # Include paths
    target_include_directories(${HW_TARGET}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}
        ${APP_DIRS}
        ${LOG_DEBUG_INCLUDES}
        ${RTT_LIBRARY_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/libs
        ${PROJECT_SOURCE_DIR}/libs/debug
        ${ARCH_F407_INCLUDES}
        ${BSP_WS407_INCLUDES}
        ${WS407_FUNTIK_HAL_INCLUDE_DIR}
        ${RINGFS_INCLUDES}
        # FatFS
        ${CMAKE_CURRENT_SOURCE_DIR}/FATFS/App
        ${CMAKE_CURRENT_SOURCE_DIR}/FATFS/Target
        ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/FatFs/src
    )

    # Project symbols
    target_compile_definitions(${HW_TARGET} PRIVATE ${FUNTIK_FW_DEFINES})

    # Compiler options
    target_compile_options(${HW_TARGET} PRIVATE ${ARM_NONE_EABI_FLAGS})

    # Linker options
    target_link_options(${HW_TARGET} PRIVATE ${WS407_LINKER_OPTION})

    # # Convert output to hex and binary
    add_custom_command(TARGET ${HW_TARGET}
        POST_BUILD
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${CMAKE_OBJCOPY} ARGS -O ihex -I elf32-littlearm ${HW_TARGET} ${HW_TARGET}.hex
        COMMAND ${CMAKE_OBJCOPY} ARGS -O binary -I elf32-littlearm ${HW_TARGET} ${HW_TARGET}.bin
    )

    build_jlink_script(${HW_TARGET} STM32F407VE)
endfunction(fw_application_extra_opt)

function(fw_application targetname)
    fw_application_extra_opt(${targetname} "")
endfunction(fw_application)

