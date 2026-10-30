
cmake_minimum_required(VERSION 3.16)

project(main LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(main main.c)

# Point to the CLM C source root.
target_include_directories(main PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/c"
)

# Threading support where required by CLM.
find_package(Threads REQUIRED)

target_link_libraries(main PRIVATE
    Threads::Threads
)

# Windows socket library.
if(WIN32)
    target_link_libraries(main PRIVATE ws2_32)
endif()
