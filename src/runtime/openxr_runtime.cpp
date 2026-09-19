#include "runtime/openxr_runtime.hpp"
#include "runtime/vr_math.hpp"

#include <openxr/openxr.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace cojvr::runtime {
namespace {

std::string ResultMessage(const char* operation, XrResult result) {
    return std::string(operation) + " failed with XrResult " + std::to_string(result);
}

bool HasExtension(
    std::span<const XrExtensionProperties> available, const char* required) noexcept {
    return std::any_of(available.begin(), available.end(), [required](const auto& extension) {
        return std::strcmp(extension.extensionName, required) == 0;
    });
}

} // namespace

struct OpenXrRuntime::Impl {
    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    XrSession session = XR_NULL_HANDLE;
    XrSpace local_space = XR_NULL_HANDLE;
    std::vector<XrSwapchain> swapchains;
    XrEnvironmentBlendMode environment_blend_mode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    bool frame_active = false;
    OpenXrStatus status = OpenXrStatus::idle;
    OpenXrSystemInfo system_info{};
    std::array<EyeRenderRecommendation, 2> recommended_views{{
        EyeRenderRecommendation{Eye::left}, EyeRenderRecommendation{Eye::right}}};
    std::string last_error;

    void Fail(std::string message) noexcept {
        last_error = std::move(message);
        status = OpenXrStatus::failed;
    }
};

OpenXrRuntime::OpenXrRuntime() : impl_(std::make_unique<Impl>()) {}
OpenXrRuntime::~OpenXrRuntime() { Shutdown(); }
OpenXrRuntime::OpenXrRuntime(OpenXrRuntime&&) noexcept = default;
OpenXrRuntime& OpenXrRuntime::operator=(OpenXrRuntime&&) noexcept = default;

bool OpenXrRuntime::Initialize(const OpenXrInitOptions& options) noexcept {
    if (!impl_) return false;
    if (impl_->status != OpenXrStatus::idle) {
        impl_->Fail("OpenXR runtime is already initialized");
        return false;
    }

    try {
        std::uint32_t extension_count = 0;
        XrResult result = xrEnumerateInstanceExtensionProperties(nullptr, 0, &extension_count, nullptr);
        if (XR_FAILED(result)) {
            impl_->Fail(ResultMessage("xrEnumerateInstanceExtensionProperties", result));
            return false;
        }

        std::vector<XrExtensionProperties> extensions(
            extension_count, XrExtensionProperties{XR_TYPE_EXTENSION_PROPERTIES});
        result = xrEnumerateInstanceExtensionProperties(
            nullptr, extension_count, &extension_count, extensions.data());
        if (XR_FAILED(result)) {
            impl_->Fail(ResultMessage("xrEnumerateInstanceExtensionProperties", result));
            return false;
        }

        for (const char* required : options.required_extensions) {
            if (!required || !HasExtension(extensions, required)) {
                impl_->Fail(std::string("required OpenXR extension unavailable: ") +
                            (required ? required : "<null>"));
                return false;
            }
        }

        XrInstanceCreateInfo create_info{XR_TYPE_INSTANCE_CREATE_INFO};
        strncpy_s(create_info.applicationInfo.applicationName,
                  XR_MAX_APPLICATION_NAME_SIZE, "Call of Juarez VR", _TRUNCATE);
        create_info.applicationInfo.applicationVersion = 1;
        strncpy_s(create_info.applicationInfo.engineName,
                  XR_MAX_ENGINE_NAME_SIZE, "Chrome Engine", _TRUNCATE);
        create_info.applicationInfo.engineVersion = 1;
        // SteamVR currently exposes an OpenXR 1.0 runtime on the primary
        // validation path. The framework only uses 1.0 core entry points here,
        // so request the oldest compatible core version instead of inheriting
        // the newer header/loader version through XR_CURRENT_API_VERSION.
        create_info.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
        create_info.enabledExtensionCount = static_cast<std::uint32_t>(options.required_extensions.size());
        create_info.enabledExtensionNames = options.required_extensions.data();

        result = xrCreateInstance(&create_info, &impl_->instance);
        if (XR_FAILED(result)) {
            impl_->Fail(ResultMessage("xrCreateInstance", result));
            return false;
        }

        XrSystemGetInfo system_get_info{XR_TYPE_SYSTEM_GET_INFO};
        system_get_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        result = xrGetSystem(impl_->instance, &system_get_info, &impl_->system);
        if (XR_FAILED(result)) {
            impl_->Fail(ResultMessage("xrGetSystem", result));
            Shutdown();
            impl_->status = OpenXrStatus::failed;
            return false;
        }

        XrSystemProperties properties{XR_TYPE_SYSTEM_PROPERTIES};
        result = xrGetSystemProperties(impl_->instance, impl_->system, &properties);
        if (XR_FAILED(result)) {
            impl_->Fail(ResultMessage("xrGetSystemProperties", result));
            Shutdown();
            impl_->status = OpenXrStatus::failed;
            return false;
        }

        impl_->system_info.name = properties.systemName;
        impl_->system_info.vendor_id = properties.vendorId;
        impl_->system_info.max_swapchain_width = properties.graphicsProperties.maxSwapchainImageWidth;
        impl_->system_info.max_swapchain_height = properties.graphicsProperties.maxSwapchainImageHeight;
        impl_->system_info.orientation_tracking = properties.trackingProperties.orientationTracking == XR_TRUE;
        impl_->system_info.position_tracking = properties.trackingProperties.positionTracking == XR_TRUE;

        std::uint32_t blend_mode_count = 0;
        result = xrEnumerateEnvironmentBlendModes(
            impl_->instance, impl_->system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            0, &blend_mode_count, nullptr);
        if (XR_FAILED(result) || blend_mode_count == 0) {
            impl_->Fail(XR_FAILED(result)
                            ? ResultMessage("xrEnumerateEnvironmentBlendModes", result)
                            : "primary stereo view configuration exposed no environment blend mode");
            Shutdown();
            impl_->status = OpenXrStatus::failed;
            return false;
        }

        std::vector<XrEnvironmentBlendMode> blend_modes(blend_mode_count);
        result = xrEnumerateEnvironmentBlendModes(
            impl_->instance, impl_->system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            blend_mode_count, &blend_mode_count, blend_modes.data());
        if (XR_FAILED(result)) {
            impl_->Fail(ResultMessage("xrEnumerateEnvironmentBlendModes", result));
            Shutdown();
            impl_->status = OpenXrStatus::failed;
            return false;
        }
        const auto opaque = std::find(
            blend_modes.begin(), blend_modes.end(), XR_ENVIRONMENT_BLEND_MODE_OPAQUE);
        impl_->environment_blend_mode =
            opaque != blend_modes.end() ? XR_ENVIRONMENT_BLEND_MODE_OPAQUE : blend_modes.front();

        std::uint32_t view_count = 0;
        result = xrEnumerateViewConfigurationViews(
            impl_->instance, impl_->system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            0, &view_count, nullptr);
        if (XR_FAILED(result) || view_count != 2) {
            impl_->Fail(XR_FAILED(result)
                            ? ResultMessage("xrEnumerateViewConfigurationViews", result)
                            : "primary stereo view configuration did not expose exactly two eyes");
            Shutdown();
            impl_->status = OpenXrStatus::failed;
            return false;
        }

        std::array<XrViewConfigurationView, 2> views{{
            XrViewConfigurationView{XR_TYPE_VIEW_CONFIGURATION_VIEW},
            XrViewConfigurationView{XR_TYPE_VIEW_CONFIGURATION_VIEW},
        }};
        result = xrEnumerateViewConfigurationViews(
            impl_->instance, impl_->system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            static_cast<std::uint32_t>(views.size()), &view_count, views.data());
        if (XR_FAILED(result)) {
            impl_->Fail(ResultMessage("xrEnumerateViewConfigurationViews", result));
            Shutdown();
            impl_->status = OpenXrStatus::failed;
            return false;
        }

        for (std::size_t index = 0; index < views.size(); ++index) {
            impl_->recommended_views[index].width = views[index].recommendedImageRectWidth;
            impl_->recommended_views[index].height = views[index].recommendedImageRectHeight;
        }

        impl_->last_error.clear();
        impl_->status = OpenXrStatus::system_ready;
        return true;
    } catch (...) {
        impl_->Fail("unexpected exception while initializing OpenXR");
        Shutdown();
        impl_->status = OpenXrStatus::failed;
        return false;
    }
}

bool OpenXrRuntime::CreateSession(OpenXrGraphicsBinding graphics_binding) noexcept {
    if (!impl_ || impl_->status != OpenXrStatus::system_ready) {
        if (impl_) impl_->Fail("OpenXR system is not ready for session creation");
        return false;
    }
    if (!graphics_binding.native) {
        impl_->Fail("OpenXR graphics binding is null");
        return false;
    }

    XrSessionCreateInfo session_info{XR_TYPE_SESSION_CREATE_INFO};
    session_info.next = graphics_binding.native;
    session_info.systemId = impl_->system;
    const XrResult session_result = xrCreateSession(impl_->instance, &session_info, &impl_->session);
    if (XR_FAILED(session_result)) {
        impl_->Fail(ResultMessage("xrCreateSession", session_result));
        return false;
    }

    XrReferenceSpaceCreateInfo space_info{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    space_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    space_info.poseInReferenceSpace.orientation.w = 1.0F;
    const XrResult space_result = xrCreateReferenceSpace(impl_->session, &space_info, &impl_->local_space);
    if (XR_FAILED(space_result)) {
        xrDestroySession(impl_->session);
        impl_->session = XR_NULL_HANDLE;
        impl_->Fail(ResultMessage("xrCreateReferenceSpace", space_result));
        return false;
    }

    impl_->last_error.clear();
    impl_->status = OpenXrStatus::session_ready;
    return true;
}

void OpenXrRuntime::DestroySession() noexcept {
    if (!impl_) return;
    impl_->frame_active = false;
    for (auto iterator = impl_->swapchains.rbegin(); iterator != impl_->swapchains.rend(); ++iterator) {
        if (*iterator != XR_NULL_HANDLE) xrDestroySwapchain(*iterator);
    }
    impl_->swapchains.clear();
    if (impl_->local_space != XR_NULL_HANDLE) {
        xrDestroySpace(impl_->local_space);
        impl_->local_space = XR_NULL_HANDLE;
    }
    if (impl_->session != XR_NULL_HANDLE) {
        xrDestroySession(impl_->session);
        impl_->session = XR_NULL_HANDLE;
    }
    if (impl_->status != OpenXrStatus::failed && impl_->instance != XR_NULL_HANDLE) {
        impl_->status = OpenXrStatus::system_ready;
    }
}

bool OpenXrRuntime::PollEvents() noexcept {
    if (!impl_ || impl_->instance == XR_NULL_HANDLE) return false;

    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    while (true) {
        const XrResult result = xrPollEvent(impl_->instance, &event);
        if (result == XR_EVENT_UNAVAILABLE) return true;
        if (XR_FAILED(result)) {
            impl_->Fail(ResultMessage("xrPollEvent", result));
            return false;
        }

        if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED && impl_->session != XR_NULL_HANDLE) {
            const auto& changed = reinterpret_cast<const XrEventDataSessionStateChanged&>(event);
            if (changed.session == impl_->session) {
                if (changed.state == XR_SESSION_STATE_READY) {
                    XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};
                    begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    const XrResult begin_result = xrBeginSession(impl_->session, &begin);
                    if (XR_FAILED(begin_result)) {
                        impl_->Fail(ResultMessage("xrBeginSession", begin_result));
                        return false;
                    }
                    impl_->status = OpenXrStatus::running;
                } else if (changed.state == XR_SESSION_STATE_STOPPING) {
                    const XrResult end_result = xrEndSession(impl_->session);
                    if (XR_FAILED(end_result)) {
                        impl_->Fail(ResultMessage("xrEndSession", end_result));
                        return false;
                    }
                    impl_->status = OpenXrStatus::session_ready;
                } else if (changed.state == XR_SESSION_STATE_EXITING ||
                           changed.state == XR_SESSION_STATE_LOSS_PENDING) {
                    impl_->status = OpenXrStatus::exit_requested;
                }
            }
        }

        event = XrEventDataBuffer{XR_TYPE_EVENT_DATA_BUFFER};
    }
}

bool OpenXrRuntime::EnumerateSwapchainFormats(
    std::span<std::int64_t> formats, std::uint32_t& format_count) noexcept {
    if (!impl_ || impl_->session == XR_NULL_HANDLE) {
        if (impl_) impl_->last_error = "OpenXR session is unavailable for swapchain format enumeration";
        return false;
    }

    const XrResult result = xrEnumerateSwapchainFormats(
        impl_->session, static_cast<std::uint32_t>(formats.size()),
        &format_count, formats.data());
    if (XR_FAILED(result)) {
        impl_->Fail(ResultMessage("xrEnumerateSwapchainFormats", result));
        return false;
    }
    impl_->last_error.clear();
    return true;
}

bool OpenXrRuntime::CreateColorSwapchain(
    std::int64_t format, std::uint32_t width, std::uint32_t height,
    std::uint32_t sample_count, OpenXrSwapchainHandle& swapchain) noexcept {
    if (!impl_ || impl_->session == XR_NULL_HANDLE || width == 0 || height == 0 || sample_count == 0) {
        if (impl_) impl_->last_error = "invalid OpenXR color swapchain request";
        return false;
    }

    XrSwapchainCreateInfo create_info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    create_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    create_info.format = format;
    create_info.sampleCount = sample_count;
    create_info.width = width;
    create_info.height = height;
    create_info.faceCount = 1;
    create_info.arraySize = 1;
    create_info.mipCount = 1;

    XrSwapchain native_swapchain = XR_NULL_HANDLE;
    const XrResult result = xrCreateSwapchain(impl_->session, &create_info, &native_swapchain);
    if (XR_FAILED(result)) {
        impl_->Fail(ResultMessage("xrCreateSwapchain", result));
        return false;
    }

    impl_->swapchains.push_back(native_swapchain);
    swapchain.native = reinterpret_cast<void*>(native_swapchain);
    impl_->last_error.clear();
    return true;
}

bool OpenXrRuntime::DestroySwapchain(OpenXrSwapchainHandle& swapchain) noexcept {
    if (!impl_ || !swapchain.native) return true;
    const auto native_swapchain = reinterpret_cast<XrSwapchain>(swapchain.native);
    const XrResult result = xrDestroySwapchain(native_swapchain);
    if (XR_FAILED(result)) {
        impl_->last_error = ResultMessage("xrDestroySwapchain", result);
        return false;
    }

    const auto found = std::find(impl_->swapchains.begin(), impl_->swapchains.end(), native_swapchain);
    if (found != impl_->swapchains.end()) impl_->swapchains.erase(found);
    swapchain.native = nullptr;
    impl_->last_error.clear();
    return true;
}

bool OpenXrRuntime::EnumerateSwapchainImages(
    OpenXrSwapchainHandle swapchain, std::uint32_t capacity,
    std::uint32_t& image_count, void* images) noexcept {
    if (!impl_ || !swapchain.native) {
        if (impl_) impl_->last_error = "OpenXR swapchain is unavailable for image enumeration";
        return false;
    }

    const XrResult result = xrEnumerateSwapchainImages(
        reinterpret_cast<XrSwapchain>(swapchain.native), capacity, &image_count,
        static_cast<XrSwapchainImageBaseHeader*>(images));
    if (XR_FAILED(result)) {
        impl_->Fail(ResultMessage("xrEnumerateSwapchainImages", result));
        return false;
    }
    impl_->last_error.clear();
    return true;
}

bool OpenXrRuntime::AcquireSwapchainImage(
    OpenXrSwapchainHandle swapchain, std::uint32_t& image_index) noexcept {
    if (!impl_ || !swapchain.native) {
        if (impl_) impl_->last_error = "OpenXR swapchain is unavailable for image acquisition";
        return false;
    }

    XrSwapchainImageAcquireInfo acquire_info{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    const XrResult result = xrAcquireSwapchainImage(
        reinterpret_cast<XrSwapchain>(swapchain.native), &acquire_info, &image_index);
    if (XR_FAILED(result)) {
        impl_->Fail(ResultMessage("xrAcquireSwapchainImage", result));
        return false;
    }
    impl_->last_error.clear();
    return true;
}

bool OpenXrRuntime::WaitSwapchainImage(OpenXrSwapchainHandle swapchain) noexcept {
    if (!impl_ || !swapchain.native) {
        if (impl_) impl_->last_error = "OpenXR swapchain is unavailable for image wait";
        return false;
    }

    XrSwapchainImageWaitInfo wait_info{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    wait_info.timeout = XR_INFINITE_DURATION;
    const XrResult result = xrWaitSwapchainImage(
        reinterpret_cast<XrSwapchain>(swapchain.native), &wait_info);
    if (XR_FAILED(result)) {
        impl_->Fail(ResultMessage("xrWaitSwapchainImage", result));
        return false;
    }
    impl_->last_error.clear();
    return true;
}

bool OpenXrRuntime::ReleaseSwapchainImage(OpenXrSwapchainHandle swapchain) noexcept {
    if (!impl_ || !swapchain.native) {
        if (impl_) impl_->last_error = "OpenXR swapchain is unavailable for image release";
        return false;
    }

    XrSwapchainImageReleaseInfo release_info{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    const XrResult result = xrReleaseSwapchainImage(
        reinterpret_cast<XrSwapchain>(swapchain.native), &release_info);
    if (XR_FAILED(result)) {
        impl_->Fail(ResultMessage("xrReleaseSwapchainImage", result));
        return false;
    }
    impl_->last_error.clear();
    return true;
}

bool OpenXrRuntime::WaitBeginFrame(OpenXrFrameState& frame) noexcept {
    if (!impl_ || impl_->session == XR_NULL_HANDLE || impl_->status != OpenXrStatus::running) {
        if (impl_) impl_->last_error = "OpenXR session is not running";
        return false;
    }
    if (impl_->frame_active) {
        impl_->last_error = "OpenXR frame is already active";
        return false;
    }

    XrFrameWaitInfo wait_info{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState xr_frame{XR_TYPE_FRAME_STATE};
    XrResult result = xrWaitFrame(impl_->session, &wait_info, &xr_frame);
    if (XR_FAILED(result)) {
        impl_->Fail(ResultMessage("xrWaitFrame", result));
        return false;
    }

    XrFrameBeginInfo begin_info{XR_TYPE_FRAME_BEGIN_INFO};
    result = xrBeginFrame(impl_->session, &begin_info);
    if (XR_FAILED(result)) {
        impl_->Fail(ResultMessage("xrBeginFrame", result));
        return false;
    }

    frame.predicted_display_time = static_cast<std::int64_t>(xr_frame.predictedDisplayTime);
    frame.predicted_display_period = static_cast<std::int64_t>(xr_frame.predictedDisplayPeriod);
    frame.should_render = xr_frame.shouldRender == XR_TRUE;
    impl_->frame_active = true;
    impl_->last_error.clear();
    return true;
}

bool OpenXrRuntime::LocateStereoViews(
    const OpenXrFrameState& frame, std::array<LocatedEyeView, 2>& views) noexcept {
    if (!impl_ || !impl_->frame_active || impl_->local_space == XR_NULL_HANDLE) {
        if (impl_) impl_->last_error = "OpenXR frame/reference space is not ready for view location";
        return false;
    }

    XrViewLocateInfo locate_info{XR_TYPE_VIEW_LOCATE_INFO};
    locate_info.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    locate_info.displayTime = static_cast<XrTime>(frame.predicted_display_time);
    locate_info.space = impl_->local_space;

    XrViewState view_state{XR_TYPE_VIEW_STATE};
    std::array<XrView, 2> xr_views{{XrView{XR_TYPE_VIEW}, XrView{XR_TYPE_VIEW}}};
    std::uint32_t view_count = 0;
    const XrResult result = xrLocateViews(
        impl_->session, &locate_info, &view_state,
        static_cast<std::uint32_t>(xr_views.size()), &view_count, xr_views.data());
    if (XR_FAILED(result)) {
        impl_->Fail(ResultMessage("xrLocateViews", result));
        return false;
    }
    if (view_count != xr_views.size()) {
        impl_->Fail("xrLocateViews did not return exactly two stereo views");
        return false;
    }

    const bool orientation_valid =
        (view_state.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) != 0;
    const bool position_valid =
        (view_state.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) != 0;
    if (!orientation_valid || !position_valid) {
        impl_->Fail("xrLocateViews returned incomplete stereo pose validity");
        return false;
    }

    for (std::size_t index = 0; index < xr_views.size(); ++index) {
        const EyeRenderRecommendation& recommendation = impl_->recommended_views[index];
        LocatedEyeView view{};
        view.eye = recommendation.eye;
        view.width = recommendation.width;
        view.height = recommendation.height;
        const auto& xr_view = xr_views[index];
        view.tracking_from_eye.orientation = Quaternion{
            xr_view.pose.orientation.x,
            xr_view.pose.orientation.y,
            xr_view.pose.orientation.z,
            xr_view.pose.orientation.w,
        };
        view.tracking_from_eye.position = Vec3{
            xr_view.pose.position.x,
            xr_view.pose.position.y,
            xr_view.pose.position.z,
        };
        view.tracking_from_eye.orientation_valid = orientation_valid;
        view.tracking_from_eye.position_valid = position_valid;
        view.fov = EyeFov{
            xr_view.fov.angleLeft,
            xr_view.fov.angleRight,
            xr_view.fov.angleUp,
            xr_view.fov.angleDown,
        };
        std::array<float, 12> validated_transform{};
        if (!RigidTransform3x4FromPose(view.tracking_from_eye, validated_transform) ||
            !IsValidEyeFov(view.fov)) {
            views = {};
            impl_->Fail("xrLocateViews returned invalid/non-finite stereo view data");
            return false;
        }
        views[index] = view;
    }

    impl_->last_error.clear();
    return true;
}

bool OpenXrRuntime::EndFrame(
    const OpenXrFrameState& frame,
    std::span<const OpenXrCompositionLayer> layers) noexcept {
    if (!impl_ || !impl_->frame_active || impl_->session == XR_NULL_HANDLE) {
        if (impl_) impl_->last_error = "OpenXR frame is not active";
        return false;
    }

    std::vector<const XrCompositionLayerBaseHeader*> native_layers;
    native_layers.reserve(layers.size());
    for (const auto& layer : layers) {
        if (!layer.native) {
            impl_->last_error = "OpenXR composition layer is null";
            return false;
        }
        native_layers.push_back(
            static_cast<const XrCompositionLayerBaseHeader*>(layer.native));
    }

    XrFrameEndInfo end_info{XR_TYPE_FRAME_END_INFO};
    end_info.displayTime = static_cast<XrTime>(frame.predicted_display_time);
    end_info.environmentBlendMode = impl_->environment_blend_mode;
    end_info.layerCount = static_cast<std::uint32_t>(native_layers.size());
    end_info.layers = native_layers.data();
    const XrResult result = xrEndFrame(impl_->session, &end_info);
    impl_->frame_active = false;
    if (XR_FAILED(result)) {
        impl_->Fail(ResultMessage("xrEndFrame", result));
        return false;
    }

    impl_->last_error.clear();
    return true;
}

void OpenXrRuntime::Shutdown() noexcept {
    if (!impl_) return;
    DestroySession();
    if (impl_->instance != XR_NULL_HANDLE) {
        xrDestroyInstance(impl_->instance);
        impl_->instance = XR_NULL_HANDLE;
    }
    impl_->system = XR_NULL_SYSTEM_ID;
    if (impl_->status != OpenXrStatus::failed) impl_->status = OpenXrStatus::idle;
}

OpenXrStatus OpenXrRuntime::status() const noexcept {
    return impl_ ? impl_->status : OpenXrStatus::failed;
}

const OpenXrSystemInfo& OpenXrRuntime::system_info() const noexcept {
    static const OpenXrSystemInfo empty{};
    return impl_ ? impl_->system_info : empty;
}

const std::array<EyeRenderRecommendation, 2>& OpenXrRuntime::recommended_views() const noexcept {
    static const std::array<EyeRenderRecommendation, 2> empty{{
        EyeRenderRecommendation{Eye::left}, EyeRenderRecommendation{Eye::right}}};
    return impl_ ? impl_->recommended_views : empty;
}

std::string_view OpenXrRuntime::last_error() const noexcept {
    return impl_ ? std::string_view(impl_->last_error) : std::string_view("OpenXR runtime unavailable");
}

void* OpenXrRuntime::native_instance() const noexcept {
    return impl_ ? reinterpret_cast<void*>(impl_->instance) : nullptr;
}

void* OpenXrRuntime::native_session() const noexcept {
    return impl_ ? reinterpret_cast<void*>(impl_->session) : nullptr;
}

void* OpenXrRuntime::native_reference_space() const noexcept {
    return impl_ ? reinterpret_cast<void*>(impl_->local_space) : nullptr;
}

std::uint64_t OpenXrRuntime::system_id() const noexcept {
    return impl_ ? static_cast<std::uint64_t>(impl_->system) : 0;
}

void* OpenXrRuntime::GetProcAddress(const char* name) const noexcept {
    if (!impl_ || impl_->instance == XR_NULL_HANDLE || !name) return nullptr;
    PFN_xrVoidFunction function = nullptr;
    if (XR_FAILED(xrGetInstanceProcAddr(impl_->instance, name, &function))) return nullptr;
    return reinterpret_cast<void*>(function);
}

} // namespace cojvr::runtime
