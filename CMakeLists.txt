cmake_minimum_required(VERSION 3.25)

project(CallOfJuarezVR VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Call of Juarez VR currently targets Windows only")
endif()

option(COJVR_BUILD_TESTS "Build host-side validation tests" ON)

add_library(cojvr_runtime STATIC
    src/runtime/build_catalog.cpp
    src/runtime/build_identity.cpp
    src/runtime/game_id.cpp
    src/runtime/host_identity.cpp
    src/runtime/log.cpp
)

target_include_directories(cojvr_runtime PUBLIC src)
target_compile_features(cojvr_runtime PUBLIC cxx_std_20)
target_compile_definitions(cojvr_runtime PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(cojvr_runtime PUBLIC bcrypt)

add_executable(cojvr_d3d9_probe tools/d3d9_probe.cpp)
target_compile_features(cojvr_d3d9_probe PRIVATE cxx_std_20)
target_compile_definitions(cojvr_d3d9_probe PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(cojvr_d3d9_probe PRIVATE d3d9)

if(MSVC)
    foreach(target cojvr_runtime cojvr_d3d9_probe)
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc)
    endforeach()
endif()

if(COJVR_BUILD_TESTS)
    enable_testing()

    add_executable(cojvr_runtime_tests tests/runtime_tests.cpp)
    target_link_libraries(cojvr_runtime_tests PRIVATE cojvr_runtime)
    target_compile_features(cojvr_runtime_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_runtime_tests PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME runtime_tests COMMAND cojvr_runtime_tests)

    add_test(NAME d3d9_probe COMMAND cojvr_d3d9_probe)
endif()
