cmake_minimum_required(VERSION 4.2.0)

project(derkjs)

add_library(frontend)
target_sources(frontend
    PUBLIC FILE_SET frontend_modules
    TYPE CXX_MODULES
    FILES src/frontend/lexicals.ixx
    src/frontend/ast.ixx
    src/frontend/parse.ixx
)

add_executable(derkjs src/main.cpp)
target_link_directories(derkjs PUBLIC ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
target_link_libraries(derkjs PUBLIC frontend)

# enable_testing()
# add_subdirectory(tests)
