cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 23)

project(examples)

string(FIND ${CMAKE_CXX_COMPILER_ID} "Clang" IS_CLANG)

add_executable(example_text text/example_text.cpp)
target_include_directories(example_text PRIVATE ${PROJECT_SOURCE_DIR}/..)
add_executable(example_graph graph/example_graph.cpp)
target_include_directories(example_graph PRIVATE ${PROJECT_SOURCE_DIR}/..)
add_executable(example_embedded embedded/example_embedded.cpp)
target_include_directories(example_embedded PRIVATE ${PROJECT_SOURCE_DIR}/..)

set(CLANG_OPTS -Os -fconstexpr-steps=33554432 -Wall -Wextra -Wpedantic -Weffc++)
set(GCC_OPTS -Os -fconstexpr-ops-limit=268435456 -Wall -Wextra -Wpedantic -Weffc++)
set(CL_OPTS /EHsc /constexpr:steps 268435456)

if(NOT ${IS_CLANG} EQUAL -1)
    target_compile_options(example_text PRIVATE ${CLANG_OPTS})
    target_compile_options(example_graph PRIVATE ${CLANG_OPTS})
    target_compile_options(example_graph PRIVATE ${CLANG_OPTS})
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(example_text PRIVATE ${GCC_OPTS})
    target_compile_options(example_graph PRIVATE ${GCC_OPTS})
    target_compile_options(example_embedded PRIVATE ${GCC_OPTS})
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(example_text PRIVATE ${CL_OPTS})
    target_compile_options(example_graph PRIVATE ${CL_OPTS})
    target_compile_options(example_embedded PRIVATE ${CL_OPTS})
else()
    message("Unknown compiler!")
endif()
