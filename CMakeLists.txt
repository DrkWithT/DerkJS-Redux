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
    src/derkjs_impl/frontend/semantics.ixx
    src/derkjs_impl/backend/ir.ixx
    src/derkjs_impl/backend/ir_generate.ixx
)

add_executable(derkjs src/main.cpp)
target_link_directories(derkjs PUBLIC ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
target_link_libraries(derkjs PUBLIC derkjs_impl)

# enable_testing()
# add_subdirectory(tests)
