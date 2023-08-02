cmake_minimum_required (VERSION 3.16)
project (BoostMiner CXX)

add_library (bm STATIC
    src/whatsonchain_api.cpp
    src/pow_co_api.cpp
    src/miner.cpp
    src/network.cpp
    src/logger.cpp
    src/jobs.cpp)

find_package (gigamonkey CONFIG REQUIRED)

target_include_directories (bm PUBLIC include)
target_link_libraries (bm PUBLIC gigamonkey::gigamonkey data::data)
target_compile_features (bm PUBLIC cxx_std_20)
set_target_properties (bm PROPERTIES CXX_EXTENSIONS OFF)

add_executable (BoostMiner src/boost_miner.cpp src/miner_options.cpp)

find_package (argh CONFIG REQUIRED)
target_link_libraries (BoostMiner PUBLIC bm argh)
target_include_directories (BoostMiner PUBLIC include)

target_compile_features (BoostMiner PUBLIC cxx_std_20)
set_target_properties (BoostMiner PROPERTIES CXX_EXTENSIONS OFF)

add_executable (CosmosWallet
    src/cosmos.cpp
    src/wallet.cpp)

target_link_libraries (CosmosWallet PUBLIC bm)
target_include_directories (CosmosWallet PUBLIC include)

target_compile_features (CosmosWallet PUBLIC cxx_std_20)
set_target_properties (CosmosWallet PROPERTIES CXX_EXTENSIONS OFF)

add_executable (DetectBoost
    src/detect_boost.cpp)

target_link_libraries (DetectBoost PUBLIC bm)
target_include_directories (DetectBoost PUBLIC include)

target_compile_features (DetectBoost PUBLIC cxx_std_20)
set_target_properties (DetectBoost PROPERTIES CXX_EXTENSIONS OFF)

add_executable (CheckAPI
    src/check_API.cpp)

target_link_libraries (CheckAPI PUBLIC bm)
target_link_libraries (CheckAPI PUBLIC bm argh)
target_include_directories (CheckAPI PUBLIC include)

target_compile_features (CheckAPI PUBLIC cxx_std_20)
set_target_properties (CheckAPI PROPERTIES CXX_EXTENSIONS OFF)

option (PACKAGE_TESTS "Build the tests" ON)

add_definitions ("-DHAS_BOOST")

if (PACKAGE_TESTS)
    include (CTest)
    find_package (GTest REQUIRED)
    enable_testing ()
    add_subdirectory (test)
endif ()

install (TARGETS CosmosWallet BoostMiner)
