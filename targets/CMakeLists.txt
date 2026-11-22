add_subdirectory(emu)

# Прошивка fntester_pcb собирается только при наличии arm-none-eabi компилятора
# и сгенерированного проекта CubeMX (targets/fntester_pcb/Core/Src/main.c)
if(CMAKE_C_COMPILER MATCHES "arm-none-eabi")
    if(EXISTS ${PROJECT_SOURCE_DIR}/targets/fntester_pcb/Core/Src/main.c)
        add_subdirectory(fntester_pcb)
    else()
        LOG_WARN("targets/fntester_pcb/Core/Src/main.c не найден: "
                 "сгенерируйте проект CubeMX, прошивка пропущена")
    endif()
endif()