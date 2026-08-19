enable_testing()

find_package(Threads REQUIRED)

function(subconv_add_unit_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE subconv_core)
  target_compile_options(${name} PRIVATE -Wall -Wextra)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

function(subconv_add_unity_test name)
  add_executable(${name} ${ARGN} ${CMAKE_CURRENT_SOURCE_DIR}/vendor/unity/unity.c)
  target_link_libraries(${name} PRIVATE subconv_core)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/unity)
  target_compile_options(${name} PRIVATE -Wall -Wextra)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

function(subconv_add_http_test name)
  add_executable(${name} ${ARGN} ${CMAKE_CURRENT_SOURCE_DIR}/vendor/unity/unity.c)
  target_link_libraries(${name} PRIVATE subconv_http Threads::Threads)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/unity)
  target_compile_options(${name} PRIVATE -Wall -Wextra)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

subconv_add_unit_test(test_smoke unit/test_smoke.c)
subconv_add_unity_test(test_base64 unit/test_base64.c)
subconv_add_unity_test(test_jsonx unit/test_jsonx.c)
subconv_add_unity_test(test_yaml_quote unit/test_yaml_quote.c)
subconv_add_unity_test(test_yaml_emit unit/test_yaml_emit.c)
subconv_add_unity_test(test_model unit/test_model.c)
subconv_add_unity_test(test_conv_singbox unit/test_conv_singbox.c)
target_compile_definitions(test_conv_singbox PRIVATE SUB_CONV_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
subconv_add_unity_test(test_conv_base64 unit/test_conv_base64.c)
target_compile_definitions(test_conv_base64 PRIVATE SUB_CONV_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
subconv_add_http_test(test_httpc unit/test_httpc.c)
subconv_add_http_test(test_server unit/test_server.c)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MHD REQUIRED IMPORTED_TARGET libmicrohttpd)

add_executable(mock_upstream integration/mock_upstream.c)
target_link_libraries(mock_upstream PRIVATE PkgConfig::MHD)
target_compile_options(mock_upstream PRIVATE -Wall -Wextra)

add_executable(harness integration/harness.c)
target_link_libraries(harness PRIVATE subconv_http Threads::Threads)
target_compile_options(harness PRIVATE -Wall -Wextra)

add_test(NAME integration
  COMMAND harness
    $<TARGET_FILE:subconvd>
    $<TARGET_FILE:mock_upstream>
    ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/integration/sub.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/singbox/example.json
    ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/expected/singbox.yaml)
