cmake_minimum_required (VERSION 3.16)
project (BoostMiner CXX)

# Setup paths to outputs of conan's "cmake_find_package" generator
include (${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup (TARGETS)

add_library (bm STATIC
    src/whatsonchain_api.cpp 
    src/pow_co_api.cpp 
    src/miner.cpp 
    src/network.cpp
    src/logger.cpp 
    src/jobs.cpp)
    
target_include_directories (bm PUBLIC include)
target_link_libraries (bm PUBLIC CONAN_PKG::Gigamonkey)
target_compile_features (bm PUBLIC cxx_std_20)
set_target_properties (bm PROPERTIES CXX_EXTENSIONS OFF)

add_executable (BoostMiner src/boost_miner.cpp src/miner_options.cpp)

target_link_libraries (BoostMiner PUBLIC bm CONAN_PKG::argh)
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

option (PACKAGE_TESTS "Build the tests" ON)

add_definitions ("-DHAS_BOOST")

if (PACKAGE_TESTS)
  include(CTest)
  find_package(GTest REQUIRED)
  enable_testing()
  add_subdirectory(test)

endif()
