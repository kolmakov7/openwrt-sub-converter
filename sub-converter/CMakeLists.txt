cmake_minimum_required(VERSION 3.16)
project(openwrt-sub-converter C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra)

option(SUB_CONVERTER_BUILD_SERVER "Build the HTTP server components (libmicrohttpd/libcurl/json-c/libyaml)" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

install(TARGETS subconvd RUNTIME DESTINATION usr/sbin)
