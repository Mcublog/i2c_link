set(LOG_DEBUG_INCLUDES
    ${PROJECT_SOURCE_DIR}/libs/debug
)
file(GLOB_RECURSE LOG_DEBUG_SOURCES
    ${PROJECT_SOURCE_DIR}/libs/debug/log_libs.c
)

set(RING_BUFFER_INCLUDES
    ${PROJECT_SOURCE_DIR}/libs/Ring-Buffer
)

file(GLOB_RECURSE RING_BUFFER_SOURCES
    ${PROJECT_SOURCE_DIR}/libs/Ring-Buffer/ringbuffer.c
)

set(SOFT_TIMER_INCLUDES
    ${PROJECT_SOURCE_DIR}/libs/SoftwareTimer
)

file(GLOB_RECURSE SOFT_TIMER_SOURCES
    ${PROJECT_SOURCE_DIR}/libs/SoftwareTimer/SoftTimers.c
)

file(GLOB_RECURSE DWT_SOURCES
    ${PROJECT_SOURCE_DIR}/libs/hw/dwt/dwt.c
)

set(COBS_INCLUDES
    ${PROJECT_SOURCE_DIR}/libs/cobs-c
)
file(GLOB_RECURSE COBS_SOURCES
    ${PROJECT_SOURCE_DIR}/libs/cobs-c/cobsr.c
    # ${PROJECT_SOURCE_DIR}/libs/cobs-c/cobs.c
)

set(RINGFS_INCLUDES
    ${PROJECT_SOURCE_DIR}/libs/ringfs
    ${PROJECT_SOURCE_DIR}/libs/ringfs/tests
)
file(GLOB_RECURSE RINGFS_SOURCES
    ${PROJECT_SOURCE_DIR}/libs/ringfs/ringfs.c
    ${PROJECT_SOURCE_DIR}/libs/ringfs/tests/flashsim.c
)

set(SERDEV_INCLUDES
    ${PROJECT_SOURCE_DIR}/libs/serdev/src
)
file(GLOB_RECURSE SERDEV_SOURCES
    ${PROJECT_SOURCE_DIR}/libs/serdev/src/*.c
    ${PROJECT_SOURCE_DIR}/libs/serdev/src/*.cpp
)