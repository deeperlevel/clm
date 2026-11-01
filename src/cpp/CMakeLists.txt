cmake_minimum_required(VERSION 4.3)
project(main LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(.)

enable_testing()

add_executable(main
        main.cpp
    tests/test_clm_base.hpp
    tests/test_clm_bpa.hpp
    tests/test_clm_console.hpp
    tests/test_clm_dictionary.hpp
    tests/test_clm_id.hpp
    tests/test_clm_storage.hpp
    tests/test_clm_string.hpp
    tests/test_clm_clp.hpp
    tests/test_clm_channel.hpp
    tests/test_clm_async.hpp
    tests/test_clm_path.hpp
    tests/test_clm_container.hpp
    tests/test.hpp
)

add_test(NAME clm_tests COMMAND main)
