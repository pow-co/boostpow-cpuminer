cmake_minimum_required(VERSION 3.10...3.14)

# Back compatibility for VERSION range
if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()
macro(package_add_test TESTNAME)
    # create an exectuable in which the tests will be stored
    add_executable(${TESTNAME} ${ARGN})
    target_include_directories(${TESTNAME} PUBLIC .)
    target_link_libraries(${TESTNAME} CONAN_PKG::gtest)
    gtest_discover_tests(${TESTNAME}
            # set a working directory so your project root so that you can find test data via paths relative to the project root
            SOURCES ${ARGN}
            WORKING_DIRECTORY ${PROJECT_DIR}
            PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_DIR}"
            DISCOVERY_MODE PRE_TEST
            )
    set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
endmacro()



package_add_test(testTests testTests.cpp)
