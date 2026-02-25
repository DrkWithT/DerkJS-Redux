cmake_minimum_required(VERSION 4.2.0)

project(derkjs)

add_library(derkjs_impl)
target_sources(
    derkjs_impl
    PUBLIC FILE_SET derkjs_impl_modules
    TYPE CXX_MODULES
    FILES src/derkjs_impl/derkjs.ixx
    src/derkjs_impl/frontend/lexicals.ixx
    src/derkjs_impl/frontend/ast.ixx
    src/derkjs_impl/frontend/parse.ixx
    src/derkjs_impl/backend/bc_generate.ixx
    src/derkjs_impl/backend/expr_gen.ixx
    src/derkjs_impl/backend/stmt_gen.ixx
    src/derkjs_impl/core/driver.ixx
    src/derkjs_impl/meta/enums.ixx
    src/derkjs_impl/runtime/objects.ixx
    src/derkjs_impl/runtime/boolean.ixx
    src/derkjs_impl/runtime/number.ixx
    src/derkjs_impl/runtime/arrays.ixx
    src/derkjs_impl/runtime/callables.ixx
    src/derkjs_impl/runtime/strings.ixx
    src/derkjs_impl/runtime/value.ixx
    src/derkjs_impl/runtime/bytecode.ixx
    src/derkjs_impl/runtime/gc.ixx
    src/derkjs_impl/runtime/vm.ixx
    src/derkjs_impl/runtime/natives.ixx
)

add_executable(derkjs_tco src/main_tco.cpp)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(WARNING "CMAKE_BUILD_TYPE was set to 'Debug'- TCO will not apply & the VM can stack overflow.")
    target_link_options(derkjs_impl PRIVATE "-fsanitize=address")
else ()
    set_property(SOURCE src/derkjs_impl/runtime/vm.ixx APPEND PROPERTY COMPILE_OPTIONS "-foptimize-sibling-calls")
endif ()

target_link_directories(derkjs_tco PUBLIC ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} PUBLIC ${MY_LLVM_LIBCXX_DIR})
target_link_libraries(derkjs_tco PUBLIC derkjs_impl)
message(NOTICE "Executable: ./build/derkjs_tco")

# enable_testing()
# add_subdirectory(tests)
