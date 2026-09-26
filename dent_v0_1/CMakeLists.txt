cmake_minimum_required(VERSION 3.20)

project(dent VERSION 0.7.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


add_library(
    dent_core

    src/model/problem.cpp

    src/matrix/sparse_matrix.cpp

    src/presolve/presolve.cpp
    src/presolve/scaling.cpp

    src/dispatch/fingerprint.cpp
    src/dispatch/dispatcher.cpp

    src/solver/simplex.cpp
    src/solver/dual_simplex.cpp
    src/solver/interior_point.cpp
    src/solver/milp.cpp
    src/solver/qp.cpp
    src/solver/solver_session.cpp

    src/warmstart/warm_start.cpp

    src/io/model_parser.cpp
    src/io/interactive_builder.cpp
)


target_include_directories(
    dent_core

    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
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


enable_testing()


add_executable(
    dent_solver_industry_test
    tests/solver/solver_industry_test.cpp
)


target_link_libraries(
    dent_solver_industry_test
    PRIVATE
        dent_core
)


add_test(
    NAME dent_solver_industry_test
    COMMAND dent_solver_industry_test
)


add_executable(
    dent_interior_point_test
    tests/solver/interior_point_test.cpp
)


target_link_libraries(
    dent_interior_point_test
    PRIVATE
        dent_core
)


add_test(
    NAME dent_interior_point_test
    COMMAND dent_interior_point_test
)


add_executable(
    dent_model_parser_test
    tests/io/model_parser_test.cpp
)


target_link_libraries(
    dent_model_parser_test
    PRIVATE
        dent_core
)


add_test(
    NAME dent_model_parser_test
    COMMAND dent_model_parser_test
)


add_executable(
    dent_scaling_test
    tests/presolve/scaling_test.cpp
)


target_link_libraries(
    dent_scaling_test
    PRIVATE
        dent_core
)


add_test(
    NAME dent_scaling_test
    COMMAND dent_scaling_test
)


add_executable(
    dent_milp_branching_test
    tests/solver/milp_branching_test.cpp
)


target_link_libraries(
    dent_milp_branching_test
    PRIVATE
        dent_core
)


add_test(
    NAME dent_milp_branching_test
    COMMAND dent_milp_branching_test
)