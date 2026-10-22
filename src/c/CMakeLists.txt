cmake_minimum_required(VERSION 4.3)
project(main C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(main
    console.h
    console_msvc.h
    console_gcc.h
    main.c
)
