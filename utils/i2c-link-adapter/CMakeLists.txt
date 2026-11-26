# Утилита i2c-link-adapter (хост, не входит в прошивку).
#
# Мост между UNIX-сокетом и CDC-портом платы. Собирается только для
# desktop (gcc); на кросс-сборке под arm-none-eabi не нужна.

include(${PROJECT_SOURCE_DIR}/libs/libs.cmake)

add_executable(i2c-link-adapter
    main.c
    cli.c
    cdc_port.c
    client_server.c
    byte_queue.c
    session.c
    semantic.c
    ${PROJECT_SOURCE_DIR}/app/proto/frame.c
    ${PROJECT_SOURCE_DIR}/app/proto/link.c
    ${COBS_SOURCES}
    ${RING_BUFFER_SOURCES}
    ${LOG_DEBUG_SOURCES}
)

target_include_directories(i2c-link-adapter
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}
    ${COBS_INCLUDES}
    ${RING_BUFFER_INCLUDES}
    ${LOG_DEBUG_INCLUDES}
)

# Логи нужны всегда: это консольная утилита, а не прошивка.
target_compile_definitions(i2c-link-adapter PRIVATE FORCE_LOG)

target_compile_options(i2c-link-adapter PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# pgrep/clock_gettime — POSIX 2008.
target_link_libraries(i2c-link-adapter PRIVATE rt)