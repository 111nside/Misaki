cmake_minimum_required(VERSION 3.16)
project(MisakiNative LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(misaki_native STATIC
    Source/MisakiCore.cpp
    Source/GuestCPU.cpp
    Source/GuestX64Backend.cpp
    Source/GuestDynamicExecution.cpp
    Source/GuestServices.cpp
    Source/MisakiCoreBridge.cpp
)
target_include_directories(misaki_native PUBLIC Include)
if (MSVC)
    target_compile_options(misaki_native PRIVATE /W4)
else()
    target_compile_options(misaki_native PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if (BUILD_TESTING)
    add_executable(misaki_native_tests Tests/MisakiCoreTests.cpp)
    target_link_libraries(misaki_native_tests PRIVATE misaki_native)
    add_test(NAME native-core-unit-tests COMMAND misaki_native_tests)
    add_executable(misaki_dynamic_module_tests Tests/GuestDynamicExecutionTests.cpp)
    target_link_libraries(misaki_dynamic_module_tests PRIVATE misaki_native)
    add_test(NAME native-dynamic-module-tests COMMAND misaki_dynamic_module_tests)

    add_executable(misaki_guest_service_tests Tests/GuestServicesTests.cpp)
    target_link_libraries(misaki_guest_service_tests PRIVATE misaki_native)
    add_test(NAME native-guest-services-tests COMMAND misaki_guest_service_tests)

    add_executable(misaki_x64_backend_tests Tests/GuestX64BackendTests.cpp)
    target_link_libraries(misaki_x64_backend_tests PRIVATE misaki_native)
    add_test(NAME x64-backend-regression-tests COMMAND misaki_x64_backend_tests)
endif()
