set(SUB_CONV_CORE_SOURCES
    base64.c
    converter.c
    conv_base64.c
    conv_singbox.c
    jsonx.c
    model.c
    node_emit.c
    util.c
    version.c
    yaml_emit.c
)

add_library(subconv_core STATIC ${SUB_CONV_CORE_SOURCES})
target_include_directories(subconv_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONC REQUIRED IMPORTED_TARGET json-c)
target_link_libraries(subconv_core PUBLIC PkgConfig::JSONC)

pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
pkg_check_modules(MHD REQUIRED IMPORTED_TARGET libmicrohttpd)

add_library(subconv_http STATIC httpc.c server.c)
target_include_directories(subconv_http PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(subconv_http PUBLIC subconv_core PkgConfig::CURL PkgConfig::MHD)

add_executable(subconvd main.c)
target_link_libraries(subconvd PRIVATE subconv_http)
target_compile_options(subconvd PRIVATE -Wall -Wextra)
