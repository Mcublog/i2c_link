include(${PROJECT_SOURCE_DIR}/targets/fntester_pcb/mcu_f0.cmake)
include(${PROJECT_SOURCE_DIR}/libs/libs.cmake)
include(${PROJECT_SOURCE_DIR}/libs/hw/third_party/third_party.cmake)
include(${PROJECT_SOURCE_DIR}/arch/arch.cmake)
include(${PROJECT_SOURCE_DIR}/bsp/bsp.cmake)

if(NOT EXISTS ${PROJECT_SOURCE_DIR}/libs/tinyusb/src/tusb.c)
    message(FATAL_ERROR
        "Сабмодуль TinyUSB не инициализирован. Выполните: "
        "git submodule update --init --recursive")
endif()

function(fw_application_extra_opt targetname extra_opt)
    set(FW_DEFINES
        ${FW_DEFINES}
        ${extra_opt}
    )

    # Исходники CubeMX (Core, Drivers, USB_DEVICE, startup) + пользовательские
    file(GLOB_RECURSE TARGET_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/*.s
        ${CMAKE_CURRENT_SOURCE_DIR}/*.S
    )

    # Startup-файлы из MDK-ARM/EWARM дублируют корневой startup
    list(FILTER TARGET_SOURCES EXCLUDE REGEX "/MDK-ARM/")
    list(FILTER TARGET_SOURCES EXCLUDE REGEX "/EWARM/")

    file(GLOB_RECURSE APP_SOURCES
        ${PROJECT_SOURCE_DIR}/app/*.c
        ${PROJECT_SOURCE_DIR}/app/*.cpp
    )

    set(HW_TARGET ${targetname})

    # Build lib
    add_executable(${HW_TARGET}
        ${TARGET_SOURCES}
        ${ARCH_F0_SOURCES}
        ${BSP_FNTESTER_PCB_SOURCES}
        ${LOG_DEBUG_SOURCES}
        ${APP_SOURCES}
        ${RTT_LIBRARY_SOURCES}
        ${RING_BUFFER_SOURCES}
        ${SOFT_TIMER_SOURCES}
        ${COBS_SOURCES}
        ${RINGFS_SOURCES}
        ${TINYUSB_SOURCES}
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
        ${ARCH_F0_INCLUDES}
        ${BSP_FNTESTER_PCB_INCLUDES}
        ${FNTESTER_PCB_HAL_INCLUDE_DIR}
        ${RINGFS_INCLUDES}
        ${COBS_INCLUDES}
        ${RING_BUFFER_INCLUDES}
        ${SERDEV_INCLUDES}
        ${TINYUSB_INCLUDES}
    )

    # Project symbols
    target_compile_definitions(${HW_TARGET} PRIVATE ${FW_DEFINES})

    # Compiler options
    target_compile_options(${HW_TARGET} PRIVATE ${ARM_NONE_EABI_FLAGS})

    # Linker options
    if(FNTESTER_PCB_LINKER_SCRIPT)
        target_link_options(${HW_TARGET} PRIVATE
            -T${FNTESTER_PCB_LINKER_SCRIPT}
            ${FNTESTER_PCB_LINKER_OPTION})
    else()
        message(WARNING "fntester_pcb: линкер-скрипт (*.ld) не найден в "
                        "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    # Convert output to hex and binary
    add_custom_command(TARGET ${HW_TARGET}
        POST_BUILD
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${CMAKE_OBJCOPY} ARGS -O ihex -I elf32-littlearm ${HW_TARGET} ${HW_TARGET}.hex
        COMMAND ${CMAKE_OBJCOPY} ARGS -O binary -I elf32-littlearm ${HW_TARGET} ${HW_TARGET}.bin
    )

    build_jlink_script(${HW_TARGET} STM32F072CB)
endfunction(fw_application_extra_opt)

function(fw_application targetname)
    fw_application_extra_opt(${targetname} "")
endfunction(fw_application)