cmake_minimum_required(VERSION 3.25)

project(CallOfJuarezVR VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Call of Juarez VR currently targets Windows only")
endif()

option(COJVR_BUILD_TESTS "Build host-side validation tests" ON)

set(
    COJVR_OPENVR_ROOT
    "${CMAKE_SOURCE_DIR}/.cache/deps/openvr-2.15.6"
    CACHE PATH
    "Extracted root of the pinned Valve OpenVR 2.15.6 SDK"
)

set(COJVR_OPENVR_INCLUDE_DIR "${COJVR_OPENVR_ROOT}/headers")
set(COJVR_OPENVR_IMPLIB "${COJVR_OPENVR_ROOT}/lib/win32/openvr_api.lib")
set(COJVR_OPENVR_DLL "${COJVR_OPENVR_ROOT}/bin/win32/openvr_api.dll")

foreach(required_path COJVR_OPENVR_INCLUDE_DIR COJVR_OPENVR_IMPLIB COJVR_OPENVR_DLL)
    if(NOT EXISTS "${${required_path}}")
        message(FATAL_ERROR
            "Pinned OpenVR 2.15.6 dependency is missing: ${${required_path}}\n"
            "Run tools/bootstrap_openvr.ps1 before configuring the project.")
    endif()
endforeach()

add_library(openvr_api SHARED IMPORTED GLOBAL)
set_target_properties(openvr_api PROPERTIES
    IMPORTED_IMPLIB "${COJVR_OPENVR_IMPLIB}"
    IMPORTED_LOCATION "${COJVR_OPENVR_DLL}"
    INTERFACE_INCLUDE_DIRECTORIES "${COJVR_OPENVR_INCLUDE_DIR}"
)

set(
    COJVR_OPENXR_ROOT
    "${CMAKE_SOURCE_DIR}/.cache/deps/OpenXR.Loader.1.1.63"
    CACHE PATH
    "Extracted root of the pinned Khronos OpenXR.Loader 1.1.63 package"
)

set(COJVR_OPENXR_INCLUDE_DIR "${COJVR_OPENXR_ROOT}/include")
set(COJVR_OPENXR_IMPLIB "${COJVR_OPENXR_ROOT}/native/Win32/release/lib/openxr_loader.lib")
set(COJVR_OPENXR_DLL "${COJVR_OPENXR_ROOT}/native/Win32/release/bin/openxr_loader.dll")

foreach(required_path COJVR_OPENXR_INCLUDE_DIR COJVR_OPENXR_IMPLIB COJVR_OPENXR_DLL)
    if(NOT EXISTS "${${required_path}}")
        message(FATAL_ERROR
            "Pinned OpenXR.Loader 1.1.63 dependency is missing: ${${required_path}}\n"
            "Run tools/bootstrap_openxr.ps1 before configuring the project.")
    endif()
endforeach()

add_library(openxr_loader SHARED IMPORTED GLOBAL)
set_target_properties(openxr_loader PROPERTIES
    IMPORTED_IMPLIB "${COJVR_OPENXR_IMPLIB}"
    IMPORTED_LOCATION "${COJVR_OPENXR_DLL}"
    INTERFACE_INCLUDE_DIRECTORIES "${COJVR_OPENXR_INCLUDE_DIR}"
)

add_library(cojvr_runtime STATIC
    src/runtime/build_catalog.cpp
    src/runtime/build_identity.cpp
    src/runtime/game_id.cpp
    src/runtime/host_identity.cpp
    src/runtime/log.cpp
    src/runtime/openxr_runtime.cpp
    src/runtime/vr_math.cpp
)

target_include_directories(cojvr_runtime PUBLIC src)
target_compile_features(cojvr_runtime PUBLIC cxx_std_20)
target_compile_definitions(cojvr_runtime PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(cojvr_runtime PUBLIC bcrypt openxr_loader)

add_library(cojvr_openvr_runtime STATIC
    src/runtime/openvr_runtime.cpp
)
target_include_directories(cojvr_openvr_runtime PUBLIC src)
target_compile_features(cojvr_openvr_runtime PUBLIC cxx_std_20)
target_compile_definitions(cojvr_openvr_runtime PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(cojvr_openvr_runtime PUBLIC cojvr_runtime PRIVATE openvr_api)

add_library(cojvr_openvr_d3d11_backend STATIC
    src/backends/openvr/d3d11_compositor.cpp
    src/backends/openvr/d3d11_session.cpp
)
target_include_directories(cojvr_openvr_d3d11_backend PUBLIC src)
target_compile_features(cojvr_openvr_d3d11_backend PUBLIC cxx_std_20)
target_compile_definitions(cojvr_openvr_d3d11_backend PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(cojvr_openvr_d3d11_backend PUBLIC cojvr_openvr_runtime PRIVATE d3d11 dxgi openvr_api)

add_executable(cojvr_openvr_probe tools/openvr_probe.cpp)
target_link_libraries(cojvr_openvr_probe PRIVATE cojvr_openvr_d3d11_backend)
target_compile_features(cojvr_openvr_probe PRIVATE cxx_std_20)
target_compile_definitions(cojvr_openvr_probe PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
add_custom_command(TARGET cojvr_openvr_probe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${COJVR_OPENVR_DLL}"
        "$<TARGET_FILE_DIR:cojvr_openvr_probe>/openvr_api.dll"
)

add_executable(cojvr_openxr_probe tools/openxr_probe.cpp)
target_link_libraries(cojvr_openxr_probe PRIVATE cojvr_runtime)
target_compile_features(cojvr_openxr_probe PRIVATE cxx_std_20)
target_compile_definitions(cojvr_openxr_probe PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
add_custom_command(TARGET cojvr_openxr_probe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${COJVR_OPENXR_DLL}"
        "$<TARGET_FILE_DIR:cojvr_openxr_probe>/openxr_loader.dll"
)

add_library(cojvr_openxr_d3d11_backend STATIC
    src/backends/openxr/d3d11_session.cpp
    src/backends/openxr/d3d11_swapchains.cpp
)
target_include_directories(cojvr_openxr_d3d11_backend PUBLIC src)
target_link_libraries(cojvr_openxr_d3d11_backend PUBLIC cojvr_runtime d3d11 dxgi)
target_compile_features(cojvr_openxr_d3d11_backend PUBLIC cxx_std_20)
target_compile_definitions(cojvr_openxr_d3d11_backend PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)

add_executable(cojvr_openxr_d3d11_probe tools/openxr_d3d11_probe.cpp)
target_link_libraries(cojvr_openxr_d3d11_probe PRIVATE cojvr_openxr_d3d11_backend)
target_compile_features(cojvr_openxr_d3d11_probe PRIVATE cxx_std_20)
target_compile_definitions(cojvr_openxr_d3d11_probe PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
add_custom_command(TARGET cojvr_openxr_d3d11_probe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${COJVR_OPENXR_DLL}"
        "$<TARGET_FILE_DIR:cojvr_openxr_d3d11_probe>/openxr_loader.dll"
)

add_executable(cojvr_d3d9_probe tools/d3d9_probe.cpp)
target_compile_features(cojvr_d3d9_probe PRIVATE cxx_std_20)
target_compile_definitions(cojvr_d3d9_probe PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(cojvr_d3d9_probe PRIVATE d3d9)

add_library(cojvr_d3d9_proxy SHARED
    src/backends/d3d9/classic_readback_bridge.cpp
    src/backends/d3d9/device_vtable_hook.cpp
    src/backends/d3d9/d3d9_proxy.cpp
    src/backends/d3d9/d3d9_proxy.def
    src/backends/d3d9/system_d3d9.cpp
)
target_include_directories(cojvr_d3d9_proxy PRIVATE src)
target_compile_features(cojvr_d3d9_proxy PRIVATE cxx_std_20)
target_compile_definitions(cojvr_d3d9_proxy PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(cojvr_d3d9_proxy PRIVATE cojvr_runtime d3d11 dxgi dxguid)
set_target_properties(cojvr_d3d9_proxy PROPERTIES OUTPUT_NAME d3d9)

add_library(cojvr_d3d9_readback_proxy SHARED
    src/backends/d3d9/classic_readback_bridge.cpp
    src/backends/d3d9/device_vtable_hook.cpp
    src/backends/d3d9/d3d9_proxy.cpp
    src/backends/d3d9/d3d9_readback_proxy.def
    src/backends/d3d9/system_d3d9.cpp
)
target_include_directories(cojvr_d3d9_readback_proxy PRIVATE src)
target_compile_features(cojvr_d3d9_readback_proxy PRIVATE cxx_std_20)
target_compile_definitions(
    cojvr_d3d9_readback_proxy
    PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE COJVR_D3D9_READBACK_ALWAYS_ON
)
target_link_libraries(cojvr_d3d9_readback_proxy PRIVATE cojvr_runtime d3d11 dxgi dxguid)
set_target_properties(cojvr_d3d9_readback_proxy PROPERTIES OUTPUT_NAME d3d9_readback)

add_library(cojvr_d3d9_openvr_flat_proxy SHARED
    src/backends/d3d9/classic_readback_bridge.cpp
    src/backends/d3d9/device_vtable_hook.cpp
    src/backends/d3d9/d3d9_proxy.cpp
    src/backends/d3d9/d3d9_openvr_flat_proxy.def
    src/backends/d3d9/openvr_flat_bridge.cpp
    src/backends/d3d9/system_d3d9.cpp
)
target_include_directories(cojvr_d3d9_openvr_flat_proxy PRIVATE src)
target_compile_features(cojvr_d3d9_openvr_flat_proxy PRIVATE cxx_std_20)
target_compile_definitions(
    cojvr_d3d9_openvr_flat_proxy
    PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE COJVR_D3D9_OPENVR_FLAT_ALWAYS_ON
)
target_link_libraries(
    cojvr_d3d9_openvr_flat_proxy
    PRIVATE cojvr_runtime cojvr_openvr_d3d11_backend d3d11 dxgi dxguid
)
set_target_properties(cojvr_d3d9_openvr_flat_proxy PROPERTIES OUTPUT_NAME d3d9_openvr_flat)
add_custom_command(TARGET cojvr_d3d9_openvr_flat_proxy POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${COJVR_OPENVR_DLL}"
        "$<TARGET_FILE_DIR:cojvr_d3d9_openvr_flat_proxy>/openvr_api.dll"
)

if(MSVC)
    foreach(target
        cojvr_runtime
        cojvr_openvr_runtime
        cojvr_openvr_d3d11_backend
        cojvr_openvr_probe
        cojvr_openxr_probe
        cojvr_openxr_d3d11_backend
        cojvr_openxr_d3d11_probe
        cojvr_d3d9_probe
        cojvr_d3d9_proxy
        cojvr_d3d9_readback_proxy
        cojvr_d3d9_openvr_flat_proxy
    )
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

    add_executable(cojvr_d3d9_proxy_smoke tests/d3d9_proxy_smoke.cpp)
    target_compile_features(cojvr_d3d9_proxy_smoke PRIVATE cxx_std_20)
    target_compile_definitions(cojvr_d3d9_proxy_smoke PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    if(MSVC)
        target_compile_options(cojvr_d3d9_proxy_smoke PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME d3d9_proxy_smoke COMMAND cojvr_d3d9_proxy_smoke $<TARGET_FILE:cojvr_d3d9_proxy>)

    add_test(
        NAME d3d9_readback_proxy_smoke
        COMMAND cojvr_d3d9_proxy_smoke
            $<TARGET_FILE:cojvr_d3d9_readback_proxy>
            --expect-readback
    )

    add_executable(cojvr_d3d9_proxy_ex_smoke tests/d3d9_proxy_ex_smoke.cpp)
    target_compile_features(cojvr_d3d9_proxy_ex_smoke PRIVATE cxx_std_20)
    target_compile_definitions(cojvr_d3d9_proxy_ex_smoke PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_link_libraries(cojvr_d3d9_proxy_ex_smoke PRIVATE dxguid)
    if(MSVC)
        target_compile_options(cojvr_d3d9_proxy_ex_smoke PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME d3d9_proxy_ex_smoke COMMAND cojvr_d3d9_proxy_ex_smoke $<TARGET_FILE:cojvr_d3d9_proxy>)
    set_tests_properties(d3d9_proxy_ex_smoke PROPERTIES ENVIRONMENT "COJVR_D3D9_EX_BRIDGE=1")

    add_executable(cojvr_d3d9_vtable_layout_test tests/d3d9_vtable_layout.cpp)
    target_compile_features(cojvr_d3d9_vtable_layout_test PRIVATE cxx_std_20)
    target_compile_definitions(cojvr_d3d9_vtable_layout_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(cojvr_d3d9_vtable_layout_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME d3d9_vtable_layout COMMAND cojvr_d3d9_vtable_layout_test)

    add_executable(
        cojvr_d3d9_device_hook_test
        tests/d3d9_device_hook.cpp
        src/backends/d3d9/device_vtable_hook.cpp
    )
    target_include_directories(cojvr_d3d9_device_hook_test PRIVATE src)
    target_compile_features(cojvr_d3d9_device_hook_test PRIVATE cxx_std_20)
    target_compile_definitions(
        cojvr_d3d9_device_hook_test
        PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
    )
    target_link_libraries(cojvr_d3d9_device_hook_test PRIVATE d3d9)
    if(MSVC)
        target_compile_options(
            cojvr_d3d9_device_hook_test
            PRIVATE /W4 /permissive- /EHsc
        )
    endif()
    add_test(NAME d3d9_device_hook COMMAND cojvr_d3d9_device_hook_test)

    add_executable(
        cojvr_d3d9_swapchain_hook_test
        tests/d3d9_swapchain_hook.cpp
        src/backends/d3d9/swapchain_vtable_hook.cpp
    )
    target_include_directories(cojvr_d3d9_swapchain_hook_test PRIVATE src)
    target_compile_features(cojvr_d3d9_swapchain_hook_test PRIVATE cxx_std_20)
    target_compile_definitions(
        cojvr_d3d9_swapchain_hook_test
        PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
    )
    target_link_libraries(cojvr_d3d9_swapchain_hook_test PRIVATE d3d9)
    if(MSVC)
        target_compile_options(
            cojvr_d3d9_swapchain_hook_test
            PRIVATE /W4 /permissive- /EHsc
        )
    endif()
    add_test(NAME d3d9_swapchain_hook COMMAND cojvr_d3d9_swapchain_hook_test)

    add_executable(cojvr_vr_math_test tests/vr_math.cpp)
    target_link_libraries(cojvr_vr_math_test PRIVATE cojvr_runtime)
    target_compile_features(cojvr_vr_math_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_vr_math_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME vr_math COMMAND cojvr_vr_math_test)

    add_executable(
        cojvr_d3d9_d3d11_shared_texture_test
        tests/d3d9_d3d11_shared_texture.cpp
        src/backends/d3d9/system_d3d9.cpp
    )
    target_include_directories(cojvr_d3d9_d3d11_shared_texture_test PRIVATE src)
    target_compile_features(cojvr_d3d9_d3d11_shared_texture_test PRIVATE cxx_std_20)
    target_compile_definitions(
        cojvr_d3d9_d3d11_shared_texture_test
        PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN
    )
    target_link_libraries(cojvr_d3d9_d3d11_shared_texture_test PRIVATE d3d11 dxgi)
    if(MSVC)
        target_compile_options(
            cojvr_d3d9_d3d11_shared_texture_test
            PRIVATE /W4 /permissive- /EHsc
        )
    endif()
    add_test(NAME d3d9_d3d11_shared_texture COMMAND cojvr_d3d9_d3d11_shared_texture_test)

    add_executable(cojvr_d3d9_classic_d3d11_shared_texture_test
        tests/d3d9_classic_d3d11_shared_texture.cpp
        src/backends/d3d9/system_d3d9.cpp
    )
    target_include_directories(cojvr_d3d9_classic_d3d11_shared_texture_test PRIVATE src)
    target_compile_features(cojvr_d3d9_classic_d3d11_shared_texture_test PRIVATE cxx_std_20)
    target_compile_definitions(cojvr_d3d9_classic_d3d11_shared_texture_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(cojvr_d3d9_classic_d3d11_shared_texture_test PRIVATE d3d11 dxgi)
    if(MSVC)
        target_compile_options(
            cojvr_d3d9_classic_d3d11_shared_texture_test
            PRIVATE /W4 /permissive- /EHsc
        )
    endif()
    add_test(NAME d3d9_classic_d3d11_shared_texture COMMAND cojvr_d3d9_classic_d3d11_shared_texture_test)

    add_executable(cojvr_d3d9_classic_readback_d3d11_test
        tests/d3d9_classic_readback_d3d11.cpp
        src/backends/d3d9/classic_readback_bridge.cpp
        src/backends/d3d9/system_d3d9.cpp
    )
    target_include_directories(cojvr_d3d9_classic_readback_d3d11_test PRIVATE src)
    target_compile_features(cojvr_d3d9_classic_readback_d3d11_test PRIVATE cxx_std_20)
    target_compile_definitions(cojvr_d3d9_classic_readback_d3d11_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(cojvr_d3d9_classic_readback_d3d11_test PRIVATE d3d11 dxgi)
    if(MSVC)
        target_compile_options(
            cojvr_d3d9_classic_readback_d3d11_test
            PRIVATE /W4 /permissive- /EHsc
        )
    endif()
    add_test(NAME d3d9_classic_readback_d3d11 COMMAND cojvr_d3d9_classic_readback_d3d11_test)
endif()
