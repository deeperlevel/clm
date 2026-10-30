cmake_minimum_required(VERSION 3.16)
project(main LANGUAGES C)

add_executable(main main.c)
target_compile_features(main PRIVATE c_std_11)
target_compile_definitions(main PRIVATE _POSIX_C_SOURCE=200809L)

# Both examples refer to the one authoritative source tree. No CLM headers
# need to be copied into this application directory.
get_filename_component(CLM_SOURCE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../../../src/c" ABSOLUTE)
target_include_directories(main PRIVATE "${CLM_SOURCE_ROOT}")

find_package(Threads REQUIRED)
target_link_libraries(main PRIVATE Threads::Threads)
if(WIN32)
    target_link_libraries(main PRIVATE ws2_32)
endif()
