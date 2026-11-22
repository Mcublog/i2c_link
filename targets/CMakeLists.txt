add_subdirectory(emu)

# ws407 прошивка собирается только при наличии arm-none-eabi компилятора
if(CMAKE_C_COMPILER MATCHES "arm-none-eabi")
    add_subdirectory(ws407)
endif()