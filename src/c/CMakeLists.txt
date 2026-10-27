cmake_minimum_required(VERSION 4.3)
project(main C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(main
    clm/console.h
    main.c
    clm/base.h
    clm/bpa.h
    clm/dictionary.h
    clm/id.h
    clm/string.h
    tests/test.h
    tests/test_clm_bpa.h
    tests/test_clm_dictionary.h
    tests/test_clm_id.h
    tests/test_clm_console.h
    tests/test_clm_string.h
)
