#pragma once

#include "runtime/vr_types.hpp"

#include <string>

struct ID3D11Texture2D;

namespace cojvr::runtime {
class OpenVrRuntime;
}

namespace cojvr::backends::openvr {

enum class EyeSubmission { Left, Right };

// SteamVR can reject the first scene submission with DoNotHaveFocus while the
// compositor transfers scene ownership to this process. This is an expected
// acquisition transient, not a sustained transport failure.
[[nodiscard]] bool IsOpenVrSceneFocusPending(int runtime_result) noexcept;

[[nodiscard]] bool SubmitEyeD3D11(
    runtime::OpenVrRuntime& runtime,
    ID3D11Texture2D* texture,
    EyeSubmission eye,
    int& runtime_result,
    std::string& error) noexcept;

[[nodiscard]] bool SubmitEyeD3D11WithPose(
    runtime::OpenVrRuntime& runtime,
    ID3D11Texture2D* texture,
    EyeSubmission eye,
    const runtime::Pose& render_hmd_pose,
    int& runtime_result,
    std::string& error) noexcept;

[[nodiscard]] bool SubmitStereoD3D11(
    runtime::OpenVrRuntime& runtime,
    ID3D11Texture2D* left_eye,
    ID3D11Texture2D* right_eye,
    std::string& error) noexcept;

} // namespace cojvr::backends::openvr
