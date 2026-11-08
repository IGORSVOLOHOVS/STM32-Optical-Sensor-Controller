cmake_minimum_required(VERSION 3.20)

# The host build: the part of the firmware that needs no HAL, and its tests.
#
#   cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=g++            what recipe.ps1 runs
#
# The firmware itself is built by STM32CubeIDE from OpticManager.ioc, as before; nothing here is read by the IDE.
project(STM32OpticalSensorController VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# clang-tidy reads this.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTING "Build tests" ON)

function(warn_as_errors target)
    if(NOT MSVC)
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

# CommunicationManager, Settings and AdcStub are header-only and include no HAL. Adc, the transports, Logger,
# Application and main.cpp include main.h and the STM32 HAL.
add_library(sensor_protocol INTERFACE)
target_include_directories(sensor_protocol INTERFACE Core/Src/Impl)

if(BUILD_TESTING)
    enable_testing()

    # No test framework: nothing is downloaded, so the build needs no network.
    add_executable(communication_manager_tests tests/communication_manager_test.cpp)
    target_link_libraries(communication_manager_tests PRIVATE sensor_protocol)
    warn_as_errors(communication_manager_tests)

    add_test(NAME communication_manager_tests COMMAND communication_manager_tests)
    # The manual list is judged where every other file is judged (section 9).
    add_test(
        NAME manual_list
        COMMAND python -m pytest tests/test_manual_list.py -q -p no:cacheprovider
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
