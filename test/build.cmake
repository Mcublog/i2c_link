include(${PROJECT_SOURCE_DIR}/libs/libs.cmake)
include(${PROJECT_SOURCE_DIR}/libs/hw/third_party/third_party.cmake)
include(${PROJECT_SOURCE_DIR}/arch/arch.cmake)
include(${PROJECT_SOURCE_DIR}/bsp/bsp.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/desktop.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/common.cmake)

function(test_target_with_main_entry targetname defines_ext includes_ext sources_ext)
    file(GLOB_RECURSE TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    )

    set(TEST_TARGET ${targetname})

    # Aggregate target for unit tests only (utest_*)
    get_property(utests GLOBAL PROPERTY UTEST_TARGETS)
    if(NOT utests)
        add_custom_target(utest_all COMMENT "Building all unit tests")
    endif()

    # Build lib
    add_executable(${TEST_TARGET}
        ${TEST_SOURCES}
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
    target_include_directories(${TEST_TARGET}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}
        ${APP_DIRS}
        ${LOG_DEBUG_INCLUDES}
        ${PROJECT_SOURCE_DIR}/libs
        ${PROJECT_SOURCE_DIR}/libs/debug
        ${ARCH_DESKTOP_INCLUDES}
        ${RINGFS_INCLUDES}
        ${COBS_INCLUDES}
        ${RING_BUFFER_INCLUDES}
        ${SERDEV_INCLUDES}
    )

    # Project symbols
    target_compile_definitions(${TEST_TARGET} PRIVATE ${EMU_DEFINES})

    if(NOT (targetname STREQUAL "dbg_sandbox"))
        add_test(NAME ${TEST_TARGET} COMMAND ${TEST_TARGET})
    endif()

    if(TEST_TARGET MATCHES "^utest_")
        set_property(GLOBAL APPEND PROPERTY UTEST_TARGETS ${TEST_TARGET})
        add_dependencies(utest_all ${TEST_TARGET})
    endif()

endfunction(test_target_with_main_entry)

