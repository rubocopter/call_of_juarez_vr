cmake_minimum_required(VERSION 3.25)

project(CallOfJuarezVR VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Call of Juarez VR currently targets Windows only")
endif()

option(COJVR_BUILD_TESTS "Build host-side validation tests" ON)
option(COJVR_ENABLE_OPENVR "Build the OpenVR runtime/backend and probes" ON)
option(COJVR_ENABLE_OPENXR "Build the experimental OpenXR runtime/backend and probes" ON)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "Call of Juarez integration artifacts require an x86/Win32 build. "
        "Configure the Visual Studio generator with -A Win32 or use the win32-debug preset.")
endif()

if(COJVR_ENABLE_OPENVR)
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
endif()

if(COJVR_ENABLE_OPENXR)
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
endif()

add_library(cojvr_runtime STATIC
    src/runtime/gameplay_utility.cpp
    src/runtime/body_ik.cpp
    src/runtime/body_tracking.cpp
    src/runtime/pose_source.cpp
    src/runtime/vr_math.cpp
)

add_library(cojvr_build_identity STATIC
    src/runtime/build_catalog.cpp
    src/runtime/build_identity.cpp
    src/runtime/game_id.cpp
    src/runtime/host_identity.cpp
)

add_library(cojvr_diagnostics STATIC
    src/runtime/log.cpp
    src/runtime/structured_telemetry.cpp
)

add_library(cojvr_d3d9_hooks STATIC
    src/backends/d3d9/diagnostic_context.cpp
    src/backends/d3d9/device_vtable_hook.cpp
    src/backends/d3d9/factory_vtable_hook.cpp
    src/backends/d3d9/hook_registry.cpp
    src/backends/d3d9/swapchain_vtable_hook.cpp
    src/backends/d3d9/vtable_patch.cpp
)

add_library(cojvr_coj_camera_probe STATIC
    src/games/call_of_juarez/hud_boundary_probe.cpp
    src/games/call_of_juarez/hud_text_reader.cpp
    src/games/call_of_juarez/gameplay_ui_reader.cpp
    src/games/call_of_juarez/gameplay_ui_adapter.cpp
    src/games/call_of_juarez/focus_zoom.cpp
    src/games/call_of_juarez/motion_reload_owner.cpp
    src/games/call_of_juarez/camera_probe.cpp
    src/games/call_of_juarez/game_shutdown_hook.cpp
    src/games/call_of_juarez/body_adapter.cpp
    src/games/call_of_juarez/java_player_bridge.cpp
    src/games/call_of_juarez/native_fx_probe.cpp
)

target_include_directories(cojvr_runtime PUBLIC src)
target_compile_features(cojvr_runtime PUBLIC cxx_std_20)
target_compile_definitions(cojvr_runtime PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_include_directories(cojvr_build_identity PUBLIC src)
target_compile_features(cojvr_build_identity PUBLIC cxx_std_20)
target_compile_definitions(cojvr_build_identity PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(cojvr_build_identity PUBLIC bcrypt)
target_include_directories(cojvr_diagnostics PUBLIC src)
target_compile_features(cojvr_diagnostics PUBLIC cxx_std_20)
target_compile_definitions(cojvr_diagnostics PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_include_directories(cojvr_d3d9_hooks PUBLIC src)
target_compile_features(cojvr_d3d9_hooks PUBLIC cxx_std_20)
target_compile_definitions(cojvr_d3d9_hooks PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(cojvr_d3d9_hooks PUBLIC dxguid)
target_include_directories(cojvr_coj_camera_probe PUBLIC src)
target_compile_features(cojvr_coj_camera_probe PUBLIC cxx_std_20)
target_compile_definitions(cojvr_coj_camera_probe PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(cojvr_coj_camera_probe PUBLIC cojvr_build_identity cojvr_d3d9_hooks cojvr_runtime)

if(COJVR_ENABLE_OPENVR)
    add_library(cojvr_openvr_runtime STATIC
        src/runtime/openvr_runtime.cpp
    )
    target_include_directories(cojvr_openvr_runtime PUBLIC src)
    target_compile_features(cojvr_openvr_runtime PUBLIC cxx_std_20)
    target_compile_definitions(cojvr_openvr_runtime PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_link_libraries(cojvr_openvr_runtime PUBLIC cojvr_runtime PRIVATE openvr_api)

    add_library(cojvr_openvr_d3d11_backend STATIC
        src/backends/openvr/d3d11_compositor.cpp
        src/backends/openvr/d3d9_shared_texture_bridge.cpp
        src/backends/openvr/d3d11_sync.cpp
        src/backends/openvr/d3d11_session.cpp
        src/backends/openvr/stereo_presenter.cpp
        src/backends/openvr/hud_text_overlay.cpp
        src/backends/openvr/gameplay_ui_raster.cpp
        src/backends/d3d9/frame_mailbox.cpp
    )
    target_include_directories(cojvr_openvr_d3d11_backend PUBLIC src)
    target_compile_features(cojvr_openvr_d3d11_backend PUBLIC cxx_std_20)
    target_compile_definitions(cojvr_openvr_d3d11_backend PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_link_libraries(cojvr_openvr_d3d11_backend PUBLIC cojvr_openvr_runtime PRIVATE d3d11 dxgi openvr_api gdi32 d3dcompiler)

    add_executable(cojvr_openvr_probe tools/openvr_probe.cpp)
    target_link_libraries(cojvr_openvr_probe PRIVATE cojvr_openvr_d3d11_backend)
    target_compile_features(cojvr_openvr_probe PRIVATE cxx_std_20)
    target_compile_definitions(cojvr_openvr_probe PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    add_custom_command(TARGET cojvr_openvr_probe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${COJVR_OPENVR_DLL}"
            "$<TARGET_FILE_DIR:cojvr_openvr_probe>/openvr_api.dll"
    )
endif()

if(COJVR_ENABLE_OPENXR)
    add_library(cojvr_openxr_runtime STATIC
        src/runtime/openxr_runtime.cpp
    )
    target_include_directories(cojvr_openxr_runtime PUBLIC src)
    target_compile_features(cojvr_openxr_runtime PUBLIC cxx_std_20)
    target_compile_definitions(cojvr_openxr_runtime PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_link_libraries(cojvr_openxr_runtime PUBLIC cojvr_runtime openxr_loader)

    add_executable(cojvr_openxr_probe tools/openxr_probe.cpp)
    target_link_libraries(cojvr_openxr_probe PRIVATE cojvr_openxr_runtime)
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
    target_link_libraries(cojvr_openxr_d3d11_backend PUBLIC cojvr_openxr_runtime d3d11 dxgi)
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
endif()

add_executable(
    cojvr_d3d9_probe
    tools/d3d9_probe.cpp
    src/backends/d3d9/system_d3d9.cpp
)
target_include_directories(cojvr_d3d9_probe PRIVATE src)
target_compile_features(cojvr_d3d9_probe PRIVATE cxx_std_20)
target_compile_definitions(cojvr_d3d9_probe PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)

add_library(cojvr_d3d9_proxy SHARED
    src/backends/d3d9/classic_readback_bridge.cpp
    src/backends/d3d9/d3d9_proxy.cpp
    src/backends/d3d9/d3d9_proxy.def
    src/backends/d3d9/system_d3d9.cpp
)
target_include_directories(cojvr_d3d9_proxy PRIVATE src)
target_compile_features(cojvr_d3d9_proxy PRIVATE cxx_std_20)
target_compile_definitions(cojvr_d3d9_proxy PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(cojvr_d3d9_proxy PRIVATE cojvr_build_identity cojvr_diagnostics cojvr_d3d9_hooks d3d11 dxgi dxguid)
set_target_properties(cojvr_d3d9_proxy PROPERTIES OUTPUT_NAME d3d9)

add_library(cojvr_d3d9_pool_probe SHARED
    src/backends/d3d9/d3d9_pool_probe.cpp
    src/backends/d3d9/d3d9_pool_probe.def
)
target_include_directories(cojvr_d3d9_pool_probe PRIVATE src)
target_compile_features(cojvr_d3d9_pool_probe PRIVATE cxx_std_20)
target_compile_definitions(cojvr_d3d9_pool_probe PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(cojvr_d3d9_pool_probe PRIVATE cojvr_d3d9_hooks)
set_target_properties(cojvr_d3d9_pool_probe PROPERTIES OUTPUT_NAME d3d9_pool_probe)

add_library(cojvr_d3d9_readback_proxy SHARED
    src/backends/d3d9/classic_readback_bridge.cpp
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
target_link_libraries(cojvr_d3d9_readback_proxy PRIVATE cojvr_build_identity cojvr_diagnostics cojvr_d3d9_hooks d3d11 dxgi dxguid)
set_target_properties(cojvr_d3d9_readback_proxy PROPERTIES OUTPUT_NAME d3d9_readback)

add_library(cojvr_d3d9_camera_probe_proxy SHARED
    src/games/call_of_juarez/camera_probe_proxy.cpp
    src/games/call_of_juarez/d3d9_camera_probe_proxy.def
    src/backends/d3d9/system_d3d9.cpp
)
target_include_directories(cojvr_d3d9_camera_probe_proxy PRIVATE src)
target_compile_features(cojvr_d3d9_camera_probe_proxy PRIVATE cxx_std_20)
target_compile_definitions(
    cojvr_d3d9_camera_probe_proxy
    PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
)
target_link_libraries(
    cojvr_d3d9_camera_probe_proxy
    PRIVATE cojvr_coj_camera_probe cojvr_diagnostics
)
set_target_properties(cojvr_d3d9_camera_probe_proxy PROPERTIES OUTPUT_NAME d3d9_camera_probe)

if(COJVR_ENABLE_OPENVR)
    add_library(cojvr_d3d9_hmd_camera_proxy SHARED
        src/games/call_of_juarez/camera_probe_proxy.cpp
        src/games/call_of_juarez/d3d9_hmd_camera_proxy.def
        src/backends/d3d9/system_d3d9.cpp
    )
    target_include_directories(cojvr_d3d9_hmd_camera_proxy PRIVATE src)
    target_compile_features(cojvr_d3d9_hmd_camera_proxy PRIVATE cxx_std_20)
    target_compile_definitions(
        cojvr_d3d9_hmd_camera_proxy
        PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE COJVR_CAMERA_HMD_OPENVR
    )
    target_link_libraries(
        cojvr_d3d9_hmd_camera_proxy
        PRIVATE cojvr_coj_camera_probe cojvr_diagnostics cojvr_openvr_runtime
    )
    set_target_properties(cojvr_d3d9_hmd_camera_proxy PROPERTIES OUTPUT_NAME d3d9_hmd_camera)
    add_custom_command(TARGET cojvr_d3d9_hmd_camera_proxy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${COJVR_OPENVR_DLL}"
            "$<TARGET_FILE_DIR:cojvr_d3d9_hmd_camera_proxy>/openvr_api.dll"
    )

    add_library(cojvr_d3d9_native_stereo_proxy SHARED
        src/games/call_of_juarez/native_stereo_proxy.cpp
        src/games/call_of_juarez/d3d9ex_legacy_resource_compat.cpp
        src/games/call_of_juarez/d3d9_native_stereo_proxy.def
        src/backends/d3d9/d3d9_stereo_capture.cpp
        src/backends/d3d9/system_d3d9.cpp
    )
    target_include_directories(cojvr_d3d9_native_stereo_proxy PRIVATE src)
    target_compile_features(cojvr_d3d9_native_stereo_proxy PRIVATE cxx_std_20)
    target_compile_definitions(
        cojvr_d3d9_native_stereo_proxy
        PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
    )
    target_link_libraries(
        cojvr_d3d9_native_stereo_proxy
        PRIVATE
            cojvr_coj_camera_probe
            cojvr_diagnostics
            cojvr_d3d9_hooks
            cojvr_openvr_d3d11_backend
            d3d11
            dxgi
            dxguid
    )
    set_target_properties(
        cojvr_d3d9_native_stereo_proxy PROPERTIES OUTPUT_NAME d3d9_native_stereo)
    add_custom_command(TARGET cojvr_d3d9_native_stereo_proxy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${COJVR_OPENVR_DLL}"
            "$<TARGET_FILE_DIR:cojvr_d3d9_native_stereo_proxy>/openvr_api.dll"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:cojvr_d3d9_native_stereo_proxy>/cojvr_openvr_input/bindings"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/openvr/actions.json"
            "$<TARGET_FILE_DIR:cojvr_d3d9_native_stereo_proxy>/cojvr_openvr_input/actions.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/openvr/bindings/psvr2_sense.json"
            "$<TARGET_FILE_DIR:cojvr_d3d9_native_stereo_proxy>/cojvr_openvr_input/bindings/psvr2_sense.json"
    )

    add_library(cojvr_d3d9_openvr_flat_proxy SHARED
        src/backends/d3d9/classic_readback_bridge.cpp
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
        PRIVATE cojvr_build_identity cojvr_diagnostics cojvr_d3d9_hooks cojvr_openvr_d3d11_backend d3d11 dxgi dxguid
    )
    set_target_properties(cojvr_d3d9_openvr_flat_proxy PROPERTIES OUTPUT_NAME d3d9_openvr_flat)
    add_custom_command(TARGET cojvr_d3d9_openvr_flat_proxy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${COJVR_OPENVR_DLL}"
            "$<TARGET_FILE_DIR:cojvr_d3d9_openvr_flat_proxy>/openvr_api.dll"
    )
endif()

if(MSVC)
    set(cojvr_warning_targets
        cojvr_runtime
        cojvr_build_identity
        cojvr_diagnostics
        cojvr_d3d9_hooks
        cojvr_coj_camera_probe
        cojvr_d3d9_probe
        cojvr_d3d9_proxy
        cojvr_d3d9_readback_proxy
        cojvr_d3d9_camera_probe_proxy
    )
    if(COJVR_ENABLE_OPENVR)
        list(APPEND cojvr_warning_targets
            cojvr_openvr_runtime
            cojvr_openvr_d3d11_backend
            cojvr_openvr_probe
            cojvr_d3d9_hmd_camera_proxy
            cojvr_d3d9_native_stereo_proxy
            cojvr_d3d9_openvr_flat_proxy
        )
    endif()
    if(COJVR_ENABLE_OPENXR)
        list(APPEND cojvr_warning_targets
            cojvr_openxr_runtime
            cojvr_openxr_probe
            cojvr_openxr_d3d11_backend
            cojvr_openxr_d3d11_probe
        )
    endif()
    foreach(target IN LISTS cojvr_warning_targets)
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc)
    endforeach()
endif()

if(COJVR_BUILD_TESTS)
    enable_testing()
    add_executable(cojvr_d3d9_pool_probe_smoke tests/d3d9_pool_probe_smoke.cpp
        src/backends/d3d9/system_d3d9.cpp)
    target_include_directories(cojvr_d3d9_pool_probe_smoke PRIVATE src)
    target_compile_features(cojvr_d3d9_pool_probe_smoke PRIVATE cxx_std_20)
    target_compile_definitions(cojvr_d3d9_pool_probe_smoke PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    add_test(NAME d3d9_pool_probe_smoke COMMAND cojvr_d3d9_pool_probe_smoke $<TARGET_FILE:cojvr_d3d9_pool_probe>)
    set_tests_properties(d3d9_pool_probe_smoke PROPERTIES SKIP_RETURN_CODE 77)

    find_program(COJVR_POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)
    find_program(COJVR_PYTHON_EXECUTABLE NAMES python REQUIRED)
    add_test(
        NAME deployment_recovery
        COMMAND "${COJVR_POWERSHELL_EXECUTABLE}" -NoProfile
            -File "${CMAKE_SOURCE_DIR}/tests/deployment_recovery.ps1"
            -SourceDirectory "${CMAKE_SOURCE_DIR}"
            -BinaryDirectory "${CMAKE_BINARY_DIR}"
    )
    if(COJVR_ENABLE_OPENVR)
        add_test(
            NAME bootstrap_openvr
            COMMAND "${COJVR_POWERSHELL_EXECUTABLE}" -NoProfile
                -File "${CMAKE_SOURCE_DIR}/tests/bootstrap_openvr.ps1"
                -SourceDirectory "${CMAKE_SOURCE_DIR}"
                -BinaryDirectory "${CMAKE_BINARY_DIR}"
        )
    endif()
    add_test(
        NAME vr_video_profile
        COMMAND "${COJVR_POWERSHELL_EXECUTABLE}"
            -NoProfile
            -File "${CMAKE_SOURCE_DIR}/tests/vr_video_profile.ps1"
            -SourceDirectory "${CMAKE_SOURCE_DIR}"
            -BinaryDirectory "${CMAKE_BINARY_DIR}"
    )
    add_test(NAME weapon_consumers COMMAND "${COJVR_PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/weapon_consumers.py")
    add_test(NAME reload_wait_probe COMMAND "${COJVR_PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/reload_wait_probe.py")
    add_test(NAME manual_reload_native COMMAND "${COJVR_PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/manual_reload_native.py")
    add_test(NAME manual_reload_analysis COMMAND "${COJVR_PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/manual_reload_analysis.py")
    add_test(NAME reload_cartridge_import COMMAND "${COJVR_PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/import_reload_cartridge.py" --check)
    add_test(NAME openvr_action_assets COMMAND "${COJVR_PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/openvr_action_assets.py")
    add_test(NAME player_mesh_split COMMAND "${COJVR_PYTHON_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/player_mesh_split.py")
    add_test(
        NAME provenance_tools
        COMMAND "${COJVR_POWERSHELL_EXECUTABLE}"
            -NoProfile
            -File "${CMAKE_SOURCE_DIR}/tests/provenance_tools.ps1"
            -SourceDirectory "${CMAKE_SOURCE_DIR}"
            -BinaryDirectory "${CMAKE_BINARY_DIR}"
    )

    add_test(
        NAME render_telemetry_tools
        COMMAND "${COJVR_POWERSHELL_EXECUTABLE}"
            -NoProfile
            -File "${CMAKE_SOURCE_DIR}/tests/render_telemetry_tools.ps1"
            -SourceDirectory "${CMAKE_SOURCE_DIR}"
            -BinaryDirectory "${CMAKE_BINARY_DIR}"
    )

    add_test(
        NAME movement_diagnostic_tools
        COMMAND "${COJVR_POWERSHELL_EXECUTABLE}"
            -NoProfile
            -File "${CMAKE_SOURCE_DIR}/tests/movement_diagnostic_tools.ps1"
            -SourceDirectory "${CMAKE_SOURCE_DIR}"
            -BinaryDirectory "${CMAKE_BINARY_DIR}"
    )

    add_executable(cojvr_build_identity_tests tests/runtime_tests.cpp)
    target_link_libraries(cojvr_build_identity_tests PRIVATE cojvr_build_identity)
    target_compile_features(cojvr_build_identity_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_build_identity_tests PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME build_identity_tests COMMAND cojvr_build_identity_tests)

    add_executable(cojvr_large_address_space_fixture tests/large_address_space.cpp)
    target_compile_features(cojvr_large_address_space_fixture PRIVATE cxx_std_20)
    target_compile_definitions(cojvr_large_address_space_fixture PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_link_options(cojvr_large_address_space_fixture PRIVATE /LARGEADDRESSAWARE:NO)
    endif()
    add_test(NAME large_address_space COMMAND pwsh -NoProfile -File
        "${CMAKE_SOURCE_DIR}/tests/large_address_space.ps1"
        -FixturePath "$<TARGET_FILE:cojvr_large_address_space_fixture>")
    set_tests_properties(large_address_space PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    add_test(NAME coj_large_address COMMAND python "${CMAKE_SOURCE_DIR}/tests/coj_large_address.py")
    add_test(NAME coj_executable_identity COMMAND pwsh -NoProfile -File
        "${CMAKE_SOURCE_DIR}/tests/coj_executable_identity.ps1")

    add_test(NAME d3d9_probe COMMAND cojvr_d3d9_probe)
    set_tests_properties(d3d9_probe PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(cojvr_d3d9_proxy_smoke tests/d3d9_proxy_smoke.cpp)
    target_compile_features(cojvr_d3d9_proxy_smoke PRIVATE cxx_std_20)
    target_compile_definitions(cojvr_d3d9_proxy_smoke PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    if(MSVC)
        target_compile_options(cojvr_d3d9_proxy_smoke PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(
        NAME d3d9_proxy_smoke
        COMMAND "${COJVR_POWERSHELL_EXECUTABLE}"
            -NoProfile
            -File "${CMAKE_SOURCE_DIR}/tests/run_proxy_smoke.ps1"
            -TestExecutable $<TARGET_FILE:cojvr_d3d9_proxy_smoke>
            -ProxyPath $<TARGET_FILE:cojvr_d3d9_proxy>
            -WorkingRoot "${CMAKE_BINARY_DIR}/test-work"
    )

    add_test(
        NAME d3d9_readback_proxy_smoke
        COMMAND "${COJVR_POWERSHELL_EXECUTABLE}"
            -NoProfile
            -File "${CMAKE_SOURCE_DIR}/tests/run_proxy_smoke.ps1"
            -TestExecutable $<TARGET_FILE:cojvr_d3d9_proxy_smoke>
            -ProxyPath $<TARGET_FILE:cojvr_d3d9_readback_proxy>
            -SmokeArgument "--expect-readback"
            -WorkingRoot "${CMAKE_BINARY_DIR}/test-work"
    )

    add_executable(cojvr_d3d9_proxy_ex_smoke tests/d3d9_proxy_ex_smoke.cpp)
    target_compile_features(cojvr_d3d9_proxy_ex_smoke PRIVATE cxx_std_20)
    target_compile_definitions(cojvr_d3d9_proxy_ex_smoke PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_link_libraries(cojvr_d3d9_proxy_ex_smoke PRIVATE dxguid)
    if(MSVC)
        target_compile_options(cojvr_d3d9_proxy_ex_smoke PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(
        NAME d3d9_proxy_ex_smoke
        COMMAND "${COJVR_POWERSHELL_EXECUTABLE}"
            -NoProfile
            -File "${CMAKE_SOURCE_DIR}/tests/run_proxy_smoke.ps1"
            -TestExecutable $<TARGET_FILE:cojvr_d3d9_proxy_ex_smoke>
            -ProxyPath $<TARGET_FILE:cojvr_d3d9_proxy>
            -WorkingRoot "${CMAKE_BINARY_DIR}/test-work"
    )
    set_tests_properties(
        d3d9_proxy_smoke
        d3d9_readback_proxy_smoke
        d3d9_proxy_ex_smoke
        PROPERTIES SKIP_RETURN_CODE 77
    )
    set_tests_properties(d3d9_proxy_ex_smoke PROPERTIES ENVIRONMENT "COJVR_D3D9_EX_BRIDGE=1")

    add_executable(cojvr_d3d9_vtable_layout_test tests/d3d9_vtable_layout.cpp)
    target_compile_features(cojvr_d3d9_vtable_layout_test PRIVATE cxx_std_20)
    target_compile_definitions(cojvr_d3d9_vtable_layout_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(cojvr_d3d9_vtable_layout_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME d3d9_vtable_layout COMMAND cojvr_d3d9_vtable_layout_test)

    add_executable(cojvr_hook_registry_test tests/hook_registry.cpp)
    target_link_libraries(cojvr_hook_registry_test PRIVATE cojvr_d3d9_hooks)
    target_compile_features(cojvr_hook_registry_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_hook_registry_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME hook_registry COMMAND cojvr_hook_registry_test)

    add_executable(cojvr_structured_telemetry_test tests/structured_telemetry.cpp)
    target_link_libraries(cojvr_structured_telemetry_test PRIVATE cojvr_diagnostics)
    target_compile_features(cojvr_structured_telemetry_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_structured_telemetry_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME structured_telemetry COMMAND cojvr_structured_telemetry_test)

    add_executable(cojvr_camera_probe_test tests/camera_probe.cpp)
    target_link_libraries(cojvr_camera_probe_test PRIVATE cojvr_coj_camera_probe)
    target_compile_features(cojvr_camera_probe_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_camera_probe_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME camera_probe COMMAND cojvr_camera_probe_test)

    add_executable(cojvr_body_adapter_test tests/body_adapter.cpp)
    add_executable(cojvr_hud_boundary_probe_test tests/hud_boundary_probe.cpp)
    add_executable(cojvr_hud_text_reader_test tests/hud_text_reader.cpp src/games/call_of_juarez/hud_text_reader.cpp)
    target_link_libraries(cojvr_hud_text_reader_test PRIVATE cojvr_runtime)
    if(MSVC)
        target_compile_options(cojvr_hud_text_reader_test PRIVATE /utf-8)
    endif()
    add_test(NAME hud_text_reader COMMAND cojvr_hud_text_reader_test)
    add_executable(cojvr_hud_text_overlay_test tests/hud_text_overlay.cpp src/backends/openvr/hud_text_overlay.cpp src/backends/openvr/gameplay_ui_raster.cpp)
    target_link_libraries(cojvr_hud_text_overlay_test PRIVATE cojvr_runtime gdi32 d3d11 d3dcompiler)
    if(MSVC)
        target_compile_options(cojvr_hud_text_overlay_test PRIVATE /utf-8)
    endif()
    add_test(NAME hud_text_overlay COMMAND cojvr_hud_text_overlay_test)
    add_executable(cojvr_gameplay_ui_raster_test tests/gameplay_ui_raster.cpp src/backends/openvr/gameplay_ui_raster.cpp)
    target_link_libraries(cojvr_gameplay_ui_raster_test PRIVATE cojvr_runtime gdi32)
    if(MSVC)
        target_compile_options(cojvr_gameplay_ui_raster_test PRIVATE /utf-8)
    endif()
    add_test(NAME gameplay_ui_raster COMMAND cojvr_gameplay_ui_raster_test)
    add_executable(cojvr_gameplay_ui_adapter_test tests/gameplay_ui_adapter.cpp src/games/call_of_juarez/gameplay_ui_adapter.cpp)
    target_link_libraries(cojvr_gameplay_ui_adapter_test PRIVATE cojvr_runtime)
    add_test(NAME gameplay_ui_adapter COMMAND cojvr_gameplay_ui_adapter_test)
    add_executable(cojvr_gameplay_ui_reader_test tests/gameplay_ui_reader.cpp src/games/call_of_juarez/gameplay_ui_reader.cpp)
    target_link_libraries(cojvr_gameplay_ui_reader_test PRIVATE cojvr_runtime)
    if(MSVC)
        target_compile_options(cojvr_gameplay_ui_reader_test PRIVATE /utf-8)
    endif()
    add_test(NAME gameplay_ui_reader COMMAND cojvr_gameplay_ui_reader_test)

    add_executable(cojvr_focus_zoom_test tests/focus_zoom.cpp src/games/call_of_juarez/focus_zoom.cpp)
    target_link_libraries(cojvr_focus_zoom_test PRIVATE cojvr_runtime)
    add_test(NAME focus_zoom COMMAND cojvr_focus_zoom_test)
    target_link_libraries(cojvr_hud_boundary_probe_test PRIVATE cojvr_coj_camera_probe)
    add_test(NAME hud_boundary_probe COMMAND cojvr_hud_boundary_probe_test)
    add_executable(cojvr_gameplay_utility_test tests/gameplay_utility.cpp)
    target_link_libraries(cojvr_gameplay_utility_test PRIVATE cojvr_runtime)
    add_test(NAME gameplay_utility COMMAND cojvr_gameplay_utility_test)
    add_executable(cojvr_ui_haptics_test tests/ui_haptics.cpp)
    target_include_directories(cojvr_ui_haptics_test PRIVATE src)
    target_compile_features(cojvr_ui_haptics_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_ui_haptics_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME ui_haptics COMMAND cojvr_ui_haptics_test)

    add_executable(cojvr_reload_haptics_test tests/reload_haptics.cpp)
    target_include_directories(cojvr_reload_haptics_test PRIVATE src)
    target_compile_features(cojvr_reload_haptics_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_reload_haptics_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME reload_haptics COMMAND cojvr_reload_haptics_test)
    add_executable(cojvr_reload_gesture_test tests/reload_gesture.cpp)
    target_link_libraries(cojvr_reload_gesture_test PRIVATE cojvr_runtime)
    target_compile_features(cojvr_reload_gesture_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_reload_gesture_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME reload_gesture COMMAND cojvr_reload_gesture_test)
    add_executable(cojvr_reload_visual_test tests/reload_visual.cpp
        src/backends/openvr/hud_text_overlay.cpp src/backends/openvr/gameplay_ui_raster.cpp)
    target_link_libraries(cojvr_reload_visual_test PRIVATE cojvr_runtime d3d11 d3dcompiler gdi32)
    target_compile_features(cojvr_reload_visual_test PRIVATE cxx_std_20)
    add_test(NAME reload_visual COMMAND cojvr_reload_visual_test)
    add_executable(cojvr_interaction_gaze_test tests/interaction_gaze.cpp
        src/backends/openvr/hud_text_overlay.cpp src/backends/openvr/gameplay_ui_raster.cpp)
    target_link_libraries(cojvr_interaction_gaze_test PRIVATE cojvr_runtime d3d11 d3dcompiler gdi32)
    target_compile_features(cojvr_interaction_gaze_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_interaction_gaze_test PRIVATE /W4 /permissive- /EHsc /utf-8)
    endif()
    add_test(NAME interaction_gaze COMMAND cojvr_interaction_gaze_test)
    add_executable(cojvr_motion_reload_owner_test tests/motion_reload_owner.cpp src/games/call_of_juarez/motion_reload_owner.cpp)
    target_link_libraries(cojvr_motion_reload_owner_test PRIVATE cojvr_runtime)
    target_compile_features(cojvr_motion_reload_owner_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_motion_reload_owner_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME motion_reload_owner COMMAND cojvr_motion_reload_owner_test)
    add_executable(cojvr_reload_trace_test tests/reload_trace.cpp src/games/call_of_juarez/motion_reload_owner.cpp)
    target_link_libraries(cojvr_reload_trace_test PRIVATE cojvr_runtime)
    target_compile_features(cojvr_reload_trace_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_reload_trace_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME reload_trace COMMAND cojvr_reload_trace_test)
    add_executable(cojvr_motion_reload_adapter_test tests/motion_reload_adapter.cpp)
    target_link_libraries(cojvr_motion_reload_adapter_test PRIVATE cojvr_runtime)
    target_compile_features(cojvr_motion_reload_adapter_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_motion_reload_adapter_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME motion_reload_adapter COMMAND cojvr_motion_reload_adapter_test)
    add_executable(cojvr_manual_reload_session_test tests/manual_reload_session.cpp)
    target_link_libraries(cojvr_manual_reload_session_test PRIVATE cojvr_runtime)
    target_compile_features(cojvr_manual_reload_session_test PRIVATE cxx_std_20)
    add_test(NAME manual_reload_session COMMAND cojvr_manual_reload_session_test)
    add_executable(cojvr_reload_geometry_test tests/reload_geometry.cpp)
    target_link_libraries(cojvr_reload_geometry_test PRIVATE cojvr_runtime)
    target_compile_features(cojvr_reload_geometry_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_reload_geometry_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME reload_geometry COMMAND cojvr_reload_geometry_test)
    add_executable(cojvr_presentation_owner_test tests/presentation_owner.cpp)
    target_link_libraries(cojvr_presentation_owner_test PRIVATE cojvr_runtime)
    target_compile_features(cojvr_presentation_owner_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_presentation_owner_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME presentation_owner COMMAND cojvr_presentation_owner_test)
    add_executable(cojvr_interaction_selection_test tests/interaction_selection.cpp)
    target_include_directories(cojvr_interaction_selection_test PRIVATE src)
    target_compile_features(cojvr_interaction_selection_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_interaction_selection_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME interaction_selection COMMAND cojvr_interaction_selection_test)
    target_link_libraries(cojvr_body_adapter_test PRIVATE cojvr_coj_camera_probe)
    target_compile_features(cojvr_body_adapter_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_body_adapter_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME body_adapter COMMAND cojvr_body_adapter_test)

    add_executable(cojvr_independent_hand_test tests/independent_hand.cpp)
    target_link_libraries(cojvr_independent_hand_test PRIVATE cojvr_coj_camera_probe)
    target_compile_features(cojvr_independent_hand_test PRIVATE cxx_std_20)
    add_test(NAME independent_hand COMMAND cojvr_independent_hand_test)

    add_executable(cojvr_reload_pinch_pose_test tests/reload_pinch_pose.cpp)
    target_include_directories(cojvr_reload_pinch_pose_test PRIVATE src)
    target_compile_features(cojvr_reload_pinch_pose_test PRIVATE cxx_std_20)
    add_test(NAME reload_pinch_pose COMMAND cojvr_reload_pinch_pose_test)

    add_executable(
        cojvr_d3d9_factory_hook_test
        tests/d3d9_factory_hook.cpp
        src/backends/d3d9/system_d3d9.cpp
    )
    target_include_directories(cojvr_d3d9_factory_hook_test PRIVATE src)
    target_link_libraries(cojvr_d3d9_factory_hook_test PRIVATE cojvr_d3d9_hooks)
    target_compile_features(cojvr_d3d9_factory_hook_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_d3d9_factory_hook_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME d3d9_factory_hook COMMAND cojvr_d3d9_factory_hook_test)
    set_tests_properties(d3d9_factory_hook PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(cojvr_d3d9ex_startup_contract_test
        tests/d3d9ex_startup_contract.cpp
        src/backends/d3d9/system_d3d9.cpp)
    target_include_directories(cojvr_d3d9ex_startup_contract_test PRIVATE src)
    target_link_libraries(cojvr_d3d9ex_startup_contract_test PRIVATE cojvr_d3d9_hooks)
    target_compile_features(cojvr_d3d9ex_startup_contract_test PRIVATE cxx_std_20)
    target_compile_definitions(cojvr_d3d9ex_startup_contract_test
        PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    if(MSVC)
        target_compile_options(cojvr_d3d9ex_startup_contract_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME d3d9ex_startup_contract COMMAND cojvr_d3d9ex_startup_contract_test)
    set_tests_properties(d3d9ex_startup_contract PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(
        cojvr_d3d9_device_hook_test
        tests/d3d9_device_hook.cpp
        src/backends/d3d9/system_d3d9.cpp
    )
    target_include_directories(cojvr_d3d9_device_hook_test PRIVATE src)
    target_compile_features(cojvr_d3d9_device_hook_test PRIVATE cxx_std_20)
    target_compile_definitions(
        cojvr_d3d9_device_hook_test
        PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
    )
    target_link_libraries(cojvr_d3d9_device_hook_test PRIVATE cojvr_d3d9_hooks)
    if(MSVC)
        target_compile_options(
            cojvr_d3d9_device_hook_test
            PRIVATE /W4 /permissive- /EHsc
        )
    endif()
    add_test(NAME d3d9_device_hook COMMAND cojvr_d3d9_device_hook_test)
    set_tests_properties(d3d9_device_hook PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(
        cojvr_d3d9_swapchain_hook_test
        tests/d3d9_swapchain_hook.cpp
        src/backends/d3d9/system_d3d9.cpp
    )
    target_include_directories(cojvr_d3d9_swapchain_hook_test PRIVATE src)
    target_compile_features(cojvr_d3d9_swapchain_hook_test PRIVATE cxx_std_20)
    target_compile_definitions(
        cojvr_d3d9_swapchain_hook_test
        PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
    )
    target_link_libraries(cojvr_d3d9_swapchain_hook_test PRIVATE cojvr_d3d9_hooks)
    if(MSVC)
        target_compile_options(
            cojvr_d3d9_swapchain_hook_test
            PRIVATE /W4 /permissive- /EHsc
        )
    endif()
    add_test(NAME d3d9_swapchain_hook COMMAND cojvr_d3d9_swapchain_hook_test)
    set_tests_properties(d3d9_swapchain_hook PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(cojvr_vr_math_test tests/vr_math.cpp)
    target_link_libraries(cojvr_vr_math_test PRIVATE cojvr_runtime)
    target_compile_features(cojvr_vr_math_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_vr_math_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME vr_math COMMAND cojvr_vr_math_test)

    add_executable(
        cojvr_frame_mailbox_test
        tests/frame_mailbox.cpp
        src/backends/d3d9/frame_mailbox.cpp
    )

    target_include_directories(cojvr_frame_mailbox_test PRIVATE src)
    target_compile_features(cojvr_frame_mailbox_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_frame_mailbox_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME frame_mailbox COMMAND cojvr_frame_mailbox_test)

    add_executable(
        cojvr_presentation_cadence_test
        tests/presentation_cadence.cpp
    )
    target_include_directories(cojvr_presentation_cadence_test PRIVATE src)
    target_compile_features(cojvr_presentation_cadence_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_presentation_cadence_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME presentation_cadence COMMAND cojvr_presentation_cadence_test)

    add_executable(cojvr_hmd_frame_pacing_test tests/hmd_frame_pacing.cpp)
    target_include_directories(cojvr_hmd_frame_pacing_test PRIVATE src)
    target_compile_features(cojvr_hmd_frame_pacing_test PRIVATE cxx_std_20)
    target_compile_definitions(cojvr_hmd_frame_pacing_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    add_test(NAME hmd_frame_pacing COMMAND cojvr_hmd_frame_pacing_test)

    add_executable(cojvr_ui_pointer_ownership_test tests/ui_pointer_ownership.cpp)
    target_link_libraries(cojvr_ui_pointer_ownership_test PRIVATE cojvr_runtime)
    target_include_directories(cojvr_ui_pointer_ownership_test PRIVATE src)
    target_compile_features(cojvr_ui_pointer_ownership_test PRIVATE cxx_std_20)
    add_test(NAME ui_pointer_ownership COMMAND cojvr_ui_pointer_ownership_test)

    add_executable(cojvr_ui_cursor_jni_test tests/ui_cursor_jni.cpp src/games/call_of_juarez/body_adapter.cpp src/games/call_of_juarez/hud_text_reader.cpp src/games/call_of_juarez/gameplay_ui_reader.cpp src/games/call_of_juarez/focus_zoom.cpp src/games/call_of_juarez/motion_reload_owner.cpp)
    target_link_libraries(cojvr_ui_cursor_jni_test PRIVATE cojvr_runtime)
    target_include_directories(cojvr_ui_cursor_jni_test PRIVATE src)
    target_compile_features(cojvr_ui_cursor_jni_test PRIVATE cxx_std_20)
    target_compile_definitions(cojvr_ui_cursor_jni_test PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    add_test(NAME ui_cursor_jni COMMAND cojvr_ui_cursor_jni_test)
    add_executable(cojvr_gameplay_input_jni_test tests/gameplay_input_jni.cpp
        src/games/call_of_juarez/body_adapter.cpp src/games/call_of_juarez/hud_text_reader.cpp src/games/call_of_juarez/gameplay_ui_reader.cpp src/games/call_of_juarez/focus_zoom.cpp src/games/call_of_juarez/motion_reload_owner.cpp)
    target_link_libraries(cojvr_gameplay_input_jni_test PRIVATE cojvr_runtime)
    target_include_directories(cojvr_gameplay_input_jni_test PRIVATE src)
    target_compile_features(cojvr_gameplay_input_jni_test PRIVATE cxx_std_20)
    target_compile_definitions(cojvr_gameplay_input_jni_test PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    add_test(NAME gameplay_input_jni COMMAND cojvr_gameplay_input_jni_test)
    add_executable(cojvr_whip_tracking_jni_test tests/whip_tracking_jni.cpp
        src/games/call_of_juarez/body_adapter.cpp src/games/call_of_juarez/hud_text_reader.cpp src/games/call_of_juarez/gameplay_ui_reader.cpp src/games/call_of_juarez/focus_zoom.cpp src/games/call_of_juarez/motion_reload_owner.cpp)
    target_link_libraries(cojvr_whip_tracking_jni_test PRIVATE cojvr_runtime)
    target_include_directories(cojvr_whip_tracking_jni_test PRIVATE src)
    target_compile_features(cojvr_whip_tracking_jni_test PRIVATE cxx_std_20)
    target_compile_definitions(cojvr_whip_tracking_jni_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    add_test(NAME whip_tracking_jni COMMAND cojvr_whip_tracking_jni_test)
    add_executable(cojvr_single_round_reload_jni_test tests/single_round_reload_jni.cpp
        src/games/call_of_juarez/body_adapter.cpp src/games/call_of_juarez/hud_text_reader.cpp
        src/games/call_of_juarez/gameplay_ui_reader.cpp src/games/call_of_juarez/focus_zoom.cpp
        src/games/call_of_juarez/motion_reload_owner.cpp)
    target_link_libraries(cojvr_single_round_reload_jni_test PRIVATE cojvr_runtime)
    target_include_directories(cojvr_single_round_reload_jni_test PRIVATE src)
    target_compile_features(cojvr_single_round_reload_jni_test PRIVATE cxx_std_20)
    target_compile_definitions(cojvr_single_round_reload_jni_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    if(MSVC)
        target_compile_options(cojvr_single_round_reload_jni_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME single_round_reload_jni COMMAND cojvr_single_round_reload_jni_test)

    add_executable(cojvr_native_fx_probe_test tests/native_fx_probe.cpp
        src/games/call_of_juarez/native_fx_probe.cpp)
    target_include_directories(cojvr_native_fx_probe_test PRIVATE src)
    target_compile_features(cojvr_native_fx_probe_test PRIVATE cxx_std_20)
    target_compile_definitions(cojvr_native_fx_probe_test PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    add_test(NAME native_fx_probe COMMAND cojvr_native_fx_probe_test)

    if(TARGET cojvr_d3d9_native_stereo_proxy)
        add_executable(cojvr_flat_ui_delivery_test
            tests/flat_ui_delivery.cpp
            src/games/call_of_juarez/d3d9ex_legacy_resource_compat.cpp
            src/backends/d3d9/d3d9_stereo_capture.cpp
            src/backends/d3d9/system_d3d9.cpp)
        target_include_directories(cojvr_flat_ui_delivery_test PRIVATE src)
        target_compile_features(cojvr_flat_ui_delivery_test PRIVATE cxx_std_20)
        target_compile_definitions(cojvr_flat_ui_delivery_test PRIVATE
            NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
        target_link_libraries(cojvr_flat_ui_delivery_test PRIVATE
            cojvr_coj_camera_probe cojvr_diagnostics cojvr_d3d9_hooks
            cojvr_openvr_d3d11_backend d3d11 dxgi dxguid)
        add_test(NAME flat_ui_delivery COMMAND cojvr_flat_ui_delivery_test)
    endif()

    add_executable(cojvr_openvr_state_test tests/openvr_state.cpp)
    target_include_directories(cojvr_openvr_state_test PRIVATE src)
    target_compile_features(cojvr_openvr_state_test PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(cojvr_openvr_state_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME openvr_state COMMAND cojvr_openvr_state_test)

    add_executable(
        cojvr_d3d11_sync_test
        tests/d3d11_sync.cpp
        src/backends/openvr/d3d11_sync.cpp
    )
    target_include_directories(cojvr_d3d11_sync_test PRIVATE src)
    target_compile_features(cojvr_d3d11_sync_test PRIVATE cxx_std_20)
    target_compile_definitions(
        cojvr_d3d11_sync_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_link_libraries(cojvr_d3d11_sync_test PRIVATE d3d11)
    if(MSVC)
        target_compile_options(cojvr_d3d11_sync_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME d3d11_sync COMMAND cojvr_d3d11_sync_test)

    add_executable(
        cojvr_d3d9_stereo_capture_test
        tests/d3d9_stereo_capture.cpp
        src/backends/d3d9/d3d9_stereo_capture.cpp
        src/backends/d3d9/system_d3d9.cpp
    )
    target_include_directories(cojvr_d3d9_stereo_capture_test PRIVATE src)
    target_compile_features(cojvr_d3d9_stereo_capture_test PRIVATE cxx_std_20)
    target_compile_definitions(
        cojvr_d3d9_stereo_capture_test
        PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
    )
    if(MSVC)
        target_compile_options(cojvr_d3d9_stereo_capture_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME d3d9_stereo_capture COMMAND cojvr_d3d9_stereo_capture_test)
    set_tests_properties(d3d9_stereo_capture PROPERTIES SKIP_RETURN_CODE 77)

    if(COJVR_ENABLE_OPENVR)
        add_executable(cojvr_openvr_runtime_test tests/openvr_runtime.cpp)
        target_link_libraries(cojvr_openvr_runtime_test PRIVATE cojvr_openvr_runtime)
        target_compile_features(cojvr_openvr_runtime_test PRIVATE cxx_std_20)
        if(MSVC)
            target_compile_options(cojvr_openvr_runtime_test PRIVATE /W4 /permissive- /EHsc)
        endif()
        add_test(NAME openvr_runtime COMMAND cojvr_openvr_runtime_test)
    endif()

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
    set_tests_properties(d3d9_d3d11_shared_texture PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(
        cojvr_d3d9ex_gpu_transport_test
        tests/d3d9ex_gpu_transport.cpp
        src/backends/d3d9/d3d9_stereo_capture.cpp
        src/backends/d3d9/system_d3d9.cpp
        src/backends/openvr/d3d11_sync.cpp
        src/backends/openvr/d3d9_shared_texture_bridge.cpp
    )
    target_include_directories(cojvr_d3d9ex_gpu_transport_test PRIVATE src)
    target_compile_features(cojvr_d3d9ex_gpu_transport_test PRIVATE cxx_std_20)
    target_compile_definitions(
        cojvr_d3d9ex_gpu_transport_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(cojvr_d3d9ex_gpu_transport_test PRIVATE d3d11 dxgi)
    if(MSVC)
        target_compile_options(
            cojvr_d3d9ex_gpu_transport_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME d3d9ex_gpu_transport COMMAND cojvr_d3d9ex_gpu_transport_test)
    set_tests_properties(d3d9ex_gpu_transport PROPERTIES SKIP_RETURN_CODE 77)

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
    set_tests_properties(d3d9_classic_d3d11_shared_texture PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(cojvr_d3d9ex_legacy_resource_contract_test
        tests/d3d9ex_legacy_resource_contract.cpp
        src/backends/d3d9/system_d3d9.cpp
    )
    target_include_directories(cojvr_d3d9ex_legacy_resource_contract_test PRIVATE src)
    target_compile_features(cojvr_d3d9ex_legacy_resource_contract_test PRIVATE cxx_std_20)
    target_compile_definitions(
        cojvr_d3d9ex_legacy_resource_contract_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(
            cojvr_d3d9ex_legacy_resource_contract_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME d3d9ex_legacy_resource_contract COMMAND cojvr_d3d9ex_legacy_resource_contract_test)
    set_tests_properties(d3d9ex_legacy_resource_contract PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(cojvr_d3d9ex_legacy_resource_compat_test
        tests/d3d9ex_legacy_resource_compat.cpp
        src/games/call_of_juarez/d3d9ex_legacy_resource_compat.cpp
        src/backends/d3d9/system_d3d9.cpp
    )
    target_include_directories(cojvr_d3d9ex_legacy_resource_compat_test PRIVATE src)
    target_compile_features(cojvr_d3d9ex_legacy_resource_compat_test PRIVATE cxx_std_20)
    target_compile_definitions(
        cojvr_d3d9ex_legacy_resource_compat_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(
        cojvr_d3d9ex_legacy_resource_compat_test PRIVATE cojvr_d3d9_hooks dxguid)
    if(MSVC)
        target_compile_options(
            cojvr_d3d9ex_legacy_resource_compat_test PRIVATE /W4 /permissive- /EHsc)
    endif()
    add_test(NAME d3d9ex_legacy_resource_compat COMMAND cojvr_d3d9ex_legacy_resource_compat_test)
    set_tests_properties(d3d9ex_legacy_resource_compat PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(cojvr_game_shutdown_hook_test
        tests/game_shutdown_hook.cpp
        src/games/call_of_juarez/game_shutdown_hook.cpp
    )
    target_include_directories(cojvr_game_shutdown_hook_test PRIVATE src)
    target_compile_features(cojvr_game_shutdown_hook_test PRIVATE cxx_std_20)
    target_compile_definitions(cojvr_game_shutdown_hook_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(cojvr_game_shutdown_hook_test PRIVATE cojvr_build_identity cojvr_d3d9_hooks)
    add_test(NAME game_shutdown_hook COMMAND cojvr_game_shutdown_hook_test)
    if(MSVC)
        target_compile_options(cojvr_game_shutdown_hook_test PRIVATE /W4 /permissive- /EHsc)
    endif()

    add_library(cojvr_game_shutdown_fixture SHARED
        tests/game_shutdown_dll.cpp
        src/games/call_of_juarez/game_shutdown_hook.cpp
    )
    target_include_directories(cojvr_game_shutdown_fixture PRIVATE src)
    target_compile_features(cojvr_game_shutdown_fixture PRIVATE cxx_std_20)
    target_compile_definitions(cojvr_game_shutdown_fixture PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(cojvr_game_shutdown_fixture PRIVATE cojvr_build_identity cojvr_d3d9_hooks)
    add_executable(cojvr_game_shutdown_host tests/game_shutdown_host.cpp)
    add_test(NAME game_shutdown_dll_lifecycle COMMAND pwsh -NoProfile -File
        "${CMAKE_SOURCE_DIR}/tests/run_game_shutdown_lifecycle.ps1"
        -HostPath "$<TARGET_FILE:cojvr_game_shutdown_host>"
        -FixturePath "$<TARGET_FILE:cojvr_game_shutdown_fixture>"
    )
    set_tests_properties(game_shutdown_dll_lifecycle PROPERTIES TIMEOUT 30)

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
    set_tests_properties(d3d9_classic_readback_d3d11 PROPERTIES SKIP_RETURN_CODE 77)
endif()
