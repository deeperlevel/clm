cmake_minimum_required(VERSION 3.16)
project(main LANGUAGES C)

add_executable(main main.c)
target_compile_features(main PRIVATE c_std_11)
target_compile_definitions(main PRIVATE _POSIX_C_SOURCE=200809L)

# Both examples refer to the one authoritative source tree. No CLM headers
# need to be copied into this application directory.
get_filename_component(CLM_SOURCE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../../src/c" ABSOLUTE)

find_package(Threads REQUIRED)

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

file(GLOB CLM_C_SOURCES CONFIGURE_DEPENDS "${CLM_SOURCE_ROOT}/clm/*.c")
set(CLM_VENDOR_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/vendor_include")
file(GLOB CLM_BROTLI_HEADERS "${CLM_SOURCE_ROOT}/clm/ext/brotli/include/*.h")
foreach(header ${CLM_BROTLI_HEADERS})
    get_filename_component(header_name "${header}" NAME)
    configure_file("${header}" "${CLM_VENDOR_INCLUDE}/brotli/${header_name}" COPYONLY)
endforeach()
file(GLOB CLM_BROTLI_SOURCES CONFIGURE_DEPENDS
    "${CLM_SOURCE_ROOT}/clm/ext/brotli/common/*.c"
    "${CLM_SOURCE_ROOT}/clm/ext/brotli/dec/*.c"
    "${CLM_SOURCE_ROOT}/clm/ext/brotli/enc/*.c")
# Link errors for Brotli, LZ4, or miniz usually mean a vendor source is missing below.
# Brotli's public headers are flat upstream; this copy satisfies its <brotli/...> includes.
set(CLM_VENDOR_SOURCES
    "${CLM_SOURCE_ROOT}/clm/ext/lz4/lz4.c"
    "${CLM_SOURCE_ROOT}/clm/ext/lz4/lz4hc.c"
    "${CLM_SOURCE_ROOT}/clm/ext/lz4/lz4frame.c"
    "${CLM_SOURCE_ROOT}/clm/ext/lz4/xxhash.c"
    "${CLM_SOURCE_ROOT}/clm/ext/miniz/miniz.c"
    "${CLM_SOURCE_ROOT}/clm/ext/miniz/miniz_tdef.c"
    "${CLM_SOURCE_ROOT}/clm/ext/miniz/miniz_tinfl.c"
    "${CLM_SOURCE_ROOT}/clm/ext/miniz/miniz_zip.c"
    ${CLM_BROTLI_SOURCES})
add_library(clm STATIC
    ${CLM_C_SOURCES}
    "${CLM_SOURCE_ROOT}/clm/ext/freehash.c"
    ${CLM_VENDOR_SOURCES})
target_include_directories(clm PRIVATE "${CLM_VENDOR_INCLUDE}")
target_compile_definitions(clm PRIVATE MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
target_include_directories(clm PUBLIC "${CLM_SOURCE_ROOT}")
target_compile_features(clm PUBLIC c_std_11)
target_link_libraries(clm PUBLIC uriparser::uriparser Threads::Threads)

if(WIN32)
    # Schannel/certificate symbols need secur32/crypt32; Winsock symbols need ws2_32.
    target_link_libraries(clm PUBLIC bcrypt normaliz secur32 crypt32)
    target_link_libraries(main PRIVATE ws2_32)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(IDN2 REQUIRED IMPORTED_TARGET libidn2)
    target_link_libraries(clm PUBLIC PkgConfig::IDN2 m)
    target_compile_definitions(clm PUBLIC _XOPEN_SOURCE=700 _GNU_SOURCE _FILE_OFFSET_BITS=64)
endif()

target_link_libraries(main PRIVATE clm)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config")
    add_custom_command(TARGET main POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/config"
            "${CMAKE_CURRENT_BINARY_DIR}/config"
        VERBATIM)
endif()
