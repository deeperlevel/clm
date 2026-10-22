cmake_minimum_required(VERSION 4.3)
project(main)

set(CMAKE_CXX_STANDARD 23)

include_directories(.)

add_executable(main
    size.hpp
    position.hpp
    console.hpp
    console_gcc.hpp
    console_msvc.hpp
    main.cpp
)
