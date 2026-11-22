include(${PROJECT_SOURCE_DIR}/libs/libs.cmake)
include(${PROJECT_SOURCE_DIR}/libs/hw/third_party/third_party.cmake)
include(${PROJECT_SOURCE_DIR}/arch/arch.cmake)
include(${PROJECT_SOURCE_DIR}/bsp/bsp.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/desktop.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/common.cmake)

function(emu_build targetname)
    file(GLOB_RECURSE EMU_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    )

    # Build lib
    add_executable(${targetname}
        ${EMU_SOURCES}
        ${LOG_DEBUG_SOURCES}
        ${APP_SOURCES}
        ${RING_BUFFER_SOURCES}
        ${SOFT_TIMER_SOURCES}
        ${COBS_SOURCES}
        ${ARCH_DESKTOP_SOURCES}
        ${BSP_DESKTOP_SOURCES}
        ${RINGFS_SOURCES}
        ${SERDEV_SOURCES}
    )

    # Include paths
    target_include_directories(${targetname}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}
        ${APP_DIRS}
        ${LOG_DEBUG_INCLUDES}
        ${PROJECT_SOURCE_DIR}/libs
        ${PROJECT_SOURCE_DIR}/libs/debug
        ${ARCH_DESKTOP_INCLUDES}
        ${RINGFS_INCLUDES}
        ${SERDEV_INCLUDES}
    )

    # Project symbols
    target_compile_definitions(${targetname} PRIVATE ${EMU_DEFINES})

endfunction()

