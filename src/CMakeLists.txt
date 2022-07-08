cmake_minimum_required(VERSION 3.1...3.14)
set(CMAKE_C_COMPILER "gcc-10")
set(CMAKE_CXX_COMPILER "g++-10")
project(BoostMiner CXX)


# Setup paths to outputs of conan's "cmake_find_package" generator
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)


add_executable(BoostMiner boost_miner.cpp)
target_link_libraries(BoostMiner PUBLIC CONAN_PKG::gigamonkey)
target_include_directories(BoostMiner PUBLIC include)

# Set C++ version
target_compile_features(BoostMiner PUBLIC cxx_std_20)
set_target_properties(BoostMiner PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(BoostMiner PUBLIC "-fconcepts")

