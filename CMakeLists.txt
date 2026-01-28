cmake_minimum_required(VERSION 4.1)
project(service C)

set(CMAKE_C_STANDARD 11)

add_library(service STATIC library.c
        main.c
        metrics.c
        metrics.h
        serial.h
        serial.c)
