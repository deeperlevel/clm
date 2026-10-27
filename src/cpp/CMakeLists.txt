cmake_minimum_required(VERSION 4.3)
project(main LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

include_directories(.)

add_executable(main
        main.cpp
    tests/test_clm_base.hpp
    tests/test_clm_bpa.hpp
    tests/test_clm_console.hpp
    tests/test_clm_dictionary.hpp
    tests/test_clm_id.hpp
    tests/test_clm_storage.hpp
    tests/test_clm_string.hpp
    tests/test.hpp
)
