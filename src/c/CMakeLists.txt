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
    clm/arena.h
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
    clm/string_httpp.h
    clm/bytes.h
    clm/stream.h
    clm/template.h
    clm/compress.h
    clm/multipart.h
    clm/tls.h
    clm/net_web.h
    clm/async.h
    clm/async_pool.h
    clm/container.h
    clm/channel.h
    clm/dispatch.h
    clm/ecs.h
    clm/managed.h
    clm/math_core.h
    clm/math.h
    clm/math_number.h
    clm/math_random.h
    clm/math_vector.h
    clm/math_matrix.h
    clm/math_interpolation.h
    clm/math_geometry.h
    clm/math_quaternion.h
    clm/math_collision.h
    clm/math_physics2d.h
    clm/math_pathfinding.h
    clm/clp.h
    clm/ua.h
    clm/gen.h
    clm/id.h
    clm/string.h
    clm/ext/freehash.h
    clm/hash.h
    clm/crypto.h
    clm/ext/monocypher.h
    tests/test.h
    tests/test_clm_bpa.h
    tests/test_clm_arena.h
    tests/test_clm_hash.h
    tests/test_clm_crypto.h
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
    tests/test_clm_string_httpp.h
    tests/test_clm_bytes.h
    tests/test_clm_stream.h
    tests/test_clm_template.h
    tests/test_clm_compress.h
    tests/test_clm_multipart.h
    tests/test_clm_tls.h
    tests/test_clm_net_web.h
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
    tests/test_clm_async.h
    tests/test_clm_container.h
    tests/test_clm_channel.h
    tests/test_clm_dispatch.h
    tests/test_clm_ecs.h
    tests/test_clm_managed.h
    tests/test_clm_math.h
    tests/test_clm_clp.h
    tests/test_clm_ua.h
    tests/test_clm_net.h
)

find_package(Threads REQUIRED)
file(GLOB CLM_C_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/clm/*.c")

# Vendored compressors. Brotli's headers are included as <brotli/...> but ship flat.
set(CLM_VENDOR_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/vendor_include")
file(GLOB CLM_BROTLI_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/clm/ext/brotli/include/*.h")
foreach(header ${CLM_BROTLI_HEADERS})
    get_filename_component(header_name "${header}" NAME)
    configure_file("${header}" "${CLM_VENDOR_INCLUDE}/brotli/${header_name}" COPYONLY)
endforeach()
file(GLOB CLM_BROTLI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/clm/ext/brotli/common/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/clm/ext/brotli/dec/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/clm/ext/brotli/enc/*.c")
set(CLM_VENDOR_SOURCES
    clm/ext/lz4/lz4.c
    clm/ext/lz4/lz4hc.c
    clm/ext/lz4/lz4frame.c
    clm/ext/lz4/xxhash.c
    clm/ext/miniz/miniz.c
    clm/ext/miniz/miniz_tdef.c
    clm/ext/miniz/miniz_tinfl.c
    clm/ext/miniz/miniz_zip.c
    ${CLM_BROTLI_SOURCES})

add_library(clm STATIC ${CLM_C_SOURCES} clm/ext/freehash.c ${CLM_VENDOR_SOURCES})
target_include_directories(clm PRIVATE "${CLM_VENDOR_INCLUDE}")
target_compile_definitions(clm PRIVATE MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
target_include_directories(clm PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(clm PUBLIC uriparser::uriparser Threads::Threads)

target_link_libraries(main PRIVATE clm)
include(CTest)
add_test(NAME clm COMMAND main)
if(WIN32)
    target_link_libraries(clm PUBLIC bcrypt normaliz secur32 crypt32)
else()
    find_package(OpenSSL REQUIRED)
    target_link_libraries(clm PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(IDN2 REQUIRED IMPORTED_TARGET libidn2)
    target_link_libraries(clm PUBLIC PkgConfig::IDN2 m)
    target_compile_definitions(clm PUBLIC _XOPEN_SOURCE=700 _GNU_SOURCE _FILE_OFFSET_BITS=64)
endif()
