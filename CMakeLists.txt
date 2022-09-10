cmake_minimum_required(VERSION 3.1...3.14)
set(CMAKE_C_COMPILER "gcc-10")
set(CMAKE_CXX_COMPILER "g++-10")

project(BoostMiner CXX)


# Setup paths to outputs of conan's "cmake_find_package" generator
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)
option(PACKAGE_TESTS "Build the tests" ON)

if(PACKAGE_TESTS)
    include(CTest)
    find_package(GTest REQUIRED)
    enable_testing()
    add_subdirectory(tests)
endif()

add_executable(BoostMiner src/boost_miner.cpp src/wallet.cpp src/logger.cpp)
target_link_libraries(BoostMiner PUBLIC CONAN_PKG::gigamonkey)
target_include_directories(BoostMiner PUBLIC include)

add_executable(CosmosWallet src/cosmos.cpp src/wallet.cpp src/logger.cpp)
target_link_libraries(CosmosWallet PUBLIC CONAN_PKG::gigamonkey)
target_include_directories(CosmosWallet PUBLIC include)

# Set C++ version
target_compile_features(BoostMiner PUBLIC cxx_std_20)
set_target_properties(BoostMiner PROPERTIES CXX_EXTENSIONS ON)

target_compile_features(CosmosWallet PUBLIC cxx_std_20)
set_target_properties(CosmosWallet PROPERTIES CXX_EXTENSIONS ON)

