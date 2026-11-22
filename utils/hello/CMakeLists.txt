# Пример хост-утилиты (не входит в прошивку).
# Собирается только для desktop (gcc/g++).
set(CMAKE_C_COMPILER    gcc)
set(CMAKE_CXX_COMPILER  g++)

add_executable(hello hello.cpp)
