cmake_minimum_required(VERSION 4.2.0)

project(derkjs)

set(USE_TCO_WITH_BC_VM, OFF, CACHE BOOL "The final safeguard before the compiler tries to use TCO in a separate kind of interpreter build: derkjs_tco. Use at your own risk... Only Clang 20 is allowed.")

add_library(derkjs_impl)
target_sources(
    derkjs_impl
    PUBLIC FILE_SET derkjs_impl_modules
    TYPE CXX_MODULES
    FILES src/derkjs_impl/derkjs.ixx
    src/derkjs_impl/frontend/lexicals.ixx
    src/derkjs_impl/frontend/ast.ixx
    src/derkjs_impl/frontend/parse.ixx
    src/derkjs_impl/runtime/objects.ixx
    src/derkjs_impl/runtime/arrays.ixx
    src/derkjs_impl/runtime/callables.ixx
    src/derkjs_impl/runtime/strings.ixx
    src/derkjs_impl/runtime/value.ixx
    src/derkjs_impl/runtime/bytecode.ixx
    src/derkjs_impl/backend/bc_generate.ixx
    src/derkjs_impl/runtime/vm.ixx
    src/derkjs_impl/runtime/natives.ixx
    src/derkjs_impl/core/driver.ixx
)

if (USE_TCO_WITH_BC_VM)
    add_executable(derkjs_tco src/main_tco.cpp)

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(WARNING "CMAKE_BUILD_TYPE was set to 'Debug'- TCO will not apply & the VM can stack overflow.")
        target_link_options(derkjs_tco PRIVATE "-fsanitize=address")
    else ()
        target_compile_options(derkjs_tco PRIVATE "-foptimize-sibling-calls")
    endif ()

    target_link_directories(derkjs_tco PUBLIC ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} PUBLIC ${MY_LLVM_LIBCXX_DIR})
    target_link_libraries(derkjs_tco PUBLIC derkjs_impl)

    message(NOTICE "Executable: ./build/derkjs_tco")
else ()
    add_executable(derkjs src/main.cpp)
    target_link_directories(derkjs PUBLIC ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} PUBLIC ${MY_LLVM_LIBCXX_DIR})
    target_link_libraries(derkjs PUBLIC derkjs_impl)

    message(NOTICE "Executable: ./build/derkjs")
endif ()


# enable_testing()
# add_subdirectory(tests)
