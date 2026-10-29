cmake_minimum_required(VERSION 4.3)
project(main C)

set(CMAKE_C_STANDARD 11)

include(FetchContent)
set(URIPARSER_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(URIPARSER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(URIPARSER_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(URIPARSER_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(URIPARSER_BUILD_WCHAR_T OFF CACHE BOOL "" FORCE)
set(URIPARSER_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(uriparser
    URL https://codeload.github.com/uriparser/uriparser/tar.gz/refs/tags/uriparser-0.9.9
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(uriparser)

include_directories(.)

add_executable(main
    clm/console.h
    main.c
    clm/base.h
    clm/bpa.h
    clm/dictionary.h
    clm/tree.h
    clm/table.h
    clm/string_format.h
    clm/string_dsv.h
    clm/string_ini.h
    clm/string_env.h
    clm/string_toml.h
    clm/string_json.h
    clm/string_yaml.h
    clm/gen.h
    clm/id.h
    clm/string.h
    tests/test.h
    tests/test_clm_bpa.h
    tests/test_clm_dictionary.h
    tests/test_clm_tree.h
    tests/test_clm_table.h
    tests/test_clm_string_format.h
    tests/test_clm_string_dsv.h
    tests/test_clm_string_ini.h
    tests/test_clm_string_env.h
    tests/test_clm_string_toml.h
    tests/test_clm_string_json.h
    tests/test_clm_string_yaml.h
    tests/test_clm_gen.h
    tests/test_clm_id.h
    tests/test_clm_console.h
    tests/test_clm_string.h
    clm/path.h
    clm/url.h
    tests/test_clm_path.h
    tests/test_clm_url.h
    clm/file.h
    clm/directory.h
    tests/test_clm_file.h
    tests/test_clm_directory.h
)

target_link_libraries(main PRIVATE uriparser::uriparser)
include(CTest)
add_test(NAME clm COMMAND main)
if(WIN32)
    target_link_libraries(main PRIVATE bcrypt normaliz)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(IDN2 REQUIRED IMPORTED_TARGET libidn2)
    target_link_libraries(main PRIVATE PkgConfig::IDN2)
    target_compile_definitions(main PRIVATE _XOPEN_SOURCE=700 _GNU_SOURCE _FILE_OFFSET_BITS=64)
endif()
