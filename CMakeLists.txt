cmake_minimum_required(VERSION 3.24)
project(SoftwareFuserCore VERSION 0.1.0 LANGUAGES CXX)

# The UWP widget is built by its Visual Studio project, not this portable target.
add_library(fuser_core STATIC native/core/src/configuration.cpp)
target_include_directories(fuser_core PUBLIC native/core/include)
target_compile_features(fuser_core PUBLIC cxx_std_20)
if(MSVC)
  target_compile_options(fuser_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(fuser_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(FUSER_BUILD_TESTS "Build the portable contract tests" OFF)
option(FUSER_BUILD_GPU_TESTS "Build Windows hardware-renderer smoke tests" OFF)
option(FUSER_BUILD_DECODER_TESTS "Build Windows FFmpeg/D3D11VA fixture tests" OFF)
option(FUSER_BUILD_DISPLAY_PROBE "Build the read-only Windows display inventory" OFF)
option(FUSER_BUILD_CONTROL_TESTS "Build controlled Sunshine pairing/credential tests" OFF)
if(FUSER_BUILD_DISPLAY_PROBE)
  if(NOT WIN32)
    message(FATAL_ERROR "The display probe requires Windows.")
  endif()
  add_executable(fuser_display_probe tools/DisplayProbe.cpp)
  target_link_libraries(fuser_display_probe PRIVATE user32)
endif()

if(FUSER_BUILD_CONTROL_TESTS)
  if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "Control tests require Windows and MSVC.")
  endif()
  enable_testing()
  set(FUSER_DEPENDENCIES "${CMAKE_CURRENT_SOURCE_DIR}/build/dependencies/x64-uwp" CACHE PATH "Built x64 UWP dependencies")
  foreach(COMPONENT libcurl libcrypto libssl libexpat)
    set(DEBUG_LIB_NAME "${COMPONENT}")
    if(COMPONENT STREQUAL "libcrypto" OR COMPONENT STREQUAL "libssl")
      set(DLL_NAME "${COMPONENT}-3-x64")
    else()
      set(DLL_NAME "${COMPONENT}")
    endif()
    set(DEBUG_DLL_NAME "${DLL_NAME}")
    if(COMPONENT STREQUAL "libcurl")
      set(DEBUG_LIB_NAME libcurl-d)
      set(DEBUG_DLL_NAME libcurl-d)
    elseif(COMPONENT STREQUAL "libexpat")
      set(DEBUG_LIB_NAME libexpatd)
      set(DEBUG_DLL_NAME libexpatd)
    endif()
    add_library(fuser_${COMPONENT} SHARED IMPORTED)
    set_target_properties(fuser_${COMPONENT} PROPERTIES IMPORTED_CONFIGURATIONS "Debug;Release"
      IMPORTED_IMPLIB_RELEASE "${FUSER_DEPENDENCIES}/lib/${COMPONENT}.lib"
      IMPORTED_LOCATION_RELEASE "${FUSER_DEPENDENCIES}/bin/${DLL_NAME}.dll"
      IMPORTED_IMPLIB_DEBUG "${FUSER_DEPENDENCIES}/debug/lib/${DEBUG_LIB_NAME}.lib"
      IMPORTED_LOCATION_DEBUG "${FUSER_DEPENDENCIES}/debug/bin/${DEBUG_DLL_NAME}.dll"
      INTERFACE_INCLUDE_DIRECTORIES "${FUSER_DEPENDENCIES}/include")
  endforeach()
  add_library(fuser_control_test_library STATIC native/streaming/SunshineControl.cpp)
  target_compile_features(fuser_control_test_library PUBLIC cxx_std_20)
  target_compile_options(fuser_control_test_library PRIVATE /W4 /permissive-)
  target_include_directories(fuser_control_test_library PUBLIC native/streaming
    "${CMAKE_CURRENT_SOURCE_DIR}/build/references/moonlight-common-c/src")
  target_link_libraries(fuser_control_test_library PUBLIC fuser_core fuser_libcurl fuser_libcrypto fuser_libssl fuser_libexpat
    "${CMAKE_CURRENT_SOURCE_DIR}/build/streaming/$<CONFIG>/moonlight-common-c.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/build/streaming/$<CONFIG>/enet.lib" ws2_32 winmm)
  add_executable(fuser_control_tests tests/control_tests.cpp)
  target_link_libraries(fuser_control_tests PRIVATE fuser_control_test_library)
  add_executable(fuser_credentials_tests tests/credentials_tests.cpp native/widget/AppCredentials.cpp)
  target_compile_features(fuser_credentials_tests PRIVATE cxx_std_20)
  target_include_directories(fuser_credentials_tests PRIVATE "C:/Program Files (x86)/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/cppwinrt")
  target_link_libraries(fuser_credentials_tests PRIVATE fuser_core windowsapp)
  add_custom_command(TARGET fuser_control_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${FUSER_DEPENDENCIES}/$<IF:$<CONFIG:Debug>,debug/bin,bin>" "$<TARGET_FILE_DIR:fuser_control_tests>" VERBATIM)
  find_program(FUSER_TEST_PYTHON python REQUIRED)
  foreach(SCENARIO success wrong-pin forged-signature rotated-certificate dtd
      desktop-launch desktop-resume stale-desktop steam-active resume-race)
    add_test(NAME fuser_control_${SCENARIO} COMMAND "${FUSER_TEST_PYTHON}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/control_server.py" "$<TARGET_FILE:fuser_control_tests>" ${SCENARIO})
    set_tests_properties(fuser_control_${SCENARIO} PROPERTIES TIMEOUT 30
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_CURRENT_SOURCE_DIR}/build/test-runtime/$<CONFIG>")
  endforeach()
  add_test(NAME fuser_private_credentials COMMAND fuser_credentials_tests "${CMAKE_CURRENT_SOURCE_DIR}/build/credential-tests")
  set_tests_properties(fuser_private_credentials PROPERTIES TIMEOUT 30)
  if(FUSER_BUILD_DECODER_TESTS)
    add_executable(fuser_sunshine_diagnostics tools/SunshineDiagnostics.cpp native/widget/AppCredentials.cpp native/streaming/OverlaySession.cpp)
    target_compile_features(fuser_sunshine_diagnostics PRIVATE cxx_std_20)
    target_link_libraries(fuser_sunshine_diagnostics PRIVATE fuser_control_test_library fuser_decoder fuser_renderer)
    add_custom_command(TARGET fuser_sunshine_diagnostics POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory "${FUSER_DEPENDENCIES}/$<IF:$<CONFIG:Debug>,debug/bin,bin>" "$<TARGET_FILE_DIR:fuser_sunshine_diagnostics>" VERBATIM)
  endif()
endif()
if(FUSER_BUILD_TESTS)
  enable_testing()
  add_executable(fuser_core_tests tests/core_tests.cpp)
  target_link_libraries(fuser_core_tests PRIVATE fuser_core)
  add_test(NAME fuser_core_contracts COMMAND fuser_core_tests)
endif()

if(FUSER_BUILD_GPU_TESTS OR FUSER_BUILD_DECODER_TESTS)
  if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "GPU smoke tests require Windows and MSVC.")
  endif()
  enable_testing()
  set(FUSER_SDK_ROOT "C:/Program Files (x86)/Windows Kits/10" CACHE PATH "Windows SDK root")
  set(FUSER_SDK_VERSION "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}" CACHE STRING "Windows SDK version")
  find_program(FUSER_FXC fxc HINTS "${FUSER_SDK_ROOT}/bin/${FUSER_SDK_VERSION}/x64" REQUIRED)
  set(FUSER_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
  foreach(SHADER FullscreenVs ChromaKeyPs)
    if(SHADER STREQUAL "FullscreenVs")
      set(PROFILE vs_5_0)
      set(SYMBOL g_fullscreen_vs)
    else()
      set(PROFILE ps_5_0)
      set(SYMBOL g_chroma_key_ps)
    endif()
    add_custom_command(OUTPUT "${FUSER_SHADER_DIR}/${SHADER}.h"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${FUSER_SHADER_DIR}"
      COMMAND "${FUSER_FXC}" /nologo /WX /O3 /T ${PROFILE} /E main
        /Fh "${FUSER_SHADER_DIR}/${SHADER}.h" /Vn ${SYMBOL}
        /Fo "${FUSER_SHADER_DIR}/${SHADER}.cso"
        "${CMAKE_CURRENT_SOURCE_DIR}/native/shaders/${SHADER}.hlsl"
      DEPENDS "native/shaders/${SHADER}.hlsl" VERBATIM)
  endforeach()
  add_library(fuser_renderer STATIC native/windows/D3D11Renderer.cpp
    "${FUSER_SHADER_DIR}/FullscreenVs.h" "${FUSER_SHADER_DIR}/ChromaKeyPs.h")
  target_include_directories(fuser_renderer PUBLIC native/windows "${FUSER_SHADER_DIR}"
    "${FUSER_SDK_ROOT}/Include/${FUSER_SDK_VERSION}/cppwinrt")
  target_compile_definitions(fuser_renderer PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
  target_compile_options(fuser_renderer PRIVATE /W4 /permissive-)
  target_link_libraries(fuser_renderer PUBLIC fuser_core d3d11 dxgi windowsapp)
  if(FUSER_BUILD_GPU_TESTS)
    add_executable(fuser_gpu_tests tests/gpu_tests.cpp)
    target_link_libraries(fuser_gpu_tests PRIVATE fuser_renderer)
    add_test(NAME fuser_gpu_shader_contracts COMMAND fuser_gpu_tests)
    set_tests_properties(fuser_gpu_shader_contracts PROPERTIES TIMEOUT 30)
  endif()
endif()

if(FUSER_BUILD_DECODER_TESTS)
  set(FUSER_DEPENDENCIES "${CMAKE_CURRENT_SOURCE_DIR}/build/dependencies/x64-uwp" CACHE PATH "Built x64 UWP dependencies")
  foreach(COMPONENT avcodec avutil)
    if(COMPONENT STREQUAL "avcodec")
      set(MAJOR 62)
    else()
      set(MAJOR 60)
    endif()
    add_library(fuser_${COMPONENT} SHARED IMPORTED)
    set_target_properties(fuser_${COMPONENT} PROPERTIES
      IMPORTED_CONFIGURATIONS "Debug;Release"
      IMPORTED_IMPLIB_RELEASE "${FUSER_DEPENDENCIES}/lib/${COMPONENT}.lib"
      IMPORTED_LOCATION_RELEASE "${FUSER_DEPENDENCIES}/bin/${COMPONENT}-${MAJOR}.dll"
      IMPORTED_IMPLIB_DEBUG "${FUSER_DEPENDENCIES}/debug/lib/${COMPONENT}.lib"
      IMPORTED_LOCATION_DEBUG "${FUSER_DEPENDENCIES}/debug/bin/${COMPONENT}-${MAJOR}.dll"
      INTERFACE_INCLUDE_DIRECTORIES "${FUSER_DEPENDENCIES}/include")
  endforeach()
  add_library(fuser_decoder STATIC native/windows/FFmpegDecoder.cpp)
  target_compile_options(fuser_decoder PRIVATE /W4 /permissive-)
  target_compile_definitions(fuser_decoder PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
  target_include_directories(fuser_decoder PUBLIC native/windows)
  target_link_libraries(fuser_decoder PUBLIC fuser_core fuser_avcodec fuser_avutil d3d11)
  add_executable(fuser_decoder_tests tests/decoder_tests.cpp)
  target_link_libraries(fuser_decoder_tests PRIVATE fuser_decoder fuser_renderer)
  add_executable(fuser_fixture_benchmark tools/FixtureBenchmark.cpp)
  target_compile_options(fuser_fixture_benchmark PRIVATE /W4 /permissive-)
  target_link_libraries(fuser_fixture_benchmark PRIVATE fuser_decoder fuser_renderer)
  add_custom_command(TARGET fuser_fixture_benchmark POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:fuser_avcodec>" "$<TARGET_FILE_DIR:fuser_fixture_benchmark>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:fuser_avutil>" "$<TARGET_FILE_DIR:fuser_fixture_benchmark>" VERBATIM)
  add_custom_command(TARGET fuser_decoder_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:fuser_avcodec>" "$<TARGET_FILE_DIR:fuser_decoder_tests>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:fuser_avutil>" "$<TARGET_FILE_DIR:fuser_decoder_tests>" VERBATIM)
  foreach(CODEC h264 hevc)
    add_test(NAME fuser_${CODEC}_hardware_decode COMMAND fuser_decoder_tests
      "${CMAKE_CURRENT_SOURCE_DIR}/build/fixtures/key-pattern.${CODEC}" ${CODEC})
    set_tests_properties(fuser_${CODEC}_hardware_decode PROPERTIES TIMEOUT 30
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_CURRENT_SOURCE_DIR}/build/test-runtime/$<CONFIG>")
    add_test(NAME fuser_${CODEC}_concurrent_present COMMAND fuser_decoder_tests
      "${CMAKE_CURRENT_SOURCE_DIR}/build/fixtures/inter-pattern.${CODEC}" ${CODEC} concurrent)
    set_tests_properties(fuser_${CODEC}_concurrent_present PROPERTIES TIMEOUT 30
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_CURRENT_SOURCE_DIR}/build/test-runtime/$<CONFIG>")
  endforeach()
endif()
