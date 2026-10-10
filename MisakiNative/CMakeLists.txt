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
    Source/GuestProcesses.cpp
    Source/GuestSystemLibraries.cpp
    Source/GuestLibraryCatalog.cpp
    Source/GuestModuleLifecycle.cpp
    Source/GuestModuleLifecycleBridge.cpp
    Source/GuestGPU.cpp
    Source/GuestGPUBridge.cpp
    Source/GuestGraphics19.cpp
    Source/GuestGraphics19Bridge.cpp
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
    add_executable(misaki_guest_process_tests Tests/GuestProcessesTests.cpp)
    target_link_libraries(misaki_guest_process_tests PRIVATE misaki_native)
    add_test(NAME native-guest-process-tests COMMAND misaki_guest_process_tests)
    add_executable(misaki_x64_backend_tests Tests/GuestX64BackendTests.cpp)
    target_link_libraries(misaki_x64_backend_tests PRIVATE misaki_native)
    add_test(NAME x64-backend-regression-tests COMMAND misaki_x64_backend_tests)
    add_executable(misaki_system_library_tests Tests/GuestSystemLibrariesTests.cpp)
    target_link_libraries(misaki_system_library_tests PRIVATE misaki_native)
    add_test(NAME native-system-library-tests COMMAND misaki_system_library_tests)
    add_executable(misaki_library_catalog_tests Tests/GuestLibraryCatalogTests.cpp)
    target_link_libraries(misaki_library_catalog_tests PRIVATE misaki_native)
    add_test(NAME native-library-catalog-tests COMMAND misaki_library_catalog_tests)
    add_executable(misaki_module_lifecycle_tests Tests/GuestModuleLifecycleTests.cpp)
    target_link_libraries(misaki_module_lifecycle_tests PRIVATE misaki_native)
    add_test(NAME native-module-lifecycle-tests COMMAND misaki_module_lifecycle_tests)
    add_executable(misaki_guest_gpu_tests Tests/GuestGPUTests.cpp)
    target_link_libraries(misaki_guest_gpu_tests PRIVATE misaki_native)
    add_test(NAME native-guest-gpu-tests COMMAND misaki_guest_gpu_tests)

    add_executable(misaki_guest_graphics19_tests Tests/GuestGraphics19Tests.cpp)
    target_link_libraries(misaki_guest_graphics19_tests PRIVATE misaki_native)
    add_test(NAME native-graphics19-tests COMMAND misaki_guest_graphics19_tests)
endif()
