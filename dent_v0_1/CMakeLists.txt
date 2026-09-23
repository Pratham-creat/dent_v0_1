cmake_minimum_required(VERSION 3.20)

project(dent VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(dent_core

    src/model/problem.cpp

    src/matrix/sparse_matrix.cpp

    src/solver/simplex.cpp
    src/solver/milp.cpp
    src/solver/qp.cpp

    src/io/model_parser.cpp
    src/io/interactive_builder.cpp
)

target_include_directories(
    dent_core
    PUBLIC
        include
)

add_executable(
    dent
    src/main.cpp
)

target_link_libraries(
    dent
    PRIVATE
        dent_core
)