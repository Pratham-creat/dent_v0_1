cmake_minimum_required(VERSION 3.20)

project(dent VERSION 0.12.0 LANGUAGES CXX)

option(DENT_ENABLE_CUDA "Build optional CUDA backend" OFF)
option(DENT_ENABLE_SANITIZERS "Enable ASan/UBSan on supported compilers" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(DENT_ENABLE_SANITIZERS AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()


# ============================================================
# CORE LIBRARY
# ============================================================

add_library(
    dent_core
    STATIC

    src/model/problem.cpp

    src/matrix/sparse_matrix.cpp
    src/numerics/lu_factorization.cpp
    src/numerics/forrest_tomlin.cpp

    src/presolve/presolve.cpp
    src/presolve/scaling.cpp

    src/dispatch/fingerprint.cpp
    src/dispatch/dispatcher.cpp

    src/validation/solution_validator.cpp

    src/solver/simplex.cpp
    src/solver/dual_simplex.cpp
    src/solver/interior_point.cpp
    src/solver/pdhg.cpp
    src/solver/pdlp.cpp
    src/solver/milp.cpp
    src/solver/milp_cuts.cpp
    src/solver/reliability_branching.cpp
    src/solver/feasibility_pump.cpp
    src/solver/simplex_pivot.cpp
    src/solver/qp.cpp
    src/solver/miqp.cpp
    src/solver/solver_session.cpp

    src/warmstart/warm_start.cpp
    src/verification/certificate_verifier.cpp

    src/io/model_parser.cpp
    src/io/mps_parser.cpp
    src/io/lp_parser.cpp
    src/io/interactive_builder.cpp
)


target_include_directories(
    dent_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


# ============================================================
# PUBLIC C API
# ============================================================

add_library(
    dent_api
    SHARED

    src/api/c_api.cpp
)


target_include_directories(
    dent_api
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_link_libraries(
    dent_api
    PRIVATE
        dent_core
)


target_compile_definitions(
    dent_api
    PRIVATE
        DENT_BUILDING_DLL
)


set_target_properties(
    dent_api
    PROPERTIES
        OUTPUT_NAME "dent_api"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
)


# ============================================================
# MAIN DENT APPLICATION
# ============================================================

add_executable(
    dent
    src/main.cpp
)

target_link_libraries(
    dent
    PRIVATE
        dent_core
)


# ============================================================
# TESTING
# ============================================================

enable_testing()


# ------------------------------------------------------------
# Solver industry test
# ------------------------------------------------------------

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

set_tests_properties(
    dent_solver_industry_test
    PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)


# ------------------------------------------------------------
# Interior Point
# ------------------------------------------------------------

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

set_tests_properties(
    dent_interior_point_test
    PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)


# ------------------------------------------------------------
# Model parser
# ------------------------------------------------------------

add_executable(
    dent_mps_parser_test
    tests/io/mps_parser_test.cpp
)

target_link_libraries(
    dent_mps_parser_test
    PRIVATE
        dent_core
)

add_test(
    NAME dent_mps_parser_test
    COMMAND dent_mps_parser_test
)

set_tests_properties(
    dent_mps_parser_test
    PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(
    dent_lp_parser_test
    tests/io/lp_parser_test.cpp
)
target_link_libraries(dent_lp_parser_test PRIVATE dent_core)
add_test(NAME dent_lp_parser_test COMMAND dent_lp_parser_test)

add_executable(dent_advanced_numerics_test tests/numerics/advanced_numerics_test.cpp)
target_link_libraries(dent_advanced_numerics_test PRIVATE dent_core)
add_test(NAME dent_advanced_numerics_test COMMAND dent_advanced_numerics_test)

add_executable(
    dent_lu_factorization_test
    dent_advanced_numerics_test
    tests/numerics/lu_factorization_test.cpp
)
target_link_libraries(dent_lu_factorization_test PRIVATE dent_core)
add_test(NAME dent_lu_factorization_test COMMAND dent_lu_factorization_test)

add_executable(
    dent_certificate_verifier_test
    tests/verification/certificate_verifier_test.cpp
)
target_link_libraries(dent_certificate_verifier_test PRIVATE dent_core)
add_test(NAME dent_certificate_verifier_test COMMAND dent_certificate_verifier_test)
add_executable(
    dent_model_parser_test
    dent_lp_parser_test
    dent_lu_factorization_test
    dent_certificate_verifier_test
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

set_tests_properties(
    dent_model_parser_test
    PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)


# ------------------------------------------------------------
# Scaling
# ------------------------------------------------------------

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

set_tests_properties(
    dent_scaling_test
    PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)


# ------------------------------------------------------------
# MILP branching / strengthening
# ------------------------------------------------------------

add_executable(
    dent_milp_branching_test
    dent_miqp_test
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

set_tests_properties(
    dent_milp_branching_test
    PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)


# ------------------------------------------------------------
# PDHG
# ------------------------------------------------------------


add_executable(
    dent_miqp_test
    tests/solver/miqp_test.cpp
)

target_link_libraries(
    dent_miqp_test
    PRIVATE
        dent_core
)

add_test(
    NAME dent_miqp_test
    COMMAND dent_miqp_test
)

set_tests_properties(
    dent_miqp_test
    PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(
    dent_pdhg_test
    tests/solver/pdhg_test.cpp
)

target_link_libraries(
    dent_pdhg_test
    PRIVATE
        dent_core
)

add_test(
    NAME dent_pdhg_test
    COMMAND dent_pdhg_test
)

set_tests_properties(
    dent_pdhg_test
    PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)


# ------------------------------------------------------------
# PDLP
# ------------------------------------------------------------

add_executable(
    dent_pdlp_test
    tests/solver/pdlp_test.cpp
)

target_link_libraries(
    dent_pdlp_test
    PRIVATE
        dent_core
)

add_test(
    NAME dent_pdlp_test
    COMMAND dent_pdlp_test
)

set_tests_properties(
    dent_pdlp_test
    PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)


# ------------------------------------------------------------
# Adaptive dispatcher
# ------------------------------------------------------------

add_executable(
    dent_dispatcher_test
    tests/dispatch/dispatcher_test.cpp
)

target_link_libraries(
    dent_dispatcher_test
    PRIVATE
        dent_core
)

add_test(
    NAME dent_dispatcher_test
    COMMAND dent_dispatcher_test
)

set_tests_properties(
    dent_dispatcher_test
    PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)


# ------------------------------------------------------------
# Solution validator
# ------------------------------------------------------------

add_executable(
    dent_solution_validator_test
    tests/validation/solution_validator_test.cpp
)

target_link_libraries(
    dent_solution_validator_test
    PRIVATE
        dent_core
)

add_test(
    NAME dent_solution_validator_test
    COMMAND dent_solution_validator_test
)

set_tests_properties(
    dent_solution_validator_test
    PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)


# ============================================================
# BENCHMARK RUNNER
# ============================================================

add_executable(
    dent_benchmark
    tests/benchmark/benchmark_runner.cpp
)

target_link_libraries(
    dent_benchmark
    PRIVATE
        dent_core
)


# ------------------------------------------------------------
# Solver comparison
# ------------------------------------------------------------

add_executable(
    dent_solver_comparison
    tests/benchmark/solver_comparison.cpp
)

target_link_libraries(
    dent_solver_comparison
    PRIVATE
        dent_core
)

target_include_directories(
    dent_solver_comparison
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)


# ============================================================
# COPY API DLL BESIDE EXECUTABLES
# ============================================================

add_custom_command(
    TARGET dent
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:dent_api>"
        "$<TARGET_FILE_DIR:dent>"
)


foreach(test_target
    dent_solver_industry_test
    dent_interior_point_test
    dent_model_parser_test
    dent_mps_parser_test
    dent_scaling_test
    dent_milp_branching_test
    dent_pdhg_test
    dent_pdlp_test
    dent_dispatcher_test
    dent_solution_validator_test
    dent_benchmark
    dent_solver_comparison
)

    add_custom_command(
        TARGET ${test_target}
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:dent_api>"
            "$<TARGET_FILE_DIR:${test_target}>"
    )

endforeach()

if(DENT_ENABLE_CUDA)
    enable_language(CUDA)
    find_package(CUDAToolkit REQUIRED)
    add_library(dent_cuda STATIC
        src/backend/cuda_kernels.cu
    )
    set_target_properties(dent_cuda PROPERTIES CUDA_STANDARD 17 CUDA_STANDARD_REQUIRED ON)
    target_link_libraries(dent_cuda PUBLIC CUDA::cudart)
    target_include_directories(dent_cuda PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()


include(GNUInstallDirs)
install(TARGETS dent dent_core dent_api
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
