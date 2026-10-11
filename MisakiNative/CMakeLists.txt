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
    Source/GuestGraphics20.cpp
    Source/GuestGraphics20Bridge.cpp
    Source/GuestGraphics21.cpp
    Source/GuestGraphics21Bridge.cpp
    Source/GuestPM4.cpp
    Source/GuestPM4Bridge.cpp
    Source/GuestPM4Indirect.cpp
    Source/GuestPM4IndirectBridge.cpp
    Source/GuestPM4State.cpp
    Source/GuestPM4StateBridge.cpp
    Source/GuestGCNISA.cpp
    Source/GuestGCNISABridge.cpp
    Source/GuestShaderIR.cpp
    Source/GuestShaderIRBridge.cpp
    Source/GuestMetalIR.cpp
    Source/GuestMetalIRBridge.cpp
    Source/GuestMetalResources.cpp
    Source/GuestMetalResourcesBridge.cpp
    Source/GuestMUBUF.cpp
    Source/GuestMUBUFBridge.cpp
    Source/GuestFusedGCN30.cpp
    Source/GuestFusedGCN30Bridge.cpp
    Source/GuestIntegratedRuntime.cpp
    Source/GuestIntegratedRuntimeBridge.cpp
    Source/GuestPS4Formats.cpp
    Source/GuestPS4Imports.cpp
    Source/GuestGCNLanes.cpp
    Source/GuestBootCoordinator.cpp
    Source/GuestBootReadiness.cpp
    Source/GuestPS4BootBridge.cpp
    Source/GuestSCEStaging.cpp
    Source/GuestSCEBinding.cpp
    Source/GuestSCEKernel.cpp
    Source/GuestGCNMemoryStage.cpp
    Source/GuestPS4PhaseTwo.cpp
    Source/GuestPS4PhaseTwoBridge.cpp
    Source/GuestSCE46.cpp
    Source/GuestSCE47.cpp
    Source/GuestVM48.cpp
    Source/GuestGCN49.cpp
    Source/GuestSCE50.cpp
    Source/GuestSCE50Bridge.cpp
    Source/MisakiCoreBridge.cpp
    Source/GuestSCE51.cpp
    Source/GuestSCE52.cpp
    Source/GuestKernel53.cpp
    Source/GuestGCN54.cpp
    Source/GuestSCE55.cpp
    Source/GuestSCE55Bridge.cpp
    Source/GuestSCE56.cpp
    Source/GuestServices57.cpp
    Source/GuestGPUMemory58.cpp
    Source/GuestPersistent59.cpp
    Source/GuestCompatibility60.cpp
    Source/GuestMega60.cpp
    Source/GuestMega60Bridge.cpp
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
    add_executable(misaki_guest_graphics20_tests Tests/GuestGraphics20Tests.cpp)
    target_link_libraries(misaki_guest_graphics20_tests PRIVATE misaki_native)
    add_test(NAME native-graphics20-queue-tests COMMAND misaki_guest_graphics20_tests)
    add_executable(misaki_guest_gpu21_tests Tests/GuestGraphics21Tests.cpp)
    target_link_libraries(misaki_guest_gpu21_tests PRIVATE misaki_native)
    add_test(NAME native-gpu21-render-state-tests COMMAND misaki_guest_gpu21_tests)
    add_executable(misaki_guest_pm4_tests Tests/GuestPM4Tests.cpp)
    target_link_libraries(misaki_guest_pm4_tests PRIVATE misaki_native)
    add_test(NAME native-guest-pm4-tests COMMAND misaki_guest_pm4_tests)
    add_executable(misaki_pm423_tests Tests/GuestPM4IndirectTests.cpp)
    target_link_libraries(misaki_pm423_tests PRIVATE misaki_native)
    add_test(NAME native-pm4-indirect-render-state-tests COMMAND misaki_pm423_tests)
    add_executable(misaki_pm424_tests Tests/GuestPM4StateTests.cpp)
    target_link_libraries(misaki_pm424_tests PRIVATE misaki_native)
    add_test(NAME native-pm424-draw-state-tests COMMAND misaki_pm424_tests)
    add_executable(misaki_guest_gcn25_tests Tests/GuestGCNISATests.cpp)
    target_link_libraries(misaki_guest_gcn25_tests PRIVATE misaki_native)
    add_test(NAME native-gcn25-shader-decoder-tests COMMAND misaki_guest_gcn25_tests)
    add_executable(misaki_guest_shader_ir26_tests Tests/GuestShaderIRTests.cpp)
    target_link_libraries(misaki_guest_shader_ir26_tests PRIVATE misaki_native)
    add_test(NAME native-gcn26-shader-ir-tests COMMAND misaki_guest_shader_ir26_tests)
    add_executable(misaki_guest_metal_ir27_tests Tests/GuestMetalIRTests.cpp)
    target_link_libraries(misaki_guest_metal_ir27_tests PRIVATE misaki_native)
    add_test(NAME native-gcn27-metal-translation-tests COMMAND misaki_guest_metal_ir27_tests)
    add_executable(misaki_guest_metal_resource28_tests Tests/GuestMetalResourcesTests.cpp)
    target_link_libraries(misaki_guest_metal_resource28_tests PRIVATE misaki_native)
    add_test(NAME native-gcn28-metal-resources-tests COMMAND misaki_guest_metal_resource28_tests)
    add_executable(misaki_guest_mubuf29_tests Tests/GuestMUBUFTests.cpp)
    target_link_libraries(misaki_guest_mubuf29_tests PRIVATE misaki_native)
    add_test(NAME native-gcn29-mubuf-metal-tests COMMAND misaki_guest_mubuf29_tests)
    add_executable(misaki_guest_gcn30_tests Tests/GuestFusedGCN30Tests.cpp)
    target_link_libraries(misaki_guest_gcn30_tests PRIVATE misaki_native)
    add_test(NAME native-gcn30-interleaved-metal-tests COMMAND misaki_guest_gcn30_tests)
endif()

# Five independent Phase-2 subsystem suites are kept separate for diagnosis.
if (BUILD_TESTING)
    foreach(stage IN ITEMS 31 32 33 34 35)
        add_executable(misaki_integrated_${stage}_tests Tests/Integrated${stage}Tests.cpp)
        target_link_libraries(misaki_integrated_${stage}_tests PRIVATE misaki_native)
        add_test(NAME integrated-${stage}-subsystem COMMAND misaki_integrated_${stage}_tests)
    endforeach()
    add_executable(misaki_compat36_tests Tests/Compatibility36Tests.cpp)
    target_link_libraries(misaki_compat36_tests PRIVATE misaki_native)
    add_test(NAME compat36-self-structure COMMAND misaki_compat36_tests)
    add_executable(misaki_compat37_tests Tests/Compatibility37Tests.cpp)
    target_link_libraries(misaki_compat37_tests PRIVATE misaki_native)
    add_test(NAME compat37-synthetic-sce-imports COMMAND misaki_compat37_tests)
    add_executable(misaki_compat38_tests Tests/Compatibility38Tests.cpp)
    target_link_libraries(misaki_compat38_tests PRIVATE misaki_native)
    add_test(NAME compat38-gcn-cooperative-lanes COMMAND misaki_compat38_tests)
    add_executable(misaki_compat39_tests Tests/Compatibility39Tests.cpp)
    target_link_libraries(misaki_compat39_tests PRIVATE misaki_native)
    add_test(NAME compat39-boot-integration COMMAND misaki_compat39_tests)
    add_executable(misaki_compat40_tests Tests/Compatibility40Tests.cpp)
    target_link_libraries(misaki_compat40_tests PRIVATE misaki_native)
    add_test(NAME compat40-firmware-readiness COMMAND misaki_compat40_tests)
endif()

# Mega update 41–45: five separate independent CTest targets.
if (BUILD_TESTING)
    foreach(stage IN ITEMS 41 42 43 44 45)
        add_executable(misaki_compat${stage}_tests Tests/Compatibility${stage}Tests.cpp)
        target_link_libraries(misaki_compat${stage}_tests PRIVATE misaki_native)
        add_test(NAME compat${stage}-integrated-stage COMMAND misaki_compat${stage}_tests)
    endforeach()
endif()

if (BUILD_TESTING)
  add_executable(misaki_compat46_tests Tests/Compatibility46Tests.cpp)
  target_link_libraries(misaki_compat46_tests PRIVATE misaki_native)
  add_test(NAME compat46-sce-multi-segment-rela COMMAND misaki_compat46_tests)
endif()

if (BUILD_TESTING)
  add_executable(misaki_compat47_tests Tests/Compatibility47Tests.cpp)
  target_link_libraries(misaki_compat47_tests PRIVATE misaki_native)
  add_test(NAME compat47-nid-module-dependency COMMAND misaki_compat47_tests)
endif()

if (BUILD_TESTING)
  add_executable(misaki_compat48_tests Tests/Compatibility48Tests.cpp)
  target_link_libraries(misaki_compat48_tests PRIVATE misaki_native)
  add_test(NAME compat48-guest-vm-protection COMMAND misaki_compat48_tests)
endif()

if (BUILD_TESTING)
  add_executable(misaki_compat49_tests Tests/Compatibility49Tests.cpp)
  target_link_libraries(misaki_compat49_tests PRIVATE misaki_native)
  add_test(NAME compat49-guest-pm4-presentation COMMAND misaki_compat49_tests)
endif()

if (BUILD_TESTING)
  add_executable(misaki_compat50_tests Tests/Compatibility50Tests.cpp)
  target_link_libraries(misaki_compat50_tests PRIVATE misaki_native)
  add_test(NAME compat50-initialization-boot-gate COMMAND misaki_compat50_tests)
endif()

# Mega update 51-55: independently runnable compatibility CTest suites.
if (BUILD_TESTING)
    foreach(stage IN ITEMS 51 52 53 54 55)
        add_executable(misaki_compat${stage}_tests Tests/Compatibility${stage}Tests.cpp)
        target_link_libraries(misaki_compat${stage}_tests PRIVATE misaki_native)
        add_test(NAME compat${stage}-ps4-research COMMAND misaki_compat${stage}_tests)
    endforeach()
endif()

# Mega update 56-60: independent regression suites; all earlier suites remain.
if (BUILD_TESTING)
    foreach(stage IN ITEMS 56 57 58 59 60)
        add_executable(misaki_compat${stage}_tests Tests/Compatibility${stage}Tests.cpp)
        target_link_libraries(misaki_compat${stage}_tests PRIVATE misaki_native)
        add_test(NAME compat${stage}-integration-tests COMMAND misaki_compat${stage}_tests)
    endforeach()
endif()
