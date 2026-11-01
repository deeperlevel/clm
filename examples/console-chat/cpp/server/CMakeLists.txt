cmake_minimum_required(VERSION 4.3)
project(main LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(main main.cpp)

target_include_directories(main PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src/cpp"
)

find_package(Threads REQUIRED)

target_link_libraries(main PRIVATE
    Threads::Threads
)

if(WIN32)
    target_link_libraries(main PRIVATE ws2_32)
endif()
