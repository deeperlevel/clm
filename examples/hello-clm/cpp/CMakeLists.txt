cmake_minimum_required(VERSION 3.16)
project(main LANGUAGES CXX)

add_executable(main main.cpp)
target_compile_features(main PRIVATE cxx_std_23)
target_compile_definitions(main PRIVATE _POSIX_C_SOURCE=200809L)

add_custom_command(TARGET main POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/config"
        "${CMAKE_CURRENT_BINARY_DIR}/config"
    VERBATIM)

get_filename_component(CLM_SOURCE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../../src/cpp" ABSOLUTE)
target_include_directories(main PRIVATE "${CLM_SOURCE_ROOT}")

find_package(Threads REQUIRED)
target_link_libraries(main PRIVATE Threads::Threads)
if(WIN32)
    target_link_libraries(main PRIVATE ws2_32)
endif()
