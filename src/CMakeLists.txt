cmake_minimum_required(VERSION 3.1...3.14)
project(BoostMiner CXX)

project(gigamonkey 	VERSION 1.0
		DESCRIPTION "open-source Bitcoin library in c++"
		LANGUAGES CXX)

# Setup paths to outputs of conan's "cmake_find_package" generator
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH})

find_package(gigamonkey 0.1 REQUIRED)
if(gigamonkey_FOUND)
	message(STATUS "gigamonkey_INCLUDE_DIRS: ${gigamonkey_INCLUDE_DIRS}")
	message(STATUS "gigamonkey_LIBRARIES: ${gigamonkey_LIBRARIES}")
    include_directories(${gigamonkey_INCLUDE_DIRS})
endif()

# Include Boost
find_package(Boost 1.76.0 COMPONENTS system locale  REQUIRED)
if(Boost_FOUND)
    message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost_LIBRARIES: ${Boost_LIBRARIES}")
    message(STATUS "Boost_VERSION: ${Boost_VERSION}")

    include_directories(${Boost_INCLUDE_DIRS})
    add_definitions("-DHAS_BOOST")
endif()

find_package(nlohmann_json 3.2.0 REQUIRED)
if(nlohmann_json_FOUND)
	message(STATUS "nlohmann_json_INCLUDE_DIRS: ${nlohmann_json_INCLUDE_DIRS}")
	message(STATUS "nlohmann_json_LIBRARIES: ${nlohmann_json_LIBRARIES}")

    include_directories(${nlohmann_json_INCLUDE_DIRS})
endif()

find_package(data REQUIRED)
if(data_INCLUDE_DIR)
    include_directories(${data_INCLUDE_DIR})
endif()

find_package(gmp REQUIRED)
if(gmp_INCLUDE_DIRS)
    include_directories(${gmp_INCLUDE_DIRS})
endif()

add_executable(BoostMiner boost_miner.cpp)
target_link_libraries(BoostMiner PUBLIC ${gigamonkey_LIBRARIES})
target_include_directories(BoostMiner PUBLIC include)

# Set C++ version
target_compile_features(BoostMiner PUBLIC cxx_std_17)
set_target_properties(BoostMiner PROPERTIES CXX_EXTENSIONS OFF)

