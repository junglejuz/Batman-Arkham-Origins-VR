cmake_minimum_required(VERSION 3.21)
project(BatmanOriginsVR VERSION 0.1.4 LANGUAGES C CXX)
if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Origins probe requires MSVC Win32: -G \"Visual Studio 17 2022\" -A Win32")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
add_subdirectory(third_party)
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS src/core/*.cpp)
add_library(batman_origins SHARED ${CORE_SOURCES}
 src/hash.cpp tests/hud_boundary_selftest.cpp src/game/adapter_registry.cpp src/game/origins/adapter.cpp
 src/game/origins/scene.cpp src/game/origins/window.cpp src/game/origins/startup.cpp src/game/origins/transitions.cpp src/game/origins/wipe.cpp src/game/origins/afw.cpp src/game/origins/dlss_bridge.cpp src/game/origins/reflect.cpp src/game/origins/menu_state.cpp src/game/origins/first_person.cpp)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
file(WRITE "${CMAKE_BINARY_DIR}/generated/bvr_version.h"
 "#pragma once\n#define BVR_VERSION \"${PROJECT_VERSION}\"\n#define BVR_BUILD_ID \"Origins-release-0.1.4\"\n")
target_include_directories(batman_origins PRIVATE src "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(batman_origins PRIVATE minhook imgui openxr_loader
 d3d11 dxgi d3dcompiler dbghelp windowscodecs ole32 bcrypt)
target_compile_definitions(batman_origins PRIVATE BVR_WITH_OPENXR)
add_custom_command(TARGET batman_origins POST_BUILD
 COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:batman_origins>/OriginsVR"
 COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/assets/bat-wipe.bin"
 "$<TARGET_FILE_DIR:batman_origins>/OriginsVR/bat-wipe.bin")
add_library(xinput1_3 SHARED src/proxy.cpp src/proxy.def)
add_executable(bo_xr_config32 tools/xr_config32.cpp)
target_link_libraries(bo_xr_config32 PRIVATE openxr_loader)
add_executable(origins_offline_check tests/offline_check.cpp)
add_executable(origins_startup_check tests/startup_policy_check.cpp)
target_include_directories(origins_startup_check PRIVATE src)
target_link_libraries(origins_offline_check PRIVATE d3d11 dxgi)
enable_testing()
add_test(NAME startup_input_lifecycle COMMAND origins_startup_check)
add_test(NAME foreign_host_guard COMMAND origins_offline_check "$<TARGET_FILE:batman_origins>" "$<TARGET_FILE:xinput1_3>")
add_test(NAME ordinary_launch_stays_flat COMMAND origins_offline_check "$<TARGET_FILE:batman_origins>" "$<TARGET_FILE:xinput1_3>" flat)
add_test(NAME graphics_profile_write_denied COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/graphics_profile_check.ps1")
