cmake_minimum_required(VERSION 4.1)
project(service C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/nvapi)

add_library(service STATIC
        library.c
        main.c
        metrics.c
        metrics.h
        serial.c
        serial.h
)

if(WIN32)
    target_link_libraries(service PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/nvapi/nvapi.lib")
endif()
