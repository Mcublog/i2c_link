include(${PROJECT_SOURCE_DIR}/test/build.cmake)

# Using desktop gcc compiler
set(CMAKE_C_COMPILER    gcc)
set(CMAKE_CXX_COMPILER  g++)

test_target_with_main_entry("utest_adapter" "" "" "")

# Тест запускает собранную утилиту как отдельный процесс
target_compile_definitions(utest_adapter PRIVATE
    ADAPTER_BIN="$<TARGET_FILE:i2c-link-adapter>"
)
add_dependencies(utest_adapter i2c-link-adapter)