cmake_minimum_required(VERSION 3.20)

project(dory_bn254 VERSION 0.1.0 LANGUAGES CXX)

# PLAN §1 and §18 (Milestone 1): the backend is a small C++17 library built
# with CMake. OPENSSL_ROOT_DIR is intentionally not consumed yet: OpenSSL is
# optional until a later Fiat-Shamir milestone, but callers may safely pass it.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# PLAN §1 Windows note: retain the caller's conda OpenSSL hint without making
# OpenSSL a Milestone 1 dependency. A later hashing/Fiat-Shamir target can pass
# this cache entry directly to find_package(OpenSSL).
set(OPENSSL_ROOT_DIR "${OPENSSL_ROOT_DIR}" CACHE PATH "Optional OpenSSL installation root")

include(FetchContent)

# PLAN §1: use herumi/mcl over BN254. Pin the archive and hash so first-time
# dependency acquisition is reproducible and does not require Git on Windows.
set(MCL_BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
set(MCL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(MCL_TEST_WITH_GMP OFF CACHE BOOL "" FORCE)
set(MCL_USE_GMP OFF CACHE BOOL "" FORCE)
if(MSVC)
    # Match the default DLL runtime used by this project and by Visual Studio.
    set(MCL_MSVC_RUNTIME_DLL ON CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
    mcl
    URL https://github.com/herumi/mcl/archive/refs/tags/v3.00.zip
    URL_HASH SHA256=8298dcc4fe8619bd6b19597b211300e4503a14838d1ae3906c44a25242280ccb
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(mcl)

# mcl v3.00 names both its shared import library and static library mcl.lib.
# Ninja rejects those duplicate outputs on Windows even though this project only
# links mcl_st, so give the unused shared target a distinct artifact name.
if(WIN32)
    set_target_properties(mcl PROPERTIES OUTPUT_NAME mcl_shared)
endif()

add_library(dory_bn254
    src/backend_mcl.cpp
    src/challenge_channel.cpp
    src/dory_batch.cpp
    src/dory_api.cpp
    src/dory_precomp.cpp
    src/dory_prover.cpp
    src/dory_verifier_deferred.cpp
    src/dory_verifier_eager.cpp
    src/gt_lincomb.cpp
    src/profiling.cpp
    src/scalar_product.cpp
    src/serialization.cpp
    src/transcript_counts.cpp
    src/vector_ops.cpp
)
add_library(dory::bn254 ALIAS dory_bn254)

target_include_directories(dory_bn254
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(dory_bn254 PUBLIC mcl::mcl_st)

if(MSVC)
    target_compile_options(dory_bn254 PRIVATE /W4 /permissive-)
else()
    target_compile_options(dory_bn254 PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(bench_dory bench/bench_dory.cpp)
target_link_libraries(bench_dory PRIVATE dory::bn254)

add_executable(dory_demo examples/dory_demo.cpp)
target_link_libraries(dory_demo PRIVATE dory::bn254)

include(CTest)
if(BUILD_TESTING)
    add_executable(test_backend tests/test_backend.cpp)
    target_link_libraries(test_backend PRIVATE dory::bn254)
    add_test(NAME backend COMMAND test_backend)

    add_executable(test_scalar_product tests/test_scalar_product.cpp)
    target_link_libraries(test_scalar_product PRIVATE dory::bn254)
    add_test(NAME scalar_product COMMAND test_scalar_product)

    add_executable(test_dory_recursive tests/test_dory_recursive.cpp)
    target_link_libraries(test_dory_recursive PRIVATE dory::bn254)
    add_test(NAME dory_recursive COMMAND test_dory_recursive)

    add_executable(test_dory_deferred_vs_eager
        tests/test_dory_deferred_vs_eager.cpp)
    target_link_libraries(test_dory_deferred_vs_eager PRIVATE dory::bn254)
    add_test(NAME dory_deferred_vs_eager COMMAND test_dory_deferred_vs_eager)

    add_executable(test_dory_batch tests/test_dory_batch.cpp)
    target_link_libraries(test_dory_batch PRIVATE dory::bn254)
    add_test(NAME dory_batch COMMAND test_dory_batch)

    add_executable(test_transcript_counts tests/test_transcript_counts.cpp)
    target_link_libraries(test_transcript_counts PRIVATE dory::bn254)
    add_test(NAME transcript_counts COMMAND test_transcript_counts)

    add_executable(test_gt_multiexp tests/test_gt_multiexp.cpp)
    target_link_libraries(test_gt_multiexp PRIVATE dory::bn254)
    add_test(NAME gt_multiexp COMMAND test_gt_multiexp)

    add_test(
        NAME benchmark_csv
        COMMAND ${CMAKE_COMMAND}
            -DBENCH_DORY=$<TARGET_FILE:bench_dory>
            -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/bench_test_output.csv
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_benchmark_csv.cmake
    )
    set_tests_properties(benchmark_csv PROPERTIES TIMEOUT 60)

    add_executable(test_profile_counts tests/test_profile_counts.cpp)
    target_link_libraries(test_profile_counts PRIVATE dory::bn254)
    add_test(NAME profile_counts COMMAND test_profile_counts)

    add_executable(test_pair_product_optimized
        tests/test_pair_product_optimized.cpp)
    target_link_libraries(test_pair_product_optimized PRIVATE dory::bn254)
    add_test(NAME pair_product_optimized COMMAND test_pair_product_optimized)

    add_executable(test_dory_precomp_optimized
        tests/test_dory_precomp_optimized.cpp)
    target_link_libraries(test_dory_precomp_optimized PRIVATE dory::bn254)
    add_test(NAME dory_precomp_optimized COMMAND test_dory_precomp_optimized)

    add_executable(test_serialization tests/test_serialization.cpp)
    target_link_libraries(test_serialization PRIVATE dory::bn254)
    add_test(NAME serialization COMMAND test_serialization)

    add_executable(test_dory_api tests/test_dory_api.cpp)
    target_link_libraries(test_dory_api PRIVATE dory::bn254)
    add_test(NAME dory_api COMMAND test_dory_api)
endif()
